package com.wavebrowser.android

import android.annotation.SuppressLint
import android.os.Bundle
import android.view.KeyEvent
import android.view.inputmethod.EditorInfo
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.EditText
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.webkit.WebSettingsCompat
import androidx.webkit.WebViewFeature

class MainActivity : AppCompatActivity() {
    private lateinit var webView: WebView
    private lateinit var addressBar: EditText

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val toolbar = findViewById<Toolbar>(R.id.toolbar)
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayShowTitleEnabled(false)

        addressBar = findViewById(R.id.address_bar)
        webView = findViewById(R.id.web_view)

        webView.settings.javaScriptEnabled = true
        webView.settings.domStorageEnabled = true
        webView.settings.loadsImagesAutomatically = true
        webView.settings.builtInZoomControls = false
        webView.webViewClient = object : WebViewClient() {
            override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean = false
            override fun onPageFinished(view: WebView, url: String) {
                addressBar.setText(url)
            }
        }

        if (WebViewFeature.isFeatureSupported(WebViewFeature.FORCE_DARK)) {
            WebSettingsCompat.setForceDark(webView.settings, WebSettingsCompat.FORCE_DARK_FOLLOW_SYSTEM)
        }

        addressBar.setOnEditorActionListener { _, actionId, event ->
            if (actionId == EditorInfo.IME_ACTION_GO ||
                (event?.keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_DOWN)) {
                navigate(addressBar.text.toString())
                true
            } else false
        }

        findViewById<android.widget.ImageButton>(R.id.back).setOnClickListener {
            if (webView.canGoBack()) webView.goBack()
        }
        findViewById<android.widget.ImageButton>(R.id.forward).setOnClickListener {
            if (webView.canGoForward()) webView.goForward()
        }
        findViewById<android.widget.ImageButton>(R.id.reload).setOnClickListener { webView.reload() }

        if (savedInstanceState == null) navigate("https://www.google.com")
        else webView.restoreState(savedInstanceState)
    }

    private fun navigate(input: String) {
        val value = input.trim()
        if (value.isEmpty()) return
        val url = if (value.startsWith("http://") || value.startsWith("https://")) {
            value
        } else if (value.contains(".") && !value.contains(" ")) {
            "https://$value"
        } else {
            "https://www.google.com/search?q=" + java.net.URLEncoder.encode(value, "UTF-8")
        }
        webView.loadUrl(url)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        webView.saveState(outState)
        super.onSaveInstanceState(outState)
    }

    override fun onBackPressed() {
        if (webView.canGoBack()) webView.goBack() else super.onBackPressed()
    }
}
