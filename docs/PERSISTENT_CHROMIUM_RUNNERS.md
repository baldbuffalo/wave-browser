# Persistent Chromium build runners

Wave's Chromium builds are designed to run on persistent GitHub Actions self-hosted runners, not disposable `*-latest` runners.

GitHub supports self-hosted runners on cloud virtual machines, and the same machine can retain its filesystem between jobs. This is important because Chromium's checkout and Git cache are intentionally kept outside `GITHUB_WORKSPACE`.

## Runner layout

Use two persistent cloud VMs:

### Windows runner

GitHub Actions label:

```text
self-hosted, wave-windows
```

Persistent paths:

```text
D:\wave-build\chromium
D:\wave-build\git-cache
D:\wave-build\depot_tools
D:\wave-build\outputs
```

Required environment/tools:

- Windows 10/11 or supported Windows Server
- Visual Studio with the Chromium Windows build requirements
- Git
- Python as required by Chromium/depot_tools

Chromium uses `DEPOT_TOOLS_WIN_TOOLCHAIN=0`, so the build uses the Visual Studio installation on this runner.

### Linux Android runner

GitHub Actions label:

```text
self-hosted, wave-android
```

Persistent paths:

```text
/opt/wave-build/chromium
/opt/wave-build/git-cache
/opt/wave-build/depot_tools
```

Required environment/tools:

- Supported 64-bit Linux distribution
- Git
- Java/Android SDK/NDK required by the Chromium Android build
- Chromium/depot_tools prerequisites

## Why this persists

The GitHub checkout only contains the Wave repository. Chromium lives outside that workspace, so later `actions/checkout` steps do not delete it.

The build scripts set `GIT_CACHE_PATH` to the persistent Git cache. Chromium's `fetch --git-cache` can seed a checkout from that shared cache, while subsequent `gclient sync` operations reuse the existing checkout and fetch only what changed.

## Workflow flow

```text
update-chromium.yml
        |
        +---- wave-android runner ----> Chromium Android build ----> WaveBrowser.apk
        |
        +---- wave-windows runner ----> Chromium Windows build ----> WaveBrowser.exe
                                      |
                                      +---- persistent Chromium checkout
                                      +---- persistent Git cache

android-build.yml  -> downloads the Android artifact
windows-build.yml  -> downloads the Windows artifact
```

## Important

The repository cannot provision cloud VMs or register self-hosted runners by itself. The two cloud VMs must first be registered with GitHub using the repository's self-hosted runner settings and the labels above.

Once registered and online, `update-chromium.yml` will target those persistent runners automatically.
