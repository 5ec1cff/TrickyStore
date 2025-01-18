# Tricky Store

A trick of keystore. **Android 10 or above is required**.

This module is used for modifying the certificate chain generated for android key attestation.

## Stop opening source / 停止开源

Due to the rampant misuse and the contributions received after open-sourcing being less than expected, this module will be closed-source starting from version 1.1.0.

考虑到二改泛滥，且开源后获得的贡献少于预期，因此本模块自 1.1.0 版本起闭源发布。

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

## Acknowledgement

- [FrameworkPatch](https://github.com/chiteroman/FrameworkPatch)
- [BootloaderSpoofer](https://github.com/chiteroman/BootloaderSpoofer)
- [KeystoreInjection](https://github.com/aviraxp/Zygisk-KeystoreInjection)
- [LSPosed](https://github.com/LSPosed/LSPosed)
