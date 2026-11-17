# Wave Browser — Windows

The Windows browser uses the same Chromium-source build model as the Android browser:

1. A specific Chromium revision is pinned in `chromium_revision.txt`.
2. The build environment fetches Chromium source at build time.
3. Wave's Chromium changes are applied during the build.
4. Chromium is compiled for Windows and packaged as Wave Browser.
5. Chromium source is not stored in this repository.

Windows-specific browser/device code belongs only in this `windows/` directory.

The Windows UI is intentionally not implemented yet. It will be designed separately from Android.
