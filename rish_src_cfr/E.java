/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.content.Context
 *  android.content.pm.PackageInfo
 *  android.content.pm.PackageManager
 *  android.content.pm.PackageManager$PackageInfoFlags
 */
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

public abstract class E {
    public static PackageInfo a(PackageManager packageManager, Context object) {
        object = object.getPackageName();
        PackageManager.PackageInfoFlags packageInfoFlags = PackageManager.PackageInfoFlags.of((long)0L);
        packageManager = packageManager.getPackageInfo((String)object, packageInfoFlags);
        return packageManager;
    }
}

