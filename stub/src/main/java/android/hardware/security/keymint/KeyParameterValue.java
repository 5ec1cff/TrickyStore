package android.hardware.security.keymint;

import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

/* loaded from: classes2.dex */
public final class KeyParameterValue implements Parcelable {
    public static final int algorithm = 1;
    public static final int blob = 14;
    public static final int blockMode = 2;
    public static final int boolValue = 10;
    public static final int dateTime = 13;
    public static final int digest = 4;
    public static final int ecCurve = 5;
    public static final int hardwareAuthenticatorType = 8;
    public static final int integer = 11;
    public static final int invalid = 0;
    public static final int keyPurpose = 7;
    public static final int longInteger = 12;
    public static final int origin = 6;
    public static final int paddingMode = 3;
    public static final int securityLevel = 9;
    public static final Creator<KeyParameterValue> CREATOR = new Creator<KeyParameterValue>() {
        @Override
        public KeyParameterValue createFromParcel(Parcel in) {
            throw new RuntimeException();
        }

        @Override
        public KeyParameterValue[] newArray(int size) {
            throw new RuntimeException();
        }
    };

    public KeyParameterValue() {
        throw new RuntimeException();
    }

    protected KeyParameterValue(Parcel in) {
        throw new RuntimeException();
    }

    public static KeyParameterValue invalid(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue algorithm(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue blockMode(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue paddingMode(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue digest(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue ecCurve(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue origin(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue keyPurpose(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue hardwareAuthenticatorType(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue securityLevel(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue boolValue(boolean _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue integer(int _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue longInteger(long _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue dateTime(long _value) {
        throw new RuntimeException();
    }

    public static KeyParameterValue blob(byte[] _value) {
        throw new RuntimeException();
    }

    public int getTag() {
        throw new RuntimeException();
    }

    public int getInvalid() {
        throw new RuntimeException();
    }

    public void setInvalid(int _value) {
        throw new RuntimeException();
    }

    public int getAlgorithm() {
        throw new RuntimeException();
    }

    public void setAlgorithm(int _value) {
        throw new RuntimeException();
    }

    public int getBlockMode() {
        throw new RuntimeException();
    }

    public void setBlockMode(int _value) {
        throw new RuntimeException();
    }

    public int getPaddingMode() {
        throw new RuntimeException();
    }

    public void setPaddingMode(int _value) {
        throw new RuntimeException();
    }

    public int getDigest() {
        throw new RuntimeException();
    }

    public void setDigest(int _value) {
        throw new RuntimeException();
    }

    public int getEcCurve() {
        throw new RuntimeException();
    }

    public void setEcCurve(int _value) {
        throw new RuntimeException();
    }

    public int getOrigin() {
        throw new RuntimeException();
    }

    public void setOrigin(int _value) {
        throw new RuntimeException();
    }

    public int getKeyPurpose() {
        throw new RuntimeException();
    }

    public void setKeyPurpose(int _value) {
        throw new RuntimeException();
    }

    public int getHardwareAuthenticatorType() {
        throw new RuntimeException();
    }

    public void setHardwareAuthenticatorType(int _value) {
        throw new RuntimeException();
    }

    public int getSecurityLevel() {
        throw new RuntimeException();
    }

    public void setSecurityLevel(int _value) {
        throw new RuntimeException();
    }

    public boolean getBoolValue() {
        throw new RuntimeException();
    }

    public void setBoolValue(boolean _value) {
        throw new RuntimeException();
    }

    public int getInteger() {
        throw new RuntimeException();
    }

    public void setInteger(int _value) {
        throw new RuntimeException();
    }

    public long getLongInteger() {
        throw new RuntimeException();
    }

    public void setLongInteger(long _value) {
        throw new RuntimeException();
    }

    public long getDateTime() {
        throw new RuntimeException();
    }

    public void setDateTime(long _value) {
        throw new RuntimeException();
    }

    public byte[] getBlob() {
        throw new RuntimeException();
    }

    public void setBlob(byte[] _value) {
        throw new RuntimeException();
    }


    @Override
    public int describeContents() {
        throw new RuntimeException();
    }

    @Override
    public void writeToParcel(@NonNull Parcel parcel, int i) {
        throw new RuntimeException();
    }
}
