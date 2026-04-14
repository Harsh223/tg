/*
    tg
    Copyright (C) 2015 Marcello Mamino

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "tg.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef DEBUG
int testing = 0;
#endif

int preset_bph[] = PRESET_BPH;

struct watch_preset_entry {
	const char *name;
	int bph;
	double la;
};

static const struct watch_preset_entry watch_presets[] = {
	{ "Custom", 0, DEFAULT_LA },
	{ "ETA 2824 / SW200", 28800, 50 },
	{ "ETA 2892", 28800, 52 },
	{ "NH35 / 4R36", 21600, 53 },
	{ "Miyota 9015", 28800, 51 },
	{ "Rolex 31xx/32xx", 28800, 52 },
	{ "Vintage 18000", 18000, 52 },
	{ NULL, 0, 0 }
};

static const char *watch_positions[] = {
	"Unspecified",
	"DU",
	"DD",
	"CU",
	"CD",
	"CL",
	"CR",
	NULL
};

static void recompute(struct main_window *w);
static void update_position_summary(struct main_window *w);

void print_debug(char *format,...)
{
	va_list args;
	va_start(args,format);
	vfprintf(stderr,format,args);
	va_end(args);
}

void error(char *format,...)
{
	char s[100];
	va_list args;

	va_start(args,format);
	int size = vsnprintf(s,100,format,args);
	va_end(args);

	char *t;
	if(size < 100) {
		t = s;
	} else {
		t = alloca(size+1);
		va_start(args,format);
		vsnprintf(t,size+1,format,args);
		va_end(args);
	}

	fprintf(stderr,"%s\n",t);

#ifdef DEBUG
	if(testing) return;
#endif

	GtkWidget *dialog = gtk_message_dialog_new(NULL,0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"%s",t);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static void refresh_results(struct main_window *w)
{
	w->active_snapshot->bph = w->bph;
	w->active_snapshot->la = w->la;
	w->active_snapshot->cal = w->cal;
	compute_results(w->active_snapshot);
}

static void safe_set_label_text(GtkWidget *widget, const char *text)
{
	if(!GTK_IS_LABEL(widget))
		return;
	gtk_label_set_text(GTK_LABEL(widget), text ? text : "");
}

static void sync_primary_actions(struct main_window *w, int can_snapshot, int can_save)
{
	gtk_widget_set_sensitive(w->snapshot_button, can_snapshot);
	if(w->header_snapshot_button)
		gtk_widget_set_sensitive(w->header_snapshot_button, can_snapshot);

	gtk_widget_set_sensitive(w->save_item, can_save);
	if(w->header_save_button)
		gtk_widget_set_sensitive(w->header_save_button, can_save);
}

#define THEME_MODE_SYSTEM 0
#define THEME_MODE_LIGHT 1
#define THEME_MODE_DARK 2

#ifdef __APPLE__
static int macos_prefers_dark_mode(void)
{
	int dark = 0;
	CFStringRef style = (CFStringRef)CFPreferencesCopyAppValue(
		CFSTR("AppleInterfaceStyle"),
		kCFPreferencesAnyApplication
	);

	if(style && CFGetTypeID(style) == CFStringGetTypeID()) {
		char buf[64];
		if(CFStringGetCString(style, buf, sizeof(buf), kCFStringEncodingUTF8)) {
			gchar *lower = g_ascii_strdown(buf, -1);
			if(lower && strstr(lower, "dark"))
				dark = 1;
			g_free(lower);
		}
	}
	if(style)
		CFRelease(style);

	return dark;
}
#endif

static int system_prefers_dark_mode(void)
{
	GtkSettings *settings = gtk_settings_get_default();
	gboolean prefer_dark = FALSE;
	gchar *theme_name = NULL;

	if(settings)
		g_object_get(settings,
			"gtk-application-prefer-dark-theme", &prefer_dark,
			"gtk-theme-name", &theme_name,
			NULL);

	if(theme_name) {
		gchar *lower = g_ascii_strdown(theme_name, -1);
		if(lower && (strstr(lower, "dark") || strstr(lower, "adwaita:dark")))
			prefer_dark = TRUE;
		g_free(lower);
		g_free(theme_name);
	}



#ifdef __APPLE__
	if(!prefer_dark)
		prefer_dark = macos_prefers_dark_mode();
#endif

	return prefer_dark ? 1 : 0;
}

static int resolved_dark_mode(const struct main_window *w)
{
	if(!w)
		return system_prefers_dark_mode();

	if(w->theme_mode == THEME_MODE_LIGHT)
		return 0;
	if(w->theme_mode == THEME_MODE_DARK)
		return 1;
	return system_prefers_dark_mode();
}

static void apply_modern_css(struct main_window *w)
{
	static GtkCssProvider *base_provider = NULL;
	static GtkCssProvider *dark_provider = NULL;
	static GtkCssProvider *light_provider = NULL;
	static int base_installed = 0;
	GdkScreen *screen;
	int use_dark;

	if(!w || !w->window)
		return;

	screen = gdk_screen_get_default();
	if(!screen)
		return;

	if(!base_provider) {
		const char *base_css =
			"#main-split {"
			"  border-width: 0;"
			"}"

			"#sidebar, #controls-bar, #status-bar, #summary-panel {"
			"  border-radius: 14px;"
			"  border-width: 1px;"
			"  border-style: solid;"
			"}"

			"#controls-bar, #status-bar, #summary-panel {"
			"  padding: 10px;"
			"}"

			"#controls-bar label, #status-bar label, #summary-panel label {"
			"  font-weight: 500;"
			"}"

			"#controls-bar entry, #controls-bar spinbutton, #controls-bar combobox box, #controls-bar combobox button {"
			"  border-radius: 10px;"
			"  border-width: 1px;"
			"  border-style: solid;"
			"  padding: 6px 9px;"
			"}"

			"menu, popover, popover.background {"
			"  border-width: 1px;"
			"  border-style: solid;"
			"  border-radius: 10px;"
			"}"

			"menuitem, modelbutton {"
			"  padding: 6px 10px;"
			"  border-width: 0;"
			"  border-radius: 6px;"
			"}"

			"#sidebar button, #controls-bar button, #status-bar button, #summary-panel button, #main-notebook button {"
			"  border-radius: 10px;"
			"  padding: 6px 10px;"
			"}"

			"#quality-badge {"
			"  font-weight: 700;"
			"  padding: 3px 12px;"
			"  border-radius: 999px;"
			"  border-width: 1px;"
			"  border-style: solid;"
			"}"
			"#quality-badge.quality-none { background-color: #64748b; color: #f8fafc; border-color: #7a8ca3; }"
			"#quality-badge.quality-unstable { background-color: #b91c1c; color: #fee2e2; border-color: #d24a4a; }"
			"#quality-badge.quality-weak { background-color: #b45309; color: #fff7d6; border-color: #cf7b3c; }"
			"#quality-badge.quality-stable { background-color: #15803d; color: #dcfce7; border-color: #35a663; }"
			"#quality-label { font-weight: 600; }";

		const char *dark_css =
			"#app-root { background-color: #0a0f1a; color: #e7eefc; }"
			"#sidebar-scroller { background-color: #0c1320; }"
			"#sidebar { background-color: #111a2b; border-color: #2d3b54; }"
			"#workspace-column { background-color: #0d1524; }"

			"headerbar {"
			"  background-color: #111a2b;"
			"  color: #e7eefc;"
			"  border-color: #2d3b54;"
			"}"
			"headerbar button {"
			"  background-color: #1a263a;"
			"  color: #e7eefc;"
			"  border-color: #3f5270;"
			"}"

			"#controls-bar, #status-bar, #summary-panel {"
			"  background-color: #172335;"
			"  border-color: #4e637f;"
			"  color: #edf3ff;"
			"}"

			"#controls-bar label, #status-bar label, #summary-panel label, #status-label {"
			"  color: #edf3ff;"
			"}"

			"#controls-bar entry, #controls-bar spinbutton, #controls-bar combobox box {"
			"  background-color: #101724;"
			"  color: #f4f8ff;"
			"  border-color: #607594;"
			"}"

			"#sidebar button, #controls-bar button, #status-bar button, #summary-panel button, #main-notebook button {"
			"  background-color: #1b2a41;"
			"  color: #edf3ff;"
			"  border-color: #3f5270;"
			"}"

			"#controls-bar combobox box, #controls-bar combobox button {"
			"  background-color: #0f1a2b;"
			"  color: #f4f8ff;"
			"  border-color: #607594;"
			"}"

			"menu, popover, popover.background {"
			"  background-color: #162238;"
			"  color: #edf3ff;"
			"  border-color: #4e637f;"
			"}"

			"menuitem, modelbutton {"
			"  color: #edf3ff;"
			"  background-color: transparent;"
			"}"

			"menuitem:hover, modelbutton:hover, menuitem:checked, modelbutton:checked {"
			"  background-color: #263a58;"
			"  color: #ffffff;"
			"}"

			"notebook, stack, #main-notebook {"
			"  background-color: #101828;"
			"}";

		const char *light_css =
			"#app-root { background-color: #edf2f8; color: #0f172a; }"
			"#sidebar-scroller { background-color: #e3eaf3; }"
			"#sidebar { background-color: #f4f8fd; border-color: #c4d0dd; }"
			"#workspace-column { background-color: #eef3f9; }"

			"headerbar {"
			"  background-color: #f7fafe;"
			"  color: #0f172a;"
			"  border-color: #c4d0dd;"
			"}"
			"headerbar button {"
			"  background-color: #ffffff;"
			"  color: #0f172a;"
			"  border-color: #b8c6d6;"
			"}"

			"#controls-bar, #status-bar, #summary-panel {"
			"  background-color: #f9fbff;"
			"  border-color: #b5c3d3;"
			"  color: #0f172a;"
			"}"

			"#controls-bar label, #status-bar label, #summary-panel label, #status-label {"
			"  color: #0f172a;"
			"}"

			"#controls-bar entry, #controls-bar spinbutton, #controls-bar combobox box {"
			"  background-color: #ffffff;"
			"  color: #0f172a;"
			"  border-color: #95a5ba;"
			"}"

			"#sidebar button, #controls-bar button, #status-bar button, #summary-panel button, #main-notebook button {"
			"  background-color: #ffffff;"
			"  color: #0f172a;"
			"  border-color: #b8c6d6;"
			"}"

			"#controls-bar combobox box, #controls-bar combobox button {"
			"  background-color: #ffffff;"
			"  color: #0f172a;"
			"  border-color: #95a5ba;"
			"}"

			"menu, popover, popover.background {"
			"  background-color: #ffffff;"
			"  color: #0f172a;"
			"  border-color: #b5c3d3;"
			"}"

			"menuitem, modelbutton {"
			"  color: #0f172a;"
			"  background-color: transparent;"
			"}"

			"menuitem:hover, modelbutton:hover, menuitem:checked, modelbutton:checked {"
			"  background-color: #e7eef8;"
			"  color: #0b1220;"
			"}"

			"notebook, stack, #main-notebook {"
			"  background-color: #f2f6fb;"
			"}";

		base_provider = gtk_css_provider_new();
		dark_provider = gtk_css_provider_new();
		light_provider = gtk_css_provider_new();

		gtk_css_provider_load_from_data(base_provider, base_css, -1, NULL);
		gtk_css_provider_load_from_data(dark_provider, dark_css, -1, NULL);
		gtk_css_provider_load_from_data(light_provider, light_css, -1, NULL);
	}

	if(!base_installed) {
		gtk_style_context_add_provider_for_screen(
			screen,
			GTK_STYLE_PROVIDER(base_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
		);
		base_installed = 1;
	}

	gtk_style_context_remove_provider_for_screen(screen, GTK_STYLE_PROVIDER(dark_provider));
	gtk_style_context_remove_provider_for_screen(screen, GTK_STYLE_PROVIDER(light_provider));

	use_dark = resolved_dark_mode(w);

	GtkSettings *settings = gtk_settings_get_default();
	if(settings)
		g_object_set(settings, "gtk-application-prefer-dark-theme", use_dark, NULL);

	gtk_style_context_add_provider_for_screen(
		screen,
		GTK_STYLE_PROVIDER(use_dark ? dark_provider : light_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1
	);
}

static void handle_system_theme_change(GObject *settings, GParamSpec *pspec, gpointer user_data)
{
	UNUSED(settings);
	UNUSED(pspec);
	struct main_window *w = user_data;
	if(!w || w->theme_mode != THEME_MODE_SYSTEM)
		return;

	int dark_now = system_prefers_dark_mode();
	if(dark_now != w->last_system_dark_mode) {
		w->last_system_dark_mode = dark_now;
		apply_modern_css(w);
	}
}

static guint theme_sync_timer(struct main_window *w)
{
	if(!w || w->zombie)
		return FALSE;

	if(w->theme_mode != THEME_MODE_SYSTEM)
		return TRUE;

	int dark_now = system_prefers_dark_mode();
	if(dark_now != w->last_system_dark_mode) {
		w->last_system_dark_mode = dark_now;
		apply_modern_css(w);
	}

	return TRUE;
}

static void set_quality_badge_class(GtkWidget *badge, const char *state_class)
{
	if(!badge)
		return;

	GtkStyleContext *context = gtk_widget_get_style_context(badge);
	if(!context)
		return;

	gtk_style_context_remove_class(context, "quality-none");
	gtk_style_context_remove_class(context, "quality-unstable");
	gtk_style_context_remove_class(context, "quality-weak");
	gtk_style_context_remove_class(context, "quality-stable");

	if(state_class)
		gtk_style_context_add_class(context, state_class);
}

static void apply_focus_mode(struct main_window *w)
{
	if(!w->notebook)
		return;

	if(w->focus_mode) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(w->notebook), FALSE);
		gtk_notebook_set_show_border(GTK_NOTEBOOK(w->notebook), FALSE);
	} else {
		int show_tabs = gtk_notebook_get_n_pages(GTK_NOTEBOOK(w->notebook)) > 1;
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(w->notebook), show_tabs);
		gtk_notebook_set_show_border(GTK_NOTEBOOK(w->notebook), show_tabs);
	}
}

static void apply_tooltips(struct main_window *w)
{
	const char *bph_tip = w->show_tooltips ? "Set known beat rate or keep 'guess' for auto detection." : NULL;
	const char *preset_tip = w->show_tooltips ? "Quickly apply common movement presets (BPH + lift angle)." : NULL;
	const char *la_tip = w->show_tooltips ? "Lift angle in degrees for amplitude computation." : NULL;
	const char *cal_tip = w->show_tooltips ? "Manual calibration offset in seconds/day." : NULL;
	const char *pos_tip = w->show_tooltips ? "Tag measurement position before taking a snapshot." : NULL;
	const char *snap_tip = w->show_tooltips ? "Capture current real-time measurements into a new tab." : NULL;

	gtk_widget_set_tooltip_text(w->bph_combo_box, bph_tip);
	gtk_widget_set_tooltip_text(w->preset_combo_box, preset_tip);
	gtk_widget_set_tooltip_text(w->la_spin_button, la_tip);
	gtk_widget_set_tooltip_text(w->cal_spin_button, cal_tip);
	gtk_widget_set_tooltip_text(w->position_combo_box, pos_tip);
	gtk_widget_set_tooltip_text(w->snapshot_button, snap_tip);
}

static void update_signal_quality_indicator(struct main_window *w)
{
	if(!w->signal_quality_label || !w->signal_quality_icon)
		return;

	struct snapshot *s = w->active_snapshot;
	if(!s || !s->pb) {
		safe_set_label_text(w->signal_quality_icon, "NO");
		safe_set_label_text(w->signal_quality_label, "No signal");
		set_quality_badge_class(w->signal_quality_icon, "quality-none");
		return;
	}

	if(s->is_old || s->signal <= 0) {
		safe_set_label_text(w->signal_quality_icon, "BAD");
		safe_set_label_text(w->signal_quality_label, "Unstable");
		set_quality_badge_class(w->signal_quality_icon, "quality-unstable");
		return;
	}

	if(s->signal < NSTEPS) {
		safe_set_label_text(w->signal_quality_icon, "LOW");
		safe_set_label_text(w->signal_quality_label, "Weak");
		set_quality_badge_class(w->signal_quality_icon, "quality-weak");
		return;
	}

	safe_set_label_text(w->signal_quality_icon, "OK");
	safe_set_label_text(w->signal_quality_label, "Stable");
	set_quality_badge_class(w->signal_quality_icon, "quality-stable");
}

static void update_status_label(struct main_window *w)
{
	if(!w->status_label)
		return;

	if(w->zombie) {
		safe_set_label_text(w->status_label, "Shutting down...");
		update_signal_quality_indicator(w);
		return;
	}

	struct snapshot *s = w->active_snapshot;
	if(!s) {
		safe_set_label_text(w->status_label, "Initializing...");
		update_signal_quality_indicator(w);
		return;
	}

	char text[160];
	if(s->calibrate) {
		switch(s->cal_state) {
			case 1:
				snprintf(text, sizeof(text), "Calibration complete: %c%d.%d s/d",
					s->cal_result < 0 ? '-' : '+',
					abs(s->cal_result) / 10,
					abs(s->cal_result) % 10);
				break;
			case -1:
				snprintf(text, sizeof(text), "Calibration failed. Check microphone position and signal level.");
				break;
			default:
				snprintf(text, sizeof(text), "Calibrating... %d%%. Keep watch in a stable position.", s->cal_percent);
				break;
		}
		safe_set_label_text(w->status_label, text);
		update_signal_quality_indicator(w);
		return;
	}

	if(!s->pb) {
		safe_set_label_text(w->status_label, "Listening for watch signal...");
		update_signal_quality_indicator(w);
		return;
	}

	if(s->signal <= 0 || s->is_old) {
		safe_set_label_text(w->status_label, "Signal unstable. Reposition watch/microphone and reduce ambient noise.");
		update_signal_quality_indicator(w);
		return;
	}

	if(s->signal < NSTEPS) {
		snprintf(text, sizeof(text), "Weak signal quality (%d/%d). Try stronger contact pressure.", s->signal, NSTEPS);
		safe_set_label_text(w->status_label, text);
		update_signal_quality_indicator(w);
		return;
	}

	snprintf(text, sizeof(text), "Tracking stable signal at %d bph", s->guessed_bph);
	safe_set_label_text(w->status_label, text);
	update_signal_quality_indicator(w);
}

static int find_matching_preset(int bph, double la)
{
	int i;
	for(i = 1; watch_presets[i].name; i++)
		if(watch_presets[i].bph == bph && fabs(watch_presets[i].la - la) < 0.6)
			return i;
	return 0;
}

static void set_bph_combo_value(struct main_window *w, int bph)
{
	int i,current = 0;
	for(i = 0; preset_bph[i]; i++) {
		if(bph == preset_bph[i]) {
			current = i+1;
			break;
		}
	}
	if(current || bph == 0) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box), current);
	} else {
		char s[32];
		sprintf(s,"%d",bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w->bph_combo_box)));
		gtk_entry_set_text(e,s);
	}
}

static char *selected_position_tag(struct main_window *w)
{
	char *text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(w->position_combo_box));
	if(!text)
		return NULL;
	if(!strcmp(text, "Unspecified")) {
		g_free(text);
		return NULL;
	}
	char *out = strdup(text);
	g_free(text);
	return out;
}

static void apply_preset_by_index(struct main_window *w, int index)
{
	if(index <= 0 || !watch_presets[index].name)
		return;

	w->bph = watch_presets[index].bph;
	w->la = watch_presets[index].la;
	set_bph_combo_value(w, w->bph);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), w->la);
	refresh_results(w);
	recompute(w);
	update_status_label(w);
	gtk_widget_queue_draw(w->notebook);
}

static void handle_preset_change(GtkComboBox *b, struct main_window *w)
{
	if(!w->controls_active) return;
	int idx = gtk_combo_box_get_active(GTK_COMBO_BOX(b));
	apply_preset_by_index(w, idx);
}

static void handle_bph_change(GtkComboBox *b, struct main_window *w)
{
	if(!w->controls_active) return;
	char *s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));
	if(s) {
		int bph;
		char *t;
		int n = (int)strtol(s,&t,10);
		if(*t || n < MIN_BPH || n > MAX_BPH) bph = 0;
		else bph = n;
		g_free(s);
		w->bph = bph;
		if(w->preset_combo_box) {
			int idx = find_matching_preset(w->bph, w->la);
			gtk_combo_box_set_active(GTK_COMBO_BOX(w->preset_combo_box), idx);
		}
		refresh_results(w);
		gtk_widget_queue_draw(w->notebook);
	}
}

static void handle_la_change(GtkSpinButton *b, struct main_window *w)
{
	if(!w->controls_active) return;
	double la = gtk_spin_button_get_value(b);
	if(la < MIN_LA || la > MAX_LA) la = DEFAULT_LA;
	w->la = la;
	if(w->preset_combo_box) {
		int idx = find_matching_preset(w->bph, w->la);
		gtk_combo_box_set_active(GTK_COMBO_BOX(w->preset_combo_box), idx);
	}
	refresh_results(w);
	gtk_widget_queue_draw(w->notebook);
}

static void handle_cal_change(GtkSpinButton *b, struct main_window *w)
{
	if(!w->controls_active) return;
	int cal = gtk_spin_button_get_value(b);
	w->cal = cal;
	refresh_results(w);
	gtk_widget_queue_draw(w->notebook);
}

static gboolean output_cal(GtkSpinButton *spin, gpointer data)
{
	UNUSED(data);
	GtkAdjustment *adj;
	gchar *text;
	int value;

	adj = gtk_spin_button_get_adjustment (spin);
	value = (int)gtk_adjustment_get_value (adj);
	text = g_strdup_printf ("%c%d.%d", value < 0 ? '-' : '+', abs(value)/10, abs(value)%10);
	gtk_entry_set_text (GTK_ENTRY (spin), text);
	g_free (text);

	return TRUE;
}

static gboolean input_cal(GtkSpinButton *spin, double *val, gpointer data)
{
	UNUSED(data);
	double x = 0;
	sscanf(gtk_entry_get_text (GTK_ENTRY (spin)), "%lf", &x);
	int n = round(x*10);
	if(n < MIN_CAL) n = MIN_CAL;
	if(n > MAX_CAL) n = MAX_CAL;
	*val = n;

	return TRUE;
}

static void on_shutdown(GApplication *app, void *p)
{
	UNUSED(p);
	debug("Main loop has terminated\n");
	struct main_window *w = g_object_get_data(G_OBJECT(app), "main-window");
	if(w) {
		if(w->theme_sync_timeout) {
			g_source_remove(w->theme_sync_timeout);
			w->theme_sync_timeout = 0;
		}
		save_config(w);
		computer_destroy(w->computer);
		op_destroy(w->active_panel);
		close_config(w);
		free(w);
	}
	terminate_portaudio();
}

static void recompute(struct main_window *w);
static void computer_callback(void *w);

static guint computer_terminated(struct main_window *w)
{
	if(w->zombie) {
		debug("Closing main window\n");
		gtk_widget_destroy(w->window);
	} else {
		debug("Restarting computer");

		struct computer *c = start_computer(w->nominal_sr, w->bph, w->la, w->cal, w->is_light);
		if(!c) {
			if(w->kick_timeout) g_source_remove(w->kick_timeout);
			if(w->save_timeout) g_source_remove(w->save_timeout);
			if(w->theme_sync_timeout) g_source_remove(w->theme_sync_timeout);
			w->kick_timeout = 0;
			w->save_timeout = 0;
			w->theme_sync_timeout = 0;
			w->zombie = 1;
			error("Failed to restart computation thread");
			gtk_widget_destroy(w->window);
		} else {
			computer_destroy(w->computer);
			w->active_panel->computer = w->computer = c;

			w->computer->callback = computer_callback;
			w->computer->callback_data = w;

			recompute(w);
		}
	}
	return FALSE;
}

static void computer_quit(void *w)
{
	gdk_threads_add_idle((GSourceFunc)computer_terminated,w);
}

static void kill_computer(struct main_window *w)
{
	w->computer->recompute = -1;
	w->computer->callback = computer_quit;
	w->computer->callback_data = w;
}

static gboolean quit(struct main_window *w)
{
	if(w->kick_timeout) g_source_remove(w->kick_timeout);
	if(w->save_timeout) g_source_remove(w->save_timeout);
	if(w->theme_sync_timeout) g_source_remove(w->theme_sync_timeout);
	w->kick_timeout = 0;
	w->save_timeout = 0;
	w->theme_sync_timeout = 0;
	w->zombie = 1;
	lock_computer(w->computer);
	kill_computer(w);
	unlock_computer(w->computer);
	return FALSE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer w)
{
	UNUSED(widget);
	UNUSED(event);
	debug("Received delete event\n");
	quit((struct main_window *)w);
	return TRUE;
}

static void handle_quit(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	quit(w);
}

static void recompute(struct main_window *w)
{
	w->computer_timeout = 0;
	lock_computer(w->computer);
	if(w->computer->recompute >= 0) {
		if(w->is_light != w->computer->actv->is_light) {
			kill_computer(w);
		} else {
			w->computer->bph = w->bph;
			w->computer->la = w->la;
			w->computer->calibrate = w->calibrate;
			w->computer->recompute = 1;
		}
	}
	unlock_computer(w->computer);
}

static guint kick_computer(struct main_window *w)
{
	w->computer_timeout++;
	if(w->calibrate && w->computer_timeout < 10) {
		return TRUE;
	} else {
		recompute(w);
		return TRUE;
	}
}

static void handle_calibrate(GtkCheckMenuItem *b, struct main_window *w)
{
	int button_state = gtk_check_menu_item_get_active(b) == TRUE;
	if(button_state != w->calibrate) {
		w->calibrate = button_state;
		recompute(w);
		update_status_label(w);
	}
}

static void handle_light(GtkCheckMenuItem *b, struct main_window *w)
{
	int button_state = gtk_check_menu_item_get_active(b) == TRUE;
	if(button_state != w->is_light) {
		w->is_light = button_state;
		recompute(w);
		update_status_label(w);
	}
}

static void handle_guided_mode(GtkCheckMenuItem *b, struct main_window *w)
{
	w->guided_mode = gtk_check_menu_item_get_active(b) == TRUE;
	if(w->guided_mode) {
		safe_set_label_text(w->status_label,
			"Guided mode enabled: 1) set BPH 2) confirm lift angle 3) place watch 4) wait for stable signal.");
	}
	update_status_label(w);
}

static void handle_focus_mode(GtkCheckMenuItem *b, struct main_window *w)
{
	w->focus_mode = gtk_check_menu_item_get_active(b) == TRUE;
	apply_focus_mode(w);
	update_status_label(w);
}

static void open_preferences_dialog(struct main_window *w)
{
	GtkWidget *dialog = gtk_dialog_new_with_buttons(
			"Preferences",
			GTK_WINDOW(w->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			"Cancel", GTK_RESPONSE_CANCEL,
			"Apply", GTK_RESPONSE_OK,
			NULL);

	GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
	gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
	gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 0);

	GtkWidget *bph_label = gtk_label_new("Default beat rate (BPH)");
	gtk_widget_set_halign(bph_label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), bph_label, 0, 0, 1, 1);

	GtkWidget *bph_combo = gtk_combo_box_text_new_with_entry();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(bph_combo), "guess");
	int i,current = 0;
	for(i = 0; preset_bph[i]; i++) {
		char s[32];
		sprintf(s,"%d", preset_bph[i]);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(bph_combo), s);
		if(w->bph == preset_bph[i]) current = i + 1;
	}
	if(current || w->bph == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(bph_combo), current);
	else {
		char s[32];
		sprintf(s,"%d", w->bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(bph_combo)));
		gtk_entry_set_text(e, s);
	}
	gtk_grid_attach(GTK_GRID(grid), bph_combo, 1, 0, 1, 1);

	GtkWidget *la_label = gtk_label_new("Default lift angle (°)");
	gtk_widget_set_halign(la_label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), la_label, 0, 1, 1, 1);

	GtkWidget *la_spin = gtk_spin_button_new_with_range(MIN_LA, MAX_LA, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(la_spin), w->la);
	gtk_grid_attach(GTK_GRID(grid), la_spin, 1, 1, 1, 1);

	GtkWidget *light_check = gtk_check_button_new_with_label("Enable light algorithm");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(light_check), w->is_light);
	gtk_grid_attach(GTK_GRID(grid), light_check, 0, 2, 2, 1);

	GtkWidget *guided_check = gtk_check_button_new_with_label("Guided mode for beginners");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(guided_check), w->guided_mode);
	gtk_grid_attach(GTK_GRID(grid), guided_check, 0, 3, 2, 1);

	GtkWidget *focus_check = gtk_check_button_new_with_label("Focus mode (minimal workspace)");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(focus_check), w->focus_mode);
	gtk_grid_attach(GTK_GRID(grid), focus_check, 0, 4, 2, 1);

	GtkWidget *tooltips_check = gtk_check_button_new_with_label("Show tooltips");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tooltips_check), w->show_tooltips);
	gtk_grid_attach(GTK_GRID(grid), tooltips_check, 0, 5, 2, 1);

	GtkWidget *confirm_save_check = gtk_check_button_new_with_label("Ask before replacing files");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(confirm_save_check), w->confirm_on_save);
	gtk_grid_attach(GTK_GRID(grid), confirm_save_check, 0, 6, 2, 1);

	GtkWidget *theme_label = gtk_label_new("Theme");
	gtk_widget_set_halign(theme_label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(grid), theme_label, 0, 7, 1, 1);

	GtkWidget *theme_combo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(theme_combo), "System");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(theme_combo), "Light");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(theme_combo), "Dark");
	int theme_index = w->theme_mode;
	if(theme_index < THEME_MODE_SYSTEM || theme_index > THEME_MODE_DARK)
		theme_index = THEME_MODE_SYSTEM;
	gtk_combo_box_set_active(GTK_COMBO_BOX(theme_combo), theme_index);
	gtk_grid_attach(GTK_GRID(grid), theme_combo, 1, 7, 1, 1);

	gtk_widget_show_all(dialog);

	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
		char *s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(bph_combo));
		if(s) {
			int bph;
			char *t;
			int n = (int)strtol(s, &t, 10);
			if(*t || n < MIN_BPH || n > MAX_BPH) bph = 0;
			else bph = n;
			g_free(s);
			w->bph = bph;
		}

		w->la = gtk_spin_button_get_value(GTK_SPIN_BUTTON(la_spin));
		w->is_light = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(light_check));
		w->guided_mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(guided_check));
		w->focus_mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(focus_check));
		w->show_tooltips = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tooltips_check));
		w->confirm_on_save = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(confirm_save_check));
		w->theme_mode = gtk_combo_box_get_active(GTK_COMBO_BOX(theme_combo));
		if(w->theme_mode < THEME_MODE_SYSTEM || w->theme_mode > THEME_MODE_DARK)
			w->theme_mode = THEME_MODE_SYSTEM;
		w->last_system_dark_mode = system_prefers_dark_mode();

		gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), w->la);

		int combo_index = 0;
		for(i = 0; preset_bph[i]; i++) {
			if(w->bph == preset_bph[i]) {
				combo_index = i + 1;
				break;
			}
		}
		if(combo_index || w->bph == 0)
			gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box), combo_index);
		else {
			char bs[32];
			sprintf(bs, "%d", w->bph);
			GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w->bph_combo_box)));
			gtk_entry_set_text(e, bs);
		}

		if(w->light_button)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w->light_button), w->is_light);
		if(w->guided_mode_button)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w->guided_mode_button), w->guided_mode);
		if(w->focus_mode_button)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w->focus_mode_button), w->focus_mode);

		apply_tooltips(w);
		apply_focus_mode(w);
		apply_modern_css(w);
		if(w->window)
			gtk_widget_queue_draw(w->window);
		refresh_results(w);
		recompute(w);
		update_status_label(w);
		gtk_widget_queue_draw(w->notebook);
	}

	gtk_widget_destroy(dialog);
}

static void handle_preferences(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	open_preferences_dialog(w);
}

static void controls_active(struct main_window *w, int active)
{
	w->controls_active = active;
	gtk_widget_set_sensitive(w->bph_combo_box, active);
	gtk_widget_set_sensitive(w->preset_combo_box, active);
	gtk_widget_set_sensitive(w->la_spin_button, active);
	gtk_widget_set_sensitive(w->cal_spin_button, active);
	gtk_widget_set_sensitive(w->position_combo_box, active);
	gtk_widget_set_sensitive(w->cal_button, active);

	if(active) {
		gtk_widget_show(w->snapshot_button);
		gtk_widget_hide(w->snapshot_name);
	} else {
		gtk_widget_hide(w->snapshot_button);
		gtk_widget_show(w->snapshot_name);
	}

	if(w->header_snapshot_button)
		gtk_widget_set_sensitive(w->header_snapshot_button, active && gtk_widget_get_sensitive(w->snapshot_button));
	if(w->header_save_button)
		gtk_widget_set_sensitive(w->header_save_button, gtk_widget_get_sensitive(w->save_item));
}

static int blank_string(char *s)
{
	if(!s) return 1;
	for(;*s;s++)
		if(!isspace((unsigned char)*s)) return 0;
	return 1;
}

static int position_index_from_tag(const char *tag)
{
	int i;
	if(!tag) return -1;
	for(i = 0; watch_positions[i]; i++)
		if(!strcmp(tag, watch_positions[i]))
			return i;
	return -1;
}

static void update_position_summary(struct main_window *w)
{
	if(!w->position_summary_label || !w->notebook)
		return;

	double rates[7] = {0};
	int has[7] = {0};

	int i;
	int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(w->notebook));
	for(i = 0; i < pages; i++) {
		GtkWidget *panel = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), i);
		struct output_panel *op = g_object_get_data(G_OBJECT(panel), "op-pointer");
		const char *tag = g_object_get_data(G_OBJECT(panel), "position-tag");
		if(!op || !tag || !op->snst || !op->snst->pb || op->snst->calibrate)
			continue;

		int idx = position_index_from_tag(tag);
		if(idx < 0) continue;

		rates[idx] = op->snst->rate;
		has[idx] = 1;
	}

	if(!(has[1] || has[2] || has[3] || has[4] || has[5] || has[6])) {
		gtk_label_set_text(GTK_LABEL(w->position_summary_label),
			"No position-tagged snapshots yet. Tag snapshots as DU/DD/CU/CD/CL/CR to compare regulation.");
		return;
	}

	GString *summary = g_string_new("Position summary (s/d)\n");

	for(i = 1; watch_positions[i]; i++) {
		if(has[i]) {
			g_string_append_printf(summary, "%s: %+0.1f  ", watch_positions[i], rates[i]);
		}
	}
	g_string_append(summary, "\n");

	double h_sum = 0;
	int h_cnt = 0;
	if(has[1]) { h_sum += rates[1]; h_cnt++; }
	if(has[2]) { h_sum += rates[2]; h_cnt++; }

	double v_sum = 0;
	int v_cnt = 0;
	for(i = 3; i <= 6; i++) {
		if(has[i]) { v_sum += rates[i]; v_cnt++; }
	}

	if(h_cnt > 0)
		g_string_append_printf(summary, "Horizontal avg: %+0.1f  ", h_sum / h_cnt);
	if(v_cnt > 0)
		g_string_append_printf(summary, "Vertical avg: %+0.1f  ", v_sum / v_cnt);
	if(h_cnt > 0 && v_cnt > 0)
		g_string_append_printf(summary, "ΔH-V: %+0.1f", (h_sum / h_cnt) - (v_sum / v_cnt));

	if(has[1] && has[2])
		g_string_append_printf(summary, "\nDU-DD: %+0.1f", rates[1] - rates[2]);
	if(has[3] && has[4])
		g_string_append_printf(summary, "  CU-CD: %+0.1f", rates[3] - rates[4]);

	gtk_label_set_text(GTK_LABEL(w->position_summary_label), summary->str);
	g_string_free(summary, TRUE);
}

static void handle_tab_changed(GtkNotebook *nbk, GtkWidget *panel, guint x, struct main_window *w)
{
	UNUSED(nbk);
	UNUSED(x);
	// These are NULL for the Real Time tab
	struct output_panel *op = g_object_get_data(G_OBJECT(panel), "op-pointer");
	char *tab_name = g_object_get_data(G_OBJECT(panel), "tab-name");

	controls_active(w, !op);

	int bph, cal;
	double la;
	const char *pos = NULL;
	struct snapshot *snap;
	if(op) {
		gtk_entry_set_text(GTK_ENTRY(w->snapshot_name_entry), tab_name ? tab_name : "");
		bph = op->snst->bph;
		cal = op->snst->cal;
		la = op->snst->la;
		pos = g_object_get_data(G_OBJECT(panel), "position-tag");
		snap = op->snst;
	} else {
		bph = w->bph;
		cal = w->cal;
		la = w->la;
		snap = w->active_snapshot;
	}

	int i,current = 0;
	for(i = 0; preset_bph[i]; i++) {
		if(bph == preset_bph[i]) {
			current = i+1;
			break;
		}
	}
	if(current || bph == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box), current);
	else {
		char s[32];
		sprintf(s,"%d",bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w->bph_combo_box)));
		gtk_entry_set_text(e,s);
	}

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), la);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), cal);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w->preset_combo_box), find_matching_preset(bph, la));

	if(op && pos && *pos) {
		int i;
		for(i = 0; watch_positions[i]; i++)
			if(!strcmp(pos, watch_positions[i])) {
				gtk_combo_box_set_active(GTK_COMBO_BOX(w->position_combo_box), i);
				break;
			}
	} else if(!op) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(w->position_combo_box), 0);
	}

	int can_save = !snap->calibrate && snap->pb;
	int can_snapshot = w->controls_active && can_save;
	sync_primary_actions(w, can_snapshot, can_save);
}

static void handle_tab_closed(GtkNotebook *nbk, GtkWidget *panel, guint x, struct main_window *w)
{
	UNUSED(x);
	if(gtk_notebook_get_n_pages(nbk) == 1 && !w->zombie) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(nbk), FALSE);
		gtk_notebook_set_show_border(GTK_NOTEBOOK(nbk), FALSE);
		gtk_widget_set_sensitive(w->save_all_item, FALSE);
		gtk_widget_set_sensitive(w->close_all_item, FALSE);
	}
	// Now, are we sure that we are not going to segfault?
	struct output_panel *op = g_object_get_data(G_OBJECT(panel), "op-pointer");
	if(op) op_destroy(op);
	free(g_object_get_data(G_OBJECT(panel), "tab-name"));
	free(g_object_get_data(G_OBJECT(panel), "position-tag"));
	update_position_summary(w);
}

static void handle_close_tab(GtkButton *b, struct output_panel *p)
{
	UNUSED(b);
	gtk_widget_destroy(p->panel);
}

static void handle_name_change(GtkEntry *e, struct main_window *w)
{
	int p = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->notebook));
	GtkWidget *panel = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), p);
	if(!panel)
		return;

	GtkWidget *label = g_object_get_data(G_OBJECT(panel), "tab-label");
	free(g_object_get_data(G_OBJECT(panel), "tab-name"));

	char *name = (char *)gtk_entry_get_text(e);
	name = blank_string(name) ? NULL : strdup(name);
	g_object_set_data(G_OBJECT(panel), "tab-name", name);

	if(GTK_IS_LABEL(label))
		gtk_label_set_text(GTK_LABEL(label), name ? name : "Snapshot");
}

#ifdef WIN_XP
static GtkWidget *image_from_file(char *filename)
{
	char *dir = g_win32_get_package_installation_directory_of_module(NULL);
	char *s;
	if(dir) {
		s = alloca( strlen(dir) + strlen(filename) + 2 );
		sprintf(s, "%s/%s", dir, filename);
	} else {
		s = filename;
	}
	GtkWidget *img = gtk_image_new_from_file(s);
	g_free(dir);
	return img;
}
#endif

static GtkWidget *make_tab_label(char *name, struct output_panel *panel_to_close)
{
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	char *nm = panel_to_close ? name ? name : "Snapshot" : "Real time";
	GtkWidget *label = gtk_label_new(nm);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	if(panel_to_close) {
#ifdef WIN_XP
		GtkWidget *image = image_from_file("window-close.png");
#else
		GtkWidget *image = gtk_image_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
#endif
		GtkWidget *button = gtk_button_new();
		gtk_button_set_image(GTK_BUTTON(button), image);
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
		g_signal_connect(button, "clicked", G_CALLBACK(handle_close_tab), panel_to_close);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(panel_to_close->panel), "op-pointer", panel_to_close);
		g_object_set_data(G_OBJECT(panel_to_close->panel), "tab-label", label);
		g_object_set_data(G_OBJECT(panel_to_close->panel), "tab-name", name ? strdup(name) : NULL);
	}

	gtk_widget_show_all(hbox);

	return hbox;
}

static void add_new_tab(struct snapshot *s, char *name, const char *position_tag, struct main_window *w)
{
	struct output_panel *op = init_output_panel(NULL, s, 5);
	GtkWidget *label = make_tab_label(name, op);
	gtk_widget_show_all(op->panel);

	if(position_tag)
		g_object_set_data(G_OBJECT(op->panel), "position-tag", strdup(position_tag));
	else
		g_object_set_data(G_OBJECT(op->panel), "position-tag", NULL);

	op_set_border(w->active_panel, 5);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(w->notebook), TRUE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(w->notebook), TRUE);
	gtk_notebook_append_page(GTK_NOTEBOOK(w->notebook), op->panel, label);
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(w->notebook), op->panel, TRUE);
	gtk_widget_set_sensitive(w->save_all_item, TRUE);
	gtk_widget_set_sensitive(w->close_all_item, TRUE);
	update_position_summary(w);
}

static void handle_snapshot(GtkButton *b, struct main_window *w)
{
	UNUSED(b);
	if(w->active_snapshot->calibrate) return;

	char *pos = selected_position_tag(w);
	char *name = NULL;
	if(pos) {
		name = malloc(strlen(pos) + 10);
		sprintf(name, "%s snapshot", pos);
	}

	struct snapshot *s = snapshot_clone(w->active_snapshot);
	s->timestamp = get_timestamp(s->is_light);
	add_new_tab(s, name, pos, w);

	free(pos);
	free(name);
}

static void chooser_set_filters(GtkFileChooser *chooser)
{
	GtkFileFilter *tgj_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(tgj_filter, ".tgj");
	gtk_file_filter_add_pattern(tgj_filter, "*.tgj");
	gtk_file_chooser_add_filter(chooser, tgj_filter);

	GtkFileFilter *all_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(all_filter, "All files");
	gtk_file_filter_add_pattern(all_filter, "*");
	gtk_file_chooser_add_filter(chooser, all_filter);

	// On windows seems not to work...
	gtk_file_chooser_set_filter(chooser, tgj_filter);
}

static FILE *fopen_check(char *filename, char *mode, struct main_window *w)
{
	FILE *f = NULL;

#ifdef _WIN32
	wchar_t *name = NULL;
	wchar_t *md = NULL;

	name = (wchar_t*)g_convert(filename, -1, "UTF-16LE", "UTF-8", NULL, NULL, NULL);
	if(!name) goto error;

	md = (wchar_t*)g_convert(mode, -1, "UTF-16LE", "UTF-8", NULL, NULL, NULL);
	if(!md) goto error;

	f = _wfopen(name, md);

error:	g_free(name);
	g_free(md);
#else
	f = fopen(filename, mode);
#endif

	if(!f) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
					"Error opening file\n");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	return f;
}

static FILE *choose_file_for_save(struct main_window *w, char *title, char *suggestion)
{
	FILE *f = NULL;
	GtkWidget *dialog = gtk_file_chooser_dialog_new (title,
			GTK_WINDOW(w->window),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			"Cancel",
			GTK_RESPONSE_CANCEL,
			"Save",
			GTK_RESPONSE_ACCEPT,
			NULL);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
	if(suggestion)
		gtk_file_chooser_set_current_name(chooser, suggestion);

	chooser_set_filters(chooser);

	if(GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
	{
		GFile *gf = gtk_file_chooser_get_file(chooser);
		char *filename = g_file_get_path(gf);
		g_object_unref(gf);
		if(!strcmp(".tgj", gtk_file_filter_get_name(gtk_file_chooser_get_filter(chooser)))) {
			char *s = strdup(filename);
			if(strlen(s) > 3 && strcasecmp(".tgj", s + strlen(s) - 4)) {
				char *t = g_malloc(strlen(filename)+5);
				sprintf(t,"%s.tgj",filename);
				g_free(filename);
				filename = t;
			}
			free(s);
		}
		struct stat stst;
		int do_open = 0;
		if(!stat(filename, &stst)) {
			if(w->confirm_on_save) {
				GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_OK_CANCEL,
					"File %s already exists. Do you want to replace it?",
					filename);
				do_open = GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(dialog));
				gtk_widget_destroy(dialog);
			} else {
				do_open = 1;
			}
		} else
			do_open = 1;
		if(do_open) {
			f = fopen_check(filename, "wb", w);
			if(f) {
				char *uri = g_filename_to_uri(filename,NULL,NULL);
				if(f && uri)
					gtk_recent_manager_add_item(
						gtk_recent_manager_get_default(), uri);
				g_free(uri);
			}
		}
		g_free (filename);
	}

	gtk_widget_destroy(dialog);

	return f;
}

static void save_current(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	int p = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->notebook));
	GtkWidget *tab = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), p);
	struct output_panel *op = g_object_get_data(G_OBJECT(tab), "op-pointer");
	struct snapshot *snapshot = op ? op->snst : w->active_snapshot;
	char *name = g_object_get_data(G_OBJECT(tab), "tab-name");

	if(snapshot->calibrate || !snapshot->pb) return;

	snapshot = snapshot_clone(snapshot);

	if(!snapshot->timestamp)
		snapshot->timestamp = get_timestamp(snapshot->is_light);

	FILE *f = choose_file_for_save(w, "Save current display", name);

	if(f) {
		if(write_file(f, &snapshot, &name, 1)) {
			GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
						"Error writing file");
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		}
		fclose(f);
	}

	snapshot_destroy(snapshot);
}

static void close_all(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	int i = 0;
	while(i < gtk_notebook_get_n_pages(GTK_NOTEBOOK(w->notebook))) {
		GtkWidget *tab = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), i);
		struct output_panel *op = g_object_get_data(G_OBJECT(tab), "op-pointer");
		if(!op) {  // This one is the real-time tab
			i++;
			continue;
		}
		gtk_widget_destroy(tab);
	}
}

static void save_all(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	FILE *f = choose_file_for_save(w, "Save all snapshots", NULL);
	if(!f) return;

	int i, j, tabs = gtk_notebook_get_n_pages(GTK_NOTEBOOK(w->notebook));
	struct snapshot *s[tabs];
	char *names[tabs];

	for(i = j = 0; i < tabs; i++) {
		GtkWidget *tab = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), i);
		struct output_panel *op = g_object_get_data(G_OBJECT(tab), "op-pointer");
		if(!op) continue; // This one is the real-time tab
		s[j] = op->snst;
		names[j++] = g_object_get_data(G_OBJECT(tab), "tab-name");
	}

	if(write_file(f, s, names, j)) {
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
					"Error writing file");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	fclose(f);
}

static void load_snapshots(FILE *f, char *name, struct main_window *w)
{
	struct snapshot **s;
	char **names;
	uint64_t cnt;
	if(!read_file(f, &s, &names, &cnt)) {
		uint64_t i;
		for(i = 0; i < cnt; i++) {
			add_new_tab(s[i], names[i] ? names[i] : name, NULL, w);
			free(names[i]);
		}
		free(s);
		free(names);
	} else {
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
					"Error reading file: %s", name);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
}

static void load_from_file(char *filename, struct main_window *w)
{
	FILE *f = fopen_check(filename, "rb", w);
	if(f) {
		char *filename_cpy = strdup(filename);
		char *name = basename(filename_cpy);
		name = g_filename_to_utf8(name, -1, NULL, NULL, NULL);
		if(name && strlen(name) > 3 && !strcasecmp(".tgj", name + strlen(name) - 4))
			name[strlen(name) - 4] = 0;
		load_snapshots(f, name, w);
		free(filename_cpy);
		g_free(name);
		fclose(f);
	}
}

static void load(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open",
			GTK_WINDOW(w->window),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			"Cancel",
			GTK_RESPONSE_CANCEL,
			"Open",
			GTK_RESPONSE_ACCEPT,
			NULL);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);

	chooser_set_filters(chooser);

	if(GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
	{
		GFile *gf = gtk_file_chooser_get_file(chooser);
		char *filename = g_file_get_path(gf);
		g_object_unref(gf);
		load_from_file(filename, w);
		g_free (filename);
	}

	gtk_widget_destroy(dialog);
}

/* Set up the main window and populate with widgets */
static void init_main_window(struct main_window *w)
{
	w->window = gtk_application_window_new(w->app);

	gtk_window_set_default_size(GTK_WINDOW(w->window), 1360, 860);
	gtk_window_set_resizable(GTK_WINDOW(w->window), TRUE);

	gtk_container_set_border_width(GTK_CONTAINER(w->window), 0);
	g_signal_connect(w->window, "delete_event", G_CALLBACK(delete_event), w);

	gtk_window_set_title(GTK_WINDOW(w->window), PROGRAM_NAME " " VERSION);
	gtk_window_set_icon_name (GTK_WINDOW(w->window), PACKAGE);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_name(vbox, "app-root");
	gtk_container_add(GTK_CONTAINER(w->window), vbox);

#if defined(__linux__)
	GtkWidget *header = gtk_header_bar_new();
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
	gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Beatscope");
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Mechanical watch analysis");
	gtk_window_set_titlebar(GTK_WINDOW(w->window), header);

	GtkWidget *header_left = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_header_bar_pack_start(GTK_HEADER_BAR(header), header_left);

	w->header_open_button = gtk_button_new_with_label("Open");
	gtk_box_pack_start(GTK_BOX(header_left), w->header_open_button, FALSE, FALSE, 0);
	g_signal_connect(w->header_open_button, "clicked", G_CALLBACK(load), w);

	w->header_save_button = gtk_button_new_with_label("Save");
	gtk_box_pack_start(GTK_BOX(header_left), w->header_save_button, FALSE, FALSE, 0);
	g_signal_connect(w->header_save_button, "clicked", G_CALLBACK(save_current), w);
	gtk_widget_set_sensitive(w->header_save_button, FALSE);

	w->header_snapshot_button = gtk_button_new_with_label("Snapshot");
	gtk_box_pack_start(GTK_BOX(header_left), w->header_snapshot_button, FALSE, FALSE, 0);
	g_signal_connect(w->header_snapshot_button, "clicked", G_CALLBACK(handle_snapshot), w);
	gtk_widget_set_sensitive(w->header_snapshot_button, FALSE);

	w->header_preferences_button = gtk_button_new_with_label("Preferences");
	gtk_header_bar_pack_end(GTK_HEADER_BAR(header), w->header_preferences_button);
	g_signal_connect(w->header_preferences_button, "clicked", G_CALLBACK(handle_preferences), w);
#else
	w->header_open_button = NULL;
	w->header_save_button = NULL;
	w->header_snapshot_button = NULL;
	w->header_preferences_button = NULL;
#endif

	GtkWidget *main_split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_name(main_split, "main-split");
	gtk_box_pack_start(GTK_BOX(vbox), main_split, TRUE, TRUE, 0);

	GtkWidget *sidebar_scroller = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_name(sidebar_scroller, "sidebar-scroller");
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scroller),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(sidebar_scroller), FALSE);
	gtk_widget_set_size_request(sidebar_scroller, 340, -1);
	gtk_paned_pack1(GTK_PANED(main_split), sidebar_scroller, FALSE, FALSE);

	GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_name(sidebar, "sidebar");
	gtk_container_set_border_width(GTK_CONTAINER(sidebar), 14);
	gtk_container_add(GTK_CONTAINER(sidebar_scroller), sidebar);

	GtkWidget *workspace_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_name(workspace_column, "workspace-column");
	gtk_container_set_border_width(GTK_CONTAINER(workspace_column), 10);
	gtk_paned_pack2(GTK_PANED(main_split), workspace_column, TRUE, FALSE);

	GtkWidget *controls_card = gtk_frame_new("Movement setup");
	gtk_widget_set_name(controls_card, "controls-bar");
	gtk_box_pack_start(GTK_BOX(sidebar), controls_card, FALSE, FALSE, 0);

	GtkWidget *controls_card_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(controls_card_box), 12);
	gtk_container_add(GTK_CONTAINER(controls_card), controls_card_box);

	GtkWidget *controls_grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(controls_grid), 8);
	gtk_grid_set_column_spacing(GTK_GRID(controls_grid), 10);
	gtk_box_pack_start(GTK_BOX(controls_card_box), controls_grid, FALSE, FALSE, 0);

	GtkWidget *label = gtk_label_new("Beat rate (BPH)");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(controls_grid), label, 0, 0, 1, 1);

	w->bph_combo_box = gtk_combo_box_text_new_with_entry();
	gtk_widget_set_hexpand(w->bph_combo_box, TRUE);
	gtk_grid_attach(GTK_GRID(controls_grid), w->bph_combo_box, 1, 0, 1, 1);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box), "guess");
	int i,current = 0;
	for(i = 0; preset_bph[i]; i++) {
		char s[100];
		sprintf(s,"%d", preset_bph[i]);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box), s);
		if(w->bph == preset_bph[i]) current = i+1;
	}
	if(current || w->bph == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box), current);
	else {
		char s[32];
		sprintf(s,"%d",w->bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w->bph_combo_box)));
		gtk_entry_set_text(e,s);
	}
	g_signal_connect (w->bph_combo_box, "changed", G_CALLBACK(handle_bph_change), w);

	label = gtk_label_new("Preset");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(controls_grid), label, 0, 1, 1, 1);

	w->preset_combo_box = gtk_combo_box_text_new();
	for(i = 0; watch_presets[i].name; i++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->preset_combo_box), watch_presets[i].name);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w->preset_combo_box), find_matching_preset(w->bph, w->la));
	gtk_widget_set_hexpand(w->preset_combo_box, TRUE);
	gtk_grid_attach(GTK_GRID(controls_grid), w->preset_combo_box, 1, 1, 1, 1);
	g_signal_connect(w->preset_combo_box, "changed", G_CALLBACK(handle_preset_change), w);

	label = gtk_label_new("Lift angle (°)");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(controls_grid), label, 0, 2, 1, 1);

	w->la_spin_button = gtk_spin_button_new_with_range(MIN_LA, MAX_LA, 1);
	gtk_widget_set_hexpand(w->la_spin_button, TRUE);
	gtk_grid_attach(GTK_GRID(controls_grid), w->la_spin_button, 1, 2, 1, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), w->la);
	g_signal_connect(w->la_spin_button, "value_changed", G_CALLBACK(handle_la_change), w);

	label = gtk_label_new("Calibration (s/d)");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(controls_grid), label, 0, 3, 1, 1);

	w->cal_spin_button = gtk_spin_button_new_with_range(MIN_CAL, MAX_CAL, 1);
	gtk_widget_set_hexpand(w->cal_spin_button, TRUE);
	gtk_grid_attach(GTK_GRID(controls_grid), w->cal_spin_button, 1, 3, 1, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), w->cal);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w->cal_spin_button), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(w->cal_spin_button), 6);
	g_signal_connect(w->cal_spin_button, "value_changed", G_CALLBACK(handle_cal_change), w);
	g_signal_connect(w->cal_spin_button, "output", G_CALLBACK(output_cal), NULL);
	g_signal_connect(w->cal_spin_button, "input", G_CALLBACK(input_cal), NULL);

	label = gtk_label_new("Position");
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_grid_attach(GTK_GRID(controls_grid), label, 0, 4, 1, 1);

	w->position_combo_box = gtk_combo_box_text_new();
	for(i = 0; watch_positions[i]; i++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->position_combo_box), watch_positions[i]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w->position_combo_box), 0);
	gtk_widget_set_hexpand(w->position_combo_box, TRUE);
	gtk_grid_attach(GTK_GRID(controls_grid), w->position_combo_box, 1, 4, 1, 1);

	GtkWidget *actions_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(controls_card_box), actions_row, FALSE, FALSE, 0);

	w->snapshot_button = gtk_button_new_with_label("Take snapshot");
	gtk_box_pack_start(GTK_BOX(actions_row), w->snapshot_button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(w->snapshot_button, FALSE);
	g_signal_connect(w->snapshot_button, "clicked", G_CALLBACK(handle_snapshot), w);

	GtkWidget *name_label = gtk_label_new("Current snapshot:");
	w->snapshot_name_entry = gtk_entry_new();
	w->snapshot_name = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(w->snapshot_name), name_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(w->snapshot_name), w->snapshot_name_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(actions_row), w->snapshot_name, TRUE, TRUE, 0);
	g_signal_connect(w->snapshot_name_entry, "changed", G_CALLBACK(handle_name_change), w);

	GtkWidget *hbox = actions_row;

	// Command menu
	GtkWidget *command_menu = gtk_menu_new();
	GtkWidget *command_menu_button = gtk_menu_button_new();
