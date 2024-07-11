package android.content.pm;

import android.os.IBinder;

public interface IPackageManager {
    String[] getPackagesForUid(int uid);

    class Stub {
        public static IPackageManager asInterface(IBinder binder) {
            throw new RuntimeException("");
        }
    }
}
