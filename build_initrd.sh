#!/bin/sh
set -e

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
INITRD_ROOT="$PROJECT_ROOT/initrd/root"
INITRD_OUT_DIR="$PROJECT_ROOT/initrd/bin"
INITRD_ARCHIVE="$INITRD_OUT_DIR/initrd.tar"
USERSPACE_BIN_DIR="$PROJECT_ROOT/userspace/bin"
STAGING_DIR=$(mktemp -d "${TMPDIR:-/tmp}/hojicha-initrd.XXXXXX")

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

if [ -d "$USERSPACE_BIN_DIR" ]; then
  find "$USERSPACE_BIN_DIR" -maxdepth 1 -type f -name '*.elf' \
    -exec cp -f {} "$STAGING_DIR/usr/bin/" \;
fi

tar --format=ustar -cf "$INITRD_ARCHIVE" -C "$STAGING_DIR" .
