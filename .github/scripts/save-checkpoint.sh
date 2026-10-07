#!/usr/bin/env bash
# Archive the Android build work tree to OneDrive as a resumable checkpoint.
#
# Runs from an always() step so a slot that dies for any reason - compile
# error, runner loss, timeout, cancellation - still leaves something for the
# next run to resume from. The disk-budget path no longer uploads its own
# checkpoint; this is the single writer, which also avoids a double upload.
set -euo pipefail

ROOT="${WAVE_WORK_ROOT:?WAVE_WORK_ROOT is not set}"

if [ ! -f "$ROOT/.wave_revision" ]; then
  echo "No work tree to checkpoint (no .wave_revision); nothing to save."
  exit 0
fi

# The restore step sets this when the engine was never fetched; there is
# nothing built to save, and uploading here would mask that failure.
if [ -f "$ROOT/.wave_no_engine" ]; then
  echo "The engine was not restored; skipping the checkpoint."
  exit 0
fi

REVISION=$(tr -d '[:space:]' < "$ROOT/.wave_revision")
STAGING="$WAVE_CHECKPOINT_ROOT/$REVISION"
PARTIAL="$STAGING/checkpoint.tar.zst.partial"
FINAL="$STAGING/checkpoint.tar.zst"

# A finished engine outranks a partial checkpoint; never create one that could
# shadow it on restore.
if rclone lsf "onedrive:$STAGING/engine.ready" >/dev/null 2>&1; then
  echo "A finished engine already exists for $REVISION; skipping checkpoint."
  exit 0
fi

rclone deletefile "onedrive:$STAGING/checkpoint.ready" >/dev/null 2>&1 || true
echo "Uploading Android build checkpoint to OneDrive."
tar -C "$ROOT" \
  --exclude='*.apk' --exclude='*.aab' --exclude='*.apks' --exclude='*.idsig' \
  --exclude='*/out/Wave/apks*' --exclude='WaveBrowser.apk' \
  -cf - . | zstd -T0 -3 | rclone rcat "onedrive:$PARTIAL"
rclone moveto "onedrive:$PARTIAL" "onedrive:$FINAL"
printf '%s\n' "$REVISION" | rclone rcat "onedrive:$STAGING/checkpoint.ready"
echo "Checkpoint saved; the next run will resume it."
