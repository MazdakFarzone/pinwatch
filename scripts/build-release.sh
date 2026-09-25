#!/bin/sh

set -eu

GPIOLIB_COMMIT="ebc4a56bac3a896d5c14e56fe27dcd6cb36dd373"

if [ "$#" -ne 3 ]; then
    echo "Usage: $0 VERSION ARCH OUTPUT_DIR" >&2
    exit 2
fi

version="$1"
architecture="$2"
output_dir="$3"

case "$version" in
    ''|*[!0-9A-Za-z.-]*)
        echo "VERSION contains unsupported characters: $version" >&2
        exit 2
        ;;
esac

case "$architecture" in
    arm64)
        expected_machine="AArch64"
        ;;
    armhf)
        expected_machine="ARM"
        ;;
    *)
        echo "Unsupported architecture: $architecture" >&2
        exit 2
        ;;
esac

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
build_root=$(mktemp -d)
trap 'rm -rf "$build_root"' EXIT

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    dpkg-dev \
    git

utils_dir="$build_root/raspberrypi-utils"
git init -q "$utils_dir"
git -C "$utils_dir" remote add origin https://github.com/raspberrypi/utils.git
git -C "$utils_dir" fetch -q --depth=1 origin "$GPIOLIB_COMMIT"
git -C "$utils_dir" checkout -q --detach FETCH_HEAD

gpiolib_build="$build_root/gpiolib-build"
cmake -S "$utils_dir/pinctrl" -B "$gpiolib_build" \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$gpiolib_build" --target gpiolib --parallel "$(nproc)"

binary="$build_root/pinwatch"
cc -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$utils_dir/pinctrl" \
    "$project_dir/pinwatch.c" \
    -L"$gpiolib_build" -lgpiolib \
    -o "$binary"

LD_LIBRARY_PATH="$gpiolib_build" "$binary" --help >/dev/null
readelf -h "$binary" | grep -F "Machine:" | grep -F "$expected_machine" >/dev/null
strip "$binary"

mkdir -p "$output_dir"

archive_name="pinwatch-$version-linux-$architecture"
archive_root="$build_root/$archive_name"
mkdir -p "$archive_root"
install -m 0755 "$binary" "$archive_root/pinwatch"
install -m 0644 "$project_dir/LICENSE" "$archive_root/LICENSE"
install -m 0644 "$project_dir/README.md" "$archive_root/README.md"
cat > "$archive_root/INSTALL.txt" <<EOF
pinwatch $version for Linux $architecture

Requirements:
  - Raspberry Pi OS Bookworm or Trixie
  - libgpiolib0 from the Raspberry Pi OS package repository

Install:
  sudo apt update
  sudo apt install libgpiolib0
  sudo install -m 0755 pinwatch /usr/local/bin/pinwatch
EOF
tar -C "$build_root" -czf "$output_dir/$archive_name.tar.gz" "$archive_name"

deb_root="$build_root/deb"
mkdir -p "$deb_root/DEBIAN" "$deb_root/usr/bin" "$deb_root/usr/share/doc/pinwatch"
install -m 0755 "$binary" "$deb_root/usr/bin/pinwatch"
install -m 0644 "$project_dir/LICENSE" "$deb_root/usr/share/doc/pinwatch/copyright"
install -m 0644 "$project_dir/README.md" "$deb_root/usr/share/doc/pinwatch/README.md"
installed_size=$(du -sk "$deb_root/usr" | cut -f1)
cat > "$deb_root/DEBIAN/control" <<EOF
Package: pinwatch
Version: $version
Section: utils
Priority: optional
Architecture: $architecture
Depends: libc6 (>= 2.36), libgpiolib0
Installed-Size: $installed_size
Maintainer: Mazdak Farzone <mazdak.farzone@gmail.com>
Homepage: https://github.com/MazdakFarzone/pinwatch
Description: Read-only GPIO change monitor for Raspberry Pi
 pinwatch watches the standard 40-pin header and emits JSON events without
 claiming or reconfiguring GPIO lines.
EOF
dpkg-deb --build --root-owner-group \
    "$deb_root" "$output_dir/pinwatch_${version}_${architecture}.deb"
