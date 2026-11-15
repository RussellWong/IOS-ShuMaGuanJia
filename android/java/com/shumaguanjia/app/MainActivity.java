package com.shumaguanjia.app;

import android.app.NativeActivity;
import android.content.Context;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.TextView;

/**
 * 数码管家：原生窗口 + 透明输入框桥接
 * 原生层负责全部界面绘制，本类仅提供系统输入法（中文输入）能力。
 */
public class MainActivity extends NativeActivity {

    // 由应用自身的类加载器再加载一次，确保 JNI 方法能绑定到本类
    static {
        System.loadLibrary("shumaguanjia");
    }

    private EditText edit;
    private boolean suppress = false;

    public native void nativeText(String text);
    public native void nativeDone();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING);

        edit = new EditText(this);
        edit.setBackgroundColor(0x00000000);
        edit.setTextColor(0x00000000);
        edit.setHighlightColor(0x00000000);
        edit.setCursorVisible(false);
        edit.setSingleLine(true);
        edit.setGravity(Gravity.TOP | Gravity.START);
        edit.setPadding(0, 0, 0, 0);
        edit.setInputType(InputType.TYPE_CLASS_TEXT);
        edit.setImeOptions(EditorInfo.IME_ACTION_DONE);

        edit.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (!suppress) nativeText(s.toString());
            }

            @Override
            public void afterTextChanged(Editable s) {}
        });

        edit.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE
                        || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                    nativeDone();
                    hideKeyboard();
                    return true;
                }
                return false;
            }
        });

        edit.setOnKeyListener(new View.OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (keyCode == KeyEvent.KEYCODE_ENTER && event.getAction() == KeyEvent.ACTION_DOWN) {
                    nativeDone();
                    hideKeyboard();
                    return true;
                }
                return false;
            }
        });

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(1, 1);
        lp.gravity = Gravity.TOP | Gravity.START;
        addContentView(edit, lp);
        edit.setVisibility(View.VISIBLE);
    }

    /** 由原生层调用：弹出系统输入法 */
    public void showKeyboard(final String text, final boolean numeric) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (edit == null) return;
                suppress = true;
                edit.setInputType(numeric
                        ? (InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL)
                        : InputType.TYPE_CLASS_TEXT);
                edit.setText(text == null ? "" : text);
                if (edit.getText() != null) edit.setSelection(edit.getText().length());
                suppress = false;
                edit.requestFocus();
                InputMethodManager imm =
                        (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) imm.showSoftInput(edit, InputMethodManager.SHOW_IMPLICIT);
            }
        });
    }

    /** 由原生层调用：收起输入法 */
    public void hideKeyboard() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (edit == null) return;
                InputMethodManager imm =
                        (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) imm.hideSoftInputFromWindow(edit.getWindowToken(), 0);
                edit.clearFocus();
            }
        });
    }
}
