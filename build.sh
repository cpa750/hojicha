#!/bin/sh

HOST=x86_64-elf

set -e
. ./headers.sh

DEBUG_FLAG=" "
if [[ "$*" == *"--debug-qemu"* ]]; then
    DEBUG_QEMU="DEBUG_QEMU=1"
fi

KMALLOC_TEST=" "
if [[ "$*" == *"--kmalloc-test"* ]]; then
    KMALLOC_TEST="KMALLOC_TEST=1"
fi

HLOG_LEVEL=" "
while [ "$#" -gt 0 ]; do
  case "$1" in
    --hlog-level=*)
      HLOG_LEVEL_VALUE="${1#*=}"
      ;;
    --hlog-level)
      shift
      if [ -z "$1" ]; then
        echo "--hlog-level requires a value" >&2
        exit 1
      fi
      HLOG_LEVEL_VALUE="$1"
      ;;
  esac
  shift
done

if [ -n "$HLOG_LEVEL_VALUE" ]; then
  case "$HLOG_LEVEL_VALUE" in
    FATAL|ERROR|WARN|INFO|DEBUG|VERBOSE)
      HLOG_LEVEL="HLOG_LEVEL=HLOG_$HLOG_LEVEL_VALUE"
      ;;
    *)
      echo "Unknown --hlog-level value: $HLOG_LEVEL_VALUE" >&2
      echo "Valid values: FATAL, ERROR, WARN, INFO, DEBUG, VERBOSE" >&2
      exit 1
      ;;
  esac
fi

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" bear --append -- $MAKE $DEBUG_QEMU $KMALLOC_TEST $HLOG_LEVEL HOST=$HOST install)
done

make -C userspace all
mkdir -p "$SYSROOT/boot"
find userspace/bin -maxdepth 1 -type f -name '*.elf' -exec cp -f {} "$SYSROOT/boot/" \;
mkdir -p "$SYSROOT/boot/limine"
cp -f limine.conf "$SYSROOT/boot/limine/limine.conf"

jq -s 'add' $(printf '%s/compile_commands.json ' $PROJECTS) > compile_commands.json
