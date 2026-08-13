#!/bin/sh
set -e

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
INITRD_ROOT="$PROJECT_ROOT/initrd/root"
INITRD_OUT_DIR="$PROJECT_ROOT/initrd/bin"
INITRD_ARCHIVE="$INITRD_OUT_DIR/initrd.tar"
USERSPACE_BIN_DIR="$PROJECT_ROOT/userspace/bin"
STAGING_DIR=$(mktemp -d "${TMPDIR:-/tmp}/hojicha-initrd.XXXXXX")
DOOM_WAD_PATH=""

while [ "$#" -gt 0 ]; do
  case "$1" in
    --doom-wad=*)
      DOOM_WAD_PATH="${1#*=}"
      ;;
    *)
      echo "Unknown build_initrd.sh argument: $1" >&2
      exit 1
      ;;
  esac
  shift
done

cleanup() {
  rm -rf "$STAGING_DIR"
}

trap cleanup EXIT INT TERM

mkdir -p "$INITRD_ROOT/etc" "$INITRD_ROOT/usr"

rm -rf "$INITRD_OUT_DIR"
mkdir -p "$INITRD_OUT_DIR"
cp -R "$INITRD_ROOT"/. "$STAGING_DIR"/

mkdir -p "$STAGING_DIR/etc" "$STAGING_DIR/usr/bin"
printf '%s' 'Hello from the other side' > "$STAGING_DIR/etc/test.txt"
printf '%s' 'foo' > "$STAGING_DIR/etc/bar.txt"
printf '%s' '++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.' > "$STAGING_DIR/etc/hw.bf"
printf '%s' ',.,.,.,.,.' > "$STAGING_DIR/etc/io.bf"

if [ -n "$DOOM_WAD_PATH" ]; then
  if [ ! -f "$DOOM_WAD_PATH" ]; then
    echo "DOOM WAD not found: $DOOM_WAD_PATH" >&2
    exit 1
  fi

  mkdir -p "$STAGING_DIR/usr/share/games/doom"
  cp -f "$DOOM_WAD_PATH" "$STAGING_DIR/usr/share/games/doom/"
fi

if [ -d "$USERSPACE_BIN_DIR" ]; then
  if [ -n "$DOOM_WAD_PATH" ]; then
    find "$USERSPACE_BIN_DIR" -maxdepth 1 -type f ! -name '*.elf' \
      -exec cp -f {} "$STAGING_DIR/usr/bin/" \;
  else
    find "$USERSPACE_BIN_DIR" -maxdepth 1 -type f ! -name '*.elf' ! -name 'doomgeneric' \
      -exec cp -f {} "$STAGING_DIR/usr/bin/" \;
  fi
fi

tar --format=ustar -cf "$INITRD_ARCHIVE" -C "$STAGING_DIR" .
