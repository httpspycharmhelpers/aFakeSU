/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.os.DeadObjectException
 *  android.os.IBinder
 *  android.os.IBinder$DeathRecipient
 *  android.os.IInterface
 *  android.os.Parcel
 *  android.os.ServiceManager
 */
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.IInterface;
import android.os.Parcel;
import android.os.ServiceManager;
import java.io.FileDescriptor;

public final class M
implements IBinder,
IBinder.DeathRecipient {
    public IBinder a;
    public IInterface b;

    /*
     * Enabled aggressive block sorting
     * Enabled unnecessary exception pruning
     * Enabled aggressive exception aggregation
     */
    public final IBinder a() {
        IBinder iBinder = this.a;
        if (iBinder != null) {
            return iBinder;
        }
        IBinder iBinder2 = ServiceManager.getService((String)"package");
        if (iBinder2 == null) {
            return null;
        }
        try {
            iBinder2.linkToDeath((IBinder.DeathRecipient)this, 0);
        }
        catch (Throwable throwable) {}
        this.a = iBinder2;
        return iBinder2;
    }

    public final void binderDied() {
        IBinder iBinder = this.a;
        iBinder.unlinkToDeath((IBinder.DeathRecipient)this, 0);
        this.a = null;
        this.b = null;
    }

    public final void dump(FileDescriptor fileDescriptor, String[] stringArray) {
        IBinder iBinder = this.a();
        if (iBinder != null) {
            iBinder.dump(fileDescriptor, stringArray);
        }
    }

    public final void dumpAsync(FileDescriptor fileDescriptor, String[] stringArray) {
        IBinder iBinder = this.a();
        if (iBinder != null) {
            iBinder.dumpAsync(fileDescriptor, stringArray);
        }
    }

    public final String getInterfaceDescriptor() {
        Object object = this.a();
        if (object != null) {
            object = object.getInterfaceDescriptor();
            return object;
        }
        return null;
    }

    public final boolean isBinderAlive() {
        IBinder iBinder = this.a();
        if (iBinder != null) {
            boolean bl = iBinder.isBinderAlive();
            return bl;
        }
        return false;
    }

    public final void linkToDeath(IBinder.DeathRecipient deathRecipient, int n2) {
        IBinder iBinder = this.a();
        if (iBinder != null) {
            iBinder.linkToDeath(deathRecipient, n2);
        }
    }

    public final boolean pingBinder() {
        IBinder iBinder = this.a();
        if (iBinder != null) {
            boolean bl = iBinder.pingBinder();
            return bl;
        }
        return false;
    }

    public final IInterface queryLocalInterface(String string) {
        IBinder iBinder = this.a();
        if (iBinder != null) {
            string = iBinder.queryLocalInterface(string);
            return string;
        }
        return null;
    }

    public final boolean transact(int n2, Parcel parcel, Parcel parcel2, int n3) {
        IBinder iBinder = this.a();
        if (iBinder == null) {
            return false;
        }
        try {
            boolean bl = iBinder.transact(n2, parcel, parcel2, n3);
            return bl;
        }
        catch (DeadObjectException deadObjectException) {
            this.a = null;
            IBinder iBinder2 = this.a();
            if (iBinder2 != null) {
                boolean bl = iBinder.transact(n2, parcel, parcel2, n3);
                return bl;
            }
            return false;
        }
    }

    public final boolean unlinkToDeath(IBinder.DeathRecipient deathRecipient, int n2) {
        IBinder iBinder = this.a();
        if (iBinder != null) {
            boolean bl = iBinder.unlinkToDeath(deathRecipient, n2);
            return bl;
        }
        return false;
    }
}

