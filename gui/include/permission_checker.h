#pragma once
#include <QString>

namespace socketspy::gui {

struct PermStatus {
    bool serialPortGroup = false;  // user in dialout or uucp
    bool canSudoHelper   = false;  // /usr/local/bin/socketspy-can-setup installed
    bool udevRules       = false;  // /etc/udev/rules.d/99-socketspy-can.rules installed
    bool plugdevGroup    = false;  // user in plugdev (true when group absent)
    bool plugdevExists   = false;  // plugdev group exists on this system

    bool allOk() const {
        return serialPortGroup && canSudoHelper && udevRules
            && (!plugdevExists || plugdevGroup);
    }
};

class PermissionChecker {
public:
    static PermStatus check();
    static bool addUserToGroup(const QString& group);
    static QString serialGroup();   // "dialout" or "uucp"

    static bool groupExists(const char* groupName);
    static bool userInGroup(const char* groupName);
};

} // namespace socketspy::gui
