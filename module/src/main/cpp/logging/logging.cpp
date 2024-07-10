#include <android/log.h>
#include <unistd.h>
#include <cstdio>
#include <string>

#include "logging.hpp"

namespace logging {
    static bool use_print = false;
    static char prio_str[] = {
            'V', 'D', 'I', 'W', 'E', 'F'
    };

    void setPrintEnabled(bool print) {
        use_print = print;
    }

    void log(int prio, const char *tag, const char *fmt, ...) {
        {
            va_list ap;
            va_start(ap, fmt);
            __android_log_vprint(prio, tag, fmt, ap);
            va_end(ap);
        }
        if (use_print) {
            char buf[BUFSIZ];
            va_list ap;
            va_start(ap, fmt);
            vsnprintf(buf, sizeof(buf), fmt, ap);
            va_end(ap);
            auto prio_char = (prio > ANDROID_LOG_DEFAULT && prio <= ANDROID_LOG_FATAL) ? prio_str[
                    prio - ANDROID_LOG_VERBOSE] : '?';
            printf("[%c][%d:%d][%s]:%s\n", prio_char, getpid(), gettid(), tag, buf);
        }
    }
}