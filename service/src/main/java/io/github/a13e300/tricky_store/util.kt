package io.github.a13e300.tricky_store

import android.content.pm.IPackageManager
import android.os.Build
import android.os.SystemProperties
import java.util.concurrent.ThreadLocalRandom

fun getTransactCode(clazz: Class<*>, method: String) =
    clazz.getDeclaredField("TRANSACTION_$method").apply { isAccessible = true }
        .getInt(null) // 2

val bootHash by lazy {
    getBootHashFromProp() ?: randomBytes()
}

// TODO: get verified boot keys
val bootKey by lazy {
    randomBytes()
}

@OptIn(ExperimentalStdlibApi::class)
fun getBootHashFromProp(): ByteArray? {
    val b = SystemProperties.get("ro.boot.vbmeta.digest", null) ?: return null
    if (b.length != 64) return null
    return b.hexToByteArray()
}

fun randomBytes() = ByteArray(32).also { ThreadLocalRandom.current().nextBytes(it) }

val patchLevel by lazy {
    Build.VERSION.SECURITY_PATCH.convertPatchLevel(false)
}

val patchLevelLong by lazy {
    Build.VERSION.SECURITY_PATCH.convertPatchLevel(true)
}

// FIXME
val osVersion by lazy {
    when (Build.VERSION.SDK_INT) {
        Build.VERSION_CODES.UPSIDE_DOWN_CAKE -> 140000
        Build.VERSION_CODES.TIRAMISU -> 130000
        Build.VERSION_CODES.S_V2 -> 120100
        Build.VERSION_CODES.S -> 120000
        else -> 0
    }
}

fun String.convertPatchLevel(long: Boolean) = kotlin.runCatching {
    val l = split("-")
    if (long) l[0].toInt() * 10000 + l[1].toInt() * 100 + l[2].toInt()
    else l[0].toInt() * 100 + l[1].toInt()
}.onFailure { Logger.e("invalid patch level $this !", it) }.getOrDefault(202404)

fun IPackageManager.getPackageInfoCompat(name: String, flags: Long, userId: Int) =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getPackageInfo(name, flags, userId)
    } else {
        getPackageInfo(name, flags.toInt(), userId)
    }

fun String.trimLine() = trim().split("\n").joinToString("\n") { it.trim() }
