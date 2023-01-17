package com.{{company_name}}.{{app_name}}

import android.Manifest
import android.annotation.SuppressLint
import android.content.pm.PackageManager
import android.util.Log
import android.webkit.*
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.{{company_name}}.{{app_name}}.BuildConfig
import com.afollestad.materialdialogs.MaterialDialog
import com.afollestad.materialdialogs.customview.customView
import com.afollestad.materialdialogs.customview.getCustomView
import com.afollestad.materialdialogs.input.input
import com.afollestad.materialdialogs.lifecycle.lifecycleOwner
import com.afollestad.materialdialogs.utils.MDUtil.getStringArray
import com.{{company_name}}.{{app_name}}.R

class MyWebChromeClient(private val activity: AppCompatActivity, private val listener: Listener) :
    WebChromeClient() {

    interface Listener {
    }

    private fun getResString(resId: Int): String {
        return activity.getString(resId)
    }

    public var myRequest: PermissionRequest? = null

    override fun onPermissionRequest(request: PermissionRequest?) {
        if (request == null)
            return
        myRequest = request
        val permissionCheck =
            ContextCompat.checkSelfPermission(activity, Manifest.permission.RECORD_AUDIO)
        if (permissionCheck != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(
                activity, arrayOf(Manifest.permission.RECORD_AUDIO),
                MainActivity.requestCodePermissionAudio
            )
        } else {
            request.grant(request.resources)
        }
    }

    private var dialog: MaterialDialog? = null

    fun onResume() {
        if (dialog != null)
            dialog?.show()
    }

    override fun onJsAlert(
        view: WebView?,
        url: String?,
        message: String?,
        result: JsResult?
    ): Boolean {
        val title = getResString(R.string.app_name)
        dialog = MaterialDialog(activity).show {
            title(text = title)
            message(text = message)
            positiveButton(text = getResString(R.string.ok)) {
                dialog = null
            }
            cancelable(false)
            cancelOnTouchOutside(false)
            lifecycleOwner(activity)
        }
        result?.confirm()
        return true
    }

    override fun onJsConfirm(
        view: WebView?,
        url: String?,
        message: String?,
        result: JsResult?
    ): Boolean {
        val title = getResString(R.string.app_name)
        dialog = MaterialDialog(activity).show {
            title(text = title)
            message(text = message)
            positiveButton(text = getResString(R.string.ok)) {
                result?.confirm()
                dialog = null
            }
            negativeButton(text = getResString(R.string.cancel)) {
                result?.cancel()
                dialog = null
            }
            cancelable(false)
            cancelOnTouchOutside(false)
            lifecycleOwner(activity)
        }
        return true
    }

    @SuppressLint("CheckResult")
    override fun onJsPrompt(
        view: WebView?,
        url: String?,
        message: String?,
        defaultValue: String?,
        result: JsPromptResult?
    ): Boolean {
        val title = getResString(R.string.app_name)
        var inputtedText: String? = null
        dialog = MaterialDialog(activity).show {
            title(text = title)
            message(text = message)
            input(hint = getResString(R.string.prompt_hint), prefill = defaultValue) { _, text ->
                inputtedText = text.toString()
            }
            positiveButton(text = getResString(R.string.ok)) {
                result?.confirm(inputtedText ?: "")
                dialog = null
            }
            negativeButton(text = getResString(R.string.cancel)) {
                result?.cancel()
                dialog = null
            }
            cancelable(false)
            cancelOnTouchOutside(false)
            lifecycleOwner(activity)
        }
        return true
    }

    override fun onConsoleMessage(consoleMessage: ConsoleMessage?): Boolean {
        if (consoleMessage != null) {
            val msg = consoleMessage.message()
            if (BuildConfig.DEBUG) {
                val line = consoleMessage.lineNumber()
                val src = consoleMessage.sourceId()
                Log.d("console", "$msg at Line $line of $src")
            }
        }
        return super.onConsoleMessage(consoleMessage)
    }
}