#ifdef WIN_XP
	GtkWidget *image = image_from_file("open-menu.png");
#else
	GtkWidget *image = gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
#endif
	gtk_button_set_image(GTK_BUTTON(command_menu_button), image);
	g_object_set(G_OBJECT(command_menu_button), "direction", GTK_ARROW_DOWN, NULL);
	g_object_set(G_OBJECT(command_menu), "halign", GTK_ALIGN_END, NULL);
	gtk_menu_button_set_popup(GTK_MENU_BUTTON(command_menu_button), command_menu);
#if defined(__linux__)
	gtk_header_bar_pack_end(GTK_HEADER_BAR(header), command_menu_button);
#else
	gtk_box_pack_end(GTK_BOX(hbox), command_menu_button, FALSE, FALSE, 0);
#endif
	
	// ... Preferences
	GtkWidget *preferences_item = gtk_menu_item_new_with_label("Preferences");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), preferences_item);
	g_signal_connect(preferences_item, "activate", G_CALLBACK(handle_preferences), w);

	// ... Open
	GtkWidget *open_item = gtk_menu_item_new_with_label("Open");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), open_item);
	g_signal_connect(open_item, "activate", G_CALLBACK(load), w);

	// ... Save
	w->save_item = gtk_menu_item_new_with_label("Save current display");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->save_item);
	g_signal_connect(w->save_item, "activate", G_CALLBACK(save_current), w);
	gtk_widget_set_sensitive(w->save_item, FALSE);

	// ... Save all
	w->save_all_item = gtk_menu_item_new_with_label("Save all snapshots");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->save_all_item);
	g_signal_connect(w->save_all_item, "activate", G_CALLBACK(save_all), w);
	gtk_widget_set_sensitive(w->save_all_item, FALSE);

	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), gtk_separator_menu_item_new());

	// ... Light checkbox
	w->light_button = gtk_check_menu_item_new_with_label("Light algorithm");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->light_button);
	g_signal_connect(w->light_button, "toggled", G_CALLBACK(handle_light), w);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w->light_button), w->is_light);

	// ... Calibrate checkbox
	w->cal_button = gtk_check_menu_item_new_with_label("Calibrate");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->cal_button);
	g_signal_connect(w->cal_button, "toggled", G_CALLBACK(handle_calibrate), w);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w->cal_button), w->calibrate);

	// ... Guided mode
	w->guided_mode_button = gtk_check_menu_item_new_with_label("Guided mode");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->guided_mode_button);
	g_signal_connect(w->guided_mode_button, "toggled", G_CALLBACK(handle_guided_mode), w);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w->guided_mode_button), w->guided_mode);

	// ... Focus mode
	w->focus_mode_button = gtk_check_menu_item_new_with_label("Focus mode");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->focus_mode_button);
	g_signal_connect(w->focus_mode_button, "toggled", G_CALLBACK(handle_focus_mode), w);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w->focus_mode_button), w->focus_mode);

	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), gtk_separator_menu_item_new());

	// ... Close all
	w->close_all_item = gtk_menu_item_new_with_label("Close all snapshots");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->close_all_item);
	g_signal_connect(w->close_all_item, "activate", G_CALLBACK(close_all), w);
	gtk_widget_set_sensitive(w->close_all_item, FALSE);

	// ... Quit
	GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), quit_item);
	g_signal_connect(quit_item, "activate", G_CALLBACK(handle_quit), w);

	gtk_widget_show_all(command_menu);

	// The tabs' container
	w->notebook = gtk_notebook_new();
	gtk_widget_set_name(w->notebook, "main-notebook");
	gtk_box_pack_start(GTK_BOX(workspace_column), w->notebook, TRUE, TRUE, 0);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(w->notebook), TRUE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(w->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(w->notebook), FALSE);
	g_signal_connect(w->notebook, "page-removed", G_CALLBACK(handle_tab_closed), w);
	g_signal_connect_after(w->notebook, "switch-page", G_CALLBACK(handle_tab_changed), w);

	// The main tab
	GtkWidget *tab_label = make_tab_label(NULL, NULL);
	gtk_notebook_append_page(GTK_NOTEBOOK(w->notebook), w->active_panel->panel, tab_label);
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(w->notebook), w->active_panel->panel, TRUE);

	// Position summary panel
	w->position_summary_panel = gtk_frame_new("Position summary");
	gtk_box_pack_start(GTK_BOX(workspace_column), w->position_summary_panel, FALSE, FALSE, 0);

	GtkWidget *summary_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width(GTK_CONTAINER(summary_box), 10);
	gtk_container_add(GTK_CONTAINER(w->position_summary_panel), summary_box);

	w->position_summary_label = gtk_label_new(
		"No position-tagged snapshots yet. Tag snapshots as DU/DD/CU/CD/CL/CR to compare regulation.");
	gtk_widget_set_halign(w->position_summary_label, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w->position_summary_label), 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(w->position_summary_label), TRUE);
	gtk_box_pack_start(GTK_BOX(summary_box), w->position_summary_label, FALSE, FALSE, 0);

	// Status and quality bar
	GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
	gtk_widget_set_name(status_box, "status-bar");
	gtk_box_pack_start(GTK_BOX(workspace_column), status_box, FALSE, FALSE, 0);

	w->signal_quality_icon = gtk_label_new("NO");
	gtk_widget_set_name(w->signal_quality_icon, "quality-badge");
	gtk_widget_set_halign(w->signal_quality_icon, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(status_box), w->signal_quality_icon, FALSE, FALSE, 0);

	w->signal_quality_label = gtk_label_new("No signal");
	gtk_widget_set_name(w->signal_quality_label, "quality-label");
	gtk_widget_set_halign(w->signal_quality_label, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w->signal_quality_label), 0.0);
	gtk_box_pack_start(GTK_BOX(status_box), w->signal_quality_label, FALSE, FALSE, 0);

	w->status_label = gtk_label_new("");
	gtk_widget_set_name(w->status_label, "status-label");
	gtk_widget_set_halign(w->status_label, GTK_ALIGN_START);
	gtk_label_set_xalign(GTK_LABEL(w->status_label), 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(w->status_label), TRUE);
	gtk_box_pack_start(GTK_BOX(status_box), w->status_label, TRUE, TRUE, 0);

	gtk_widget_set_name(controls_card, "controls-bar");
	gtk_widget_set_name(w->position_summary_panel, "summary-panel");

	apply_tooltips(w);
	apply_focus_mode(w);
	update_position_summary(w);
	update_status_label(w);
	apply_modern_css(w);

	GtkSettings *settings = gtk_settings_get_default();
	if(settings) {
		g_signal_connect(settings, "notify::gtk-application-prefer-dark-theme",
			G_CALLBACK(handle_system_theme_change), w);
		g_signal_connect(settings, "notify::gtk-theme-name",
			G_CALLBACK(handle_system_theme_change), w);
	}

	set_quality_badge_class(w->signal_quality_icon, "quality-none");
	sync_primary_actions(w, 0, 0);

	gtk_window_set_resizable(GTK_WINDOW(w->window), TRUE);
	gtk_widget_show_all(w->window);
	gtk_widget_hide(w->snapshot_name);
	gtk_window_set_focus(GTK_WINDOW(w->window), NULL);
}

