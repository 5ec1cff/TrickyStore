package io.github.a13e300.tricky_store;

import android.util.Log;

public class Logger {
    private static final String TAG = "TrickyStore";
    public static void d(String msg) {
        Log.d(TAG, msg);
    }

    public static void e(String msg) {
        Log.e(TAG, msg);
    }

    public static void e(String msg, Throwable t) {
        Log.e(TAG, msg, t);
    }

    public static void i(String msg) {
        Log.i(TAG, msg);
    }

}
