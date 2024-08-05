package io.github.a13e300.tricky_store.keystore;

import android.content.pm.PackageManager;
import android.hardware.security.keymint.Algorithm;
import android.hardware.security.keymint.EcCurve;
import android.hardware.security.keymint.KeyParameter;
import android.hardware.security.keymint.Tag;
import android.security.keystore.KeyProperties;
import android.system.keystore2.KeyDescriptor;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.bouncycastle.asn1.ASN1Boolean;
import org.bouncycastle.asn1.ASN1Encodable;
import org.bouncycastle.asn1.ASN1EncodableVector;
import org.bouncycastle.asn1.ASN1Enumerated;
import org.bouncycastle.asn1.ASN1Integer;
import org.bouncycastle.asn1.ASN1ObjectIdentifier;
import org.bouncycastle.asn1.ASN1OctetString;
import org.bouncycastle.asn1.ASN1Sequence;
import org.bouncycastle.asn1.ASN1TaggedObject;
import org.bouncycastle.asn1.DERNull;
import org.bouncycastle.asn1.DEROctetString;
import org.bouncycastle.asn1.DERSequence;
import org.bouncycastle.asn1.DERSet;
import org.bouncycastle.asn1.DERTaggedObject;
import org.bouncycastle.asn1.x500.X500Name;
import org.bouncycastle.asn1.x509.Extension;
import org.bouncycastle.asn1.x509.KeyUsage;
import org.bouncycastle.cert.X509CertificateHolder;
import org.bouncycastle.cert.X509v3CertificateBuilder;
import org.bouncycastle.cert.jcajce.JcaX509CertificateConverter;
import org.bouncycastle.cert.jcajce.JcaX509v3CertificateBuilder;
import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.bouncycastle.openssl.PEMKeyPair;
import org.bouncycastle.openssl.PEMParser;
import org.bouncycastle.openssl.jcajce.JcaPEMKeyConverter;
import org.bouncycastle.operator.ContentSigner;
import org.bouncycastle.operator.jcajce.JcaContentSignerBuilder;
import org.bouncycastle.util.io.pem.PemReader;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.StringReader;
import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.MessageDigest;
import java.security.Security;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.cert.CertificateParsingException;
import java.security.cert.X509Certificate;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.RSAKeyGenParameterSpec;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

import javax.security.auth.x500.X500Principal;

import io.github.a13e300.tricky_store.Config;
import io.github.a13e300.tricky_store.Logger;
import io.github.a13e300.tricky_store.UtilKt;

public final class CertHack {
    private static final ASN1ObjectIdentifier OID = new ASN1ObjectIdentifier("1.3.6.1.4.1.11129.2.1.17");

    private static final int ATTESTATION_APPLICATION_ID_PACKAGE_INFOS_INDEX = 0;
    private static final int ATTESTATION_APPLICATION_ID_SIGNATURE_DIGESTS_INDEX = 1;
    private static final Map<String, KeyBox> keyboxes = new HashMap<>();
    private static final int ATTESTATION_PACKAGE_INFO_PACKAGE_NAME_INDEX = 0;

    private static final CertificateFactory certificateFactory;

    static {
        try {
            certificateFactory = CertificateFactory.getInstance("X.509");
        } catch (Throwable t) {
            Logger.e("", t);
            throw new RuntimeException(t);
        }
    }

    private static final int ATTESTATION_PACKAGE_INFO_VERSION_INDEX = 1;

    public static boolean canHack() {
        return !keyboxes.isEmpty();
    }

    private static PEMKeyPair parseKeyPair(String key) throws Throwable {
        try (PEMParser parser = new PEMParser(new StringReader(UtilKt.trimLine(key)))) {
            return (PEMKeyPair) parser.readObject();
        }
    }

    private static Certificate parseCert(String cert) throws Throwable {
        try (PemReader reader = new PemReader(new StringReader(UtilKt.trimLine(cert)))) {
            return certificateFactory.generateCertificate(new ByteArrayInputStream(reader.readPemObject().getContent()));
        }
    }

    private static byte[] getByteArrayFromAsn1(ASN1Encodable asn1Encodable) throws CertificateParsingException {
        if (!(asn1Encodable instanceof DEROctetString derOctectString)) {
            throw new CertificateParsingException("Expected DEROctetString");
        }
        return derOctectString.getOctets();
    }

