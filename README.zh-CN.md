# Tricky Store

**支持 Android 10 及以上版本**.

该模块用于修改 Android Keystore 生成的 Android KeyAttestation 证书链。

[中文 README](README.zh-CN.md)

## 停止开源

考虑到二改泛滥，且开源后获得的贡献少于预期，因此本模块自 1.1.0 版本起闭源发布。

## 用法

1. 刷入模块并重启。
2. 为实现更高层次的设备完整性验证，请将未吊销的硬件密钥文件keybox.xml放置到目录`/data/adb/tricky_store/keybox.xml` （可选）。
3. 在 `/data/adb/tricky_store/target.txt` 自定义修改生效的应用包名（可选） 。
4. 大功告成！  

**所有配置会立即生效**

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

## 支持 TEE 损坏的设备

TrickyStore 默认采用修改来自 TEE 的叶证书的方式。
这在 TEE 损坏的设备上无法工作，因为 TEE 无法提供证书链。
因此，TrickyStore 会自动切换到生成证书链模式。  

在 target.txt 中，在包名后添加一个 `!` 可以强制使用生成证书链模式。
添加 `?` 到包名后可强制使用修改证书链模式。如无后缀则自动选择。

例子

```
# target.txt
# 对 KeyAttestation App 使用自动模式
io.github.vvb2060.keyattestation
# 对 momo 使用修改证书链模式
io.github.vvb2060.mahoshojo?
# 对 gms 使用生成证书链模式
com.google.android.gms!
```

## 自定义安全补丁级别(1.2.1+)

配置文件 `/data/adb/tricky_store/security_patch.txt`

简易：

```
# 修改 os/vendor/boot 的安全补丁级别
20241101
```

高级：

```
# os 安全补丁级别为 202411
system=202411
# 不要修改 boot 安全补丁级别
boot=no
# vendor 安全补丁级别 20241101 （使用了另一种格式）
vendor=2024-11-01
# 默认值
# all=20241101
# system 安全补丁级别与系统属性一致
# system=prop
```

注意：该功能仅修改 KeyAttestation 返回的结果，不会重置系统属性。

## Acknowledgement

- [FrameworkPatch](https://github.com/chiteroman/FrameworkPatch)
- [BootloaderSpoofer](https://github.com/chiteroman/BootloaderSpoofer)
- [KeystoreInjection](https://github.com/aviraxp/Zygisk-KeystoreInjection)
- [LSPosed](https://github.com/LSPosed/LSPosed)
