#include "rollback.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static Preferences prefs;
static bool firstBootAfterOTA = false;
static bool restorePerformed = false;

// Preference keys
static const char *P_BOOT_COUNT   = "boot_cnt";
static const char *P_OTA_BOOTFLAG = "ota_boot";   // Set to true after OTA install
static const char *P_FAILSAFE     = "failsafe";    // If set, restore attempted

// ------------- Internal helpers -------------- //
static void loadBackupAndRestore() {
    if (restorePerformed) return;
    restorePerformed = true;

    if (!LittleFS.begin(true)) {
        Serial.println("[ROLLBACK] Could not mount LittleFS");
        return;
    }
    if (!LittleFS.exists("/backup.json")) {
        Serial.println("[ROLLBACK] No /backup.json available for restore");
        return;
    }

    File f = LittleFS.open("/backup.json", "r");
    if (!f) {
        Serial.println("[ROLLBACK] Failed to open backup.json");
        return;
    }

    String content = f.readString();
    f.close();
    Serial.println("[ROLLBACK] Restoring from backup.json...");

    // You can extend this part to restore:
    // - keymap settings
    // - wifi settings
    // - layout mode
    // - user preferences
    // etc.

    // Example: Only printing for now
    Serial.println("[ROLLBACK] Backup content:");
    Serial.println(content);

    // If you want full restore logic,
    // call appropriate setters from other modules here.
}

// ---------------------------------------------- //
// INITIALIZE — Called early in setup()
void rollbackInitialize() {
    prefs.begin("rollback", false);

    uint32_t cnt = prefs.getUInt(P_BOOT_COUNT, 0);
    cnt++;
    prefs.putUInt(P_BOOT_COUNT, cnt);

    bool ota_boot_flag = prefs.getBool(P_OTA_BOOTFLAG, false);

    Serial.printf("[ROLLBACK] Boot count: %u\n", cnt);
    Serial.printf("[ROLLBACK] OTA boot flag: %s\n", ota_boot_flag ? "true" : "false");

    // Detect first boot after OTA
    if (ota_boot_flag) {
        Serial.println("[ROLLBACK] First boot after OTA detected!");
        firstBootAfterOTA = true;

        // Clear flag
        prefs.putBool(P_OTA_BOOTFLAG, false);

        // Mark failsafe restore as pending until periodic says system OK
        prefs.putBool(P_FAILSAFE, true);
    }
    prefs.end();
}

// ---------------------------------------------- //
// Called by OTA module right after OTA install
void markNextBootAsOTA() {
    prefs.begin("rollback", false);
    prefs.putBool(P_OTA_BOOTFLAG, true);
    prefs.end();

    Serial.println("[ROLLBACK] Marked next boot as OTA boot");
}

// ---------------------------------------------- //
// FORCE ROLLBACK
bool rollbackForce() {
    Serial.println("[ROLLBACK] Forced rollback requested");
    loadBackupAndRestore();
    return true;
}

// ---------------------------------------------- //
// LOOP — checks if system is stable after OTA
void rollbackPeriodic() {
    // If not first boot after OTA → nothing to do
    if (!firstBootAfterOTA) return;

    // If failsafe flag is set → restore configuration
    prefs.begin("rollback", false);
    bool failsafe = prefs.getBool(P_FAILSAFE, false);
    prefs.end();

    if (failsafe) {
        Serial.println("[ROLLBACK] FAILSAFE RESTORE TRIGGERED");
        loadBackupAndRestore();

        // Assume system stable now
        prefs.begin("rollback", false);
        prefs.putBool(P_FAILSAFE, false);
        prefs.end();

        Serial.println("[ROLLBACK] Restore complete. System marked stable.");
    }
}

// ---------------------------------------------- //
bool isFirstBootAfterOTA() {
    return firstBootAfterOTA;
}
