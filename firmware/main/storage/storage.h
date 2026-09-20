#pragma once
#include <Arduino.h>

// NVS (Preferences) wrapper — spec §1, §8.7, §9 "NVS Namespaces" table.
namespace storage {

bool initFat();

struct Identity {
    String name;
    String email;
    String phone;
    String org;
};

struct Contact {
    String mac;
    Identity identity;
};

// ---- my_id namespace: name, email, phone, org ----
Identity loadMyIdentity();
void saveMyIdentity(const Identity& id);

// ---- settings namespace: brightness ----
uint8_t loadBrightness();
void saveBrightness(uint8_t percent);
bool loadSoundEnabled();
void saveSoundEnabled(bool enabled);

// ---- settings namespace: Badge Attack (see main/glitch/glitch.h) ----
// True once this badge has landed a confirmed Attack packet — unlocks the "Secure Badge" toggle.
bool hasAttackedSomeone();
void setHasAttackedSomeone();
bool isSecureBadgeEnabled(); // when true, incoming Attack packets are ignored
void setSecureBadgeEnabled(bool enabled);

// ---- contacts namespace: count, mac0/mac1..., data0/data1... ----
int contactCount();
Contact loadContact(int index);
void appendContact(const Contact& c);
void deleteContact(int index); // re-packs and re-saves remaining entries
void clearContacts();

} // namespace storage
