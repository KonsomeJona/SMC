# ProGuard / R8 rules for SMC Android release build.
#
# The native libsmc.so calls back into the Java side via JNI (SDL2's
# Java bridge classes + our SMCActivity subclass). R8 must not rename
# or strip those classes/methods.

# Keep our app entry point and any subclasses
-keep public class org.smc.** { *; }

# Keep the SDL2 Java bridge intact — JNI lookups use original names.
-keep public class org.libsdl.app.** { *; }
-keepclassmembers class org.libsdl.app.** { *; }

# Native callback methods (any class) — preserve names.
-keepclasseswithmembernames class * {
    native <methods>;
}

# AndroidX SplashScreen (added later in release prep)
-keep class androidx.core.splashscreen.** { *; }
