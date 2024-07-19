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
    std::optional<std::string> MANUFACTURER;
    std::optional<std::string> MODEL;
    std::optional<std::string> FINGERPRINT;
    std::optional<std::string> BRAND;
    std::optional<std::string> BOARD;
    std::optional<std::string> HARDWARE;
    std::optional<std::string> PRODUCT;
    std::optional<std::string> DEVICE;
    std::optional<std::string> RELEASE;
    std::optional<std::string> ID;
    std::optional<std::string> INCREMENTAL;
    std::optional<std::string> SECURITY_PATCH;
    std::optional<std::string> TYPE;
    std::optional<std::string> TAGS;
};

ssize_t xread(int fd, void *buffer, size_t count) {
    ssize_t total = 0;
    char *buf = (char *)buffer;
    while (count > 0) {
        ssize_t ret = read(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

ssize_t xwrite(int fd, void *buffer, size_t count) {
    ssize_t total = 0;
    char *buf = (char *)buffer;
    while (count > 0) {
        ssize_t ret = write(fd, buf, count);
        if (ret < 0) return -1;
        buf += ret;
        total += ret;
        count -= ret;
    }
    return total;
}

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
                xread(fd, &enabled, sizeof(enabled));
                if (enabled) {
                    size_t bufferSize = 0;
                    std::string buffer;
                    // read size first
                    xread(fd, &bufferSize, sizeof(bufferSize));
                    // resize and receive
                    buffer.resize(bufferSize);
                    xread(fd, buffer.data(), bufferSize);
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

            setField(buildClass, "MANUFACTURER", std::move(spoofConfig.MANUFACTURER));
            setField(buildClass, "MODEL", std::move(spoofConfig.MODEL));
            setField(buildClass, "FINGERPRINT", std::move(spoofConfig.FINGERPRINT));
            setField(buildClass, "BRAND", std::move(spoofConfig.BRAND));
            setField(buildClass, "BOARD", std::move(spoofConfig.BOARD));
            setField(buildClass, "HARDWARE", std::move(spoofConfig.HARDWARE));
            setField(buildClass, "PRODUCT", std::move(spoofConfig.PRODUCT));
            setField(buildClass, "DEVICE", std::move(spoofConfig.DEVICE));
            setField(buildVersionClass, "RELEASE", std::move(spoofConfig.RELEASE));
            setField(buildClass, "ID", std::move(spoofConfig.ID));
            setField(buildVersionClass, "INCREMENTAL", std::move(spoofConfig.INCREMENTAL));
            setField(buildVersionClass, "SECURITY_PATCH", std::move(spoofConfig.SECURITY_PATCH));
            setField(buildClass, "TYPE", std::move(spoofConfig.TYPE));
            setField(buildClass, "TAGS", std::move(spoofConfig.TAGS));
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

    inline void setField(jclass clazz, const char* field, std::optional<std::string>&& value) {
        if (value == std::nullopt) return;
        LOGI("Spoofing build var %s to %s", field, value.value().c_str());
        auto id = env_->GetStaticFieldID(clazz, field, "Ljava/lang/String;");
        env_->SetStaticObjectField(clazz, id, env_->NewStringUTF(value.value().c_str()));
    }
};

static inline void write_spoof_configs(struct spoof_config& spoofConfig) {
    std::string buffer{};

    // Default values
    spoofConfig.MANUFACTURER = "Google";
    spoofConfig.MODEL = "Pixel";
    spoofConfig.FINGERPRINT = "google/sailfish/sailfish:8.1.0/OPM1.171019.011/4448085:user/release-keys";
    spoofConfig.BRAND = "google";
    spoofConfig.PRODUCT = "sailfish";
    spoofConfig.DEVICE = "sailfish";
    spoofConfig.RELEASE = "8.1.0";
    spoofConfig.ID = "OPM1.171019.011";
    spoofConfig.INCREMENTAL = "4448085";
    spoofConfig.SECURITY_PATCH = "2017-12-05";
    spoofConfig.TYPE = "user";
    spoofConfig.TAGS = "release-keys";

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
    xwrite(fd, &enabled, sizeof(enabled));

    if (!enabled) {
        return;
    }

    spoof_config spoofConfig{};
    auto ec = glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>
            (spoofConfig, "/data/adb/tricky_store/spoof_build_vars"sv, std::string{});
    if (ec) [[unlikely]] {
        LOGW("[companion_handler] Failed to parse spoof_build_vars, writing and using default spoof config...");
        // Reset struct to prevent half-read values
        spoofConfig = {};
        write_spoof_configs(spoofConfig);
    }

    std::string buffer = glz::write_json(spoofConfig).value_or("");
    size_t bufferSize = buffer.size();
    // Send buffer size first
    xwrite(fd, &bufferSize, sizeof(bufferSize));
    // client resize string stl and receive buffer
    xwrite(fd, buffer.data(), bufferSize);
}

// Register our module class and the companion handler function
REGISTER_ZYGISK_MODULE(TrickyStore)
REGISTER_ZYGISK_COMPANION(companion_handler)
