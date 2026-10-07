"""Apply Wave's UI layer to a Chromium checkout.

This is the Wave UI layer, the same approach Brave and Vivaldi use: Wave's UI
is written into Chromium's source and compiled in, producing one browser.

Keep every change here small and self-contained. The compiled engine is cached
per Chromium revision and restored before this script runs, so the files edited
below become newer than the cached objects and ninja rebuilds only them plus the
final link. A UI-only change therefore never recompiles the engine.
"""

import os
from pathlib import Path

UI_VERSION = "1"


def chromium_src() -> Path:
    explicit = os.environ.get("CHROMIUM_SRC")
    if explicit:
        return Path(explicit).resolve()
    workspace = os.environ.get("GITHUB_WORKSPACE")
    if workspace:
        return (Path(workspace).parent.parent / "src").resolve()
    return (Path(__file__).resolve().parent.parent / "src").resolve()


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if old not in text:
        raise SystemExit(f"Wave UI patch could not find expected source in {path}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def apply_ui(chromium: Path) -> None:
    # Use Chromium's real LocationBarView as Wave's minimal desktop
    # search/address bar. Navigation and rendering stay inside Chromium.
    replace_once(
        chromium / "chrome/browser/ui/views/toolbar/toolbar_view.cc",
        "ToolbarView::DisplayMode GetDisplayMode(Browser* browser) {\n"
        "  // Checked in this order because even tabbed PWAs use the CUSTOM_TAB\n"
        "  // display mode.\n",
        "ToolbarView::DisplayMode GetDisplayMode(Browser* browser) {\n"
        "  // Wave desktop intentionally uses a single minimal location toolbar.\n"
        "  // Chromium remains responsible for the actual browser content.\n"
        "  return ToolbarView::DisplayMode::kLocation;\n\n"
        "  // Checked in this order because even tabbed PWAs use the CUSTOM_TAB\n"
        "  // display mode.\n",
    )

    # Hide Chromium's tab strip so the desktop UI stays minimal.
    replace_once(
        chromium / "chrome/browser/ui/views/frame/browser_view_layout.cc",
        "void BrowserViewLayout::LayoutTabStripRegion(gfx::Rect& available_bounds) {\n"
        '  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutTabStripRegion");\n',
        "void BrowserViewLayout::LayoutTabStripRegion(gfx::Rect& available_bounds) {\n"
        '  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutTabStripRegion");\n'
        "  // Wave uses a minimal desktop UI without a tab strip for now.\n"
        "  SetViewVisibility(tab_strip_region_view_, false);\n"
        "  tab_strip_region_view_->SetBounds(0, 0, 0, 0);\n"
        "  return;\n",
    )


def main() -> None:
    chromium = chromium_src()
    if not chromium.is_dir():
        raise SystemExit(f"Chromium source not found at {chromium}")
    apply_ui(chromium)
    print(f"Wave UI layer (version {UI_VERSION}) applied to {chromium}.")


if __name__ == "__main__":
    main()
