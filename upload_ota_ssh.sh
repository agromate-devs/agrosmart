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
  ./upload_ota.sh [-c|--clean] -u|--user USER -i|--ip HOST -p|--path REMOTE_PATH

Opzioni:
  -c, --clean       Run idf.py clean before build
  -u, --user USER   SSH user of the server.
  -i, --ip HOST     IP or hostname of the server.
  -p, --path PATH   Remote directory where upload build artifacts
  -ev, --esp-idf-version Version of ESP-IDF to use for building
  -h, --help
EOF
}

die() {
    printf '%sError:%s %s\n' "$COLOR_RED" "$COLOR_RESET" "$*" >&2
    exit 1
}

clean_build=false
remote_user=''
remote_host=''
remote_path=''
esp_idf_version=''

check_params() {
  [[ -n "$remote_user" ]] || die "Missing user"
  [[ -n "$remote_host" ]] || die "Missing host"
  [[ -n "$remote_path" ]] || die "Missing remote path"
  [[ -n "$esp_idf_version" ]] || die "Missing ESP-IDF version"
}

if [[ $# -lt 3 ]]
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
        -u|--user)
            (($# >= 2)) || usage
            remote_user=$2
            shift 2
            ;;
        -i|--ip)
            (($# >= 2)) || usage
            remote_host=$2
            shift 2
            ;;
        -p|--path)
            (($# >= 2)) || usage
            remote_path=$2
            shift 2
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
build_dir="$project_dir/build"
firmware="$build_dir/agrosmart.bin"
version_file="$project_dir/version.txt"

cd "$project_dir"

if [[ "$clean_build" == true ]]; then
    warning "Cleaning ESP-IDF build artifacts..."
    idf.py fullclean
fi

progress "Building project..."
idf.py build

[[ -f "$firmware" ]] || die "Firmware not found: $firmware"

image_info=$(esptool image-info "$firmware") || die "esptool image-info failed for $firmware"
firmware_hash=$(printf '%s\n' "$image_info" | awk -F: '/ELF file SHA256:/ { sub(/^[[:space:]]*/, "", $2); sub(/[[:space:]]*$/, "", $2); print $2; exit }')
[[ -n "$firmware_hash" ]] || die "ELF file SHA256 not found in esptool image-info output"

printf '%s\n' "$firmware_hash" > "$version_file"
info "Firmware ELF SHA-256: $firmware_hash"

upload_to_server() {
  remote_destination="${remote_user}@${remote_host}"
  remote_directory=${remote_path%/}
  [[ -n "$remote_directory" ]] || remote_directory=/
  progress "Uploading $(basename "$firmware") and version.txt to $remote_destination:$remote_directory via SFTP..."
  sftp "$remote_destination" <<EOF
  cd "$remote_directory"
  put "$firmware"
  put "$version_file"
EOF
}

upload_to_server

success "OTA upload completed. Cleaning up."

rm version.txt