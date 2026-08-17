/*
 * Decompiled with CFR 0.152.
 * 
 * Could not load the following classes:
 *  android.os.Parcelable
 */
import android.os.Parcelable;
import java.lang.reflect.GenericDeclaration;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public abstract class O {
    public final n a;
    public final n b;
    public final n c;

    public O(n n2, n n3, n n4) {
        this.a = n2;
        this.b = n3;
        this.c = n4;
    }

    public abstract P a();

    public final Class b(Class object) {
        Class<?> clazz = ((Class)object).getName();
        n n2 = this.c;
        clazz = n2.d(clazz);
        Object object2 = clazz;
        clazz = object2;
        if (object2 == null) {
            clazz = ((Class)object).getPackage();
            clazz = ((Package)((Object)clazz)).getName();
            object2 = ((Class)object).getSimpleName();
            StringBuilder stringBuilder = new StringBuilder();
            stringBuilder.append((String)((Object)clazz));
            stringBuilder.append(".");
            stringBuilder.append((String)object2);
            stringBuilder.append("Parcelizer");
            object2 = stringBuilder.toString();
            clazz = ((Class)object).getClassLoader();
            clazz = Class.forName((String)object2, false, (ClassLoader)((Object)clazz));
            object = ((Class)object).getName();
            n2.put(object, clazz);
        }
        return clazz;
    }

    public final Method c(String string) {
        n n2 = this.a;
        GenericDeclaration genericDeclaration = n2.d(string);
        Method method = (Method)genericDeclaration;
        genericDeclaration = method;
        if (method == null) {
            System.currentTimeMillis();
            genericDeclaration = O.class.getClassLoader();
            genericDeclaration = Class.forName(string, true, (ClassLoader)((Object)genericDeclaration));
            genericDeclaration = genericDeclaration.getDeclaredMethod("read", O.class);
            n2.put(string, genericDeclaration);
        }
        return genericDeclaration;
    }

    public final Method d(Class object) {
        Object object2 = ((Class)object).getName();
        n n2 = this.b;
        object2 = n2.d(object2);
        Method method = (Method)object2;
        object2 = method;
        if (method == null) {
            object2 = this.b((Class)object);
            System.currentTimeMillis();
            object2 = ((Class)object2).getDeclaredMethod("write", new Class[]{object, O.class});
            object = ((Class)object).getName();
            n2.put(object, object2);
        }
        return object2;
    }

    public abstract boolean e(int var1);

    public final Parcelable f(Parcelable object, int n2) {
        boolean bl = this.e(n2);
        if (!bl) {
            return object;
        }
        P p2 = (P)this;
        object = P.class.getClassLoader();
        p2 = p2.e;
        object = p2.readParcelable((ClassLoader)object);
        return object;
    }

    public abstract void g(int var1);

    public final void h(Q object) {
        IllegalAccessException illegalAccessException2;
        block12: {
            InvocationTargetException invocationTargetException2;
            Object object2;
            block11: {
                NoSuchMethodException noSuchMethodException2;
                block10: {
                    ClassNotFoundException classNotFoundException3;
                    block9: {
                        int n2;
                        if (object == null) {
                            object = (P)this;
                            object = ((P)object).e;
                            object.writeString(null);
                            return;
                        }
                        try {
                            object2 = object.getClass();
                            object2 = this.b((Class)object2);
                            object2 = ((Class)object2).getName();
                        }
                        catch (ClassNotFoundException classNotFoundException2) {
                            object = object.getClass();
                            object = ((Class)object).getSimpleName();
                            object = ((String)object).concat(" does not have a Parcelizer");
                            object = new RuntimeException((String)object, classNotFoundException2);
                            throw object;
                        }
                        Object object3 = (P)this;
                        object3 = ((P)object3).e;
                        object3.writeString((String)object2);
                        object2 = this.a();
                        try {
                            object3 = object.getClass();
                            object3 = this.d((Class)object3);
                            ((Method)object3).invoke(null, object, object2);
                            n2 = ((P)object2).i;
                            if (n2 < 0) break block9;
                            object = ((P)object2).d;
                        }
                        catch (ClassNotFoundException classNotFoundException3) {
                        }
                        catch (NoSuchMethodException noSuchMethodException2) {
                            break block10;
                        }
                        catch (InvocationTargetException invocationTargetException2) {
                            break block11;
                        }
                        catch (IllegalAccessException illegalAccessException2) {
                            break block12;
                        }
                        int n3 = object.get(n2);
                        object = ((P)object2).e;
                        n2 = object.dataPosition();
                        object.setDataPosition(n3);
                        object.writeInt(n2 - n3);
                        object.setDataPosition(n2);
                    }
                    return;
                    RuntimeException runtimeException = new RuntimeException("VersionedParcel encountered ClassNotFoundException", classNotFoundException3);
                    throw runtimeException;
                }
                RuntimeException runtimeException = new RuntimeException("VersionedParcel encountered NoSuchMethodException", noSuchMethodException2);
                throw runtimeException;
            }
            object2 = invocationTargetException2.getCause();
            boolean bl = object2 instanceof RuntimeException;
            if (bl) {
                Throwable throwable = invocationTargetException2.getCause();
                throwable = (RuntimeException)throwable;
                throw throwable;
            }
            RuntimeException runtimeException = new RuntimeException("VersionedParcel encountered InvocationTargetException", invocationTargetException2);
            throw runtimeException;
        }
        RuntimeException runtimeException = new RuntimeException("VersionedParcel encountered IllegalAccessException", illegalAccessException2);
        throw runtimeException;
    }
}

