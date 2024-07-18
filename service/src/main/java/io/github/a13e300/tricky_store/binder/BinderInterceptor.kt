package io.github.a13e300.tricky_store.binder

import android.os.Binder
import android.os.IBinder
import android.os.Parcel
import io.github.a13e300.tricky_store.Logger

open class BinderInterceptor : Binder() {
    sealed class Result
    data object Skip : Result()
    data object Continue : Result()
    data class OverrideData(val data: Parcel) : Result()
    data class OverrideReply(val code: Int = 0, val reply: Parcel) : Result()

    companion object {
        fun getBinderBackdoor(b: IBinder): IBinder? {
            val data = Parcel.obtain()
            val reply = Parcel.obtain()
            try {
                if (!b.transact(0xdeadbeef.toInt(), data, reply, 0)) {
                    Logger.d("remote return false!")
                    return null
                }
                Logger.d("remote return true!")
                return reply.readStrongBinder()
            } catch (t: Throwable) {
                Logger.e("failed to read binder", t)
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
    }

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
                reply.appendFrom(result.reply, 0, result.reply.dataSize())
                result.reply.recycle()
            }
            is OverrideData -> {
                reply!!.writeInt(4)
                reply.writeLong(result.data.dataSize().toLong())
                reply.appendFrom(result.data, 0, result.data.dataSize())
                result.data.recycle()
            }
        }
        return true
    }
}
