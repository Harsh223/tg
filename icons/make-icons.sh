#!/bin/bash
set -euo pipefail

SIZES="$(cat sizes)"

for A in $SIZES; do
	mkdir -p "${A}x${A}/apps"
	mkdir -p "${A}x${A}/mimetypes"

	inkscape -z -e "${A}x${A}/apps/beatscope.png" -w "$A" -h "$A" "scalable/apps/beatscope.svg" &>/dev/null
	optipng -quiet "${A}x${A}/apps/beatscope.png"

	inkscape -z -e "${A}x${A}/mimetypes/application-x-beatscope-data.png" -w "$A" -h "$A" "scalable/mimetypes/application-x-beatscope-data.svg" &>/dev/null
	optipng -quiet "${A}x${A}/mimetypes/application-x-beatscope-data.png"
done
