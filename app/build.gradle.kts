import java.security.MessageDigest

plugins { id("com.android.application") }

val generatedEngine = layout.buildDirectory.dir("generated/engine")
val buildEngine = tasks.register<Exec>("buildEngine") {
    workingDir(rootProject.file("engine"))
    commandLine(providers.environmentVariable("SILPH_NODE").getOrElse("node"), "build.cjs")
    inputs.files(fileTree("../engine/src"), fileTree("../engine/location"),
        rootProject.file("engine/build.cjs"), rootProject.file("engine/package-lock.json"))
    outputs.dir(generatedEngine)
}
val checkNative = tasks.register("checkNativeRuntime") {
    val runtimes = mapOf(
        "librootcatch-inject.so" to "fde5dd3d72cdc062e31c9f262038d9bfb637b6ac9665131341cfddd1fb7f05c0",
        "libsilph-spawn.so" to "bc4e1a0c7ede20af387e9e002a535cb3ff0072f523c460692820473b3f07e122",
        "libsilph-loader.so" to "45e5b68538d166afbf28157c5a52ca44a5b896bac105c19c8e189553fd475703"
    )
    inputs.files(runtimes.keys.map { file("src/main/jniLibs/arm64-v8a/$it") })
    doLast {
      runtimes.forEach { (name, expected) ->
        val runtime = file("src/main/jniLibs/arm64-v8a/$name")
        check(runtime.isFile) { "Prepare Frida: python tools/prepare_native.py" }
        val hash = MessageDigest.getInstance("SHA-256")
        runtime.inputStream().use { stream ->
            val buffer = ByteArray(32768)
            var count = stream.read(buffer)
            while (count != -1) { hash.update(buffer, 0, count); count = stream.read(buffer) }
        }
        val actual = hash.digest().joinToString("") { "%02x".format(it) }
        check(actual == expected) {
            "Frida runtime integrity mismatch"
        }
      }
    }
}
val releaseStore = providers.environmentVariable("SILPH_KEYSTORE").orNull
android {
    namespace = "dev.rootcatch"
    compileSdk = 36
    defaultConfig {
        applicationId = "dev.rootcatch.ugphonetest"
        minSdk = 29
        targetSdk = 36
        versionCode = 62
        versionName = "1.9.3"
    }
    buildFeatures { buildConfig = true }
    sourceSets.getByName("main") {
        java.directories.add(generatedEngine.get().dir("java").asFile.absolutePath)
        assets.directories.add(generatedEngine.get().dir("assets").asFile.absolutePath)
    }
    if (releaseStore != null) {
        signingConfigs.create("publication") {
            storeFile = rootProject.file(releaseStore)
            storePassword = providers.environmentVariable("SILPH_STORE_PASSWORD").get()
            keyAlias = providers.environmentVariable("SILPH_KEY_ALIAS").get()
            keyPassword = providers.environmentVariable("SILPH_KEY_PASSWORD").get()
        }
    }
    buildTypes {
        release {
            isMinifyEnabled = false
            isDebuggable = false
            isCrunchPngs = false
            if (releaseStore != null) signingConfig = signingConfigs.getByName("publication")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    packaging { jniLibs { useLegacyPackaging = true; keepDebugSymbols += setOf("**/librootcatch-inject.so", "**/libsilph-spawn.so") } }
}
tasks.named("preBuild").configure { dependsOn(buildEngine, checkNative) }
