#!/usr/bin/env bash

set -Eeuo pipefail

if [[ -t 1 && -z "${NO_COLOR:-}" ]]; then
    COLOR_RED=$'\033[31m'
    COLOR_YELLOW=$'\033[33m'
    COLOR_BLUE=$'\033[34m'
    COLOR_CYAN=$'\033[36m'
    COLOR_GREEN=$'\033[32m'
    COLOR_RESET=$'\033[0m'
else
    COLOR_RED=''
    COLOR_YELLOW=''
    COLOR_BLUE=''
    COLOR_CYAN=''
    COLOR_GREEN=''
    COLOR_RESET=''
fi

info() {
    printf '%sInfo:%s %s\n' "$COLOR_BLUE" "$COLOR_RESET" "$*"
}

warning() {
    printf '%sWarning:%s %s\n' "$COLOR_YELLOW" "$COLOR_RESET" "$*" >&2
}

progress() {
    printf '%s%s%s\n' "$COLOR_CYAN" "$*" "$COLOR_RESET"
}

success() {
    printf '%s%s%s\n' "$COLOR_GREEN" "$*" "$COLOR_RESET"
}

usage() {
    cat <<'EOF'
Uso:
  ./upload_ota.sh [-c|--clean] -ev --esp-idf-version

Opzioni:
  -c, --clean       Delete the build-release directory before building
  -ev, --esp-idf-version Version of ESP-IDF to use for building
  -h, --help
EOF
}

die() {
    printf '%sError:%s %s\n' "$COLOR_RED" "$COLOR_RESET" "$*" >&2
    exit 1
}

clean_build=false
esp_idf_version=''
tmp_dir='/tmp/agromate_ota'

check_params() {
  [[ -n "$esp_idf_version" ]] || die "Missing ESP-IDF version"
}

if [[ $# -lt 1 ]]
then
    usage
    exit 1
fi

while (($#)); do
    case "$1" in
        -c|--clean)
            clean_build=true
            shift
            ;;
        -ev | --esp_idf_version)
            (($# >= 2)) || usage
            esp_idf_version=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            die "unrecognized option: $1"
            ;;
    esac
done

check_params

# shellcheck disable=SC1090
source ~/.espressif/v"$esp_idf_version"/esp-idf/export.sh

check_deps() {
  command -v idf.py >/dev/null 2>&1 || die "idf.py not found, please initialize the esp-idf environment"
  command -v esptool >/dev/null 2>&1 || die "esptool not found, please initialize the esp-idf environment"
  command -v sftp >/dev/null 2>&1 || die "sftp not found"
}

check_deps

project_dir=$(pwd)
# Standard firmware: same configuration as the GitHub release (.github/workflows/release.yml),
# built from sdkconfig.defaults in its own directory so the local sdkconfig is never used
build_dir="$project_dir/build-release"
defaults="$project_dir/sdkconfig.defaults"
firmware="$build_dir/agrosmart.bin"
version_file="$tmp_dir/OTA/version.txt"
idf=(idf.py -B "$build_dir" -D SDKCONFIG="$build_dir/sdkconfig" -D SDKCONFIG_DEFAULTS="$defaults")

[[ -f "$defaults" ]] || die "sdkconfig.defaults not found: $defaults"

if [[ "$clean_build" == true ]]; then
    warning "Cleaning ESP-IDF build artifacts..."
    rm -rf "$build_dir"
fi

# Regenerate the configuration on every run, so changes to sdkconfig.defaults are always applied
rm -f "$build_dir/sdkconfig"

progress "Building standard firmware from sdkconfig.defaults..."
"${idf[@]}" build

[[ -f "$firmware" ]] || die "Firmware not found: $firmware"

image_info=$(esptool image-info "$firmware") || die "esptool image-info failed for $firmware"
firmware_hash=$(printf '%s\n' "$image_info" | awk -F: '/ELF file SHA256:/ { sub(/^[[:space:]]*/, "", $2); sub(/[[:space:]]*$/, "", $2); print $2; exit }')
[[ -n "$firmware_hash" ]] || die "ELF file SHA256 not found in esptool image-info output"

info "Cloning OTA repo..."

git clone https://github.com/agromate-devs/OTA.git $tmp_dir/OTA
cp "$firmware" $tmp_dir/OTA
printf '%s\n' "$firmware_hash" > "$version_file"
info "Firmware ELF SHA-256: $firmware_hash"

cd $tmp_dir/OTA
git add .
git commit -m "Deploy $(date -R)" || exit 255
git push

success "OTA upload completed. Cleaning up."

rm -rf $tmp_dir