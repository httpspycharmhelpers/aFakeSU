/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.view.Choreographer$FrameCallback
 */
import android.view.Choreographer;

public final class A
implements Choreographer.FrameCallback {
    public final Runnable a;

    public /* synthetic */ A(Runnable runnable) {
        this.a = runnable;
    }

    public final void doFrame(long l2) {
        Runnable runnable = this.a;
        runnable.run();
    }
}

