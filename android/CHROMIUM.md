# Bundled Chromium engine

Wave Android is intended to use a forked Chromium checkout rather than Android WebView.

## Source checkout

Chromium is maintained as a large multi-repository checkout using `depot_tools` and `gclient`. The complete Chromium source should be fetched into a separate working checkout and linked to this project rather than copied into the Wave Git repository.

Official Android build flow:

```bash
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$PATH:$PWD/depot_tools"
mkdir chromium && cd chromium
fetch --nohooks android
gclient runhooks
```

For a 64-bit ARM Android build, configure GN with:

```gn
target_os = "android"
target_cpu = "arm64"
is_component_build = false
```

Build the Chromium browser target with:

```bash
autoninja -C out/Default chrome_public_apk
```

Wave-specific browser UI and branding should be implemented in the Chromium `chrome/android` layer, while the Chromium engine remains the rendering/network/runtime foundation.

## Important

Do not put a fake WebView wrapper here and call it bundled Chromium. The actual Chromium checkout is intentionally external because the upstream source plus its dependencies is far too large for a normal application repository. The build integration can fetch a pinned Chromium revision during the Android build process.
