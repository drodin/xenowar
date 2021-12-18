package com.drodin.xenowar;

import org.libsdl.app.SDLActivity;

public class MySDLActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {
                "xenowar"
        };
    }
}
