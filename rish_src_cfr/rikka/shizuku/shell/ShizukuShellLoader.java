/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.app.ActivityManagerNative
 *  android.app.IActivityManager$Stub
 *  android.content.Intent
 *  android.content.pm.IPackageManager
 *  android.content.pm.IPackageManager$Stub
 *  android.os.Binder
 *  android.os.Build$VERSION
 *  android.os.Bundle
 *  android.os.Handler
 *  android.os.IBinder
 *  android.os.Looper
 *  android.os.ServiceManager
 *  android.system.Os
 *  android.text.TextUtils
 *  dalvik.system.BaseDexClassLoader
 *  dalvik.system.VMRuntime
 */
package rikka.shizuku.shell;

import android.app.ActivityManagerNative;
import android.app.IActivityManager;
import android.content.Intent;
import android.content.pm.IPackageManager;
import android.os.Binder;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.ServiceManager;
import android.system.Os;
import android.text.TextUtils;
import dalvik.system.BaseDexClassLoader;
import dalvik.system.VMRuntime;
import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Objects;

public class ShizukuShellLoader {
    public static String[] a;
    public static String b;
    public static Handler c;
    public static final K d;

    static {
        Binder binder = new Binder();
        d = binder;
    }

    public static void a(IBinder iBinder, String stringArray) {
        int n2 = stringArray.lastIndexOf(47);
        Object object = stringArray.substring(0, n2);
        Object object2 = new StringBuilder();
        ((StringBuilder)object2).append((String)object);
        ((StringBuilder)object2).append("/lib/");
        object = VMRuntime.getRuntime();
        object = object.vmInstructionSet();
        ((StringBuilder)object2).append((String)object);
        object = ((StringBuilder)object2).toString();
        String string = System.getProperty("java.library.path");
        boolean bl = TextUtils.isEmpty((CharSequence)string);
        object2 = object;
        if (!bl) {
            object2 = new StringBuilder();
            ((StringBuilder)object2).append((String)object);
            char c2 = File.pathSeparatorChar;
            ((StringBuilder)object2).append(c2);
            ((StringBuilder)object2).append(string);
            object2 = ((StringBuilder)object2).toString();
        }
        try {
            object = ClassLoader.getSystemClassLoader();
            string = new BaseDexClassLoader((String)stringArray, null, (String)object2, (ClassLoader)object);
            stringArray = ((ClassLoader)((Object)string)).loadClass("moe.shizuku.manager.shell.Shell");
            object2 = stringArray.getDeclaredMethod("main", String[].class, String.class, IBinder.class, Handler.class);
            stringArray = a;
            string = b;
            object = c;
            ((Method)object2).invoke(null, stringArray, string, iBinder, object);
            return;
        }
        catch (Throwable throwable) {
            stringArray = System.err;
            throwable.printStackTrace((PrintStream)stringArray);
            PrintStream printStream = System.err;
            printStream.flush();
            System.exit(1);
        }
        catch (ClassNotFoundException classNotFoundException) {
            PrintStream printStream = System.err;
            printStream.println("Class not found");
            printStream = System.err;
            printStream.println("Make sure you have Shizuku v12.0.0 or above installed");
            printStream = System.err;
            printStream.flush();
            System.exit(1);
        }
    }

    public static void b() {
        Bundle bundle = new Bundle();
        K k2 = d;
        bundle.putBinder("binder", (IBinder)k2);
        k2 = new Intent("rikka.shizuku.intent.action.REQUEST_BINDER");
        k2 = k2.setPackage("moe.shizuku.privileged.api");
        k2 = k2.addFlags(32);
        Intent intent = k2.putExtra("data", bundle);
        k2 = ServiceManager.getService((String)"activity");
        int n2 = Build.VERSION.SDK_INT;
        k2 = n2 >= 26 ? IActivityManager.Stub.asInterface((IBinder)k2) : ActivityManagerNative.asInterface((IBinder)k2);
        try {
            k2.broadcastIntent(null, intent, null, null, 0, null, null, null, -1, null, true, false, 0);
            return;
        }
        catch (Throwable throwable) {
            String string;
            boolean bl;
            n2 = Build.VERSION.SDK_INT;
            if ((n2 == 26 || (n2 = Build.VERSION.SDK_INT) == 27) && (bl = Objects.equals(string = throwable.getMessage(), "Calling application did not provide package name"))) {
                Object object = System.err;
                ((PrintStream)object).println("broadcastIntent fails on Android 8.0 or 8.1, fallback to startActivity");
                object = System.err;
                ((PrintStream)object).flush();
                object = new Intent("rikka.shizuku.intent.action.REQUEST_BINDER");
                object = object.addFlags(0x4000000);
                object = object.addFlags(0x10000000);
                object = object.addFlags(524288);
                bundle = object.putExtra("data", bundle);
                bundle = Intent.createChooser((Intent)bundle, (CharSequence)"Request binder from Shizuku");
                object = b;
                n2 = Os.getuid();
                k2.startActivityAsUser(null, (String)object, (Intent)bundle, null, null, null, 0, 0, null, null, n2 /= 100000);
                return;
            }
            throw throwable;
        }
    }

    /*
     * Enabled aggressive block sorting
     * Enabled unnecessary exception pruning
     * Enabled aggressive exception aggregation
     */
    public static void main(String[] object) {
        Object object2;
        block15: {
            block16: {
                block14: {
                    int n2;
                    block13: {
                        a = object;
                        n2 = Os.getuid();
                        object2 = new ArrayList();
                        Object object3 = t.m;
                        object = ((M)object3).b;
                        if (object == null) {
                            object = ((M)object3).a();
                            if (object == null) {
                                object = null;
                            } else {
                                object = IPackageManager.Stub.asInterface((IBinder)object);
                                ((M)object3).b = object;
                            }
                        }
                        object = (IPackageManager)object;
                        if ((object = object.getPackagesForUid(n2)) == null) break block13;
                        int n3 = ((IPackageManager)object).length;
                        for (n2 = 0; n2 < n3; ++n2) {
                            object3 = object[n2];
                            if (object3 == null) continue;
                            try {
                                ((ArrayList)object2).add(object3);
                            }
                            catch (Throwable throwable) {}
                            continue;
                            break;
                        }
                    }
                    if ((n2 = ((ArrayList)object2).size()) != 1) break block14;
                    object = ((ArrayList)object2).get(0);
                    object = (String)object;
                    break block15;
                }
                object2 = System.getenv("RISH_APPLICATION_ID");
                boolean bl = TextUtils.isEmpty((CharSequence)object2);
                if (bl) break block16;
                bl = "PKG".equals(object2);
                object = object2;
                if (!bl) break block15;
            }
            object = System.err;
            ((PrintStream)object).println("RISH_APPLICATION_ID is not set, set this environment variable to the id of current application (package name)");
            object = System.err;
            ((PrintStream)object).flush();
            System.exit(1);
            System.exit(1);
            object = object2;
        }
        b = object;
        object2 = Looper.getMainLooper();
        if (object2 == null) {
            Looper.prepareMainLooper();
        }
        object2 = Looper.getMainLooper();
        object2 = new Handler((Looper)object2);
        c = object2;
        try {
            ShizukuShellLoader.b();
        }
        catch (Throwable throwable) {
            object2 = System.err;
            throwable.printStackTrace((PrintStream)object2);
            object2 = System.err;
            ((PrintStream)object2).flush();
            System.exit(1);
        }
        object2 = c;
        object = new z(3, object);
        object2.postDelayed((Runnable)object, 5000L);
        Looper.loop();
        System.exit(0);
    }
}

