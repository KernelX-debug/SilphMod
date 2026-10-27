import java.security.MessageDigest

plugins { id("com.android.application") }
val nativeLibraries = file("src/main/jniLibs/arm64-v8a")
val generatedNative = layout.buildDirectory.dir("generated/native/java")
val prepareNative = tasks.register<Exec>("prepareNative") {
    workingDir(rootProject.projectDir)
    val windows = System.getProperty("os.name").startsWith("Windows")
    commandLine(if (windows) "powershell.exe" else "pwsh", "-NoProfile", "-File", "tools/build_native.ps1")
    inputs.files(rootProject.fileTree("native"), rootProject.file("tools/build_native.ps1"))
    outputs.dir(nativeLibraries)
}
val nativeArtifacts = tasks.register("generateNativeArtifacts") {
    dependsOn(prepareNative)
    inputs.dir(nativeLibraries)
    outputs.dir(generatedNative)
    doLast {
        val names = listOf("libsilph-native-probe.so", "libsilph-control.so", "libsilph-farmer-check.so", "libsilph-kecleon-check.so", "libsilph-farmer-status.so", "libsilph-kecleon-status.so", "libsilph-rocket-check.so", "libsilph-rocket-status.so", "libshadowhook.so", "libshadowhook_nothing.so")
        val cases = names.joinToString("\n") { name ->
            val bytes = nativeLibraries.resolve(name).readBytes()
            check(bytes.size > 20 && bytes[0] == 127.toByte() && bytes[1] == 69.toByte() && bytes[4] == 2.toByte() && bytes[18] == 183.toByte()) { "Invalid ARM64 component: $name" }
            val hash = MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }
            "case \"$name\": return \"$hash\";"
        }
        val destination = generatedNative.get().file("dev/silphmod/nativecheck/NativeArtifacts.java").asFile
        destination.parentFile.mkdirs()
        destination.writeText("package dev.silphmod.nativecheck; final class NativeArtifacts { static String hash(String name) { switch(name) { $cases default: throw new IllegalArgumentException(\"Unknown native component\"); } } }")
    }
}
val releaseStore = providers.environmentVariable("SILPH_KEYSTORE").orNull
android {
    namespace = "dev.silphmod.nativecheck"
    compileSdk = 36
    defaultConfig {
        applicationId = "dev.rootcatch.ugphonetest"
        minSdk = 29
        targetSdk = 36
        versionCode = 134
        versionName = "2.2.12"
        ndk { abiFilters += "arm64-v8a" }
    }
    buildFeatures { buildConfig = true }
    sourceSets.getByName("main").java.directories.add(generatedNative.get().asFile.absolutePath)
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    packaging { jniLibs { useLegacyPackaging = true; keepDebugSymbols += "**/*.so" } }
    if (releaseStore != null) {
        signingConfigs.create("publication") {
            storeFile = rootProject.file(releaseStore)
            storePassword = providers.environmentVariable("SILPH_STORE_PASSWORD").get()
            keyAlias = providers.environmentVariable("SILPH_KEY_ALIAS").get()
            keyPassword = providers.environmentVariable("SILPH_KEY_PASSWORD").get()
        }
    }
    buildTypes { release { isMinifyEnabled = false; if (releaseStore != null) signingConfig = signingConfigs.getByName("publication") } }
}
tasks.named("preBuild").configure { dependsOn(nativeArtifacts) }
