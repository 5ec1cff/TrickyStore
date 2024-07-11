package io.github.a13e300.tricky_store

import android.os.FileObserver
import io.github.a13e300.tricky_store.keystore.CertHack
import java.io.File

object Config {
    val targetPackages = mutableSetOf<String>()
    private val DEFAULT_TARGET_PACKAGES = listOf(
        "com.google.android.gms",
        "icu.nullptr.nativetest",
        "io.github.vvb2060.mahoshojo",
        "io.github.vvb2060.keyattestation"
    )

    private fun updateTargetPackages(f: File?) = runCatching {
        targetPackages.clear()
        f?.readLines()?.mapNotNullTo(targetPackages) {
            if (it.isNotBlank() && !it.startsWith("#")) it else null
        }
        Logger.i("update target packages: $targetPackages")
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

    object ConfigObserver : FileObserver(root, CLOSE_WRITE or DELETE) {
        override fun onEvent(event: Int, path: String?) {
            path ?: return
            val f = when (event) {
                CLOSE_WRITE -> File(root, path)
                DELETE -> null
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
}
