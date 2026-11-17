package com.wavebrowser.android

import android.os.Bundle
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.appcompat.app.AppCompatActivity
import com.wavebrowser.android.WaveToolbar

/** Wave's Android host for the Chromium-backed browser surface. */
class MainActivity : AppCompatActivity() {
    private lateinit var toolbar: WaveToolbar

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = FrameLayout(this)
        toolbar = WaveToolbar(this) { url -> navigateChromium(url) }
        root.addView(toolbar, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ))

        // The Chromium content surface is supplied by the Chromium Android build.
        // WaveToolbar is kept as the navigation host; Chromium owns page rendering.
        setContentView(root)
    }

    private fun navigateChromium(url: String) {
        // Chromium navigation is wired through the Android Chromium host at build time.
        // The URL is intentionally kept in the Wave toolbar until that host is attached.
        toolbar.addressBar.setText(url)
        toolbar.addressBar.setSelection(toolbar.addressBar.length())
    }
}
