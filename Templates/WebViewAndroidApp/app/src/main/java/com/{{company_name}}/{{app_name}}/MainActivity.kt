package com.{{company_name}}.{{app_name}}

import android.annotation.SuppressLint
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Process
import android.util.Log
import android.view.View
import android.webkit.*
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import java.util.*
import com.{{company_name}}.{{app_name}}.BuildConfig
import com.{{company_name}}.{{app_name}}.R

class MainActivity : AppCompatActivity(), ValueCallback<String> {

    companion object {
        const val requestCodePermissionAudio = 1
    }

    private var webView: WebView? = null
    private var chromeClient: MyWebChromeClient? = null
    private var webViewThread: WebViewThread? = null

    private var resultString = ""

    private fun logD(msg: String?, tr: Throwable? = null) {
        if (BuildConfig.DEBUG) {
            Log.d("MainActivity", msg, tr)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        logD("onCreate")
        installSplashScreen()
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        supportActionBar?.hide()
        if (false) {
            initWebView()
        } else {
            webViewThread = WebViewThread(this)
            webViewThread?.start()
        }
    }

    override fun onStart() {
        logD("onStart")
        super.onStart()
    }

    override fun onResume() {
        logD("onResume")
        super.onResume()
        webView?.onResume()
        chromeClient?.onResume()
    }

    override fun onPause() {
        logD("onPause")
        super.onPause()
        webView?.onPause()
    }

    override fun onStop() {
        logD("onStop")
        super.onStop()
        webView?.onPause()
    }

    override fun onDestroy() {
        logD("onDestroy")
        webView?.destroy()
        super.onDestroy()
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray)
    {
        if (requestCode == requestCodePermissionAudio) {
            if (grantResults.isNotEmpty()) {
                if (grantResults[0] != PackageManager.PERMISSION_GRANTED) {
                    logD("Not PERMISSION_GRANTED!")
                } else {
                    val myRequest = chromeClient?.myRequest
                    if (myRequest != null) {
                        myRequest.grant(myRequest.resources)
                    }
                }
            }
        }
    }

    // ValueCallback<String>
    override fun onReceiveValue(value: String) {
        resultString = value
    }

    fun initWebView() {
        webView = findViewById(R.id.web_view)
        webView?.post {
            webView?.setBackgroundColor(0)
            initWebSettings()
        }
        webView?.post {
            webView?.webViewClient = MyWebViewClient(object: MyWebViewClient.Listener {
                override fun onReceivedError(view: WebView?, request: WebResourceRequest?,
                                             error: WebResourceError?)
                {
                    logD("onReceivedError")
                }

                override fun onReceivedHttpError(view: WebView?, request: WebResourceRequest?,
                                                 errorResponse: WebResourceResponse?)
                {
                    logD("onReceivedHttpError")
                }

                override fun onPageFinished(view: WebView?, url: String?) {
                    logD("onPageFinished")
                }
            })

            chromeClient = MyWebChromeClient(this, object: MyWebChromeClient.Listener {
            })
            webView?.webChromeClient = chromeClient
            webView?.addJavascriptInterface(chromeClient!!, "android")
            webView?.loadUrl(getString(R.string.url))
        }
    }

    @SuppressLint("SetJavaScriptEnabled")
    private fun initWebSettings() {
        val settings = webView?.settings
        settings?.javaScriptEnabled = true
        settings?.domStorageEnabled = true
        settings?.mediaPlaybackRequiresUserGesture = false
        if (BuildConfig.DEBUG) {
            settings?.cacheMode = WebSettings.LOAD_NO_CACHE
            WebView.setWebContentsDebuggingEnabled(true)
        }
        if (settings != null) {
            val versionName = getVersionName()
            updateUserAgent(settings, versionName)
        }
    }

    private fun updateUserAgent(settings: WebSettings, versionName: String) {
        var userAgent: String? = settings.userAgentString
        if (userAgent != null) {
            userAgent += "/native-app/$versionName/"
            settings.userAgentString = userAgent
        }
    }

    private fun getVersionName(): String {
        val appName: String = this.packageName
        val pm: PackageManager = this.packageManager
        val pi: PackageInfo = pm.getPackageInfo(appName, PackageManager.GET_META_DATA)
        return pi.versionName
    }

    class WebViewThread(private val activity: MainActivity) : Thread() {
        override fun run() {
            Process.setThreadPriority(Process.THREAD_PRIORITY_MORE_FAVORABLE)
            activity.initWebView()
        }
    }
}