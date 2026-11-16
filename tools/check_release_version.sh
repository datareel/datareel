#!/bin/bash
# DataReel release check (11/16/2026, after 7.4-beta4 shipped saying "7.4 beta3"): the version the library reports
# must match the release being cut.
#
#   tools/check_release_version.sh [TAG]
#
# Checks, from the source tree this script is in:
#   - include/gxdlcode.h DATAREEL_VERSION_STRING ("7.4 beta5", "7.4") against TAG ("7.4-beta5", "7.4"), when given
#   - its MAJOR.MINOR against version.txt
#   - the first line of RELEASE_NOTES_<MAJOR.MINOR>.txt ("DataReel 7.4 Beta 5 Release Notes ...") and the <title> of
#     docs/release_notes_<MAJOR>_<MINOR>.html against the version string
# Exit 0 when everything matches, 1 with one line per mismatch. run_tests.sh runs it without a TAG (the files must
# agree with each other); run it with the tag before tagging.
SRC=$(cd "$(dirname "$0")/.." && pwd)
TAG=$1
bad=0
say() { echo "release check: $*"; bad=1; }

hdr="$SRC/include/gxdlcode.h"
vs=$(sed -n 's/^#define DATAREEL_VERSION_STRING "\(.*\)"/\1/p' "$hdr")
maj=$(sed -n 's/^#define DATAREEL_VERSION_MAJOR \([0-9]*\)/\1/p' "$hdr")
min=$(sed -n 's/^#define DATAREEL_VERSION_MINOR \([0-9]*\)/\1/p' "$hdr")
[ -n "$vs" ] && [ -n "$maj" ] && [ -n "$min" ] || { echo "release check: cannot read the version from $hdr"; exit 1; }

# "7.4 beta5" -> base "7.4", stage "beta", number "5"
base=${vs%% *}
pre=""
[ "$base" != "$vs" ] && pre=${vs#* }
case "$base" in "$maj.$min"|"$maj.$min".*) ;; *) say "DATAREEL_VERSION_STRING \"$vs\" is not version $maj.$min";; esac

if [ -n "$TAG" ]; then
  want="$base${pre:+-$pre}"
  [ "$TAG" = "$want" ] || say "tag $TAG, but the library says \"$vs\" (tag $want)"
fi

vt=$(tr -d ' \r\n' < "$SRC/version.txt" 2>/dev/null)
[ "$vt" = "$maj.$min" ] || [ "$vt" = "$base" ] || say "version.txt \"$vt\", the library $maj.$min"

# The release notes title: "DataReel 7.4 Beta 5 Release Notes" for "7.4 beta5", "DataReel 7.4 Release Notes" for "7.4"
title="DataReel $base"
if [ -n "$pre" ]; then
  stage=$(echo "$pre" | sed 's/[0-9]*$//'); num=$(echo "$pre" | sed 's/^[a-z]*//')
  title="$title $(echo "${stage:0:1}" | tr a-z A-Z)${stage:1}${num:+ $num}"
fi
title="$title Release Notes"
notes="$SRC/RELEASE_NOTES_$maj.$min.txt"
if [ -f "$notes" ]; then
  first=$(head -1 "$notes")
  case "$first" in "$title"*) ;; *) say "$(basename "$notes") starts \"$first\", not \"$title\"";; esac
fi
html="$SRC/docs/release_notes_${maj}_${min}.html"
if [ -f "$html" ]; then
  t=$(sed -n 's:.*<title>\([^<]*\)</title>.*:\1:p' "$html" | head -1)
  case "$t" in "$title"*) ;; *) say "$(basename "$html") title \"$t\", not \"$title\"";; esac
fi

[ $bad -eq 0 ] && echo "release check: \"$vs\"${TAG:+ = tag $TAG}: version.txt, release notes and their page agree"
exit $bad
