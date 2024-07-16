# Tricky Store

A trick of keystore.

## Usage

1. Flash this module and reboot.  
2. Put keybox.xml to /data/adb/tricky_store/keybox.xml .  
3. Put target packages to /data/adb/tricky_store/target.txt (Optional).  
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

If you can not pass strong integrity, you can try to enable build vars spoofing
by creating a file `/data/adb/tricky_store/spoof_build_vars`.

Zygisk (or Zygisk Next) is needed for this feature to work.

## Support TEE broken devices

TrickyStore will hack leaf certificate by default.
On TEE broken devices, this will not work because we can't retrieve leaf certificate from TEE.
You can add a `!` after package name to enable certificate generate support for this package.

For example:

```
# target.txt
# use leaf certificate hacking mode for KeyAttestation App
io.github.vvb2060.keyattestation
# use certificate generating mode for gms
com.google.android.gms!
```

## Acknowledgement

- [PlayIntegrityFix](https://github.com/chiteroman/PlayIntegrityFix)
- [FrameworkPatch](https://github.com/chiteroman/FrameworkPatch)
- [BootloaderSpoofer](https://github.com/chiteroman/BootloaderSpoofer)
- [KeystoreInjection](https://github.com/aviraxp/Zygisk-KeystoreInjection)
- [LSPosed](https://github.com/LSPosed/LSPosed)  