    public static void readFromXml(String data) {
        keyboxes.clear();
        if (data == null) {
            Logger.i("clear all keyboxes");
            return;
        }
        XMLParser xmlParser = new XMLParser(data);

        try {
            int numberOfKeyboxes = Integer.parseInt(Objects.requireNonNull(xmlParser.obtainPath(
                    "AndroidAttestation.NumberOfKeyboxes").get("text")));
            for (int i = 0; i < numberOfKeyboxes; i++) {
                String keyboxAlgorithm = xmlParser.obtainPath(
                        "AndroidAttestation.Keybox.Key[" + i + "]").get("algorithm");
                String privateKey = xmlParser.obtainPath(
                        "AndroidAttestation.Keybox.Key[" + i + "].PrivateKey").get("text");
                int numberOfCertificates = Integer.parseInt(Objects.requireNonNull(xmlParser.obtainPath(
                        "AndroidAttestation.Keybox.Key[" + i + "].CertificateChain.NumberOfCertificates").get("text")));

                LinkedList<Certificate> certificateChain = new LinkedList<>();

                for (int j = 0; j < numberOfCertificates; j++) {
                    Map<String, String> certData = xmlParser.obtainPath(
                            "AndroidAttestation.Keybox.Key[" + i + "].CertificateChain.Certificate[" + j + "]");
                    certificateChain.add(parseCert(certData.get("text")));
                }
                String algo;
                if (keyboxAlgorithm.toLowerCase().equals("ecdsa")) {
                    algo = KeyProperties.KEY_ALGORITHM_EC;
                } else {
                    algo = KeyProperties.KEY_ALGORITHM_RSA;
                }
                var pemKp = parseKeyPair(privateKey);
                var kp = new JcaPEMKeyConverter().getKeyPair(pemKp);
                keyboxes.put(algo, new KeyBox(pemKp, kp, certificateChain));
            }
            Logger.i("update " + numberOfKeyboxes + " keyboxes");
        } catch (Throwable t) {
            Logger.e("Error loading xml file (keyboxes cleared): " + t);
        }
    }

