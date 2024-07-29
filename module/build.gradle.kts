import android.databinding.tool.ext.capitalizeUS
import org.apache.tools.ant.filters.FixCrLfFilter
import org.apache.tools.ant.filters.ReplaceTokens
import java.security.MessageDigest

plugins {
    alias(libs.plugins.agp.app)
    alias(libs.plugins.lsplugin.cmaker)
}

val moduleId: String by rootProject.extra
val moduleName: String by rootProject.extra
val verCode: Int by rootProject.extra
val verName: String by rootProject.extra
val commitHash: String by rootProject.extra
val abiList: List<String> by rootProject.extra
val androidMinSdkVersion: Int by rootProject.extra
val author: String by rootProject.extra
val description: String by rootProject.extra
val moduleDescription = description

android {
    defaultConfig {
        ndk {
            abiFilters.addAll(abiList)
        }
    }

    buildFeatures {
        prefab = true
    }

    externalNativeBuild {
        cmake {
            version = "3.28.0+"
            path("src/main/cpp/CMakeLists.txt")
        }
    }
}

cmaker {
    default {
        arguments += arrayOf(
            "-DANDROID_STL=none",
            "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
            "-DANDROID_ALLOW_UNDEFINED_SYMBOLS=ON",
            "-DMODULE_NAME=$moduleId",
            "-DCMAKE_CXX_STANDARD=23",
            "-DCMAKE_C_STANDARD=23",
            "-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON",
            "-DCMAKE_VISIBILITY_INLINES_HIDDEN=ON",
            "-DCMAKE_CXX_VISIBILITY_PRESET=hidden",
            "-DCMAKE_C_VISIBILITY_PRESET=hidden",
            )
        abiFilters(*abiList.toTypedArray())
    }
}

dependencies {
    implementation(libs.cxx)
}

androidComponents.onVariants { variant ->
    afterEvaluate {
        val variantLowered = variant.name.lowercase()
        val variantCapped = variant.name.capitalizeUS()
        val buildTypeLowered = variant.buildType?.lowercase()
        val supportedAbis = abiList.map {
            when (it) {
                "arm64-v8a" -> "arm64"
                "armeabi-v7a" -> "arm"
                "x86" -> "x86"
                "x86_64" -> "x64"
                else -> error("unsupported abi $it")
            }
        }.joinToString(" ")

        val moduleDir = layout.buildDirectory.file("outputs/module/$variantLowered")
        val zipFileName =
            "$moduleName-$verName-$verCode-$commitHash-$buildTypeLowered.zip".replace(' ', '-')

        val prepareModuleFilesTask = task<Sync>("prepareModuleFiles$variantCapped") {
            group = "module"
            dependsOn("assemble$variantCapped", ":service:assemble$variantCapped")
            into(moduleDir)
            from(rootProject.layout.projectDirectory.file("README.md"))
            from(layout.projectDirectory.file("template")) {
                exclude("module.prop", "customize.sh", "post-fs-data.sh", "service.sh", "daemon")
                filter<FixCrLfFilter>("eol" to FixCrLfFilter.CrLf.newInstance("lf"))
            }
            from(layout.projectDirectory.file("template")) {
                include("module.prop")
                expand(
                    "moduleId" to moduleId,
                    "moduleName" to moduleName,
                    "versionName" to "$verName ($verCode-$commitHash-$variantLowered)",
                    "versionCode" to verCode,
                    "author" to author,
                    "description" to moduleDescription,
                )
            }
            from(layout.projectDirectory.file("template")) {
                include("customize.sh", "post-fs-data.sh", "service.sh", "daemon")
                val tokens = mapOf(
                    "DEBUG" to if (buildTypeLowered == "debug") "true" else "false",
                    "SONAME" to moduleId,
                    "SUPPORTED_ABIS" to supportedAbis,
                    "MIN_SDK" to androidMinSdkVersion.toString()
                )
                filter<ReplaceTokens>("tokens" to tokens)
                filter<FixCrLfFilter>("eol" to FixCrLfFilter.CrLf.newInstance("lf"))
            }
            from(project(":service").layout.buildDirectory.file("outputs/apk/$variantLowered/service-$variantLowered.apk")) {
                rename { "service.apk" }
            }
            from(layout.buildDirectory.file("intermediates/stripped_native_libs/$variantLowered/strip${variantCapped}DebugSymbols/out/lib")) {
                exclude("**/libbinder.so", "**/libutils.so")
                into("lib")
            }

            doLast {
                fileTree(moduleDir).visit {
                    if (isDirectory) return@visit
                    val md = MessageDigest.getInstance("SHA-256")
                    file.forEachBlock(4096) { bytes, size ->
                        md.update(bytes, 0, size)
                    }
                    file(file.path + ".sha256").writeText(
                        org.apache.commons.codec.binary.Hex.encodeHexString(
                            md.digest()
                        )
                    )
                }
            }
        }

        val zipTask = task<Zip>("zip$variantCapped") {
            group = "module"
            dependsOn(prepareModuleFilesTask)
            archiveFileName.set(zipFileName)
            destinationDirectory.set(layout.projectDirectory.file("release").asFile)
            from(moduleDir)
        }

        val pushTask = task<Exec>("push$variantCapped") {
            group = "module"
            dependsOn(zipTask)
            commandLine("adb", "push", zipTask.outputs.files.singleFile.path, "/data/local/tmp")
        }

        val installKsuTask = task<Exec>("installKsu$variantCapped") {
            group = "module"
            dependsOn(pushTask)
            commandLine(
                "adb", "shell", "su", "-c",
                "/data/adb/ksud module install /data/local/tmp/$zipFileName"
            )
        }

        val installMagiskTask = task<Exec>("installMagisk$variantCapped") {
            group = "module"
            dependsOn(pushTask)
            commandLine(
                "adb",
                "shell",
                "su",
                "-M",
                "-c",
                "magisk --install-module /data/local/tmp/$zipFileName"
            )
        }

        task<Exec>("installKsuAndReboot$variantCapped") {
            group = "module"
            dependsOn(installKsuTask)
            commandLine("adb", "reboot")
        }

        task<Exec>("installMagiskAndReboot$variantCapped") {
            group = "module"
            dependsOn(installMagiskTask)
            commandLine("adb", "reboot")
        }
    }
}
