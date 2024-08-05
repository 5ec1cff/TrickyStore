package io.github.a13e300.tricky_store.keystore;

import android.system.keystore2.KeyEntryResponse;
import android.system.keystore2.KeyMetadata;
import android.util.Base64;
import android.util.Log;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;

public class Utils {
    private final static String TAG = "Utils";

    private static final String SAKV1_ROOT_PUBLIC_KEY = "" +
            "MIGbMBAGByqGSM49AgEGBSuBBAAjA4GGAAQBs9Qjr//REhkXW7jUqjY9KNwWac4r" +
            "5+kdUGk+TZjRo1YEa47Axwj6AJsbOjo4QsHiYRiWTELvFeiuBsKqyuF0xyAAKvDo" +
            "fBqrEq1/Ckxo2mz7Q4NQes3g4ahSjtgUSh0k85fYwwHjCeLyZ5kEqgHG9OpOH526" +
            "FFAK3slSUgC8RObbxys=";

    private static final String SAKV2_ROOT_PUBLIC_KEY = "" +
            "MIGbMBAGByqGSM49AgEGBSuBBAAjA4GGAAQBhbGuLrpql5I2WJmrE5kEVZOo+dgA" +
            "46mKrVJf/sgzfzs2u7M9c1Y9ZkCEiiYkhTFE9vPbasmUfXybwgZ2EM30A1ABPd12" +
            "4n3JbEDfsB/wnMH1AcgsJyJFPbETZiy42Fhwi+2BCA5bcHe7SrdkRIYSsdBRaKBo" +
            "ZsapxB0gAOs0jSPRX5M=";

    private static final String SAKMV1_ROOT_PUBLIC_KEY = "" +
            "MIGbMBAGByqGSM49AgEGBSuBBAAjA4GGAAQB9XeEN8lg6p5xvMVWG42P2Qi/aRKX" +
            "2rPRNgK92UlO9O/TIFCKHC1AWCLFitPVEow5W+yEgC2wOiYxgepY85TOoH0AuEkL" +
            "oiC6ldbF2uNVU3rYYSytWAJg3GFKd1l9VLDmxox58Hyw2Jmdd5VSObGiTFQ/SgKs" +
            "n2fbQPtpGlNxgEfd6Y8=";

    static X509Certificate toCertificate(byte[] bytes) {
        try {
            final CertificateFactory certFactory = CertificateFactory.getInstance("X.509");
            return (X509Certificate) certFactory.generateCertificate(
                    new ByteArrayInputStream(bytes));
        } catch (CertificateException e) {
            Log.w(TAG, "Couldn't parse certificate in keystore", e);
            return null;
        }
    }

    @SuppressWarnings("unchecked")
    private static Collection<X509Certificate> toCertificates(byte[] bytes) {
        try {
            final CertificateFactory certFactory = CertificateFactory.getInstance("X.509");
            return (Collection<X509Certificate>) certFactory.generateCertificates(
                    new ByteArrayInputStream(bytes));
        } catch (CertificateException e) {
            Log.w(TAG, "Couldn't parse certificates in keystore", e);
            return new ArrayList<>();
        }
    }

    public static Certificate[] getCertificateChain(KeyEntryResponse response) {
        if (response == null || response.metadata.certificate == null) return null;
        var leaf = toCertificate(response.metadata.certificate);
        Certificate[] chain;
        if (response.metadata.certificateChain != null) {
            var certs = toCertificates(response.metadata.certificateChain);
            chain = new Certificate[certs.size() + 1];
            final Iterator<X509Certificate> it = certs.iterator();
            int i = 1;
            while (it.hasNext()) {
                chain[i++] = it.next();
            }
        } else {
            chain = new Certificate[1];
        }
        chain[0] = leaf;
        return chain;
    }

    public static void putCertificateChain(KeyEntryResponse response, Certificate[] chain) throws Throwable {
        putCertificateChain(response.metadata, chain);
    }

    public static void putCertificateChain(KeyMetadata metadata, Certificate[] chain) throws Throwable {
        if (chain == null || chain.length == 0) return;
        metadata.certificate = chain[0].getEncoded();
        var output = new ByteArrayOutputStream();
        for (int i = 1; i < chain.length; i++) {
            output.write(chain[i].getEncoded());
        }
        metadata.certificateChain = output.toByteArray();
    }

    public static boolean isSAK(Certificate[] chain) throws Throwable {
        var root = toCertificate(chain[chain.length - 1].getEncoded());
        byte[] publicKey = root.getPublicKey().getEncoded();
        if (Arrays.equals(publicKey, Base64.decode(SAKV1_ROOT_PUBLIC_KEY, 0))
                || Arrays.equals(publicKey, Base64.decode(SAKV2_ROOT_PUBLIC_KEY, 0))
                || Arrays.equals(publicKey, Base64.decode(SAKMV1_ROOT_PUBLIC_KEY, 0))) {
            Log.w(TAG, "SAK detected, certificate chain won't be touched");
            return true;
        }
        return false;
    }
}
