/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.app.PendingIntent
 *  android.os.Parcel
 *  android.os.Parcelable
 *  android.text.TextUtils
 */
package androidx.core.app;

import android.app.PendingIntent;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import androidx.core.app.RemoteActionCompat;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class RemoteActionCompatParcelizer {
    /*
     * Enabled aggressive block sorting
     * Enabled unnecessary exception pruning
     * Enabled aggressive exception aggregation
     */
    public static RemoteActionCompat read(O o2) {
        int n2;
        Object object;
        RemoteActionCompat remoteActionCompat = new RemoteActionCompat();
        Object object2 = remoteActionCompat.a;
        boolean bl = true;
        boolean bl2 = o2.e(1);
        if (bl2) {
            o2.getClass();
            object2 = (P)o2;
            object2 = ((P)object2).e;
            object = object2.readString();
            if (object == null) {
                object2 = null;
            } else {
                object2 = o2.a();
                try {
                    object = o2.c((String)object);
                    object2 = ((Method)object).invoke(null, object2);
                    object2 = (Q)object2;
                }
                catch (ClassNotFoundException classNotFoundException) {
                    RuntimeException runtimeException = new RuntimeException("VersionedParcel encountered ClassNotFoundException", classNotFoundException);
                    throw runtimeException;
                }
                catch (NoSuchMethodException noSuchMethodException) {
                    RuntimeException runtimeException = new RuntimeException("VersionedParcel encountered NoSuchMethodException", noSuchMethodException);
                    throw runtimeException;
                }
                catch (InvocationTargetException invocationTargetException) {
                    object2 = invocationTargetException.getCause();
                    bl2 = object2 instanceof RuntimeException;
                    if (bl2) {
                        Throwable throwable = invocationTargetException.getCause();
                        throwable = (RuntimeException)throwable;
                        throw throwable;
                    }
                    RuntimeException runtimeException = new RuntimeException("VersionedParcel encountered InvocationTargetException", invocationTargetException);
                    throw runtimeException;
                }
                catch (IllegalAccessException illegalAccessException) {
                    RuntimeException runtimeException = new RuntimeException("VersionedParcel encountered IllegalAccessException", illegalAccessException);
                    throw runtimeException;
                }
            }
        }
        remoteActionCompat.a = object2;
        object2 = remoteActionCompat.b;
        bl2 = o2.e(2);
        if (bl2) {
            object = (P)o2;
            object2 = TextUtils.CHAR_SEQUENCE_CREATOR;
            object = ((P)object).e;
            object2 = object2.createFromParcel((Parcel)object);
            object2 = (CharSequence)object2;
        }
        remoteActionCompat.b = object2;
        object2 = remoteActionCompat.c;
        bl2 = o2.e(3);
        if (bl2) {
            object = (P)o2;
            object2 = TextUtils.CHAR_SEQUENCE_CREATOR;
            object = ((P)object).e;
            object2 = object2.createFromParcel((Parcel)object);
            object2 = (CharSequence)object2;
        }
        remoteActionCompat.c = object2;
        object2 = remoteActionCompat.d;
        object2 = o2.f((Parcelable)object2, 4);
        object2 = (PendingIntent)object2;
        remoteActionCompat.d = object2;
        bl2 = remoteActionCompat.e;
        boolean bl3 = o2.e(5);
        if (bl3) {
            object2 = (P)o2;
            object2 = ((P)object2).e;
            n2 = object2.readInt();
            bl2 = n2 != 0;
        }
        remoteActionCompat.e = bl2;
        bl2 = remoteActionCompat.f;
        bl3 = o2.e(6);
        if (bl3) {
            o2 = (P)o2;
            o2 = ((P)o2).e;
            n2 = o2.readInt();
            bl2 = n2 != 0 ? bl : false;
        }
        remoteActionCompat.f = bl2;
        return remoteActionCompat;
    }

    public static void write(RemoteActionCompat remoteActionCompat, O o2) {
        o2.getClass();
        Object object = remoteActionCompat.a;
        o2.g(1);
        o2.h((Q)object);
        CharSequence charSequence = remoteActionCompat.b;
        o2.g(2);
        object = (P)o2;
        object = ((P)object).e;
        TextUtils.writeToParcel((CharSequence)charSequence, (Parcel)object, (int)0);
        charSequence = remoteActionCompat.c;
        o2.g(3);
        TextUtils.writeToParcel((CharSequence)charSequence, (Parcel)object, (int)0);
        charSequence = remoteActionCompat.d;
        o2.g(4);
        object.writeParcelable((Parcelable)charSequence, 0);
        int n2 = remoteActionCompat.e;
        o2.g(5);
        object.writeInt(n2);
        int n3 = remoteActionCompat.f;
        o2.g(6);
        object.writeInt(n3);
    }
}

