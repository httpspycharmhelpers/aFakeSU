/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.content.res.ColorStateList
 *  android.graphics.Bitmap
 *  android.graphics.PorterDuff$Mode
 *  android.os.Build$VERSION
 *  android.os.Parcelable
 *  android.util.Log
 */
package androidx.core.graphics.drawable;

import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.PorterDuff;
import android.os.Build;
import android.os.Parcelable;
import android.util.Log;
import androidx.versionedparcelable.CustomVersionedParcelable;
import java.lang.reflect.InvocationTargetException;

public class IconCompat
extends CustomVersionedParcelable {
    public static final PorterDuff.Mode k;
    public int a = -1;
    public Object b;
    public byte[] c = null;
    public Parcelable d = null;
    public int e = 0;
    public int f = 0;
    public ColorStateList g = null;
    public PorterDuff.Mode h;
    public String i;
    public String j;

    static {
        PorterDuff.Mode mode;
        k = mode = PorterDuff.Mode.SRC_IN;
    }

    public IconCompat() {
        PorterDuff.Mode mode;
        this.h = mode = k;
        this.i = null;
    }

    /*
     * Unable to fully structure code
     */
    public final String toString() {
        var1_1 = this.a;
        if (var1_1 == -1) {
            var2_2 = this.b;
            var2_2 = String.valueOf(var2_2);
            return var2_2;
        }
        var3_7 = new StringBuilder("Icon(typ=");
        var1_1 = this.a;
        switch (var1_1) {
            default: {
                var2_3 = "UNKNOWN";
                break;
            }
            case 6: {
                var2_3 = "URI_MASKABLE";
                break;
            }
            case 5: {
                var2_3 = "BITMAP_MASKABLE";
                break;
            }
            case 4: {
                var2_3 = "URI";
                break;
            }
            case 3: {
                var2_3 = "DATA";
                break;
            }
            case 2: {
                var2_3 = "RESOURCE";
                break;
            }
            case 1: {
                var2_3 = "BITMAP";
            }
        }
        var3_7.append((String)var2_3);
        var1_1 = this.a;
        switch (var1_1) {
            default: {
                break;
            }
            case 4: 
            case 6: {
                var3_7.append(" uri=");
                var2_3 = this.b;
                var3_7.append(var2_3);
                break;
            }
            case 3: {
                var3_7.append(" len=");
                var1_1 = this.e;
                var3_7.append(var1_1);
                var1_1 = this.f;
                if (var1_1 == 0) break;
                var3_7.append(" off=");
                var1_1 = this.f;
                var3_7.append(var1_1);
                break;
            }
            case 2: {
                var3_7.append(" pkg=");
                var2_3 = this.j;
                var3_7.append((String)var2_3);
                var3_7.append(" id=");
                var1_1 = this.a;
                if (var1_1 != -1) ** GOTO lbl94
                var1_1 = Build.VERSION.SDK_INT;
                var2_3 = this.b;
                if (var1_1 < 28) ** GOTO lbl71
                var1_1 = s.a(var2_3);
                ** GOTO lbl96
lbl71:
                // 1 sources

                try {
                    var4_8 = var2_3.getClass();
                    var4_8 = var4_8.getMethod("getResId", null);
                    var2_3 = var4_8.invoke(var2_3, null);
                    var2_3 = (Integer)var2_3;
                    var1_1 = var2_3.intValue();
                    ** GOTO lbl96
                }
                catch (NoSuchMethodException var2_4) {
                }
                catch (InvocationTargetException var2_5) {
                    ** GOTO lbl87
                }
                catch (IllegalAccessException var2_6) {
                    ** GOTO lbl90
                }
                Log.e((String)"IconCompat", (String)"Unable to get icon resource", (Throwable)var2_4);
                ** GOTO lbl92
lbl87:
                // 1 sources

                Log.e((String)"IconCompat", (String)"Unable to get icon resource", (Throwable)var2_5);
                ** GOTO lbl92
lbl90:
                // 1 sources

                Log.e((String)"IconCompat", (String)"Unable to get icon resource", (Throwable)var2_6);
lbl92:
                // 3 sources

                var1_1 = 0;
                ** GOTO lbl96
lbl94:
                // 1 sources

                if (var1_1 != 2) ** GOTO lbl100
                var1_1 = this.e;
lbl96:
                // 4 sources

                var2_3 = String.format("0x%08x", new Object[]{var1_1});
                var3_7.append((String)var2_3);
                break;
lbl100:
                // 1 sources

                var2_3 = new StringBuilder("called getResId() on ");
                var2_3.append(this);
                var2_3 = var2_3.toString();
                var2_3 = new IllegalStateException((String)var2_3);
                throw var2_3;
            }
            case 1: 
            case 5: {
                var3_7.append(" size=");
                var2_3 = this.b;
                var2_3 = (Bitmap)var2_3;
                var1_1 = var2_3.getWidth();
                var3_7.append(var1_1);
                var3_7.append("x");
                var2_3 = this.b;
                var2_3 = (Bitmap)var2_3;
                var1_1 = var2_3.getHeight();
                var3_7.append(var1_1);
            }
        }
        var2_3 = this.g;
        if (var2_3 != null) {
            var3_7.append(" tint=");
            var2_3 = this.g;
            var3_7.append(var2_3);
        }
        if ((var4_8 = this.h) != (var2_3 = IconCompat.k)) {
            var3_7.append(" mode=");
            var2_3 = this.h;
            var3_7.append(var2_3);
        }
        var3_7.append(")");
        var2_3 = var3_7.toString();
        return var2_3;
    }
}

