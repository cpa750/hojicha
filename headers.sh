#!/bin/sh
set -e
. ./config.sh

mkdir -p "$SYSROOT"

HOSTARCH=$(./target_triplet_to_arch.sh "$HOST")
BUILDDIR="build/$HOSTARCH/headers"

"$CMAKE" -S . -B "$BUILDDIR" \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/elf.cmake \
  -DHOJICHA_HOST="$HOST" \
  -DHOJICHA_ARCH="$HOSTARCH" \
  -DHOJICHA_SYSROOT="$SYSROOT" \
  -DHOJICHA_BUILD_FLAVOR=headers \
  -DCMAKE_INSTALL_PREFIX=/usr

DESTDIR="$SYSROOT" "$CMAKE" --install "$BUILDDIR" --component headers
