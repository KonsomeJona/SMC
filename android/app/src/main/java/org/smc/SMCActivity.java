package org.smc;

import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * SMCActivity — wraps SDL2's SDLActivity and bootstraps the game's data path.
 *
 * On first run (or after an APK update) all files under assets/data/ are
 * extracted to getFilesDir()/data/ so the C++ code can read them via the
 * normal file system.  DATA_DIR is compiled in as that same path
 * (/data/user/0/org.smc/files/data) so no runtime env-var lookup is needed.
 *
 * Writable user data (save games, preferences) lives in getFilesDir() as well.
 */
public class SMCActivity extends SDLActivity {

    private static final String TAG = "SMC";
    private static final String PREFS_NAME = "smc_prefs";
    private static final String PREFS_KEY_VERSION = "assets_version";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // ------------------------------------------------------------------
        // 1. Extract game assets from the APK to internal storage.
        //    This MUST happen before super.onCreate() loads the native lib,
        //    because the C++ code reads DATA_DIR files at startup.
        // ------------------------------------------------------------------
        extractAssets();

        // ------------------------------------------------------------------
        // 2. Advertise paths to the C++ side via environment variables.
        //    DATA_DIR is also baked in at compile time (see CMakeLists.txt),
        //    but setting env vars here provides a runtime safety net and
        //    gives the save-game / preferences subsystem its writable root.
        // ------------------------------------------------------------------
        String dataDir = getFilesDir().getAbsolutePath() + "/data";
        String userDir = getFilesDir().getAbsolutePath();

        setenv("DATA_DIR",      dataDir, true);   // C++ game data (read-only after extraction)
        setenv("SMC_DATA_DIR",  dataDir, true);   // same, legacy name used in some paths
        setenv("SMC_ASSETS_DIR", dataDir, true);  // alias kept for compatibility
        setenv("SMC_USER_DIR",  userDir, true);   // writable: saves, preferences

        Log.i(TAG, "DATA_DIR      = " + dataDir);
        Log.i(TAG, "SMC_USER_DIR  = " + userDir);

        super.onCreate(savedInstanceState);
    }

    // -----------------------------------------------------------------------
    // Asset extraction
    // -----------------------------------------------------------------------

    /**
     * Copies assets/data/ → getFilesDir()/data/ when the APK version changes.
     *
     * Version gating: the APK versionCode is stored in SharedPreferences after
     * a successful extraction.  On the next launch the stored value is compared
     * to the current versionCode; if they match, extraction is skipped entirely.
     *
     * Individual files are skipped (not overwritten) if they already exist on
     * disk — this preserves any files the user may have placed there manually,
     * and avoids redundant I/O within the same version.  A full clean
     * extraction only happens after an APK upgrade (new versionCode).
     */
    private void extractAssets() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        int lastVersion = prefs.getInt(PREFS_KEY_VERSION, -1);

        int currentVersion;
        try {
            currentVersion = getPackageManager()
                    .getPackageInfo(getPackageName(), 0).versionCode;
        } catch (Exception e) {
            Log.w(TAG, "Could not read versionCode, defaulting to 1");
            currentVersion = 1;
        }

        if (lastVersion == currentVersion) {
            Log.i(TAG, "Assets already extracted for version " + currentVersion + ", skipping.");
            return;
        }

        Log.i(TAG, "Extracting game assets (APK version " + currentVersion + ") …");
        try {
            extractDir("data", getFilesDir().getAbsolutePath());
            prefs.edit().putInt(PREFS_KEY_VERSION, currentVersion).apply();
            Log.i(TAG, "Asset extraction complete.");
        } catch (IOException e) {
            // Non-fatal: the game will log errors about missing files rather
            // than crashing here.  The version stamp is NOT written so the
            // extraction will be retried on the next launch.
            Log.e(TAG, "Asset extraction failed: " + e.getMessage());
        }
    }

    /**
     * Recursively copies the asset tree rooted at {@code assetPath} into
     * {@code targetBase}/{@code assetPath}/ on the file system.
     *
     * @param assetPath  Path inside the APK assets/ folder (e.g. "data" or
     *                   "data/pixmaps/maryo").
     * @param targetBase Absolute path of the extraction root
     *                   (i.e. {@code getFilesDir().getAbsolutePath()}).
     */
    private void extractDir(String assetPath, String targetBase) throws IOException {
        AssetManager am = getAssets();
        String[] entries = am.list(assetPath);
        if (entries == null || entries.length == 0) {
            // assetPath is a file, not a directory — copy it directly.
            copyAssetFile(am, assetPath, targetBase);
            return;
        }

        // assetPath is a directory — ensure the target directory exists.
        File targetDir = new File(targetBase, assetPath);
        if (!targetDir.exists() && !targetDir.mkdirs()) {
            throw new IOException("Failed to create directory: " + targetDir.getAbsolutePath());
        }

        for (String entry : entries) {
            String childAsset = assetPath + "/" + entry;
            String[] subEntries = am.list(childAsset);
            if (subEntries != null && subEntries.length > 0) {
                // Subdirectory — recurse.
                extractDir(childAsset, targetBase);
            } else {
                // File — copy.
                copyAssetFile(am, childAsset, targetBase);
            }
        }
    }

    /**
     * Copies a single asset file to disk.  Skips the file if it already exists
     * (within the same APK version, presence is sufficient).
     */
    private void copyAssetFile(AssetManager am, String assetFile, String targetBase)
            throws IOException {
        File outFile = new File(targetBase, assetFile);

        // Ensure parent directory exists (handles files in the root asset dir).
        File parent = outFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IOException("Failed to create directory: " + parent.getAbsolutePath());
        }

        if (outFile.exists()) {
            // File is already on disk from a previous extraction within this
            // APK version — skip to avoid redundant I/O.
            return;
        }

        try (InputStream in = am.open(assetFile);
             FileOutputStream out = new FileOutputStream(outFile)) {
            byte[] buf = new byte[16384];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
        }
    }

    // -----------------------------------------------------------------------
    // SDL2 library list
    // -----------------------------------------------------------------------

    /**
     * Tell SDL2 which shared libraries to load, in dependency order.
     * SDL2 must come first; the game library ("smc") must come last.
     */
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL2",
            "SDL2_image",
            "SDL2_mixer",
            "SDL2_ttf",
            "smc"
        };
    }

    // -----------------------------------------------------------------------
    // JNI helper — thin wrapper around SDLActivity.nativeSetenv().
    // -----------------------------------------------------------------------

    /**
     * Delegates to SDL2's built-in nativeSetenv so we can call setenv(3) from
     * Java.  The {@code overwrite} parameter is accepted for API clarity but
     * SDL2's implementation always overwrites.
     */
    private static void setenv(String name, String value,
                               @SuppressWarnings("unused") boolean overwrite) {
        SDLActivity.nativeSetenv(name, value);
    }
}
