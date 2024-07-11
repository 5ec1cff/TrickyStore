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
