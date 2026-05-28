#!/bin/sh

HOST=x86_64-elf

set -e
. ./headers.sh

DEBUG_FLAG=" "
case "$*" in
  *--debug-qemu*)
    DEBUG_QEMU="DEBUG_QEMU=1"
    ;;
esac

TEST_KMALLOC=" "
case "$*" in
  *--test-kmalloc*)
    TEST_KMALLOC="TEST_KMALLOC=1"
    ;;
esac

TEST_INITRD=" "
case "$*" in
  *--test-initrd*)
    TEST_INITRD="TEST_INITRD=1"
    ;;
esac

TEST_VFS=" "
case "$*" in
  *--test-vfs*)
    TEST_VFS="TEST_VFS=1"
    ;;
esac

TEST_CHARDEV=" "
case "$*" in
  *--test-chardev*)
    TEST_CHARDEV="TEST_CHARDEV=1"
    ;;
esac

TEST_ALL=" "
case "$*" in
  *--test-all*)
    TEST_ALL="TEST_ALL=1"
    ;;
esac

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
  (cd $PROJECT && DESTDIR="$SYSROOT" bear --append -- $MAKE $DEBUG_QEMU $TEST_KMALLOC $TEST_INITRD $TEST_VFS $TEST_CHARDEV $TEST_ALL $HLOG_LEVEL HOST=$HOST install)
done

make -C userspace all HOST=$HOST SYSROOT="$SYSROOT"
./build_initrd.sh
mkdir -p "$SYSROOT/boot"
cp -f initrd/bin/initrd.tar "$SYSROOT/boot/"
mkdir -p "$SYSROOT/boot/limine"
cp -f limine.conf "$SYSROOT/boot/limine/limine.conf"

jq -s 'add' $(printf '%s/compile_commands.json ' $PROJECTS) > compile_commands.json
