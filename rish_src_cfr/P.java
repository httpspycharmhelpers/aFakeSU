/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.os.Parcel
 *  android.util.SparseIntArray
 */
import android.os.Parcel;
import android.util.SparseIntArray;

public final class P
extends O {
    public final SparseIntArray d = new SparseIntArray();
    public final Parcel e;
    public final int f;
    public final int g;
    public final String h;
    public int i = -1;
    public int j;
    public int k = -1;

    public P(Parcel parcel, int n2, int n3, String string, n n4, n n5, n n6) {
        super(n4, n5, n6);
        this.e = parcel;
        this.f = n2;
        this.g = n3;
        this.j = n2;
        this.h = string;
    }

    @Override
    public final P a() {
        Object object = this.e;
        int n2 = object.dataPosition();
        int n3 = this.j;
        int n4 = this.f;
        int n5 = n3;
        if (n3 == n4) {
            n5 = this.g;
        }
        CharSequence charSequence = new StringBuilder();
        Object object2 = this.h;
        charSequence.append((String)object2);
        charSequence.append("  ");
        charSequence = charSequence.toString();
        n n6 = this.b;
        object2 = this.c;
        n n7 = this.a;
        object = new P((Parcel)object, n2, n5, (String)charSequence, n7, n6, (n)object2);
        return object;
    }

    @Override
    public final boolean e(int n2) {
        block7: {
            block6: {
                int n3;
                int n4;
                while ((n4 = this.j) < (n3 = this.g)) {
                    n4 = this.k;
                    if (n4 == n2) break block6;
                    if ((n4 = String.valueOf(n4).compareTo(String.valueOf(n2))) <= 0) {
                        n4 = this.j;
                        Parcel parcel = this.e;
                        parcel.setDataPosition(n4);
                        n4 = parcel.readInt();
                        this.k = n3 = parcel.readInt();
                        n3 = this.j;
                        this.j = n3 + n4;
                        continue;
                    }
                    break block7;
                }
                n4 = this.k;
                if (n4 != n2) break block7;
            }
            return true;
        }
        return false;
    }

    @Override
    public final void g(int n2) {
        int n3 = this.i;
        SparseIntArray sparseIntArray = this.d;
        Parcel parcel = this.e;
        if (n3 >= 0) {
            int n4 = sparseIntArray.get(n3);
            n3 = parcel.dataPosition();
            parcel.setDataPosition(n4);
            parcel.writeInt(n3 - n4);
            parcel.setDataPosition(n3);
        }
        this.i = n2;
        n3 = parcel.dataPosition();
        sparseIntArray.put(n2, n3);
        parcel.writeInt(0);
        parcel.writeInt(n2);
    }
}

