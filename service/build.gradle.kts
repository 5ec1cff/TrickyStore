import com.android.build.api.dsl.Packaging

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
}

dependencies {
    compileOnly(project(":stub"))
    compileOnly(libs.annotation)
    implementation(libs.bcpkix.jdk18on)
}