guint save_on_change_timer(struct main_window *w)
{
	save_on_change(w);
	return TRUE;
}

guint refresh(struct main_window *w)
{
	lock_computer(w->computer);
	struct snapshot *s = w->computer->curr;
	if(s) {
		double trace_centering = w->active_snapshot->trace_centering;
		snapshot_destroy(w->active_snapshot);
		w->active_snapshot = s;
		w->computer->curr = NULL;
		s->trace_centering = trace_centering;
		if(w->computer->clear_trace && !s->calibrate)
			memset(s->events,0,s->events_count*sizeof(uint64_t));
		if(s->calibrate && s->cal_state == 1 && s->cal_result != w->cal) {
			w->cal = s->cal_result;
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), s->cal_result);
		}
	}
	unlock_computer(w->computer);
	refresh_results(w);
	op_set_snapshot(w->active_panel, w->active_snapshot);

	int p = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->notebook));
	GtkWidget *panel = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), p);
	int photogenic = 0;
	int can_save = gtk_widget_get_sensitive(w->save_item);

	if(!g_object_get_data(G_OBJECT(panel), "op-pointer")) {
		photogenic = !w->active_snapshot->calibrate && w->active_snapshot->pb;
		can_save = photogenic;
		gtk_widget_queue_draw(w->notebook);
	}

	sync_primary_actions(w, photogenic, can_save);
	apply_focus_mode(w);
	update_position_summary(w);
	update_status_label(w);
	return FALSE;
}

