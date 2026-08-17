/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.view.Choreographer
 *  android.view.Choreographer$FrameCallback
 */
import android.view.Choreographer;

public abstract class B {
    public static void a(Runnable object) {
        Choreographer choreographer = Choreographer.getInstance();
        object = new A((Runnable)object);
        choreographer.postFrameCallback((Choreographer.FrameCallback)object);
    }
}

