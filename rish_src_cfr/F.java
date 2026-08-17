/*
 * Decompiled with CFR 0.152.
 */
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Objects;

public final class F {
    public final int a;
    public final int b;
    public final long c;
    public final long d;

    public F(int n2, int n3, long l2, long l3) {
        this.a = n2;
        this.b = n3;
        this.c = l2;
        this.d = l3;
    }

    public static F a(File object) {
        object = new FileInputStream((File)object);
        try (DataInputStream dataInputStream = new DataInputStream((InputStream)object);){
            int n2 = dataInputStream.readInt();
            int n3 = dataInputStream.readInt();
            long l2 = dataInputStream.readLong();
            long l3 = dataInputStream.readLong();
            object = new F(n2, n3, l2, l3);
            return object;
        }
    }

    public final void b(File object) {
        ((File)object).delete();
        object = new FileOutputStream((File)object);
        try (DataOutputStream dataOutputStream = new DataOutputStream((OutputStream)object);){
            int n2 = this.a;
            dataOutputStream.writeInt(n2);
            n2 = this.b;
            dataOutputStream.writeInt(n2);
            long l2 = this.c;
            dataOutputStream.writeLong(l2);
            l2 = this.d;
            dataOutputStream.writeLong(l2);
            return;
        }
    }

    public final boolean equals(Object object) {
        boolean bl;
        if (this == object) {
            return true;
        }
        if (object != null && (bl = object instanceof F)) {
            long l2;
            long l3;
            object = (F)object;
            int n2 = this.b;
            int n3 = ((F)object).b;
            if (n2 == n3 && (l3 = this.c) == (l2 = ((F)object).c) && (n2 = this.a) == (n3 = ((F)object).a) && (l3 = this.d) == (l2 = ((F)object).d)) {
                return true;
            }
        }
        return false;
    }

    public final int hashCode() {
        int n2 = this.b;
        long l2 = this.c;
        int n3 = this.a;
        long l3 = this.d;
        n3 = Objects.hash(n2, l2, n3, l3);
        return n3;
    }
}

