/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.content.ComponentName
 *  android.content.ContentProvider
 *  android.content.ContentValues
 *  android.content.Context
 *  android.content.pm.PackageManager$NameNotFoundException
 *  android.database.Cursor
 *  android.net.Uri
 *  android.os.Bundle
 *  android.os.Trace
 */
package androidx.startup;

import android.content.ComponentName;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Trace;

public class InitializationProvider
extends ContentProvider {
    public final int delete(Uri object, String string, String[] stringArray) {
        object = new IllegalStateException("Not allowed.");
        throw object;
    }

    public final String getType(Uri object) {
        object = new IllegalStateException("Not allowed.");
        throw object;
    }

    public final Uri insert(Uri object, ContentValues contentValues) {
        object = new IllegalStateException("Not allowed.");
        throw object;
    }

    /*
     * WARNING - Removed back jump from a try to a catch block - possible behaviour change.
     * Loose catch block
     * Enabled aggressive block sorting
     * Enabled unnecessary exception pruning
     * Enabled aggressive exception aggregation
     */
    public final boolean onCreate() {
        Throwable throwable3222222;
        block11: {
            Object object;
            Object object2;
            Object object3;
            block12: {
                object3 = this.getContext();
                if (object3 == null) {
                    object3 = new RuntimeException("Context cannot be null");
                    throw object3;
                }
                object2 = object3.getApplicationContext();
                if (object2 == null) return true;
                object2 = h.d;
                if (object2 == null) {
                    object2 = h.e;
                    synchronized (object2) {
                        Throwable throwable222;
                        block10: {
                            block9: {
                                try {
                                    object = h.d;
                                    if (object != null) break block9;
                                    h.d = object = new h((Context)object3);
                                }
                                catch (Throwable throwable222) {
                                    break block10;
                                }
                            }
                            break block12;
                        }
                        throw throwable222;
                    }
                }
            }
            object3 = h.d;
            Context context = ((h)object3).c;
            Trace.beginSection((String)"Startup");
            object = context.getPackageName();
            String string = InitializationProvider.class.getName();
            object2 = new ComponentName((String)object, string);
            object = context.getPackageManager();
            object2 = object.getProviderInfo((ComponentName)object2, 128);
            object2 = object2.metaData;
            ((h)object3).a((Bundle)object2);
            {
                catch (Throwable throwable3222222) {
                    break block11;
                }
                catch (PackageManager.NameNotFoundException nameNotFoundException) {}
                {
                    object3 = new RuntimeException(nameNotFoundException);
                    throw object3;
                }
            }
            Trace.endSection();
            return true;
        }
        Trace.endSection();
        throw throwable3222222;
    }

    public final Cursor query(Uri object, String[] stringArray, String string, String[] stringArray2, String string2) {
        object = new IllegalStateException("Not allowed.");
        throw object;
    }

    public final int update(Uri object, ContentValues contentValues, String string, String[] stringArray) {
        object = new IllegalStateException("Not allowed.");
        throw object;
    }
}

