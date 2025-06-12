# Tricky Store

A trick of keystore. **Android 10 or above is required**.

This [Zygisk](https://topjohnwu.github.io/Magisk/guides.html) module is used for modifying the certificate chain generated for android key attestation.

[中文 README](README.zh-CN.md)

## Stop opening source

Due to the rampant misuse and the contributions received after open-sourcing being less than expected, this module will be closed-source starting from version 1.1.0.

## Usage

1. Flash this module and reboot.  
2. For more than DEVICE integrity, put an unrevoked hardware keybox.xml at `/data/adb/tricky_store/keybox.xml` (Optional).  
3. Customize target packages at `/data/adb/tricky_store/target.txt` (Optional).  
4. Enjoy!  

**All configuration files will take effect immediately.**

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

## Support TEE broken devices

Tricky Store will hack the leaf certificate by default.
On TEE broken devices, this will not work because we can't retrieve the leaf certificate from TEE.
In this case, we fallback to use generate key mode automatically.  

You can add a `!` after a package name to force use generate certificate support for this package.
Also, you can add a `?` after a package name to force use leaf hack mode for this package.

For example:

```
# target.txt
# use auto mode for KeyAttestation App
io.github.vvb2060.keyattestation
# always use leaf hack mode 
io.github.vvb2060.mahoshojo?
# always use certificate generating mode for gms
com.google.android.gms!
```

## Customize security patch level (1.2.1+)

Create the file `/data/adb/tricky_store/security_patch.txt`.

Simple:

```
# Hack os/vendor/boot security patch level
20241101
```

Advanced:

```
# os security patch level is 202411
system=202411
# do not hack boot patch level
boot=no
# vendor patch level is 20241101 (another format)
vendor=2024-11-01
# default value
# all=20241101
# keep consistent with system prop
# system=prop
```

Note: this feature will only hack the result of KeyAttestation, it will not do resetprop, you need do it yourself.

## Acknowledgement

- [FrameworkPatch](https://github.com/chiteroman/FrameworkPatch)
- [BootloaderSpoofer](https://github.com/chiteroman/BootloaderSpoofer)
- [KeystoreInjection](https://github.com/aviraxp/Zygisk-KeystoreInjection)
- [LSPosed](https://github.com/LSPosed/LSPosed)
