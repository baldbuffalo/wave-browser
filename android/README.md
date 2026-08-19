# Wave Browser — Android

Minimal Android browser implementation using **Chromium via AndroidX WebKit/WebView**.

## Features

- Address/search bar
- Back / forward
- Reload
- Basic page loading and navigation
- HTTPS support through Chromium's Android WebView
- Simple native Android UI

This is intentionally a small Android port of Wave Browser. Platform-specific code lives under `android/` so the existing platform implementations remain separate.

## Build

Open the repository in Android Studio and build the `android` module/project. The Android system WebView provides the Chromium-based rendering engine.

> This implementation uses the Android WebView/AndroidX WebKit API rather than bundling a full Chromium build. A bundled Chromium engine would require a substantially larger Android-specific build system and Chromium source/dependencies.