    public static Certificate[] hackCertificateChain(Certificate[] caList) {
        if (caList == null) throw new UnsupportedOperationException("caList is null!");
        try {
            if (Utils.isSAK(caList)) return caList;

            X509Certificate leaf = (X509Certificate) certificateFactory.generateCertificate(new ByteArrayInputStream(caList[0].getEncoded()));
            byte[] bytes = leaf.getExtensionValue(OID.getId());
            if (bytes == null) return caList;

            X509CertificateHolder leafHolder = new X509CertificateHolder(leaf.getEncoded());
            Extension ext = leafHolder.getExtension(OID);
            ASN1Sequence sequence = ASN1Sequence.getInstance(ext.getExtnValue().getOctets());
            ASN1Encodable[] encodables = sequence.toArray();
            ASN1Sequence teeEnforced = (ASN1Sequence) encodables[7];
            ASN1EncodableVector vector = new ASN1EncodableVector();
            ASN1Encodable rootOfTrust = null;

            for (ASN1Encodable asn1Encodable : teeEnforced) {
                ASN1TaggedObject taggedObject = (ASN1TaggedObject) asn1Encodable;
                if (taggedObject.getTagNo() == 704) {
                    rootOfTrust = taggedObject.getBaseObject().toASN1Primitive();
                    continue;
                }
                vector.add(taggedObject);
            }

            LinkedList<Certificate> certificates;
            X509v3CertificateBuilder builder;
            ContentSigner signer;

            var k = keyboxes.get(leaf.getPublicKey().getAlgorithm());
            if (k == null)
                throw new UnsupportedOperationException("unsupported algorithm " + leaf.getPublicKey().getAlgorithm());
            certificates = new LinkedList<>(k.certificates);
            builder = new X509v3CertificateBuilder(
                    new X509CertificateHolder(
                            certificates.get(0).getEncoded()
                    ).getSubject(),
                    leafHolder.getSerialNumber(),
                    leafHolder.getNotBefore(),
                    leafHolder.getNotAfter(),
                    leafHolder.getSubject(),
                    leafHolder.getSubjectPublicKeyInfo()
            );
            signer = new JcaContentSignerBuilder(leaf.getSigAlgName())
                    .build(k.keyPair.getPrivate());

            byte[] verifiedBootKey = UtilKt.getBootKey();
            byte[] verifiedBootHash = null;
            try {
                if (!(rootOfTrust instanceof ASN1Sequence r)) {
                    throw new CertificateParsingException("Expected sequence for root of trust, found "
                            + rootOfTrust.getClass().getName());
                }
                verifiedBootHash = getByteArrayFromAsn1(r.getObjectAt(3));
            } catch (Throwable t) {
                Logger.e("failed to get verified boot key or hash from original, use randomly generated instead", t);
            }

            if (verifiedBootHash == null) {
                verifiedBootHash = UtilKt.getBootHash();
            }

            ASN1Encodable[] rootOfTrustEnc = {
                    new DEROctetString(verifiedBootKey),
                    ASN1Boolean.TRUE,
                    new ASN1Enumerated(0),
                    new DEROctetString(verifiedBootHash)
            };

            ASN1Sequence hackedRootOfTrust = new DERSequence(rootOfTrustEnc);
            ASN1TaggedObject rootOfTrustTagObj = new DERTaggedObject(704, hackedRootOfTrust);
            vector.add(rootOfTrustTagObj);

            ASN1Sequence hackEnforced = new DERSequence(vector);
            encodables[7] = hackEnforced;
            ASN1Sequence hackedSeq = new DERSequence(encodables);

            ASN1OctetString hackedSeqOctets = new DEROctetString(hackedSeq);
            Extension hackedExt = new Extension(OID, false, hackedSeqOctets);
            builder.addExtension(hackedExt);

            for (ASN1ObjectIdentifier extensionOID : leafHolder.getExtensions().getExtensionOIDs()) {
                if (OID.getId().equals(extensionOID.getId())) continue;
                builder.addExtension(leafHolder.getExtension(extensionOID));
            }
            certificates.addFirst(new JcaX509CertificateConverter().getCertificate(builder.build(signer)));

            return certificates.toArray(new Certificate[0]);

        } catch (Throwable t) {
            Logger.e("", t);
        }
        return caList;
    }

    public static Pair<KeyPair, List<Certificate>> generateKeyPair(int uid, KeyDescriptor descriptor, KeyGenParameters params) {
        Logger.i("Requested KeyPair with alias: " + descriptor.alias);
        KeyPair rootKP;
        X500Name issuer;
        int size = params.keySize;
        KeyPair kp = null;
        KeyBox keyBox = null;
        try {
            var algo = params.algorithm;
            if (algo == Algorithm.EC) {
                Logger.d("GENERATING EC KEYPAIR OF SIZE " + size);
                kp = buildECKeyPair(params);
                keyBox = keyboxes.get(KeyProperties.KEY_ALGORITHM_EC);
            } else if (algo == Algorithm.RSA) {
                Logger.d("GENERATING RSA KEYPAIR OF SIZE " + size);
                kp = buildRSAKeyPair(params);
                keyBox = keyboxes.get(KeyProperties.KEY_ALGORITHM_RSA);
            }
            if (keyBox == null) {
                Logger.e("UNSUPPORTED ALGORITHM: " + algo);
                return null;
            }
            rootKP = keyBox.keyPair;
            issuer = new X509CertificateHolder(
                    keyBox.certificates.get(0).getEncoded()
            ).getSubject();

            X509v3CertificateBuilder certBuilder = new JcaX509v3CertificateBuilder(issuer,
                    params.certificateSerial,
                    params.certificateNotBefore,
                    params.certificateNotAfter,
                    params.certificateSubject,
                    kp.getPublic()
            );

            KeyUsage keyUsage = new KeyUsage(KeyUsage.keyCertSign);
            certBuilder.addExtension(Extension.keyUsage, true, keyUsage);
            certBuilder.addExtension(createExtension(params, uid));

            ContentSigner contentSigner;
            if (algo == Algorithm.EC) {
                contentSigner = new JcaContentSignerBuilder("SHA256withECDSA").build(rootKP.getPrivate());
            } else {
                contentSigner = new JcaContentSignerBuilder("SHA256withRSA").build(rootKP.getPrivate());
            }
            X509CertificateHolder certHolder = certBuilder.build(contentSigner);
            var leaf = new JcaX509CertificateConverter().getCertificate(certHolder);
            List<Certificate> chain = new ArrayList<>(keyBox.certificates);
            chain.add(0, leaf);
            Logger.d("Successfully generated X500 Cert for alias: " + descriptor.alias);
            return new Pair<>(kp, chain);
        } catch (Throwable t) {
            Logger.e("", t);
        }
        return null;
    }

