# Wave Browser — Android

Minimal Android browser frontend designed around a Chromium engine.

## Architecture

`Wave UI → Chromium engine → websites`

Wave does **not** embed Chrome's UI or Chrome content. Google is only the default search engine.

The current Android implementation uses Android's Chromium-based WebView engine. It does not vendor the full Chromium source tree. A privately bundled Chromium engine would require integrating Chromium's Android build/dependency system (GN/Ninja, DEPS, native libraries, and packaging) and is a substantially larger engine-integration task.

## Features

- Address/search bar
- Google search for plain-text queries
- Direct URL navigation
- Back / forward
- Reload
- Chromium-backed web rendering
- Native Wave UI
