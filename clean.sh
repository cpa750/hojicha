#!/bin/sh
set -e
. ./config.sh

for PROJECT in $PROJECTS; do
  (cd $PROJECT && rm -f -- compile_commands.json && $MAKE clean)
done

(cd userspace && $MAKE clean)

rm -f  -- compile_commands.json
rm -rf sysroot
rm -rf iso_root
rm -rf hojicha.iso
