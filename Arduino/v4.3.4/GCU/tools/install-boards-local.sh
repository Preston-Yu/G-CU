#!/usr/bin/env bash
set -euo pipefail

ARDUINO_DATA_DIR="${1:-$HOME/Library/Arduino15}"
PAUSE=1
if [[ "${2:-}" == "--no-pause" ]]; then
  PAUSE=0
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE="${SCRIPT_DIR}/boards.local.txt"
PACKAGES_ROOT="${ARDUINO_DATA_DIR}/packages/esp32/hardware/esp32"

if [[ ! -f "${SOURCE}" ]]; then
  echo "Source file not found: ${SOURCE}" >&2
  exit 1
fi

if [[ ! -d "${PACKAGES_ROOT}" ]]; then
  echo "ESP32 core not found under: ${PACKAGES_ROOT}" >&2
  exit 1
fi

found=0
for version_dir in "${PACKAGES_ROOT}"/*; do
  if [[ -d "${version_dir}" && -f "${version_dir}/boards.txt" ]]; then
    cp -f "${SOURCE}" "${version_dir}/boards.local.txt"
    echo "Installed boards.local.txt -> ${version_dir}/boards.local.txt"
    found=1
  fi
done

if [[ "${found}" -eq 0 ]]; then
  echo "No ESP32 core versions with boards.txt were found under: ${PACKAGES_ROOT}" >&2
  exit 1
fi

echo
echo "Done. Restart Arduino IDE to see the Hardware Model menu."
if [[ "${PAUSE}" -eq 1 && -t 0 ]]; then
  read -r -p "Press Enter to exit" _
fi
