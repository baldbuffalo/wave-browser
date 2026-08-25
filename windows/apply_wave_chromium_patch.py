from pathlib import Path
import os

ROOT = Path(__file__).resolve().parent
# GitHub Actions fetches Chromium into D:\a\src. Prefer the explicit
# CHROMIUM_SRC value, then derive the same location from GITHUB_WORKSPACE.
CHROMIUM = Path(
    os.environ.get(
        'CHROMIUM_SRC',
        str(Path(os.environ['GITHUB_WORKSPACE']).parent.parent / 'src')
        if os.environ.get('GITHUB_WORKSPACE')
        else str(ROOT / '..' / '..' / 'src'),
    )
).resolve()


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding='utf-8')
    if new in text:
        return
    if old not in text:
        raise SystemExit(f'Wave Chromium patch could not find expected source in {path}')
    path.write_text(text.replace(old, new, 1), encoding='utf-8')

# Use Chromium's real LocationBarView as Wave's minimal desktop search/address bar.
# This keeps navigation/rendering inside Chromium instead of embedding another engine.
toolbar = CHROMIUM / 'chrome/browser/ui/views/toolbar/toolbar_view.cc'
replace_once(
    toolbar,
    '''ToolbarView::DisplayMode GetDisplayMode(Browser* browser) {\n  // Checked in this order because even tabbed PWAs use the CUSTOM_TAB\n  // display mode.\n''',
    '''ToolbarView::DisplayMode GetDisplayMode(Browser* browser) {\n  // Wave desktop intentionally uses a single minimal location toolbar.\n  // Chromium remains responsible for the actual browser content and navigation.\n  return ToolbarView::DisplayMode::kLocation;\n\n  // Checked in this order because even tabbed PWAs use the CUSTOM_TAB\n  // display mode.\n''',
)

# Hide Chromium's tab strip so the desktop UI remains minimal.
layout = CHROMIUM / 'chrome/browser/ui/views/frame/browser_view_layout.cc'
replace_once(
    layout,
    '''void BrowserViewLayout::LayoutTabStripRegion(gfx::Rect& available_bounds) {\n  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutTabStripRegion");\n''',
    '''void BrowserViewLayout::LayoutTabStripRegion(gfx::Rect& available_bounds) {\n  TRACE_EVENT0("ui", "BrowserViewLayout::LayoutTabStripRegion");\n  // Wave uses a minimal desktop UI without a tab strip for now.\n  SetViewVisibility(tab_strip_region_view_, false);\n  tab_strip_region_view_->SetBounds(0, 0, 0, 0);\n  return;\n''',
)

print(f'Wave Chromium Windows UI patch applied to {CHROMIUM}.')
