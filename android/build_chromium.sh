#!/usr/bin/env bash
set -euo pipefail

ANDROID_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHECKOUT_DIR="${ANDROID_DIR}/chromium-checkout"
SRC_DIR="${CHECKOUT_DIR}/src"
OUT_DIR="${SRC_DIR}/out/Wave"
REVISION_FILE="${ANDROID_DIR}/chromium_revision.txt"

REVISION="$(grep -v '^#' "${REVISION_FILE}" | tr -d '[:space:]')"
if [[ ! "${REVISION}" =~ ^[0-9a-f]{40}$ ]]; then
  echo "Invalid Chromium revision in ${REVISION_FILE}: ${REVISION}" >&2
  exit 1
fi

mkdir -p "${CHECKOUT_DIR}"
cd "${CHECKOUT_DIR}"

if [[ ! -d "${SRC_DIR}/.git" ]]; then
  fetch --nohooks https://chromium.googlesource.com/chromium/src.git src
fi

cd "${SRC_DIR}"
git fetch origin main
git checkout --detach "${REVISION}"
gclient sync --nohooks --revision "src@${REVISION}"
gclient runhooks

# Wave currently uses Chromium's Android browser target as the engine/UI base.
# Wave-specific UI patches will be applied here as they are added.
cat > "${OUT_DIR}.args" <<'EOF'
target_os = "android"
target_cpu = "arm64"
is_component_build = false
is_official_build = false
chrome_public_manifest_package = "com.wavebrowser.android"
EOF

gn gen "${OUT_DIR}" --args="$(cat "${OUT_DIR}.args")"
autoninja -C "${OUT_DIR}" chrome_public_apk

APK="${OUT_DIR}/apks/ChromePublic.apk"
if [[ ! -f "${APK}" ]]; then
  echo "Chromium build completed but APK was not found at ${APK}" >&2
  exit 1
fi

cp "${APK}" "${ANDROID_DIR}/WaveBrowser.apk"
echo "Built ${ANDROID_DIR}/WaveBrowser.apk from Chromium ${REVISION}."
