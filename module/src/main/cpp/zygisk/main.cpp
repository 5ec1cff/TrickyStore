#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>
#include <string_view>

#include "logging.hpp"
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;
using namespace std::string_view_literals;

class TrickyStore : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api_ = api;
        this->env_ = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        int enabled = 0;
        api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        {
            auto fd = api_->connectCompanion();
            if (fd >= 0) {
                read(fd, &enabled, sizeof(enabled));
                close(fd);
            }
        }
        if (!enabled) return;
        const char *process = env_->GetStringUTFChars(args->nice_name, nullptr);
        if (process == "com.google.android.gms.unstable"sv) {
            LOGI("spoofing build vars in %s!", process);
            auto buildClass = env_->FindClass("android/os/Build");
            auto buildVersionClass = env_->FindClass("android/os/Build$VERSION");

#define SET_FIELD(CLAZZ, FIELD, VALUE) ({ \
            auto id = env_->GetStaticFieldID(CLAZZ, FIELD, "Ljava/lang/String;"); \
            env_->SetStaticObjectField(buildClass, id, env_->NewStringUTF(VALUE)); })

            SET_FIELD(buildClass, "MANUFACTURER", "Google");
            SET_FIELD(buildClass, "MODEL", "Pixel");
            SET_FIELD(buildClass, "FINGERPRINT",
                            "google/sailfish/sailfish:8.1.0/OPM1.171019.011/4448085:user/release-keys");
            SET_FIELD(buildClass, "BRAND", "google");
            SET_FIELD(buildClass, "PRODUCT", "sailfish");
            SET_FIELD(buildClass, "DEVICE", "sailfish");
            SET_FIELD(buildVersionClass, "RELEASE", "8.1.0");
            SET_FIELD(buildClass, "ID", "OPM1.171019.011");
            SET_FIELD(buildVersionClass, "INCREMENTAL", "4448085");
            SET_FIELD(buildVersionClass, "SECURITY_PATCH", "2017-12-05");
            SET_FIELD(buildClass, "TYPE", "user");
            SET_FIELD(buildClass, "TAGS", "release-keys");
        }
        env_->ReleaseStringUTFChars(args->nice_name, process);
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api_;
    JNIEnv *env_;
};

static void companion_handler(int fd) {
    int enabled = access("/data/adb/tricky_store/spoof_build_vars", F_OK) == 0;
    write(fd, &enabled, sizeof(enabled));
}

// Register our module class and the companion handler function
REGISTER_ZYGISK_MODULE(TrickyStore)

REGISTER_ZYGISK_COMPANION(companion_handler)
