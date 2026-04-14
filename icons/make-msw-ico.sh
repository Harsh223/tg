#!/bin/bash
set -euo pipefail

SIZES="$(cat sizes)"

APP_PNGS="$(for A in $SIZES; do echo "${A}x${A}/apps/beatscope.png"; done)"
icotool -c -o beatscope.ico $APP_PNGS

MIME_PNGS="$(for A in $SIZES; do echo "${A}x${A}/mimetypes/application-x-beatscope-data.png"; done)"
icotool -c -o beatscope-document.ico $MIME_PNGS
