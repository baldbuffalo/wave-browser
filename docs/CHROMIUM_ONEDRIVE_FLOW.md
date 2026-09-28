# Chromium OneDrive build flow

- `.github/workflows/update-chromium.yml` is the only workflow that fetches a new Chromium revision.
- It packages the Chromium checkout and dependencies once and stores the revisioned archive in OneDrive.
- Android and Windows download that same revisioned archive and compile it locally.
- A platform runner keeps its extracted checkout and skips the OneDrive download when the revision is already present.
- UI-only changes therefore reuse the existing Chromium checkout/build state.

The workflows expect a GitHub Actions repository secret named `WAVE_CHROMIUM_BACKUP_ONEDRIVE`. It must contain the raw contents of an rclone configuration containing an `onedrive` remote. The rclone config must not be committed to the repository.

The packaging job runs on the persistent `wave-android` runner because standard GitHub-hosted runners currently provide only 14 GB of SSD storage, which is not appropriate for a Chromium checkout.