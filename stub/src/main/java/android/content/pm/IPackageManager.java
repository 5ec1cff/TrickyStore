package android.content.pm;

import android.os.IBinder;

public interface IPackageManager {
    String[] getPackagesForUid(int uid);

    PackageInfo getPackageInfo(String packageName, long flags, int userId);

    class Stub {
        public static IPackageManager asInterface(IBinder binder) {
            throw new RuntimeException("");
        }
    }
}
