#!/bin/sh
set -e
. ./build.sh "$@"

if [ ! -d 'limine' ]; then
  git clone https://codeberg.org/Limine/Limine.git limine --branch=v10.x-binary --depth=1
  make -C limine
fi

mkdir -p iso_root/boot/limine
cp -v sysroot/boot/hojicha.kernel iso_root/boot/hojicha
USERSPACE_BIN_DIR=sysroot/boot
USERSPACE_BINS=$(find "$USERSPACE_BIN_DIR" -maxdepth 1 -type f -name '*.elf' -printf '%f\n' | sort)

for bin in $USERSPACE_BINS; do
  cp -v "$USERSPACE_BIN_DIR/$bin" "iso_root/boot/$bin"
done

awk -v bins="$USERSPACE_BINS" '
  { print }
  /^[[:space:]]*path:/ && !done {
    count = split(bins, names, "\n")
    for (i = 1; i <= count; i++) {
      if (names[i] != "") {
        printf "    module_path: boot():/boot/%s\n", names[i]
        printf "    module_string: %s\n", names[i]
      }
    }
    done = 1
  }
' sysroot/boot/limine/limine.conf > iso_root/boot/limine/limine.conf

cp -v limine/limine-bios.sys limine/limine-bios-cd.bin \
      limine/limine-uefi-cd.bin iso_root/boot/limine/

mkdir -p iso_root/EFI/BOOT
cp -v limine/BOOTX64.EFI iso_root/EFI/BOOT/
cp -v limine/BOOTIA32.EFI iso_root/EFI/BOOT/

xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        iso_root -o hojicha.iso

./limine/limine bios-install hojicha.iso