static void computer_callback(void *w)
{
	gdk_threads_add_idle((GSourceFunc)refresh,w);
}

static void start_interface(GApplication* app, void *p)
{
	UNUSED(p);
	double real_sr;

	initialize_palette();

	struct main_window *w = malloc(sizeof(struct main_window));

	if(start_portaudio(&w->nominal_sr, &real_sr)) {
		g_application_quit(app);
		return;
	}

	w->app = GTK_APPLICATION(app);

	w->zombie = 0;
	w->controls_active = 1;
	w->cal = MIN_CAL - 1;
	w->bph = 0;
	w->la = DEFAULT_LA;
	w->calibrate = 0;
	w->is_light = 0;
	w->guided_mode = 1;
	w->focus_mode = 0;
	w->show_tooltips = 1;
	w->confirm_on_save = 1;
	w->theme_mode = THEME_MODE_SYSTEM;
	w->theme_sync_timeout = 0;
	w->last_system_dark_mode = system_prefers_dark_mode();

	load_config(w);

	if(w->theme_mode < THEME_MODE_SYSTEM || w->theme_mode > THEME_MODE_DARK)
		w->theme_mode = THEME_MODE_SYSTEM;
	w->last_system_dark_mode = system_prefers_dark_mode();
	if(w->la < MIN_LA || w->la > MAX_LA) w->la = DEFAULT_LA;
	if(w->bph < MIN_BPH || w->bph > MAX_BPH) w->bph = 0;
	if(w->cal < MIN_CAL || w->cal > MAX_CAL)
		w->cal = (real_sr - w->nominal_sr) * (3600*24) / w->nominal_sr;

	w->computer_timeout = 0;

	w->computer = start_computer(w->nominal_sr, w->bph, w->la, w->cal, w->is_light);
	if(!w->computer) {
		error("Error starting computation thread");
		g_application_quit(app);
		return;
	}
	w->computer->callback = computer_callback;
	w->computer->callback_data = w;

	w->active_snapshot = w->computer->curr;
	w->computer->curr = NULL;
	compute_results(w->active_snapshot);

	w->active_panel = init_output_panel(w->computer, w->active_snapshot, 0);

	init_main_window(w);

	w->kick_timeout = g_timeout_add_full(G_PRIORITY_LOW,100,(GSourceFunc)kick_computer,w,NULL);
	w->save_timeout = g_timeout_add_full(G_PRIORITY_LOW,10000,(GSourceFunc)save_on_change_timer,w,NULL);
	w->theme_sync_timeout = g_timeout_add_full(G_PRIORITY_LOW,750,(GSourceFunc)theme_sync_timer,w,NULL);
#ifdef DEBUG
	if(testing)
		g_timeout_add_full(G_PRIORITY_LOW,3000,(GSourceFunc)quit,w,NULL);
#endif

	g_object_set_data(G_OBJECT(app), "main-window", w);
}

