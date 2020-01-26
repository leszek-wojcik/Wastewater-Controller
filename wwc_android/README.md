Before build generation please set environment varialbe `ANDROID_SDK_ROOT` to
point to your adnroid SDK.  Typically this resides in your home directory:
"~/Android/Sdk/" `export ANDROID_SDK_ROOT=~/Android/Sdk`

After setting you run `./gradlew assemble` in order to generate `apk` package
for Android.

In case you are using Android Studio IDE you might just import project and this
will adjust `local.properties` to include proper place of your SDK.
