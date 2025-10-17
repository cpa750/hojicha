#!/bin/sh
if [[ "$*" == *"--h64"* ]]; then
    HOST=x86_64-elf
fi

set -e
. ./headers.sh

DEBUG_FLAG=" "
if [[ "$*" == *"--debug-qemu"* ]]; then
    DEBUG_QEMU="DEBUG_QEMU=1"
fi

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE $DEBUG_QEMU HOST=$HOST install)
done
