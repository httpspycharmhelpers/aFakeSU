/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.os.Binder
 *  android.os.IBinder
 *  android.os.Parcel
 */
import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import java.io.PrintStream;
import rikka.shizuku.shell.ShizukuShellLoader;

public final class K
extends Binder {
    public final boolean onTransact(int n2, Parcel object, Parcel object2, int n3) {
        if (n2 == 1) {
            object2 = object.readStrongBinder();
            String string = object.readString();
            if (object2 != null) {
                object = ShizukuShellLoader.c;
                object2 = new J((IBinder)object2, string);
                object.post((Runnable)object2);
                return true;
            }
            object = System.err;
            ((PrintStream)object).println("Server is not running");
            object = System.err;
            ((PrintStream)object).flush();
            System.exit(1);
            return true;
        }
        boolean bl = super.onTransact(n2, (Parcel)object, object2, n3);
        return bl;
    }
}