    private static KeyPair buildECKeyPair(KeyGenParameters params) throws Exception {
        Security.removeProvider(BouncyCastleProvider.PROVIDER_NAME);
        Security.addProvider(new BouncyCastleProvider());
        ECGenParameterSpec spec = new ECGenParameterSpec(params.ecCurveName);
        KeyPairGenerator kpg = KeyPairGenerator.getInstance("ECDSA", BouncyCastleProvider.PROVIDER_NAME);
        kpg.initialize(spec);
        return kpg.generateKeyPair();
    }

    private static KeyPair buildRSAKeyPair(KeyGenParameters params) throws Exception {
        Security.removeProvider(BouncyCastleProvider.PROVIDER_NAME);
        Security.addProvider(new BouncyCastleProvider());
        RSAKeyGenParameterSpec spec = new RSAKeyGenParameterSpec(
                params.keySize, params.rsaPublicExponent);
        KeyPairGenerator kpg = KeyPairGenerator.getInstance("RSA", BouncyCastleProvider.PROVIDER_NAME);
        kpg.initialize(spec);
        return kpg.generateKeyPair();
    }

    private static ASN1Encodable[] fromIntList(List<Integer> list) {
        ASN1Encodable[] result = new ASN1Encodable[list.size()];
        for (int i = 0; i < list.size(); i++) {
            result[i] = new ASN1Integer(list.get(i));
        }
        return result;
    }

    private static Extension createExtension(KeyGenParameters params, int uid) {
        try {
            byte[] key = UtilKt.getBootKey();
            byte[] hash = UtilKt.getBootHash();

            ASN1Encodable[] rootOfTrustEncodables = {new DEROctetString(key), ASN1Boolean.TRUE,
                    new ASN1Enumerated(0), new DEROctetString(hash)};

            ASN1Sequence rootOfTrustSeq = new DERSequence(rootOfTrustEncodables);

            var Apurpose = new DERSet(fromIntList(params.purpose));
            var Aalgorithm = new ASN1Integer(params.algorithm);
            var AkeySize = new ASN1Integer(params.keySize);
            var Adigest = new DERSet(fromIntList(params.digest));
            var AecCurve = new ASN1Integer(params.ecCurve);
            var AnoAuthRequired = DERNull.INSTANCE;

            // To be loaded
            var AosVersion = new ASN1Integer(UtilKt.getOsVersion());
            var AosPatchLevel = new ASN1Integer(UtilKt.getPatchLevel());

            var AapplicationID = createApplicationId(uid);
            var AbootPatchlevel = new ASN1Integer(UtilKt.getPatchLevelLong());
            var AvendorPatchLevel = new ASN1Integer(UtilKt.getPatchLevelLong());

            var AcreationDateTime = new ASN1Integer(System.currentTimeMillis());
            var Aorigin = new ASN1Integer(0);

            var purpose = new DERTaggedObject(true, 1, Apurpose);
            var algorithm = new DERTaggedObject(true, 2, Aalgorithm);
            var keySize = new DERTaggedObject(true, 3, AkeySize);
            var digest = new DERTaggedObject(true, 5, Adigest);
            var ecCurve = new DERTaggedObject(true, 10, AecCurve);
            var noAuthRequired = new DERTaggedObject(true, 503, AnoAuthRequired);
            var creationDateTime = new DERTaggedObject(true, 701, AcreationDateTime);
            var origin = new DERTaggedObject(true, 702, Aorigin);
            var rootOfTrust = new DERTaggedObject(true, 704, rootOfTrustSeq);
            var osVersion = new DERTaggedObject(true, 705, AosVersion);
            var osPatchLevel = new DERTaggedObject(true, 706, AosPatchLevel);
            var applicationID = new DERTaggedObject(true, 709, AapplicationID);
            var vendorPatchLevel = new DERTaggedObject(true, 718, AvendorPatchLevel);
            var bootPatchLevel = new DERTaggedObject(true, 719, AbootPatchlevel);

            ASN1Encodable[] teeEnforcedEncodables;

            // Support device properties attestation
            if (params.brand != null) {
                var Abrand = new DEROctetString(params.brand);
                var Adevice = new DEROctetString(params.device);
                var Aproduct = new DEROctetString(params.product);
                var Amanufacturer = new DEROctetString(params.manufacturer);
                var Amodel = new DEROctetString(params.model);
                var brand = new DERTaggedObject(true, 710, Abrand);
                var device = new DERTaggedObject(true, 711, Adevice);
                var product = new DERTaggedObject(true, 712, Aproduct);
                var manufacturer = new DERTaggedObject(true, 716, Amanufacturer);
                var model = new DERTaggedObject(true, 717, Amodel);

                teeEnforcedEncodables = new ASN1Encodable[]{purpose, algorithm, keySize, digest, ecCurve,
                        noAuthRequired, origin, rootOfTrust, osVersion, osPatchLevel, vendorPatchLevel,
                        bootPatchLevel, brand, device, product, manufacturer, model};
            } else {
                teeEnforcedEncodables = new ASN1Encodable[]{purpose, algorithm, keySize, digest, ecCurve,
                        noAuthRequired, origin, rootOfTrust, osVersion, osPatchLevel, vendorPatchLevel,
                        bootPatchLevel};
            }

            ASN1Encodable[] softwareEnforced = {applicationID, creationDateTime};

            ASN1OctetString keyDescriptionOctetStr = getAsn1OctetString(teeEnforcedEncodables, softwareEnforced, params);

            return new Extension(new ASN1ObjectIdentifier("1.3.6.1.4.1.11129.2.1.17"), false, keyDescriptionOctetStr);
        } catch (Throwable t) {
            Logger.e("", t);
        }
        return null;
    }

