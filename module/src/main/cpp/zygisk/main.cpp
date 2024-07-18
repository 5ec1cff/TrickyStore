#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>
#include <string_view>
#include <utility>

#include "glaze/glaze.hpp"
#include "logging.hpp"
#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;
using namespace std::string_view_literals;

struct spoof_config {
    std::string manufacturer{"Google"};
    std::string model{"Pixel"};
    std::string fingerprint{"google/sailfish/sailfish:8.1.0/OPM1.171019.011/4448085:user/release-keys"};
    std::string brand{"google"};
    std::string product{"sailfish"};
    std::string device{"sailfish"};
    std::string release{"8.1.0"};
    std::string id{"OPM1.171019.011"};
    std::string incremental{"4448085"};
    std::string security_patch{"2017-12-05"};
    std::string type{"user"};
    std::string tags{"release-keys"};
};

class TrickyStore : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api_ = api;
        this->env_ = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
        int enabled = 0;
        spoof_config spoofConfig{};
        {
            auto fd = api_->connectCompanion();
            if (fd >= 0) [[likely]] {
                // read enabled
                read(fd, &enabled, sizeof(enabled));
                if (enabled) {
                    size_t bufferSize = 0;
                    std::string buffer;
                    // read size first
                    read(fd, &bufferSize, sizeof(bufferSize));
                    // resize and receive
                    buffer.resize(bufferSize);
                    read(fd, buffer.data(), bufferSize);
                    // parse
                    if (glz::read_json(spoofConfig, buffer)) [[unlikely]] {
                        LOGE("[preAppSpecialize] spoofConfig parse error");
                    }
                }
                close(fd);
            }
        }

        if (!enabled) return;
        if (args->app_data_dir == nullptr) {
            return;
        }

        auto app_data_dir = env_->GetStringUTFChars(args->app_data_dir, nullptr);
        auto nice_name = env_->GetStringUTFChars(args->nice_name, nullptr);

        std::string_view process(nice_name);
        std::string_view dir(app_data_dir);

        if (dir.ends_with("/com.google.android.gms") &&
            process == "com.google.android.gms.unstable") {
            LOGI("spoofing build vars in GMS!");
            auto buildClass = env_->FindClass("android/os/Build");
            auto buildVersionClass = env_->FindClass("android/os/Build$VERSION");

            setField(buildClass, "MANUFACTURER", std::move(spoofConfig.manufacturer));
            setField(buildClass, "MODEL", std::move(spoofConfig.model));
            setField(buildClass, "FINGERPRINT", std::move(spoofConfig.fingerprint));
            setField(buildClass, "BRAND", std::move(spoofConfig.brand));
            setField(buildClass, "PRODUCT", std::move(spoofConfig.product));
            setField(buildClass, "DEVICE", std::move(spoofConfig.device));
            setField(buildVersionClass, "RELEASE", std::move(spoofConfig.release));
            setField(buildClass, "ID", std::move(spoofConfig.id));
            setField(buildVersionClass, "INCREMENTAL", std::move(spoofConfig.incremental));
            setField(buildVersionClass, "SECURITY_PATCH", std::move(spoofConfig.security_patch));
            setField(buildClass, "TYPE", std::move(spoofConfig.type));
            setField(buildClass, "TAGS", std::move(spoofConfig.tags));
        }

        env_->ReleaseStringUTFChars(args->nice_name, nice_name);
        env_->ReleaseStringUTFChars(args->app_data_dir, app_data_dir);
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api_->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api_;
    JNIEnv *env_;

    inline void setField(jclass clazz, const char* field, std::string&& value) {
        auto id = env_->GetStaticFieldID(clazz, field, "Ljava/lang/String;");
        env_->SetStaticObjectField(clazz, id, env_->NewStringUTF(value.c_str()));
    }
};

static inline void write_spoof_configs(const struct spoof_config& spoofConfig) {
    std::string buffer{};

    if (glz::write<glz::opts{.prettify = true}>(spoofConfig, buffer)) [[unlikely]] {
        // This should NEVER happen, but it's not the reason we don't handle the case
        LOGE("[write_spoof_configs] Failed to parse json to std::string");
        return;
    }

    // Remove old one first
    std::filesystem::remove("/data/adb/tricky_store/spoof_build_vars"sv);
    FILE* file = fopen("/data/adb/tricky_store/spoof_build_vars", "w");
    if (!file) [[unlikely]] {
        LOGE("[write_spoof_configs] Failed to open spoof_build_vars");
        return;
    }

    if (fprintf(file, "%s", buffer.c_str()) < 0) [[unlikely]] {
        LOGE("[write_spoof_configs] Failed to write spoof_build_vars");
        fclose(file);
        return;
    }

    fclose(file);
    LOGI("[write_spoof_configs] write done!");
}

static void companion_handler(int fd) {
    int enabled = access("/data/adb/tricky_store/spoof_build_vars", F_OK) == 0;
    write(fd, &enabled, sizeof(enabled));

    if (!enabled) {
        return;
    }

    spoof_config spoofConfig{};
    auto ec = glz::read_file_json(spoofConfig, "/data/adb/tricky_store/spoof_build_vars"sv, std::string{});
    if (ec) [[unlikely]] {
        LOGW("[companion_handler] Failed to parse spoof_build_vars, writing and using default spoof config...");
        write_spoof_configs(spoofConfig);
    }

    std::string buffer = glz::write_json(spoofConfig).value_or("");
    size_t bufferSize = buffer.size();
    // Send buffer size first
    write(fd, &bufferSize, sizeof(bufferSize));
    // client resize string stl and receive buffer
    write(fd, buffer.data(), bufferSize);
}

// Register our module class and the companion handler function
REGISTER_ZYGISK_MODULE(TrickyStore)

REGISTER_ZYGISK_COMPANION(companion_handler)