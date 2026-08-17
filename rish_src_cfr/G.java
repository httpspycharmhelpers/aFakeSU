/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.content.Context
 *  android.content.pm.PackageManager
 *  android.content.pm.PackageManager$NameNotFoundException
 *  android.os.Build$VERSION
 */
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import java.io.File;
import java.io.IOException;

public abstract class G {
    public static final H a;
    public static final Object b;
    public static x c;

    static {
        Object object;
        a = object = new Object();
        b = object = new Object();
        c = null;
    }

    public static long a(Context object) {
        Context context = object.getApplicationContext();
        context = context.getPackageManager();
        int n2 = Build.VERSION.SDK_INT;
        if (n2 >= 33) {
            object = E.a((PackageManager)context, object);
            long l2 = object.lastUpdateTime;
            return l2;
        }
        object = object.getPackageName();
        object = context.getPackageInfo((String)object, 0);
        long l3 = object.lastUpdateTime;
        return l3;
    }

    public static x b() {
        x x2;
        c = x2 = new x(3);
        Object object = a;
        object.getClass();
        t t2 = g.f;
        boolean bl = t2.d((g)object, null, x2);
        if (bl) {
            g.b((g)object);
        }
        object = c;
        return object;
    }

    /*
     * Loose catch block
     * Enabled aggressive block sorting
     * Enabled unnecessary exception pruning
     * Enabled aggressive exception aggregation
     */
    public static void c(Context object, boolean bl) {
        Object object2;
        if (!bl && (object2 = c) != null) {
            return;
        }
        object2 = b;
        synchronized (object2) {
            block27: {
                Throwable throwable2;
                block26: {
                    block28: {
                        int n2;
                        long l2;
                        int n3;
                        Object object3;
                        if (!bl) {
                            try {
                                object3 = c;
                                if (object3 != null) {
                                    return;
                                }
                            }
                            catch (Throwable throwable2) {
                                break block26;
                            }
                        }
                        if ((n3 = Build.VERSION.SDK_INT) < 28 || n3 == 30) break block28;
                        Object object4 = object.getPackageName();
                        Object object5 = new File("/data/misc/profiles/ref/", (String)object4);
                        object3 = new File((File)object5, "primary.prof");
                        long l3 = ((File)object3).length();
                        boolean bl2 = ((File)object3).exists();
                        int n4 = 0;
                        n3 = bl2 && l3 > 0L ? 1 : 0;
                        object5 = object.getPackageName();
                        object4 = new File("/data/misc/profiles/cur/0/", (String)object5);
                        object3 = new File((File)object4, "primary.prof");
                        long l4 = ((File)object3).length();
                        bl2 = ((File)object3).exists();
                        int n5 = bl2 && l4 > 0L ? 1 : 0;
                        long l5 = G.a((Context)object);
                        object = object.getFilesDir();
                        object3 = new File((File)object, "profileInstalled");
                        bl2 = ((File)object3).exists();
                        if (bl2) {
                            try {
                                object = F.a((File)object3);
                            }
                            catch (IOException iOException) {
                                G.b();
                                return;
                            }
                        } else {
                            object = null;
                        }
                        if (object != null && (l2 = ((F)object).c) == l5 && (n2 = ((F)object).b) != 2) {
                            n3 = n2;
                        } else if (n3 != 0) {
                            n3 = 1;
                        } else {
                            n3 = n4;
                            if (n5 != 0) {
                                n3 = 2;
                            }
                        }
                        n4 = n3;
                        if (bl) {
                            n4 = n3;
                            if (n5 != 0) {
                                n4 = n3;
                                if (n3 != 1) {
                                    n4 = 2;
                                }
                            }
                        }
                        n3 = n4;
                        if (object != null) {
                            n5 = ((F)object).b;
                            n3 = n4;
                            if (n5 == 2) {
                                n3 = n4;
                                if (n4 == 1) {
                                    l2 = ((F)object).d;
                                    n3 = n4;
                                    if (l3 < l2) {
                                        n3 = 3;
                                    }
                                }
                            }
                        }
                        object5 = new F(1, n3, l5, l4);
                        if (object != null && (bl = ((F)object).equals(object5))) break block27;
                        try {
                            ((F)object5).b((File)object3);
                            break block27;
                        }
                        catch (IOException iOException) {}
                        catch (PackageManager.NameNotFoundException nameNotFoundException) {
                            G.b();
                        }
                        return;
                    }
                    G.b();
                    return;
                }
                throw throwable2;
            }
            G.b();
            return;
        }
    }
}

