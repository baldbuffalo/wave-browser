#!/usr/bin/env bash
set -euo pipefail

ANDROID_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# WAVE_CHROMIUM_ROOT may be a relative path (e.g. chromium-checkout). Resolve it
# to an absolute path here, because this script cd's into CHECKOUT_DIR below;
# a relative CHECKOUT_DIR would otherwise be re-interpreted against that new
# working directory when SRC_DIR is used, doubling the path segment.
CHECKOUT_DIR="${WAVE_CHROMIUM_ROOT:-${ANDROID_DIR}/chromium-checkout}"
CHECKOUT_DIR="$(cd "${CHECKOUT_DIR}" 2>/dev/null && pwd || printf '%s' "${CHECKOUT_DIR}")"
SRC_DIR="${CHECKOUT_DIR}/src"
OUT_DIR="${SRC_DIR}/out/Wave"
REVISION_FILE="${ANDROID_DIR}/chromium_revision.txt"

export GIT_CACHE_PATH="${GIT_CACHE_PATH:-${CHECKOUT_DIR}/git-cache}"
mkdir -p "${CHECKOUT_DIR}" "${GIT_CACHE_PATH}"

REVISION="$(grep -v '^#' "${REVISION_FILE}" | tr -d '[:space:]')"
if [[ ! "${REVISION}" =~ ^[0-9a-f]{40}$ ]]; then
  echo "Invalid Chromium revision in ${REVISION_FILE}: ${REVISION}" >&2
  exit 1
fi

cd "${CHECKOUT_DIR}"

if [[ "${WAVE_CHROMIUM_PREPARED:-0}" != "1" ]]; then
  if [[ ! -d "${SRC_DIR}/.git" ]]; then
    echo "Chromium checkout is missing. Run the Chromium package workflow first." >&2
    exit 1
  fi
  cd "${SRC_DIR}"
  git fetch origin main
  git checkout --detach "${REVISION}"
  gclient sync --nohooks --revision "src@${REVISION}"
fi

cd "${SRC_DIR}"

# A restored finished engine already contains the hook output and the ninja
# build files, so re-running hooks is wasted work. It also keeps the build off
# the ciopfs/libfuse path entirely. gn gen and autoninja still run: the engine
# is stored without APKs, so the package step has to produce the APK.
if [[ "${WAVE_ENGINE_RESTORED:-0}" == "1" && -d "${OUT_DIR}" ]]; then
  echo "Finished Android engine restored; skipping hooks and reconfiguration."
else
  # Chromium's vs_toolchain.py invokes ciopfs, which runhooks downloads into
  # src/build/. ciopfs links against libfuse.so.2; the Ubuntu 24.04 runner ships
  # fuse3 only, so the download succeeds but execution fails with
  # "libfuse.so.2: cannot open shared object file". Install the fuse2
  # compatibility library before runhooks needs it.
  if ! ldconfig -p 2>/dev/null | grep -q 'libfuse\.so\.2'; then
    echo "Installing libfuse2 required by Chromium's ciopfs helper."
    sudo apt-get update -qq
    # Ubuntu 24.04 renamed the package to libfuse2t64 during the time_t
    # transition; older images still ship libfuse2. Install whichever exists.
    sudo apt-get install -y libfuse2 || sudo apt-get install -y libfuse2t64
    ldconfig -p 2>/dev/null | grep -q 'libfuse\.so\.2' || {
      echo "libfuse.so.2 is still missing after installing libfuse2/libfuse2t64." >&2
      exit 1
    }
  fi

  gclient runhooks
fi

cat > "${OUT_DIR}.args" <<'EOF'
target_os = "android"
target_cpu = "arm64"
is_component_build = false
is_official_build = false
chrome_public_manifest_package = "com.wavebrowser.android"
EOF

# Reuse compiled objects across builds the way Chromium and Brave do, with a
# compiler cache wired in as Chromium's cc_wrapper. Objects keyed by source and
# flags are reused, so rebuilding after a UI change skips unchanged files.
if [[ -n "${SCCACHE_PATH:-}" ]]; then
  echo "cc_wrapper = \"${SCCACHE_PATH}\"" >> "${OUT_DIR}.args"
  echo "Using sccache compiler cache: ${SCCACHE_PATH}"
fi

gn gen "${OUT_DIR}" --args="$(cat "${OUT_DIR}.args")"
autoninja -C "${OUT_DIR}" chrome_public_apk

APK="${OUT_DIR}/apks/ChromePublic.apk"
if [[ ! -f "${APK}" ]]; then
  echo "Chromium build completed but APK was not found at ${APK}" >&2
  exit 1
fi

cp "${APK}" "${ANDROID_DIR}/WaveBrowser.apk"
echo "Built ${ANDROID_DIR}/WaveBrowser.apk from Chromium ${REVISION}."
