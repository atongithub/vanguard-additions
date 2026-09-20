#include "storage.h"
#include "../../include/config.h"
#include <Preferences.h>
#include <FFat.h>

namespace storage {

bool initFat() {
    // Mount the ffat partition without formatting on failure
    return FFat.begin(false, "/fat");
}

// Opened read-write, not read-only: NVS_READONLY fails with NOT_FOUND on a namespace
// that's never been written (e.g. fresh badge), while read-write auto-creates it.
Identity loadMyIdentity() {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_MY_ID, false);
    Identity id;
    id.name = prefs.getString("name", "");
    id.email = prefs.getString("email", "");
    id.phone = prefs.getString("phone", "");
    id.org = prefs.getString("org", "");
    prefs.end();
    return id;
}

void saveMyIdentity(const Identity& id) {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_MY_ID, false);
    prefs.putString("name", id.name);
    prefs.putString("email", id.email);
    prefs.putString("phone", id.phone);
    prefs.putString("org", id.org);
    prefs.end();
}

uint8_t loadBrightness() {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_SETTINGS, false);
    uint8_t v = prefs.getUChar("brightness", 80);
    prefs.end();
    return v;
}

void saveBrightness(uint8_t percent) {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_SETTINGS, false);
    prefs.putUChar("brightness", percent);
    prefs.end();
}

bool loadSoundEnabled() {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_SETTINGS, false);
    bool v = prefs.getBool("sound", true);
    prefs.end();
    return v;
}

void saveSoundEnabled(bool enabled) {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_SETTINGS, false);
    prefs.putBool("sound", enabled);
    prefs.end();
}

bool hasAttackedSomeone() {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_SETTINGS, false);
    bool v = prefs.getBool("attacked", false);
    prefs.end();
    return v;
}

void setHasAttackedSomeone() {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_SETTINGS, false);
    prefs.putBool("attacked", true);
    prefs.end();
}

bool isSecureBadgeEnabled() {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_SETTINGS, false);
    bool v = prefs.getBool("secure_badge", false);
    prefs.end();
    return v;
}

void setSecureBadgeEnabled(bool enabled) {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_SETTINGS, false);
    prefs.putBool("secure_badge", enabled);
    prefs.end();
}

int contactCount() {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_CONTACTS, false);
    int c = prefs.getInt("count", 0);
    prefs.end();
    return c;
}

static String macKey(int i) { return String("mac") + i; }
static String dataKey(int i) { return String("data") + i; }

// data blob packs Name/Email/Phone/Organisation joined by '\x1f'.
static String packIdentity(const Identity& id) {
    return id.name + "\x1f" + id.email + "\x1f" + id.phone + "\x1f" + id.org;
}

static Identity unpackIdentity(const String& blob) {
    Identity id;
    // Bail to an all-empty Identity if a delimiter is missing (corrupted/legacy entry),
    // rather than relying on substring()'s clamping of an unclamped -1 index.
    int p1 = blob.indexOf('\x1f');
    if (p1 < 0) return id;
    int p2 = blob.indexOf('\x1f', p1 + 1);
    if (p2 < 0) return id;
    int p3 = blob.indexOf('\x1f', p2 + 1);
    if (p3 < 0) return id;
    id.name = blob.substring(0, p1);
    id.email = blob.substring(p1 + 1, p2);
    id.phone = blob.substring(p2 + 1, p3);
    id.org = blob.substring(p3 + 1);
    return id;
}

Contact loadContact(int index) {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_CONTACTS, false);
    Contact c;
    c.mac = prefs.getString(macKey(index).c_str(), "");
    c.identity = unpackIdentity(prefs.getString(dataKey(index).c_str(), ""));
    prefs.end();
    return c;
}

void appendContact(const Contact& c) {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_CONTACTS, false);
    int count = prefs.getInt("count", 0);
    if (count >= cfg::NVS_MAX_CONTACTS) {
        prefs.end();
        return;
    }
    prefs.putString(macKey(count).c_str(), c.mac);
    prefs.putString(dataKey(count).c_str(), packIdentity(c.identity));
    prefs.putInt("count", count + 1);
    prefs.end();
}

void deleteContact(int index) {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_CONTACTS, false);
    int count = prefs.getInt("count", 0);
    if (index < 0 || index >= count) {
        prefs.end();
        return;
    }
    for (int i = index; i < count - 1; i++) {
        String mac = prefs.getString(macKey(i + 1).c_str(), "");
        String data = prefs.getString(dataKey(i + 1).c_str(), "");
        prefs.putString(macKey(i).c_str(), mac);
        prefs.putString(dataKey(i).c_str(), data);
    }
    prefs.remove(macKey(count - 1).c_str());
    prefs.remove(dataKey(count - 1).c_str());
    prefs.putInt("count", count - 1);
    prefs.end();
}

void clearContacts() {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_CONTACTS, false);
    prefs.clear();
    prefs.end();
}

} // namespace storage
