package com.trae.healthmonitor;

import android.app.Activity;
import android.media.AudioManager;
import android.media.ToneGenerator;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.webkit.JavascriptInterface;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

public class MainActivity extends Activity {

    private WebView web;
    private ToneGenerator tone;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        web = new WebView(this);
        WebSettings s = web.getSettings();
        s.setJavaScriptEnabled(true);
        s.setDomStorageEnabled(true);       // 地图Key、闹钟保存在localStorage
        s.setAllowFileAccess(true);
        s.setCacheMode(WebSettings.LOAD_DEFAULT);
        s.setTextZoom(100);                 // 不受系统“超大字体”影响，避免文字撑破布局
        s.setSupportZoom(false);
        s.setBuiltInZoomControls(false);

        web.addJavascriptInterface(this, "Android");   // 页面里调 Android.beep() / Android.vibrate()
        web.setWebViewClient(new WebViewClient());
        web.setBackgroundColor(0xFF0C1524);
        web.loadUrl("file:///android_asset/index.html");

        setContentView(web);
    }

    /** 网页报警时调用：响铃（用提示音通道，音量独立于媒体） */
    @JavascriptInterface
    public void beep() {
        try {
            if (tone == null) tone = new ToneGenerator(AudioManager.STREAM_MUSIC, 100);
            tone.startTone(ToneGenerator.TONE_CDMA_ALERT_CALL_GUARD, 2500);
        } catch (Throwable ignored) { }
    }

    /** 网页报警时调用：震动 */
    @JavascriptInterface
    public void vibrate() {
        try {
            Vibrator v = (Vibrator) getSystemService(VIBRATOR_SERVICE);
            if (v == null || !v.hasVibrator()) return;
            if (android.os.Build.VERSION.SDK_INT >= 26) {
                v.vibrate(VibrationEffect.createOneShot(1200, VibrationEffect.DEFAULT_AMPLITUDE));
            } else {
                v.vibrate(1200);
            }
        } catch (Throwable ignored) { }
    }

    @Override
    public void onBackPressed() {
        if (web != null && web.canGoBack()) web.goBack();
        else super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        if (tone != null) { tone.release(); tone = null; }
        super.onDestroy();
    }
}
