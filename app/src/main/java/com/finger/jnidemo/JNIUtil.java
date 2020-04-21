package com.finger.jnidemo;

public class JNIUtil {

    static {
        System.loadLibrary("fingerprint");
    }

    public native String stringFromJNI();

}
