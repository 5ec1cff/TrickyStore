package io.github.a13e300.tricky_store

import android.content.pm.IPackageManager
import android.os.FileObserver
import android.os.ServiceManager
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties

import io.github.a13e300.tricky_store.keystore.CertHack

import java.io.File
import java.security.KeyPairGenerator
import java.security.spec.ECGenParameterSpec
import java.util.Date

object Config {
    private val hackPackages = mutableSetOf<String>()
    private val generatePackages = mutableSetOf<String>()
    private var canHackLeaf: Boolean = true

    private fun updateTargetPackages(f: File?) = runCatching {
        hackPackages.clear()
        generatePackages.clear()
        f?.readLines()?.forEach {
            if (it.isNotBlank() && !it.startsWith("#")) {
                val n = it.trim()
                if (!canHackLeaf || n.endsWith("!")) generatePackages.add(
                    n.removeSuffix("!").trim()
                )
                else hackPackages.add(n)
            }
        }
        Logger.i("update hack packages: $hackPackages, generate packages=$generatePackages")
    }.onFailure {
        Logger.e("failed to update target files", it)
    }

    private fun canHackLeaf(): Boolean = runCatching {
        val builder = KeyGenParameterSpec.Builder("TrickyStore", KeyProperties.PURPOSE_SIGN)
            .setAlgorithmParameterSpec(ECGenParameterSpec("secp256r1"))
            .setDigests(KeyProperties.DIGEST_SHA256)
            .setCertificateNotBefore(Date())
            .setAttestationChallenge(Date().toString().toByteArray())
        val keyPairGenerator = KeyPairGenerator.getInstance(
            KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore"
        )
        keyPairGenerator.initialize(builder.build())
        keyPairGenerator.generateKeyPair()
        true
    }.getOrElse {
        Logger.i("cannot generate keypair, always generating it on our own")
        false
    }

    private fun updateKeyBox(f: File?) = runCatching {
        CertHack.readFromXml(f?.readText())
    }.onFailure {
        Logger.e("failed to update keybox", it)
    }

    private const val CONFIG_PATH = "/data/adb/tricky_store"
    private const val TARGET_FILE = "target.txt"
    private const val KEYBOX_FILE = "keybox.xml"
    private val root = File(CONFIG_PATH)

    object ConfigObserver : FileObserver(root, CLOSE_WRITE or DELETE or MOVED_FROM or MOVED_TO) {
        override fun onEvent(event: Int, path: String?) {
            path ?: return
            val f = when (event) {
                CLOSE_WRITE, MOVED_TO -> File(root, path)
                DELETE, MOVED_FROM -> null
                else -> return
            }
            when (path) {
                TARGET_FILE -> updateTargetPackages(f)
                KEYBOX_FILE -> updateKeyBox(f)
            }
        }
    }

    fun initialize() {
        canHackLeaf = canHackLeaf()
        root.mkdirs()
        val scope = File(root, TARGET_FILE)
        if (scope.exists()) {
            updateTargetPackages(scope)
        } else {
            Logger.e("target.txt file not found, please put it to $scope !")
        }
        val keybox = File(root, KEYBOX_FILE)
        if (!keybox.exists()) {
            Logger.e("keybox file not found, please put it to $keybox !")
        } else {
            updateKeyBox(keybox)
        }
        ConfigObserver.startWatching()
    }

    private var iPm: IPackageManager? = null

    fun getPm(): IPackageManager? {
        if (iPm == null) {
            iPm = IPackageManager.Stub.asInterface(ServiceManager.getService("package"))
        }
        return iPm
    }

    fun needHack(callingUid: Int) = kotlin.runCatching {
        if (hackPackages.isEmpty()) return false
        val ps = getPm()?.getPackagesForUid(callingUid)
        ps?.any { it in hackPackages }
    }.onFailure { Logger.e("failed to get packages", it) }.getOrNull() ?: false

    fun needGenerate(callingUid: Int) = kotlin.runCatching {
        if (generatePackages.isEmpty()) return false
        val ps = getPm()?.getPackagesForUid(callingUid)
        ps?.any { it in generatePackages }
    }.onFailure { Logger.e("failed to get packages", it) }.getOrNull() ?: false
}
