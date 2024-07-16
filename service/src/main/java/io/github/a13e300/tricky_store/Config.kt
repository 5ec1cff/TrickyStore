package io.github.a13e300.tricky_store

import android.content.pm.IPackageManager
import android.os.FileObserver
import android.os.ServiceManager
import io.github.a13e300.tricky_store.keystore.CertHack
import java.io.File

object Config {
    private val hackPackages = mutableSetOf<String>()
    private val generatePackages = mutableSetOf<String>()
    private val DEFAULT_TARGET_PACKAGES = listOf(
        "com.google.android.gms",
        "icu.nullptr.nativetest",
        "io.github.vvb2060.mahoshojo",
        "io.github.vvb2060.keyattestation"
    )

    private fun updateTargetPackages(f: File?) = runCatching {
        hackPackages.clear()
        generatePackages.clear()
        f?.readLines()?.forEach {
            if (it.isNotBlank() && !it.startsWith("#")) {
                val n = it.trim()
                if (n.endsWith("!")) generatePackages.add(n.removeSuffix("!").trim())
                else hackPackages.add(n)
            }
        }
        Logger.i("update hack packages: $hackPackages, generate packages=$generatePackages")
    }.onFailure {
        Logger.e("failed to update target files", it)
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
        root.mkdirs()
        val target = File(root, TARGET_FILE)
        if (!target.exists()) {
            target.createNewFile()
            target.writeText(DEFAULT_TARGET_PACKAGES.joinToString("\n"))
        }
        updateTargetPackages(target)
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
