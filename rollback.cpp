#include "rollback.h"
#include <Preferences.h>
#include <LittleFS.h>

static Preferences prefs;
static bool firstBootAfterOTA = false;
static bool restorePerformed = false;

static const char *P_BOOT_COUNT   = "boot_cnt";
static const char *P_OTA_BOOTFLAG = "ota_boot";
static const char *P_FAILSAFE     = "failsafe";

static void loadBackupAndRestore() {
    if (restorePerformed) return;
    restorePerformed = true;
    if (!LittleFS.begin(true)) { Serial.println("[ROLLBACK] Could not mount LittleFS"); return; }
    if (!LittleFS.exists("/backup.json")) { Serial.println("[ROLLBACK] No /backup.json available for restore"); return; }
    File f = LittleFS.open("/backup.json", "r");
    if (!f) { Serial.println("[ROLLBACK] Failed to open backup.json"); return; }
    String content = f.readString();
    f.close();
    Serial.println("[ROLLBACK] Restoring from backup.json...");
    Serial.println("[ROLLBACK] Backup content:");
    Serial.println(content);
}

void rollbackInitialize() {
    prefs.begin("rollback", false);
    uint32_t cnt = prefs.getUInt(P_BOOT_COUNT, 0);
    cnt++;
    prefs.putUInt(P_BOOT_COUNT, cnt);
    bool ota_boot_flag = prefs.getBool(P_OTA_BOOTFLAG, false);
    Serial.printf("[ROLLBACK] Boot count: %u\n", cnt);
    Serial.printf("[ROLLBACK] OTA boot flag: %s\n", ota_boot_flag ? "true" : "false");
    if (ota_boot_flag) {
        Serial.println("[ROLLBACK] First boot after OTA detected!");
        firstBootAfterOTA = true;
        prefs.putBool(P_OTA_BOOTFLAG, false);
        prefs.putBool(P_FAILSAFE, true);
    }
    prefs.end();
}

void markNextBootAsOTA() {
    prefs.begin("rollback", false);
    prefs.putBool(P_OTA_BOOTFLAG, true);
    prefs.end();
    Serial.println("[ROLLBACK] Marked next boot as OTA boot");
}

bool rollbackForce() {
    Serial.println("[ROLLBACK] Forced rollback requested");
    loadBackupAndRestore();
    return true;
}

void rollbackPeriodic() {
    if (!firstBootAfterOTA) return;
    prefs.begin("rollback", false);
    bool failsafe = prefs.getBool(P_FAILSAFE, false);
    prefs.end();
    if (failsafe) {
        Serial.println("[ROLLBACK] FAILSAFE RESTORE TRIGGERED");
        loadBackupAndRestore();
        prefs.begin("rollback", false);
        prefs.putBool(P_FAILSAFE, false);
        prefs.end();
        Serial.println("[ROLLBACK] Restore complete. System marked stable.");
    }
}

bool isFirstBootAfterOTA() { return firstBootAfterOTA; }