    private static ASN1OctetString getAsn1OctetString(ASN1Encodable[] teeEnforcedEncodables, ASN1Encodable[] softwareEnforcedEncodables, KeyGenParameters params) throws IOException {
        ASN1Integer attestationVersion = new ASN1Integer(100);
        ASN1Enumerated attestationSecurityLevel = new ASN1Enumerated(1);
        ASN1Integer keymasterVersion = new ASN1Integer(100);
        ASN1Enumerated keymasterSecurityLevel = new ASN1Enumerated(1);
        ASN1OctetString attestationChallenge = new DEROctetString(params.attestationChallenge);
        ASN1OctetString uniqueId = new DEROctetString("".getBytes());
        ASN1Encodable softwareEnforced = new DERSequence(softwareEnforcedEncodables);
        ASN1Sequence teeEnforced = new DERSequence(teeEnforcedEncodables);

        ASN1Encodable[] keyDescriptionEncodables = {attestationVersion, attestationSecurityLevel, keymasterVersion,
                keymasterSecurityLevel, attestationChallenge, uniqueId, softwareEnforced, teeEnforced};

        ASN1Sequence keyDescriptionHackSeq = new DERSequence(keyDescriptionEncodables);

        return new DEROctetString(keyDescriptionHackSeq);
    }

    private static DEROctetString createApplicationId(int uid) throws Throwable {
        var pm = Config.INSTANCE.getPm();
        if (pm == null) {
            throw new IllegalStateException("createApplicationId: pm not found!");
        }
        var packages = pm.getPackagesForUid(uid);
        var size = packages.length;
        ASN1Encodable[] packageInfoAA = new ASN1Encodable[size];
        Set<Digest> signatures = new HashSet<>();
        var dg = MessageDigest.getInstance("SHA-256");
        for (int i = 0; i < size; i++) {
            var name = packages[i];
            var info = UtilKt.getPackageInfoCompat(pm, name, PackageManager.GET_SIGNATURES, uid / 100000);
            ASN1Encodable[] arr = new ASN1Encodable[2];
            arr[ATTESTATION_PACKAGE_INFO_PACKAGE_NAME_INDEX] =
                    new DEROctetString(packages[i].getBytes(StandardCharsets.UTF_8));
            arr[ATTESTATION_PACKAGE_INFO_VERSION_INDEX] = new ASN1Integer(info.getLongVersionCode());
            packageInfoAA[i] = new DERSequence(arr);
            for (var s : info.signatures) {
                signatures.add(new Digest(dg.digest(s.toByteArray())));
            }
        }

        ASN1Encodable[] signaturesAA = new ASN1Encodable[signatures.size()];
        var i = 0;
        for (var d : signatures) {
            signaturesAA[i] = new DEROctetString(d.digest);
            i++;
        }

        ASN1Encodable[] applicationIdAA = new ASN1Encodable[2];
        applicationIdAA[ATTESTATION_APPLICATION_ID_PACKAGE_INFOS_INDEX] =
                new DERSet(packageInfoAA);
        applicationIdAA[ATTESTATION_APPLICATION_ID_SIGNATURE_DIGESTS_INDEX] =
                new DERSet(signaturesAA);

        return new DEROctetString(new DERSequence(applicationIdAA).getEncoded());
    }

