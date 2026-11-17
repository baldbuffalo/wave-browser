#!/usr/bin/env pwsh
# Save the Windows build outputs to OneDrive.
#
# Usage: save-build-outputs.ps1 <checkpoint|engine>
#
# PowerShell port of save-build-outputs.sh. Only the build output tree is
# archived, never the Chromium source: the source is ~20 GB of pristine checkout
# that the finalized package already holds on OneDrive, and re-uploading it is
# what made saving too slow to finish.
#
# Runs from an always() step so a slot that dies for any reason - compile error,
# runner loss, cancellation - still leaves something for the next run to resume.
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('checkpoint', 'engine')]
  [string]$Mode
)

$ErrorActionPreference = 'Stop'

$root = $env:WAVE_WORK_ROOT
if (-not $root) { throw 'WAVE_WORK_ROOT is not set' }

$revisionFile = Join-Path $root '.wave_revision'
if (-not (Test-Path $revisionFile)) {
  Write-Host 'No work tree to save (no .wave_revision).'
  exit 0
}

# The restore step sets this when the engine was never fetched; there is
# nothing built to save, and saving here would mask that failure.
if (Test-Path (Join-Path $root '.wave_no_engine')) {
  Write-Host 'The engine was not restored; skipping the save.'
  exit 0
}

$revision = ((Get-Content $revisionFile -Raw) -replace '\s', '')
$staging = "$env:WAVE_CHECKPOINT_ROOT/$revision"

if ($Mode -eq 'checkpoint') {
  # A finished engine outranks a partial checkpoint; never create one that could
  # shadow it on restore.
  rclone lsf "onedrive:$staging/engine.ready" *> $null
  if ($LASTEXITCODE -eq 0) {
    Write-Host "A finished engine already exists for $revision; skipping the checkpoint."
    exit 0
  }
  $remote = "onedrive:$staging/checkpoint.tar.zst"
  $ready = 'checkpoint.ready'
} else {
  $remote = "onedrive:$staging/engine.tar.zst"
  $ready = 'engine.ready'
}

$outRel = if ($env:WAVE_OUT_REL) { $env:WAVE_OUT_REL } else { 'src/out/WaveWin' }
$outFull = Join-Path $root $outRel
if (-not (Test-Path $outFull)) {
  Write-Host "No build output at $outFull; nothing to save."
  exit 0
}

rclone deletefile "onedrive:$staging/$ready" *> $null

Write-Host "Saving Windows build $Mode for $revision."

# tar | zstd | rclone must run inside cmd.exe: PowerShell pipes a native
# command's output as text, which corrupts a binary stream. --format=pax is
# required, not cosmetic - it preserves the sub-second mtimes ninja compares to
# decide what is stale, so a restored object is not seen as modified.
$outRelWin = $outRel -replace '/', '\'
$includes = $outRelWin
if (Test-Path (Join-Path $root '.gclient')) { $includes = "$outRelWin .gclient" }

$pipeline = "tar -C `"$root`" --format=pax -cf - $includes " +
  "| zstd -T0 -1 " +
  "| rclone rcat `"$remote.partial`" --onedrive-chunk-size 250M --buffer-size 256M " +
  "--retries 5 --low-level-retries 20 --timeout 10m --stats 30s --stats-one-line"
cmd.exe /c $pipeline
if ($LASTEXITCODE -ne 0) { throw "Build output upload failed with exit code $LASTEXITCODE." }

rclone moveto "$remote.partial" "$remote"
if ($LASTEXITCODE -ne 0) { throw 'Could not finalize the uploaded archive.' }
$revision | rclone rcat "onedrive:$staging/$ready"
if ($LASTEXITCODE -ne 0) { throw "Could not write $ready." }

if ($Mode -eq 'engine') {
  # The finished engine supersedes every checkpoint artefact for this revision.
  rclone deletefile "onedrive:$staging/checkpoint.ready" *> $null
  rclone deletefile "onedrive:$staging/checkpoint.tar.zst" *> $null
  rclone deletefile "onedrive:$staging/checkpoint.tar.zst.partial" *> $null
  rclone deletefile "onedrive:$staging/chain.txt" *> $null
}

Write-Host "Saved the Windows $Mode for $revision."
