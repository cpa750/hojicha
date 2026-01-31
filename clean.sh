#!/bin/sh
set -e
. ./config.sh

for PROJECT in $PROJECTS; do
  (cd $PROJECT && $MAKE clean)
done

rm -rf compile_commands.json
rm -rf sysroot
rm -rf iso_root
rm -rf hojicha.iso
