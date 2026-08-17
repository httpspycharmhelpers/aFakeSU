/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.content.BroadcastReceiver
 *  android.content.Context
 *  android.content.Intent
 *  android.content.pm.PackageInfo
 *  android.content.pm.PackageManager
 *  android.content.pm.PackageManager$NameNotFoundException
 *  android.os.Process
 *  android.util.Log
 */
package androidx.profileinstaller;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Process;
import android.util.Log;
import java.io.File;
import java.io.Serializable;
import java.util.concurrent.Executor;

public class ProfileInstallReceiver
extends BroadcastReceiver {
    public final void onReceive(Context object, Intent object2) {
        block8: {
            boolean bl;
            Object object3;
            block9: {
                block10: {
                    if (object2 == null) break block8;
                    object3 = object2.getAction();
                    bl = "androidx.profileinstaller.action.INSTALL_PROFILE".equals(object3);
                    if (bl) {
                        object2 = new Object();
                        object3 = new w(this);
                        t.A((Context)object, (Executor)object2, (y)object3, true);
                        return;
                    }
                    bl = "androidx.profileinstaller.action.SKIP_FILE".equals(object3);
                    if (!bl) break block9;
                    if ((object2 = object2.getExtras()) == null) break block8;
                    bl = "WRITE_SKIP_FILE".equals(object2 = object2.getString("EXTRA_SKIP_FILE_OPERATION"));
                    if (!bl) break block10;
                    object2 = new w(this);
                    object3 = object.getApplicationContext();
                    object3 = object3.getPackageName();
                    PackageManager packageManager = object.getPackageManager();
                    try {
                        object3 = packageManager.getPackageInfo((String)object3, 0);
                    }
                    catch (PackageManager.NameNotFoundException nameNotFoundException) {
                        ((w)object2).b(7, (Serializable)((Object)nameNotFoundException));
                        break block8;
                    }
                    object = object.getFilesDir();
                    t.k((PackageInfo)object3, (File)object);
                    ((w)object2).b(10, null);
                    break block8;
                }
                bl = "DELETE_SKIP_FILE".equals(object2);
                if (bl) {
                    object = object.getFilesDir();
                    object = new File((File)object, "profileinstaller_profileWrittenFor_lastUpdateTime.dat");
                    ((File)object).delete();
                    Log.d((String)"ProfileInstaller", (String)"RESULT_DELETE_SKIP_FILE_SUCCESS");
                    this.setResultCode(11);
                    return;
                }
                break block8;
            }
            bl = "androidx.profileinstaller.action.SAVE_PROFILE".equals(object3);
            if (bl) {
                int n2 = Process.myPid();
                Process.sendSignal((int)n2, (int)10);
                Log.d((String)"ProfileInstaller", (String)"");
                this.setResultCode(12);
                return;
            }
            bl = "androidx.profileinstaller.action.BENCHMARK_OPERATION".equals(object3);
            if (bl && (object2 = object2.getExtras()) != null) {
                object3 = object2.getString("EXTRA_BENCHMARK_OPERATION");
                object2 = new w(this);
                bl = "DROP_SHADER_CACHE".equals(object3);
                if (bl) {
                    object = object.createDeviceProtectedStorageContext();
                    bl = t.i((File)(object = object.getCodeCacheDir()));
                    if (bl) {
                        ((w)object2).b(14, null);
                        return;
                    }
                    ((w)object2).b(15, null);
                    return;
                }
                ((w)object2).b(16, null);
            }
        }
    }
}

