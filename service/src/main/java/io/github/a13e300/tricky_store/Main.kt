package io.github.a13e300.tricky_store

import android.annotation.SuppressLint
import android.content.pm.IPackageManager
import android.os.Binder
import android.os.FileObserver
import android.os.IBinder
import android.os.Looper
import android.os.Parcel
import android.os.ServiceManager
import android.system.keystore2.IKeystoreService
import android.system.keystore2.KeyEntryResponse
import android.util.Log
import io.github.a13e300.tricky_store.fwpatch.Android
import io.github.a13e300.tricky_store.fwpatch.Utils
import java.io.File
import kotlin.system.exitProcess

open class BinderInterceptor : Binder() {
    sealed class Result
    data object Skip : Result()
    data object Continue : Result()
    data class OverrideData(val data: Parcel) : Result()
    data class OverrideReply(val code: Int = 0, val reply: Parcel) : Result()

    open fun onPreTransact(target: IBinder, code: Int, flags: Int, callingUid: Int, callingPid: Int, data: Parcel): Result = Skip
    open fun onPostTransact(target: IBinder, code: Int, flags: Int, callingUid: Int, callingPid: Int, data: Parcel, reply: Parcel?, resultCode: Int): Result = Skip

    override fun onTransact(code: Int, data: Parcel, reply: Parcel?, flags: Int): Boolean {
        val result = when (code) {
            1 -> { // PRE_TRANSACT
                val target = data.readStrongBinder()
                val theCode = data.readInt()
                val theFlags = data.readInt()
                val callingUid = data.readInt()
                val callingPid = data.readInt()
                val sz = data.readLong()
                val theData = Parcel.obtain()
                try {
                    theData.appendFrom(data, data.dataPosition(), sz.toInt())
                    theData.setDataPosition(0)
                    onPreTransact(target, theCode, theFlags, callingUid, callingPid, theData)
                } finally {
                    theData.recycle()
                }
            }
            2 -> { // POST_TRANSACT
                val target = data.readStrongBinder()
                val theCode = data.readInt()
                val theFlags = data.readInt()
                val callingUid = data.readInt()
                val callingPid = data.readInt()
                val resultCode = data.readInt()
                val theData = Parcel.obtain()
                val theReply = Parcel.obtain()
                try {
                    val sz = data.readLong().toInt()
                    theData.appendFrom(data, data.dataPosition(), sz)
                    theData.setDataPosition(0)
                    data.setDataPosition(data.dataPosition() + sz)
                    val sz2 = data.readLong().toInt()
                    if (sz2 != 0) {
                        theReply.appendFrom(data, data.dataPosition(), sz2)
                        theReply.setDataPosition(0)
                    }
                    onPostTransact(target, theCode, theFlags, callingUid, callingPid, theData, if (sz2 == 0) null else theReply, resultCode)
                } finally {
                    theData.recycle()
                    theReply.recycle()
                }
            }
            else -> return super.onTransact(code, data, reply, flags)
        }
        when (result) {
            Skip -> reply!!.writeInt(1)
            Continue -> reply!!.writeInt(2)
            is OverrideReply -> {
                reply!!.writeInt(3)
                reply.writeInt(result.code)
                reply.writeLong(result.reply.dataSize().toLong())
                println("override reply code=${result.code} size=${result.reply.dataSize()}")
                reply.appendFrom(result.reply, 0, result.reply.dataSize())
                result.reply.recycle()
            }
            is OverrideData -> {
                reply!!.writeInt(4)
                reply.writeLong(result.data.dataSize().toLong())
                reply.appendFrom(result.data, 0, result.data.dataSize())
                result.data.recycle()
            }
            else -> {}
        }
        return true
    }
}

fun getBinderBackdoor(b: IBinder): IBinder? {
    val data = Parcel.obtain()
    val reply = Parcel.obtain()
    try {
        b.transact(0xdeadbeef.toInt(), data, reply, 0)
        return reply.readStrongBinder()
    } catch (ignored: Throwable) {
        return null
    } finally {
        data.recycle()
        reply.recycle()
    }
}

fun registerBinderInterceptor(backdoor: IBinder, target: IBinder, interceptor: BinderInterceptor) {
    val data = Parcel.obtain()
    val reply = Parcel.obtain()
    data.writeStrongBinder(target)
    data.writeStrongBinder(interceptor)
    backdoor.transact(1, data, reply, 0)
}

val targetPackages = mutableSetOf<String>()
val DEFAULT_TARGET_PACKAGES = listOf("com.google.android.gms", "icu.nullptr.nativetest", "io.github.vvb2060.mahoshojo", "io.github.vvb2060.keyattestation")

var iPm: IPackageManager? = null

fun getPm(): IPackageManager? {
    if (iPm == null) {
        iPm = IPackageManager.Stub.asInterface(ServiceManager.getService("package"))
    }
    return iPm
}

