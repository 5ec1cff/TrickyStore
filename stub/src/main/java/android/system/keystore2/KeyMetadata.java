package android.system.keystore2;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

public class KeyMetadata implements Parcelable {
    // public Authorization[] authorizations;
    public byte[] certificate;
    public byte[] certificateChain;
    public KeyDescriptor key;
    public int keySecurityLevel = 0;
    public long modificationTimeMs = 0;

    protected KeyMetadata(Parcel in) {
        throw new RuntimeException("");
    }

    public static final Creator<KeyMetadata> CREATOR = new Creator<KeyMetadata>() {
        @Override
        public KeyMetadata createFromParcel(Parcel in) {
            throw new RuntimeException("");
        }

        @Override
        public KeyMetadata[] newArray(int size) {
            throw new RuntimeException("");
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
