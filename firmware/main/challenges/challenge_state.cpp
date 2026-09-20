#include "challenge_state.h"
#include "../../include/config.h"
#include <Preferences.h>

namespace challenges {

bool isCompleted(const char* id) {
    if (id != nullptr && strcmp(id, "lvl1") == 0) {
        return true;
    }
    Preferences prefs;
    // Read-write, not read-only: NVS_READONLY errors if the namespace was
    // never written (fresh badge). Read-write auto-creates it; still never writes here.
    prefs.begin(cfg::NVS_NS_CHALLENGES, false);
    bool done = prefs.getBool(id, false);
    prefs.end();
    return done;
}

void setCompleted(const char* id, bool done) {
    Preferences prefs;
    prefs.begin(cfg::NVS_NS_CHALLENGES, false);
    prefs.putBool(id, done);
    prefs.end();
}

} // namespace challenges