fun updateTargetPackages(f: File) = runCatching {
    targetPackages.clear()
    f.readLines().mapNotNullTo(targetPackages) {
        if (it.isNotBlank() && !it.startsWith("#")) it else null
    }
    Logger.d("update target packages: $targetPackages")
}.onFailure {
    Logger.e("failed to update target files", it)
}

fun updateKeyBox(f: File) = runCatching {
    Android.readFromXml(f.readText())
}.onFailure {
    Logger.e("failed to update keybox", it)
}

const val CONFIG_PATH = "/data/adb/tricky_store"
const val TARGET_FILE = "target.txt"
const val KEYBOX_FILE = "keybox.xml"

class ConfigObserver : FileObserver(CONFIG_PATH, CLOSE_WRITE) {
    private val root = File(CONFIG_PATH)
    override fun onEvent(event: Int, path: String?) {
        path ?: return
        if (event == CLOSE_WRITE) {
            val f = File(root, path)
            when (f.name) {
                TARGET_FILE -> updateTargetPackages(f)
                KEYBOX_FILE -> updateKeyBox(f)
            }
        }
    }
}

@SuppressLint("BlockedPrivateApi")
fun tryRunKeystoreInterceptor(): Boolean {
    val b = ServiceManager.getService("android.system.keystore2.IKeystoreService/default") ?: return false
    b.linkToDeath({
        Logger.d("keystore exit, daemon exit")
        exitProcess(0)
    }, 0)
    val bd = getBinderBackdoor(b) ?: return true
    val targetTransaction = IKeystoreService.Stub::class.java.getDeclaredField("TRANSACTION_getKeyEntry").apply { isAccessible = true }.getInt(null) // 2
    val interceptor = object : BinderInterceptor() {
        override fun onPreTransact(
            target: IBinder,
            code: Int,
            flags: Int,
            callingUid: Int,
            callingPid: Int,
            data: Parcel
        ): Result {
            if (code == targetTransaction) {
                Logger.d("intercept pre  $target uid=$callingUid pid=$callingPid dataSz=${data.dataSize()}")
                kotlin.runCatching {
                    val ps = getPm()?.getPackagesForUid(callingUid)
                    if (ps?.any { it in targetPackages } == true) return Continue
                }.onFailure { Logger.e("failed to get packages", it) }
            }
            return Skip
        }

        override fun onPostTransact(
            target: IBinder,
            code: Int,
            flags: Int,
            callingUid: Int,
            callingPid: Int,
            data: Parcel,
            reply: Parcel?,
            resultCode: Int
        ): Result {
            if (code != targetTransaction || reply == null) return Skip
            val p = Parcel.obtain()
            Logger.d("intercept post $target uid=$callingUid pid=$callingPid dataSz=${data.dataSize()} replySz=${reply.dataSize()}")
            try {
                reply.readException()
                val response = reply.readTypedObject(KeyEntryResponse.CREATOR)
                val chain = Utils.getCertificateChain(response)
                val newChain = Android.engineGetCertificateChain(chain)
                Utils.putCertificateChain(response, newChain)
                p.writeNoException()
                p.writeTypedObject(response, 0)
                return OverrideReply(0, p)
            } catch (t: Throwable) {
                Logger.e("failed to hack certificate chain!", t)
                p.recycle()
            }
            return Skip
        }
    }
    val path = File(CONFIG_PATH)
    path.mkdirs()
    val target = File(path, TARGET_FILE)
    if (!target.exists()) {
        target.createNewFile()
        target.writeText(DEFAULT_TARGET_PACKAGES.joinToString("\n"))
    }
    updateTargetPackages(target)
    val keybox = File(path, KEYBOX_FILE)
    if (!keybox.exists()) {
        Logger.e("keybox file not found, please put it to $keybox !")
    } else {
        updateKeyBox(keybox)
    }
    val monitor = ConfigObserver()
    monitor.startWatching()
    registerBinderInterceptor(bd, b, interceptor)
    while (true) {
        Thread.sleep(1000000)
    }
}

fun main(args: Array<String>) {
    while (true) {
        Thread.sleep(1000)
        // true -> can inject, false -> service not found, loop -> running
        if (!tryRunKeystoreInterceptor()) continue
        // no binder hook, try inject
        val p = Runtime.getRuntime().exec(
            arrayOf(
                "/system/bin/sh",
                "-c",
                "exec ./inject `pidof keystore2` libtricky_store.so entry"
            )
        )
        // logD(p.inputStream.readBytes().decodeToString())
        // logD(p.errorStream.readBytes().decodeToString())
        if (p.waitFor() != 0) {
            Logger.e("failed to inject! daemon exit")
            exitProcess(1)
        }
    }
}
