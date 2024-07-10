#pragma once

#include <android/log.h>
#include <cerrno>
#include <cstring>
#include <string>

#ifndef LOG_TAG
# define LOG_TAG "TrickyStore"
#endif

#ifndef NDEBUG
#define LOGD(...)  logging::log(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGV(...)  logging::log(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...)
#define LOGV(...)
#endif
#define LOGI(...)  logging::log(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...)  logging::log(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  logging::log(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGF(...)  logging::log(ANDROID_LOG_FATAL, LOG_TAG, __VA_ARGS__)
#define PLOGE(fmt, args...) LOGE(fmt " failed with %d: %s", ##args, errno, strerror(errno))

namespace logging {
    void setPrintEnabled(bool print);

    [[gnu::format(printf, 3, 4)]]
    void log(int prio, const char *tag, const char *fmt, ...);
}
