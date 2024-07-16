package android.system.keystore2;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

public class KeyEntryResponse implements Parcelable {
    public IKeystoreSecurityLevel iSecurityLevel;
    public KeyMetadata metadata;

    public static final Creator<KeyEntryResponse> CREATOR = new Creator<KeyEntryResponse>() {
        @Override
        public KeyEntryResponse createFromParcel(Parcel in) {
            throw new RuntimeException("");
        }

        @Override
        public KeyEntryResponse[] newArray(int size) {
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
