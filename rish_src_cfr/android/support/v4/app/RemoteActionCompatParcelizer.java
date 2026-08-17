/*
 * Decompiled with CFR 0.152.
 */
package android.support.v4.app;

import androidx.core.app.RemoteActionCompat;

public final class RemoteActionCompatParcelizer
extends androidx.core.app.RemoteActionCompatParcelizer {
    public static RemoteActionCompat read(O object) {
        object = androidx.core.app.RemoteActionCompatParcelizer.read((O)object);
        return object;
    }

    public static void write(RemoteActionCompat remoteActionCompat, O o2) {
        androidx.core.app.RemoteActionCompatParcelizer.write(remoteActionCompat, o2);
    }
}

