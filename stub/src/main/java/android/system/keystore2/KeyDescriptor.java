package android.system.keystore2;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

public class KeyDescriptor implements Parcelable {
    public String alias;
    public byte[] blob;
    public int domain = 0;
    public long nspace = 0;

    protected KeyDescriptor(Parcel in) {
        throw new RuntimeException("");
    }

    public static final Creator<KeyDescriptor> CREATOR = new Creator<KeyDescriptor>() {
        @Override
        public KeyDescriptor createFromParcel(Parcel in) {
            return new KeyDescriptor(in);
        }

        @Override
        public KeyDescriptor[] newArray(int size) {
            return new KeyDescriptor[size];
        }
    };

    @Override
    public int describeContents() {
        throw new RuntimeException("");
    }

    @Override
    public void writeToParcel(@NonNull Parcel parcel, int i) {
        throw new RuntimeException("");
    }
}
