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

OUT_ARGS="${OUT_DIR}.args"
cat > "${OUT_ARGS}" <<'EOF'
target_os = "android"
target_cpu = "arm64"
is_component_build = false
is_official_build = true
is_debug = false
symbol_level = 0
blink_symbol_level = 0
v8_symbol_level = 0
use_thin_lto = true
treat_warnings_as_errors = false
v8_enable_builtins_optimization = false
# Chrome PGO profile. Set to 0 here and raised to 2 below only when a profile
# is actually present, because chrome_pgo_phase = 2 with no profile fails.
chrome_pgo_phase = 0
chrome_public_manifest_package = "com.wavebrowser.android"
EOF

# update-chromium.yml downloads the public android-arm64 PGO profile into
# src/chrome/build/pgo_profiles when it packages the source. Use it only if it
# is there: the profile is keyed to the Chromium revision, and a stale or
# absent profile would either skew optimization or fail the build outright.
if compgen -G "${SRC_DIR}/chrome/build/pgo_profiles/*.profdata" >/dev/null; then
  echo "Using the PGO profile in src/chrome/build/pgo_profiles."
  sed -i 's/^chrome_pgo_phase = 0$/chrome_pgo_phase = 2/' "${OUT_ARGS}"
else
  echo "No PGO profile present; building without PGO."
fi

# No custom keystore or release-signing configuration is used.

# No compiler cache is wired in. sccache refuses every Chromium compile because
# of -fmodules, so it stored nothing and only added a wrapper process per file.
# Resuming a build is handled by the ninja checkpoint instead.

gn gen "${OUT_DIR}" --args="$(cat "${OUT_ARGS}")"

# The ThinLTO link of libchrome is a long single-threaded step that a slot's
# time budget can interrupt, and a killed link leaves nothing to checkpoint. So
# the compile slots build the object files and stop before the link, and a
# dedicated linking job (WAVE_LINK_ONLY=1) runs the link to completion without a
# slot budget. Ninja does not expose a "compile only" target, so detect the
# handoff point with a dry run: once no compile commands remain and a link is
# the only work left, stop. A dry run that fails is treated as "compiles
# remain" so the slot never hands off on an unreliable signal.
if [[ "${WAVE_LINK_ONLY:-0}" == "1" ]]; then
  autoninja -C "${OUT_DIR}" chrome_public_apk
else
  set +e
  dry_run="$(ninja -C "${OUT_DIR}" -n chrome_public_apk 2>&1)"
  dry_rc=$?
  set -e
  compiles_left=1
  if [[ "${dry_rc}" -eq 0 ]]; then
    compiles_left="$(printf '%s\n' "${dry_run}" | grep -cE '(^|/)(clang|clang\+\+)(-[0-9]+)? .* (-c|/c) ' || true)"
  fi
  if [[ "${dry_rc}" -eq 0 && "${compiles_left}" -eq 0 ]] &&
     printf '%s\n' "${dry_run}" | grep -qE 'ld\.lld|solink'; then
    echo "All compiles are done; only the ThinLTO link remains."
    : > "${ANDROID_DIR}/.wave_link_only"
    exit 0
  fi
  autoninja -C "${OUT_DIR}" chrome_public_apk
fi

APK="${OUT_DIR}/apks/ChromePublic.apk"
if [[ ! -f "${APK}" ]]; then
  echo "Chromium build completed but APK was not found at ${APK}" >&2
  exit 1
fi

cp "${APK}" "${ANDROID_DIR}/WaveBrowser.apk"
echo "Built ${ANDROID_DIR}/WaveBrowser.apk from Chromium ${REVISION}."
