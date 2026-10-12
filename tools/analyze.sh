#!/bin/sh
# Regenerate analysis/ with decomp-toolkit (tools/dtk; gcn-recomp/tools/fetch_dtk.sh fetches
# it). Needs a build first: it reads build/main.dol and the modules in build/rels/.
#
# dtk cannot read back its own output for fze.main (on reload it re-derives a jump table
# that runs into a string, and stops), so that module is analysed from scratch every time.
# Name things in fze.main through recomp/names.txt, not in its symbols file.
set -e
cd "$(dirname "$0")/../analysis"
: > rels/fze.main.txt
: > rels/fze.main.splits.txt
../tools/dtk dol split config.yml out
find out -name '*.s' | sort | xargs cat > all.s
