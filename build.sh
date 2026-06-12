#!/bin/sh

HOST=${HOST:-x86_64-elf}

set -e
. ./config.sh

DEBUG_QEMU=0
case "$*" in
  *--debug-qemu*)
    DEBUG_QEMU=1
    ;;
esac

TEST_KMALLOC=0
case "$*" in
  *--test-kmalloc*)
    TEST_KMALLOC=1
    ;;
esac

STRESS_KMALLOC=0
case "$*" in
  *--stress-kmalloc*)
    STRESS_KMALLOC=1
    ;;
esac

TEST_INITRD=0
case "$*" in
  *--test-initrd*)
    TEST_INITRD=1
    ;;
esac

TEST_VFS=0
case "$*" in
  *--test-vfs*)
    TEST_VFS=1
    ;;
esac

TEST_CHARDEV=0
case "$*" in
  *--test-chardev*)
    TEST_CHARDEV=1
    ;;
esac

TEST_RINGBUFFER=0
case "$*" in
  *--test-ringbuffer*)
    TEST_RINGBUFFER=1
    ;;
esac

AST_SCHEDULER=0
case "$*" in
  *--ast-scheduler*)
    AST_SCHEDULER=1
    ;;
esac

TEST_ALL=0
case "$*" in
  *--test-all*)
    TEST_ALL=1
    ;;
esac

HLOG_LEVEL=""
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
      HLOG_LEVEL="HLOG_$HLOG_LEVEL_VALUE"
      ;;
    *)
      echo "Unknown --hlog-level value: $HLOG_LEVEL_VALUE" >&2
      echo "Valid values: FATAL, ERROR, WARN, INFO, DEBUG, VERBOSE" >&2
      exit 1
      ;;
  esac
fi

HOSTARCH=$(./target_triplet_to_arch.sh "$HOST")
if [ "$DEBUG_QEMU" = "1" ]; then
  BUILD_FLAVOR=debug
  CMAKE_BUILD_TYPE=Debug
else
  BUILD_FLAVOR=release
  CMAKE_BUILD_TYPE=Release
fi

BUILDDIR="build/$HOSTARCH/$BUILD_FLAVOR"

echo "Calling CMake:"
(
  set -x
  "$CMAKE" -S . -B "$BUILDDIR" \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/elf.cmake \
    -DHOJICHA_HOST="$HOST" \
    -DHOJICHA_ARCH="$HOSTARCH" \
    -DHOJICHA_SYSROOT="$SYSROOT" \
    -DHOJICHA_BUILD_FLAVOR="$BUILD_FLAVOR" \
    -DHOJICHA_DEBUG_QEMU="$DEBUG_QEMU" \
    -DHOJICHA_TEST_KMALLOC="$TEST_KMALLOC" \
    -DHOJICHA_STRESS_KMALLOC="$STRESS_KMALLOC" \
    -DHOJICHA_TEST_INITRD="$TEST_INITRD" \
    -DHOJICHA_TEST_VFS="$TEST_VFS" \
    -DHOJICHA_TEST_CHARDEV="$TEST_CHARDEV" \
    -DHOJICHA_TEST_RINGBUFFER="$TEST_RINGBUFFER" \
    -DHOJICHA_AST_SCHEDULER="$AST_SCHEDULER" \
    -DHOJICHA_TEST_ALL="$TEST_ALL" \
    -DHOJICHA_HLOG_LEVEL="$HLOG_LEVEL" \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX=/usr
)

"$CMAKE" --build "$BUILDDIR"
DESTDIR="$SYSROOT" "$CMAKE" --install "$BUILDDIR"

./build_initrd.sh
mkdir -p "$SYSROOT/boot"
cp -f initrd/bin/initrd.tar "$SYSROOT/boot/"

if [ -f "$BUILDDIR/compile_commands.json" ]; then
  cp -f "$BUILDDIR/compile_commands.json" compile_commands.json
fi
