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

# gclient runhooks calls vs_toolchain.py update --force, which fetches the
# Windows toolchain whenever DEPOT_TOOLS_WIN_TOOLCHAIN is unset, even on Linux.
# That download needs LUCI auth and fails with a 401 against the
# chrome-wintoolchain bucket. An Android build never uses it, so opt out.
export DEPOT_TOOLS_WIN_TOOLCHAIN=0

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

# Hooks must run every time: the finalized package is produced with --nohooks
# and the saved build outputs cover only src/out/Wave, so the clang toolchain and
# other hook output are never present on restore. The saved objects still make
# the subsequent compile a near no-op, which is the expensive part.
gclient runhooks

cat > "${OUT_DIR}.args" <<'EOF'
target_os = "android"
target_cpu = "arm64"
is_component_build = false
is_official_build = true
# is_debug defaults to true, which is what made this a debug build: it emitted a
# split-debug (.dwo) file for nearly every compile and produced ~20 GB of
# output. It must be off for is_official_build to take effect at all.
is_debug = false
# Debug info is the single largest cost in the build. With no symbols the
# compiles are roughly twice as fast and the checkpoint roughly half the size.
symbol_level = 0
blink_symbol_level = 0
v8_symbol_level = 0
# is_official_build turns ThinLTO on by default for Android, and the LTO link
# of libchrome is a long single-threaded step that the disk-budget checkpoint
# would interrupt. This build is about producing the engine, not shipping a
# maximally optimized binary.
use_thin_lto = false
# Warnings are already errors in every Chromium build (treat_warnings_as_errors
# defaults to true), so this is belt-and-braces. Our own patch cannot fail
# harder in official mode than it already can today.
treat_warnings_as_errors = false
# is_official_build also turns on V8 builtins PGO, which makes gen/v8/embedded.S
# depend on v8/tools/builtins-pgo/profiles/x64.profile. That profile is fetched
# by a gclient hook and the finalized package is produced with --nohooks, so the
# file is absent and no rule can build it:
#   "../../v8/tools/builtins-pgo/profiles/x64.profile", needed by
#   "gen/v8/embedded.S", missing and no known rule to make it
# Build V8 without builtins PGO instead of adding a profile download.
v8_enable_builtins_optimization = false
# Chrome PGO likewise wants a downloaded profile that a no-hooks checkout does
# not have. Off.
chrome_pgo_phase = 0
chrome_public_manifest_package = "com.wavebrowser.android"
EOF

# No compiler cache is wired in. sccache refuses every Chromium compile because
# of -fmodules, so it stored nothing and only added a wrapper process per file.
# Resuming a build is handled by the ninja checkpoint instead.

gn gen "${OUT_DIR}" --args="$(cat "${OUT_DIR}.args")"
autoninja -C "${OUT_DIR}" chrome_public_apk

APK="${OUT_DIR}/apks/ChromePublic.apk"
if [[ ! -f "${APK}" ]]; then
  echo "Chromium build completed but APK was not found at ${APK}" >&2
  exit 1
fi

cp "${APK}" "${ANDROID_DIR}/WaveBrowser.apk"
echo "Built ${ANDROID_DIR}/WaveBrowser.apk from Chromium ${REVISION}."
