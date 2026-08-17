/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.content.res.ColorStateList
 *  android.graphics.PorterDuff$Mode
 *  android.os.Parcel
 *  android.os.Parcelable
 */
package androidx.core.graphics.drawable;

import android.content.res.ColorStateList;
import android.graphics.PorterDuff;
import android.os.Parcel;
import android.os.Parcelable;
import androidx.core.graphics.drawable.IconCompat;
import java.nio.charset.Charset;

public class IconCompatParcelizer {
    /*
     * WARNING - void declaration
     * Enabled force condition propagation
     * Lifted jumps to return sites
     */
    public static IconCompat read(O object) {
        PorterDuff.Mode mode;
        void var0_5;
        Object object2;
        IconCompat iconCompat = new IconCompat();
        int n2 = iconCompat.a;
        boolean bl = ((O)object).e(1);
        if (bl) {
            object2 = (P)object;
            object2 = ((P)object2).e;
            n2 = object2.readInt();
        }
        iconCompat.a = n2;
        object2 = iconCompat.c;
        bl = ((O)object).e(2);
        if (bl) {
            object2 = (P)object;
            Parcel parcel = ((P)object2).e;
            n2 = parcel.readInt();
            if (n2 < 0) {
                object2 = null;
            } else {
                object2 = new byte[n2];
                parcel.readByteArray((byte[])object2);
            }
        }
        iconCompat.c = (byte[])object2;
        object2 = iconCompat.d;
        object2 = ((O)object).f((Parcelable)object2, 3);
        iconCompat.d = object2;
        n2 = iconCompat.e;
        bl = ((O)object).e(4);
        if (bl) {
            object2 = (P)object;
            object2 = ((P)object2).e;
            n2 = object2.readInt();
        }
        iconCompat.e = n2;
        n2 = iconCompat.f;
        bl = ((O)object).e(5);
        if (bl) {
            object2 = (P)object;
            object2 = ((P)object2).e;
            n2 = object2.readInt();
        }
        iconCompat.f = n2;
        object2 = iconCompat.g;
        object2 = ((O)object).f((Parcelable)object2, 6);
        object2 = (ColorStateList)object2;
        iconCompat.g = object2;
        object2 = iconCompat.i;
        bl = ((O)object).e(7);
        if (bl) {
            object2 = (P)object;
            object2 = ((P)object2).e;
            object2 = object2.readString();
        }
        iconCompat.i = object2;
        object2 = iconCompat.j;
        bl = ((O)object).e(8);
        if (!bl) {
            Object object3 = object2;
        } else {
            P p2 = (P)object;
            Parcel parcel = p2.e;
            String string = parcel.readString();
        }
        iconCompat.j = var0_5;
        String string = iconCompat.i;
        iconCompat.h = mode = PorterDuff.Mode.valueOf((String)string);
        n2 = iconCompat.a;
        switch (n2) {
            default: {
                return iconCompat;
            }
            case 3: {
                byte[] byArray = iconCompat.c;
                iconCompat.b = byArray;
                return iconCompat;
            }
            case 2: 
            case 4: 
            case 6: {
                String string2;
                String string3;
                byte[] byArray = iconCompat.c;
                object2 = Charset.forName("UTF-16");
                iconCompat.b = object2 = new String(byArray, (Charset)object2);
                n2 = iconCompat.a;
                if (n2 != 2 || (string3 = iconCompat.j) != null) return iconCompat;
                String[] stringArray = ((String)object2).split(":", -1);
                iconCompat.j = string2 = stringArray[0];
                return iconCompat;
            }
            case 1: 
            case 5: {
                Parcelable parcelable = iconCompat.d;
                if (parcelable != null) {
                    iconCompat.b = parcelable;
                    return iconCompat;
                }
                byte[] byArray = iconCompat.c;
                iconCompat.b = byArray;
                iconCompat.a = 3;
                iconCompat.e = 0;
                iconCompat.f = n2 = byArray.length;
                return iconCompat;
            }
            case -1: 
        }
        Parcelable parcelable = iconCompat.d;
        if (parcelable != null) {
            iconCompat.b = parcelable;
            return iconCompat;
        }
        IllegalArgumentException illegalArgumentException = new IllegalArgumentException("Invalid icon");
        throw illegalArgumentException;
    }

    public static void write(IconCompat object, O o2) {
        Object object2;
        o2.getClass();
        Object object3 = ((IconCompat)object).h;
        object3 = object3.name();
        ((IconCompat)object).i = object3;
        int n2 = ((IconCompat)object).a;
        switch (n2) {
            default: {
                break;
            }
            case 4: 
            case 6: {
                object3 = ((IconCompat)object).b;
                object3 = object3.toString();
                object2 = Charset.forName("UTF-16");
                object3 = object3.getBytes((Charset)object2);
                ((IconCompat)object).c = (byte[])object3;
                break;
            }
            case 3: {
                object3 = ((IconCompat)object).b;
                object3 = (byte[])object3;
                ((IconCompat)object).c = (byte[])object3;
                break;
            }
            case 2: {
                object3 = ((IconCompat)object).b;
                object2 = (String)object3;
                object3 = Charset.forName("UTF-16");
                object3 = ((String)object2).getBytes((Charset)object3);
                ((IconCompat)object).c = (byte[])object3;
                break;
            }
            case 1: 
            case 5: {
                object3 = ((IconCompat)object).b;
                object3 = (Parcelable)object3;
                ((IconCompat)object).d = object3;
                break;
            }
            case -1: {
                object3 = ((IconCompat)object).b;
                object3 = (Parcelable)object3;
                ((IconCompat)object).d = object3;
            }
        }
        n2 = ((IconCompat)object).a;
        if (-1 != n2) {
            o2.g(1);
            object3 = (P)o2;
            object3 = object3.e;
            object3.writeInt(n2);
        }
        if ((object3 = (Object)((IconCompat)object).c) != null) {
            o2.g(2);
            object2 = (P)o2;
            n2 = ((PorterDuff.Mode)object3).length;
            object2 = ((P)object2).e;
            object2.writeInt(n2);
            object2.writeByteArray((byte[])object3);
        }
        if ((object3 = ((IconCompat)object).d) != null) {
            o2.g(3);
            object2 = (P)o2;
            object2 = ((P)object2).e;
            object2.writeParcelable((Parcelable)object3, 0);
        }
        if ((n2 = ((IconCompat)object).e) != 0) {
            o2.g(4);
            object3 = (P)o2;
            object3 = object3.e;
            object3.writeInt(n2);
        }
        if ((n2 = ((IconCompat)object).f) != 0) {
            o2.g(5);
            object3 = (P)o2;
            object3 = object3.e;
            object3.writeInt(n2);
        }
        if ((object3 = ((IconCompat)object).g) != null) {
            o2.g(6);
            object2 = (P)o2;
            object2 = ((P)object2).e;
            object2.writeParcelable((Parcelable)object3, 0);
        }
        if ((object3 = ((IconCompat)object).i) != null) {
            o2.g(7);
            object2 = (P)o2;
            object2 = ((P)object2).e;
            object2.writeString((String)object3);
        }
        if ((object = ((IconCompat)object).j) != null) {
            o2.g(8);
            o2 = (P)o2;
            o2 = ((P)o2).e;
            o2.writeString((String)object);
        }
    }
}

