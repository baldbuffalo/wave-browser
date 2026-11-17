package com.wavebrowser.android;

import android.content.Context;
import android.graphics.Color;
import android.net.Uri;
import android.text.InputType;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;

/** Minimal Wave toolbar for Chromium-backed Android browsing. */
public final class WaveToolbar extends LinearLayout {
    public interface NavigationListener {
        void navigate(String url);
    }

    public final EditText addressBar;

    public WaveToolbar(Context context, NavigationListener listener) {
        super(context);
        setOrientation(HORIZONTAL);
        setGravity(Gravity.CENTER_VERTICAL);
        setPadding(12, 8, 8, 8);
        setBackgroundColor(Color.WHITE);

        addressBar = new EditText(context);
        addressBar.setSingleLine(true);
        addressBar.setHint("Search or enter address");
        addressBar.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        addressBar.setImeOptions(EditorInfo.IME_ACTION_GO);
        addressBar.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_GO ||
                    (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                listener.navigate(toNavigationUrl(addressBar.getText().toString()));
                return true;
            }
            return false;
        });
        addView(addressBar, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        ImageButton bookmark = new ImageButton(context);
        bookmark.setContentDescription("Bookmark current page");
        bookmark.setImageResource(android.R.drawable.btn_star_big_off);
        bookmark.setBackgroundColor(Color.TRANSPARENT);
        addView(bookmark, new LinearLayout.LayoutParams(56, 56));
    }

    private static String toNavigationUrl(String input) {
        String text = input.trim();
        if (text.isEmpty()) return "https://www.google.com/";
        Uri parsed = Uri.parse(text);
        if (parsed.getScheme() != null) return text;
        if (text.contains(".") && !text.contains(" ")) return "https://" + text;
        return "https://www.google.com/search?q=" + Uri.encode(text);
    }
}