static void handle_activate(GApplication* app, void *p)
{
	UNUSED(p);
	struct main_window *w = g_object_get_data(G_OBJECT(app), "main-window");
	if(w) gtk_window_present(GTK_WINDOW(w->window));
}

static void handle_open(GApplication* app, GFile **files, int cnt, char *hint, void *p)
{
	UNUSED(hint);
	UNUSED(p);
	struct main_window *w = g_object_get_data(G_OBJECT(app), "main-window");
	if(w) {
		int i;
		for(i = 0; i < cnt; i++) {
			char *path = g_file_get_path(files[i]);
			// This partially works around a bug in XP (i.e. gtk+ bundle 3.6.4)
			path = g_locale_to_utf8(path, -1, NULL, NULL, NULL);
			if(!path) continue;
			load_from_file(path, w);
			g_free(path);
		}
		gtk_notebook_set_current_page(GTK_NOTEBOOK(w->notebook), -1);
		gtk_window_present(GTK_WINDOW(w->window));
	}
}

int main(int argc, char **argv)
{
	gtk_disable_setlocale();

#ifdef DEBUG
	if(argc > 1 && !strcmp("test",argv[1])) {
		testing = 1;
		argv++; argc--;
	}
#endif

	GtkApplication *app = gtk_application_new ("io.github.harsh223.beatscope", G_APPLICATION_HANDLES_OPEN);
	g_signal_connect (app, "startup", G_CALLBACK (start_interface), NULL);
	g_signal_connect (app, "activate", G_CALLBACK (handle_activate), NULL);
	g_signal_connect (app, "open", G_CALLBACK (handle_open), NULL);
	g_signal_connect (app, "shutdown", G_CALLBACK (on_shutdown), NULL);
	int ret = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	debug("Interface exited with status %d\n",ret);

	return ret;
}
