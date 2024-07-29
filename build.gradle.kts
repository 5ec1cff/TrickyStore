import com.android.build.gradle.AppExtension
import com.android.build.gradle.LibraryExtension
import org.jetbrains.kotlin.daemon.common.toHexString
import java.io.ByteArrayOutputStream
import java.security.MessageDigest

plugins {
    alias(libs.plugins.agp.app) apply false
    alias(libs.plugins.jetbrains.kotlin.android) apply false
    alias(libs.plugins.android.library) apply false
}

fun String.execute(currentWorkingDir: File = file("./")): String {
    val byteOut = ByteArrayOutputStream()
    project.exec {
        workingDir = currentWorkingDir
        commandLine = split("\\s".toRegex())
        standardOutput = byteOut
    }
    return String(byteOut.toByteArray()).trim()
}

val gitCommitCount = "git rev-list HEAD --count".execute().toInt()
val gitCommitHash = "git rev-parse --verify --short HEAD".execute()

// also the soname
val moduleId by extra("tricky_store")
val moduleName by extra("Tricky Store")
val author by extra("5ec1cff")
val description by extra("A trick of keystore")
val verName by extra("v1.0.3")
val verCode by extra(gitCommitCount)
val commitHash by extra(gitCommitHash)
val abiList by extra(listOf("arm64-v8a", "x86_64"))

fun calculateChecksum(): String {
    return MessageDigest.getInstance("SHA-256").run {
        update(moduleId.toByteArray(Charsets.UTF_8))
        update(moduleName.toByteArray(Charsets.UTF_8))
        update(verName.toByteArray(Charsets.UTF_8))
        update(verCode.toString().toByteArray(Charsets.UTF_8))
        update(author.toByteArray(Charsets.UTF_8))
        update(description.toByteArray(Charsets.UTF_8))
        digest().toHexString()
    }
}

val androidMinSdkVersion by extra(31)
val androidTargetSdkVersion by extra(34)
val androidCompileSdkVersion by extra(34)
val androidBuildToolsVersion by extra("34.0.0")
val androidCompileNdkVersion by extra("27.0.12077973")
val androidSourceCompatibility by extra(JavaVersion.VERSION_17)
val androidTargetCompatibility by extra(JavaVersion.VERSION_17)

tasks.register("Delete", Delete::class) {
    delete(layout.buildDirectory)
}

fun Project.configureBaseExtension() {
    extensions.findByType(AppExtension::class)?.run {
        namespace = "io.github.a13e300.tricky_store"
        compileSdkVersion(androidCompileSdkVersion)
        ndkVersion = androidCompileNdkVersion
        buildToolsVersion = androidBuildToolsVersion

        defaultConfig {
            minSdk = androidMinSdkVersion
            targetSdk = androidCompileSdkVersion
            versionCode = verCode
            versionName = verName
        }

        compileOptions {
            sourceCompatibility = androidSourceCompatibility
            targetCompatibility = androidTargetCompatibility
        }
    }

    extensions.findByType(LibraryExtension::class)?.run {
        namespace = "io.github.a13e300.tricky_store"
        compileSdk = androidCompileSdkVersion
        ndkVersion = androidCompileNdkVersion
        buildToolsVersion = androidBuildToolsVersion

        defaultConfig {
            minSdk = androidMinSdkVersion
        }

        lint {
            checkReleaseBuilds = false
            abortOnError = true
        }

        compileOptions {
            sourceCompatibility = androidSourceCompatibility
            targetCompatibility = androidTargetCompatibility
        }
    }
}

subprojects {
    plugins.withId("com.android.application") {
        configureBaseExtension()
    }
    plugins.withId("com.android.library") {
        configureBaseExtension()
    }
    plugins.withType(JavaPlugin::class.java) {
        extensions.configure(JavaPluginExtension::class.java) {
            sourceCompatibility = androidSourceCompatibility
            targetCompatibility = androidTargetCompatibility
        }
    }
}
