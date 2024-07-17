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
        api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);

        int enabled = 0;
        {
            auto fd = api_->connectCompanion();
            if (fd >= 0) {
                read(fd, &enabled, sizeof(enabled));
                close(fd);
            }
        }
        if (!enabled) return;

        auto nice_name = env_->GetStringUTFChars(args->nice_name, nullptr);
        auto app_data_dir = env_->GetStringUTFChars(args->app_data_dir, nullptr);
        // Prevent crash on apps with no data dir
        if (app_data_dir == nullptr) {
            env_->ReleaseStringUTFChars(args->nice_name, nice_name);
            return;
        }

        std::string_view process(nice_name);
        std::string_view dir(app_data_dir);

        env_->ReleaseStringUTFChars(args->nice_name, nice_name);
        env_->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);

        if (!dir.ends_with("/com.google.android.gms") || process != "com.google.android.gms.unstable") {
            return;
        }

        LOGI("spoofing build vars in GMS!");
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
