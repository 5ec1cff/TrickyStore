package io.github.a13e300.tricky_store

import java.io.File
import java.security.MessageDigest
import kotlin.system.exitProcess

fun main(args: Array<String>) {
    verifySelf()
    Logger.i("Welcome to TrickyStore!")
    while (true) {
        if (!KeystoreInterceptor.tryRunKeystoreInterceptor()) {
            Thread.sleep(1000)
            continue
        }
        Config.initialize()
        while (true) {
            Thread.sleep(1000000)
        }
    }
}

@OptIn(ExperimentalStdlibApi::class)
fun verifySelf() {
    val kv = mutableMapOf<String, String>()
    val prop = File("./module.prop")
    runCatching {
        if (prop.canonicalPath != "/data/adb/modules/tricky_store/module.prop") error("wrong directory ${prop.canonicalPath}!")
        prop.forEachLine(Charsets.UTF_8) {
            val a = it.split("=", limit = 2)
            if (a.size != 2) return@forEachLine
            kv[a[0]] = a[1]
        }
        val checksum = MessageDigest.getInstance("SHA-256").run {
            update(kv["id"]!!.toByteArray(Charsets.UTF_8))
            update(kv["name"]!!.toByteArray(Charsets.UTF_8))
            update(kv["version"]!!.toByteArray(Charsets.UTF_8))
            update(kv["versionCode"]!!.toByteArray(Charsets.UTF_8))
            update(kv["author"]!!.toByteArray(Charsets.UTF_8))
            update(kv["description"]!!.toByteArray(Charsets.UTF_8))
            digest().toHexString()
        }
        if (checksum != BuildConfig.CHECKSUM) {
            Logger.e("unverified module files! ($checksum != ${BuildConfig.CHECKSUM})")
            prop.writeText(kv.entries.joinToString("\n") { (k, v) ->
                when (k) {
                    "description" -> "description=×Module files corrupted, please re-download it from github.com/5ec1cff/TrickyStore"
                    "author" -> "author=5ec1cff"
                    else -> "$k=$v"
                }
            })
            File("./remove").createNewFile()
            exitProcess(1)
        }
        Logger.d("verify success!")
    }.onFailure {
        Logger.e("error while verifying self", it)
        exitProcess(1)
    }
}
