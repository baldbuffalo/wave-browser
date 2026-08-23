# Wave Windows Chromium integration

Wave's Windows browser is built from Chromium source at build time. The Wave desktop UI is implemented in the Chromium Windows target rather than embedding Chrome or WebView.

Current Wave desktop UI scope:
- Wave address/search bar
- Chromium page content surface

Navigation rules:
- `http://` and `https://` inputs navigate directly.
- Domain-like input gets `https://`.
- Other input becomes a Google search.

The Chromium checkout is intentionally not committed to this repository. `windows/build_chromium.bat` obtains it during the build.
