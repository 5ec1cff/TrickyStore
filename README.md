# Tricky Store

A trick of keystore. **Android 12 or above is required**.

## Usage

1. Flash this module and reboot.  
2. For more than DEVICE integrity, put an unrevoked hardware keybox.xml at `/data/adb/tricky_store/keybox.xml` (Optional).  
3. Customize target packages at `/data/adb/tricky_store/target.txt` (Optional).  
4. Enjoy!  

## keybox.xml

format:

```xml
<?xml version="1.0"?>
<AndroidAttestation>
    <NumberOfKeyboxes>1</NumberOfKeyboxes>
    <Keybox DeviceID="...">
        <Key algorithm="ecdsa|rsa">
            <PrivateKey format="pem">
-----BEGIN EC PRIVATE KEY-----
...
-----END EC PRIVATE KEY-----
            </PrivateKey>
            <CertificateChain>
                <NumberOfCertificates>...</NumberOfCertificates>
                    <Certificate format="pem">
-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----
                    </Certificate>
                ... more certificates
            </CertificateChain>
        </Key>...
    </Keybox>
</AndroidAttestation>
```

## Build Vars Spoofing

> **Zygisk (or Zygisk Next) is needed for this feature to work.**

If you still do not pass you can try enabling Build variable spoofing by creating the file `/data/adb/tricky_store/spoof_build_vars`.

Tricky Store will automatically generate example config props inside `/data/adb/tricky_store/spoof_build_vars` on next reboot, then you can manually edit your spoof config.

Here is an example of spoof config:

```
MANUFACTURER=Google
MODEL=Pixel
FINGERPRINT=google/sailfish/sailfish:8.1.0/OPM1.171019.011/4448085:user/release-keys
BRAND=google
PRODUCT=sailfish
DEVICE=sailfish
RELEASE=8.1.0
ID=OPM1.171019.011
INCREMENTAL=4448085
TYPE=user
TAGS=release-keys
SECURITY_PATCH=2017-12-05
```

## Support TEE broken devices

Tricky Store will hack the leaf certificate by default. On TEE broken devices, this will not work because we can't retrieve the leaf certificate from TEE. You can add a `!` after a package name to enable generate certificate support for this package.

For example:

```
# target.txt
# use leaf certificate hacking mode for KeyAttestation App
io.github.vvb2060.keyattestation
# use certificate generating mode for gms
com.google.android.gms!
```

## Known issues

- App Attest Key is not supported.
- StrongBox is not supported.

## Acknowledgement

- [PlayIntegrityFix](https://github.com/chiteroman/PlayIntegrityFix)
- [FrameworkPatch](https://github.com/chiteroman/FrameworkPatch)
- [BootloaderSpoofer](https://github.com/chiteroman/BootloaderSpoofer)
- [KeystoreInjection](https://github.com/aviraxp/Zygisk-KeystoreInjection)
- [LSPosed](https://github.com/LSPosed/LSPosed)
