#!/usr/bin/env bash
# Save the Android build outputs to OneDrive.
#
# Usage: save-build-outputs.sh <checkpoint|engine>
#
# Only src/out/Wave is archived, never the Chromium source tree. The source is
# ~20 GB of pristine checkout that the finalized package already holds on
# OneDrive, and re-uploading it is what made the checkpoint too slow to finish:
# OneDrive uploads at roughly 5 MB/s while downloads run near 65 MB/s. The
# restore path reconstitutes the source from the package and overlays these
# outputs on top, so the source never has to be uploaded.
#
# Runs from an always() step so a slot that dies for any reason - compile error,
# runner loss, cancellation - still leaves something for the next run to resume.
set -euo pipefail

MODE="${1:?usage: save-build-outputs.sh <checkpoint|engine>}"

ROOT="${WAVE_WORK_ROOT:?WAVE_WORK_ROOT is not set}"

if [ ! -f "$ROOT/.wave_revision" ]; then
  echo "No work tree to save (no .wave_revision)."
  exit 0
fi

# The restore step sets this when the engine was never fetched; there is
# nothing built to save, and saving here would mask that failure.
if [ -f "$ROOT/.wave_no_engine" ]; then
  echo "The engine was not restored; skipping the save."
  exit 0
fi

REVISION=$(tr -d '[:space:]' < "$ROOT/.wave_revision")
STAGING="$WAVE_CHECKPOINT_ROOT/$REVISION"

if [ "$MODE" = "checkpoint" ]; then
  # A finished engine outranks a partial checkpoint; never create one that could
  # shadow it on restore.
  if rclone lsf "onedrive:$STAGING/engine.ready" >/dev/null 2>&1; then
    echo "A finished engine already exists for $REVISION; skipping the checkpoint."
    exit 0
  fi
  REMOTE="onedrive:$STAGING/checkpoint.tar.zst"
  READY="checkpoint.ready"
elif [ "$MODE" = "engine" ]; then
  REMOTE="onedrive:$STAGING/engine.tar.zst"
  READY="engine.ready"
else
  echo "Unknown mode: $MODE" >&2
  exit 2
fi

OUT="$ROOT/src/out/Wave"
if [ ! -d "$OUT" ]; then
  echo "No build output at $OUT; nothing to save."
  exit 0
fi

# Only build outputs are archived; packaged artifacts are excluded so the store
# never holds a release artefact.
INCLUDES=(src/out/Wave)
if [ -e "$ROOT/.gclient" ]; then
  INCLUDES+=(.gclient)
fi

rclone deletefile "onedrive:$STAGING/$READY" >/dev/null 2>&1 || true
echo "Saving Android build $MODE for $REVISION."

# --format=pax is required, not cosmetic. The default (gnu) format stores
# second-resolution mtimes, and ninja compares nanosecond mtimes to decide what
# is stale. Truncating them makes every restored object look modified, so a
# resumed slot recompiles the whole tree from scratch instead of continuing.
#
# The file-changed warnings are suppressed because GNU tar exits 1 on them. On
# the failure path the killed build may still be flushing an object, which would
# otherwise turn a save that actually succeeded into a failed step.
tar -C "$ROOT" --format=pax \
  --warning=no-file-changed --warning=no-file-removed \
  --exclude='*.apk' --exclude='*.aab' --exclude='*.apks' --exclude='*.idsig' \
  --exclude='*/out/Wave/apks*' \
  -cf - "${INCLUDES[@]}" \
  | zstd -T0 -1 \
  | rclone rcat "$REMOTE.partial" \
      --onedrive-chunk-size 250M \
      --buffer-size 256M \
      --retries 5 \
      --low-level-retries 20 \
      --timeout 10m \
      --stats 30s \
      --stats-one-line

rclone moveto "$REMOTE.partial" "$REMOTE"
printf '%s\n' "$REVISION" | rclone rcat "onedrive:$STAGING/$READY"

if [ "$MODE" = "engine" ]; then
  # The finished engine supersedes every checkpoint artefact for this revision.
  rclone deletefile "onedrive:$STAGING/checkpoint.ready" >/dev/null 2>&1 || true
  rclone deletefile "onedrive:$STAGING/checkpoint.tar.zst" >/dev/null 2>&1 || true
  rclone deletefile "onedrive:$STAGING/checkpoint.tar.zst.partial" >/dev/null 2>&1 || true
  rclone deletefile "onedrive:$STAGING/chain.txt" >/dev/null 2>&1 || true
  rclone deletefile "onedrive:$STAGING/link.ready" >/dev/null 2>&1 || true
fi

echo "Saved the Android $MODE for $REVISION."
