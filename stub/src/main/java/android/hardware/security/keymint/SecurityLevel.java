package android.hardware.security.keymint;

public @interface SecurityLevel {
    int KEYSTORE = 100;
    int SOFTWARE = 0;
    int STRONGBOX = 2;
    int TRUSTED_ENVIRONMENT = 1;
}
