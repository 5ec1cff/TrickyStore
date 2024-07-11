package io.github.a13e300.tricky_store

import android.annotation.SuppressLint
import android.content.pm.IPackageManager
import android.os.IBinder
import android.os.Parcel
import android.os.ServiceManager
import android.system.keystore2.IKeystoreService
import android.system.keystore2.KeyEntryResponse
import io.github.a13e300.tricky_store.binder.BinderInterceptor
import io.github.a13e300.tricky_store.keystore.CertHack
import io.github.a13e300.tricky_store.keystore.Utils
import kotlin.system.exitProcess

@SuppressLint("BlockedPrivateApi")
object KeystoreInterceptor  : BinderInterceptor() {
    private val targetTransaction = IKeystoreService.Stub::class.java.getDeclaredField("TRANSACTION_getKeyEntry").apply { isAccessible = true }.getInt(null) // 2

    private var iPm: IPackageManager? = null

    private fun getPm(): IPackageManager? {
        if (iPm == null) {
            iPm = IPackageManager.Stub.asInterface(ServiceManager.getService("package"))
        }
        return iPm
    }

    override fun onPreTransact(
        target: IBinder,
        code: Int,
        flags: Int,
        callingUid: Int,
        callingPid: Int,
        data: Parcel
    ): Result {
        if (code == targetTransaction && CertHack.canHack() && Config.targetPackages.isNotEmpty()) {
            Logger.d("intercept pre  $target uid=$callingUid pid=$callingPid dataSz=${data.dataSize()}")
            kotlin.runCatching {
                val ps = getPm()?.getPackagesForUid(callingUid)
                if (ps?.any { it in Config.targetPackages } == true) return Continue
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
            if (chain != null) {
                val newChain = CertHack.engineGetCertificateChain(chain)
                Utils.putCertificateChain(response, newChain)
                p.writeNoException()
                p.writeTypedObject(response, 0)
                return OverrideReply(0, p)
            } else {
                p.recycle()
            }
        } catch (t: Throwable) {
            Logger.e("failed to hack certificate chain of uid=$callingUid pid=$callingPid!", t)
            p.recycle()
        }
        return Skip
    }

    fun tryRunKeystoreInterceptor(): Boolean {
        Logger.i("trying to register keystore interceptor ...")
        val b = ServiceManager.getService("android.system.keystore2.IKeystoreService/default") ?: return false
        val bd = getBinderBackdoor(b)
        if (bd == null) {
            // no binder hook, try inject
            Logger.i("trying to inject keystore ...")
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
            return false
        }
        registerBinderInterceptor(bd, b, this)
        b.linkToDeath({
            Logger.d("keystore exit, daemon restart")
            exitProcess(0)
        }, 0)
        return true
    }
}
