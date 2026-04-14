SUBDIRS = icons
bin_PROGRAMS = beatscope
EXTRA_PROGRAMS = beatscope-dbg \
		 beatscope-prf \
		 beatscope-vlg
MOSTLYCLEANFILES = $(EXTRA_PROGRAMS)

beatscope_SOURCES = src/algo.c \
		    src/audio.c \
		    src/computer.c \
		    src/config.c \
		    src/interface.c \
		    src/output_panel.c \
		    src/serializer.c \
		    src/tg.h

beatscope_dbg_SOURCES = $(beatscope_SOURCES)
beatscope_prf_SOURCES = $(beatscope_SOURCES)
beatscope_vlg_SOURCES = $(beatscope_SOURCES)

LIBS = $(GTK_LIBS) \
       $(GTHREAD_LIBS) \
       $(PORTAUDIO_LIBS) \
       $(FFTW_LIBS) \
       -lpthread \
       -lm

AM_CPPFLAGS = -DPROGRAM_NAME=\"Beatscope\" -DVERSION=\"$(PACKAGE_VERSION)\"
beatscope_dbg_CPPFLAGS = $(AM_CPPFLAGS) -DDEBUG

AM_CFLAGS = $(GTK_CFLAGS) \
	    $(GTHREAD_CFLAGS) \
	    $(PORTAUDIO_CFLAGS) \
	    $(FFTW_CFLAGS)
beatscope_dbg_CFLAGS = $(AM_CFLAGS) -ggdb
beatscope_prf_CFLAGS = $(AM_CFLAGS) -pg
beatscope_vlg_CFLAGS = $(AM_CFLAGS) -g

beatscope_LDFLAGS = $(AM_LDFLAGS)
beatscope_dbg_LDFLAGS = $(AM_LDFLAGS)
beatscope_prf_LDFLAGS = $(AM_LDFLAGS)
beatscope_vlg_LDFLAGS = $(AM_LDFLAGS)

if BE_WINDOWS
if HAVE_WINDRES
beatscope_SOURCES += icons/beatscope.rc
endif
beatscope_LDFLAGS += -mwindows
beatscope_dbg_LDFLAGS += -mconsole
beatscope_prf_LDFLAGS += -mconsole
beatscope_vlg_LDFLAGS += -mconsole
endif

desktopdir = $(datadir)/applications
dist_desktop_DATA = icons/beatscope.desktop

mimedir = $(datadir)/mime/packages
dist_mime_DATA = icons/beatscope.xml

dist_man_MANS = docs/beatscope.1

EXTRA_DIST = autogen.sh \
	     icons \
	     packaging \
	     docs/beatscope.1 \
	     icons/beatscope.desktop \
	     icons/beatscope.xml \
	     LICENSE \
	     README.md

.rc.o:
	$(WINDRES) $< -O coff -o $@

test: beatscope-dbg
	./beatscope-dbg test
.PHONY: test

valgrind: beatscope-vlg
	valgrind --leak-check=full -v --num-callers=99 --suppressions=.valgrind.supp ./$^
.PHONY: valgrind
