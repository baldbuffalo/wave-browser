plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.wavebrowser.android"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.wavebrowser.android"
        minSdk = 23
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
    }

    // Wave Android always produces the browser APK with this name.
    applicationVariants.all {
        outputs.all {
            outputFileName = "WaveBrowser.apk"
        }
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.17.0")
    implementation("androidx.appcompat:appcompat:1.7.1")
    implementation("androidx.activity:activity-ktx:1.10.1")
    implementation("androidx.webkit:webkit:1.14.0")
}
