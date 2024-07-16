package android.hardware.security.keymint;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

public class KeyParameter implements Parcelable {
    public static final Creator<KeyParameter> CREATOR = new Creator<>() {
        @Override
        public KeyParameter createFromParcel(Parcel in) {
            throw new RuntimeException();
        }

        @Override
        public KeyParameter[] newArray(int size) {
            throw new RuntimeException();
        }
    };
    public int tag = 0;
    public KeyParameterValue value;

    @Override
    public int describeContents() {
        throw new RuntimeException();
    }

    @Override
    public void writeToParcel(@NonNull Parcel parcel, int i) {
        throw new RuntimeException();
    }
}