    record Digest(byte[] digest) {
        @Override
        public boolean equals(@Nullable Object o) {
            if (o instanceof Digest d)
                return Arrays.equals(digest, d.digest);
            return false;
        }

        @Override
        public int hashCode() {
            return Arrays.hashCode(digest);
        }
    }

    record KeyBox(PEMKeyPair pemKeyPair, KeyPair keyPair, List<Certificate> certificates) {
    }

    public static class KeyGenParameters {
        public int keySize;
        public int algorithm;
        public BigInteger certificateSerial;
        public Date certificateNotBefore;
        public Date certificateNotAfter;
        public X500Name certificateSubject;

        public BigInteger rsaPublicExponent;
        public int ecCurve;
        public String ecCurveName;

        public List<Integer> purpose = new ArrayList<>();
        public List<Integer> digest = new ArrayList<>();

        public byte[] attestationChallenge;
        public byte[] brand;
        public byte[] device;
        public byte[] product;
        public byte[] manufacturer;
        public byte[] model;

        public KeyGenParameters(KeyParameter[] params) {
            for (var kp : params) {
                var p = kp.value;
                switch (kp.tag) {
                    case Tag.KEY_SIZE -> keySize = p.getInteger();
                    case Tag.ALGORITHM -> algorithm = p.getAlgorithm();
                    case Tag.CERTIFICATE_SERIAL -> certificateSerial = new BigInteger(p.getBlob());
                    case Tag.CERTIFICATE_NOT_BEFORE ->
                            certificateNotBefore = new Date(p.getDateTime());
                    case Tag.CERTIFICATE_NOT_AFTER ->
                            certificateNotAfter = new Date(p.getDateTime());
                    case Tag.CERTIFICATE_SUBJECT ->
                            certificateSubject = new X500Name(new X500Principal(p.getBlob()).getName());
                    case Tag.RSA_PUBLIC_EXPONENT -> rsaPublicExponent = new BigInteger(p.getBlob());
                    case Tag.EC_CURVE -> {
                        ecCurve = p.getEcCurve();
                        ecCurveName = getEcCurveName(ecCurve);
                    }
                    case Tag.PURPOSE -> {
                        purpose.add(p.getKeyPurpose());
                    }
                    case Tag.DIGEST -> {
                        digest.add(p.getDigest());
                    }
                    case Tag.ATTESTATION_CHALLENGE -> attestationChallenge = p.getBlob();
                    case Tag.ATTESTATION_ID_BRAND -> brand = p.getBlob();
                    case Tag.ATTESTATION_ID_DEVICE -> device = p.getBlob();
                    case Tag.ATTESTATION_ID_PRODUCT -> product = p.getBlob();
                    case Tag.ATTESTATION_ID_MANUFACTURER -> manufacturer = p.getBlob();
                    case Tag.ATTESTATION_ID_MODEL -> model = p.getBlob();
                }
            }
        }

        private static String getEcCurveName(int curve) {
            String res;
            switch (curve) {
                case EcCurve.CURVE_25519 -> res = "CURVE_25519";
                case EcCurve.P_224 -> res = "secp224r1";
                case EcCurve.P_256 -> res = "secp256r1";
                case EcCurve.P_384 -> res = "secp384r1";
                case EcCurve.P_521 -> res = "secp521r1";
                default -> throw new IllegalArgumentException("unknown curve");
            }
            return res;
        }
    }
}
