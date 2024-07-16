package io.github.a13e300.tricky_store

import android.os.Build
import android.os.SystemProperties

fun getTransactCode(clazz: Class<*>, method: String) =
    clazz.getDeclaredField("TRANSACTION_$method").apply { isAccessible = true }
        .getInt(null) // 2

@OptIn(ExperimentalStdlibApi::class)
val bootHashFromProp by lazy {
    val b = SystemProperties.get("ro.boot.vbmeta.digest", null) ?: return@lazy null
    if (b.length != 64) return@lazy null
    b.hexToByteArray()
}

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
