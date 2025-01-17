package com.tealeaf.extractassets;

public interface Logger {
    boolean isEnabled();

    void setEnabled(boolean isEnabled);

    void write(String msg);

    void write(String tag, String msg);
}
