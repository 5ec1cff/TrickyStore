import android.databinding.tool.ext.capitalizeUS

plugins {
    alias(libs.plugins.jetbrains.kotlin.android)
    alias(libs.plugins.agp.app)
}

android {
    namespace = "io.github.a13e300.tricky_store"
    compileSdk = 34

    defaultConfig {
        applicationId = "io.github.a13e300.tricky_store"
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildTypes {
        release {
            signingConfig = signingConfigs["debug"]
        }
    }

    packaging {
        resources {
            excludes += "**"
        }
    }

    lint {
        checkReleaseBuilds = false
        abortOnError = true
    }
}

dependencies {
    compileOnly(project(":stub"))
    compileOnly(libs.annotation)
    implementation(libs.bcpkix.jdk18on)
}

androidComponents.onVariants { variant ->
    afterEvaluate {
        val variantLowered = variant.name.lowercase()
        val variantCapped = variant.name.capitalizeUS()
        val pushTask = task<Task>("pushService$variantCapped") {
            group = "Service"
            dependsOn("assemble$variantCapped")
            doLast {
                exec {
                    commandLine(
                        "adb",
                        "push",
                        layout.buildDirectory.file("outputs/apk/$variantLowered/service-$variantLowered.apk")
                            .get().asFile.absolutePath,
                        "/data/local/tmp/service.apk"
                    )
                }
                exec {
                    commandLine(
                        "adb",
                        "shell",
                        "su -c 'rm /data/adb/modules/tricky_store/service.apk; mv /data/local/tmp/service.apk /data/adb/modules/tricky_store/'"
                    )
                }
            }
        }

        task<Task>("pushAndRestartService$variantCapped") {
            group = "Service"
            dependsOn(pushTask)
            doLast {
                exec {
                    commandLine("adb", "shell", "su -c \"setprop ctl.restart keystore2\"")
                }
            }
        }
    }
}
