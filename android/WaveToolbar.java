package com.wavebrowser.android;

import android.content.Context;
import android.graphics.Color;
import android.text.InputType;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;

/** Minimal Wave toolbar intended to sit above Chromium's content surface. */
public final class WaveToolbar extends LinearLayout {
    public interface NavigationListener {
        void navigate(String text);
        void bookmarkCurrentPage();
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
        addressBar.setOnEditorActionListener((v, actionId, event) -> {
            listener.navigate(addressBar.getText().toString());
            return true;
        });
        addView(addressBar, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        ImageButton bookmark = new ImageButton(context);
        bookmark.setContentDescription("Bookmark current page");
        bookmark.setImageResource(android.R.drawable.btn_star_big_off);
        bookmark.setBackgroundColor(Color.TRANSPARENT);
        bookmark.setOnClickListener(v -> listener.bookmarkCurrentPage());
        addView(bookmark, new LinearLayout.LayoutParams(56, 56));
    }
}
