/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.os.IBinder
 */
import android.os.IBinder;
import rikka.shizuku.shell.ShizukuShellLoader;

public final class J
implements Runnable {
    public final IBinder a;
    public final String b;

    public /* synthetic */ J(IBinder iBinder, String string) {
        this.a = iBinder;
        this.b = string;
    }

    @Override
    public final void run() {
        IBinder iBinder = this.a;
        String string = this.b;
        ShizukuShellLoader.a(iBinder, string);
    }
}

