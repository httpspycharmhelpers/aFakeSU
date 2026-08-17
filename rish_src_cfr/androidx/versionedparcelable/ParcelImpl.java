/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.os.Parcel
 *  android.os.Parcelable
 */
package androidx.versionedparcelable;

import android.os.Parcel;
import android.os.Parcelable;

public class ParcelImpl
implements Parcelable {
    public final int describeContents() {
        return 0;
    }

    public final void writeToParcel(Parcel object, int n2) {
        int n3 = object.dataPosition();
        n2 = object.dataSize();
        n n4 = new n();
        n n5 = new n();
        n n6 = new n();
        object = new P((Parcel)object, n3, n2, "", n4, n5, n6);
        ((O)object).h(null);
    }
}

