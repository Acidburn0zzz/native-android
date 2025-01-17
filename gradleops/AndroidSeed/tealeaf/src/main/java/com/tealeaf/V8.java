package com.tealeaf;

public class V8 {


    public static native String stringFromJNI();

    public static native String stringFromJNI2();

    public static native void initV8();

    public static native void init();

    public static native void connect(Object connection);

    public static native void waitForFrontend();

    public static native void scheduleBreak();

    public static native void disconnect();

    public static native void dispatchMessage(String message);

    public static native void require(String filePath);

}
