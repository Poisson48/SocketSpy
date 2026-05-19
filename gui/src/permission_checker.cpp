#include "permission_checker.h"

#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace socketspy::gui {

bool PermissionChecker::groupExists(const char* groupName) {
    return getgrnam(groupName) != nullptr;
}

bool PermissionChecker::userInGroup(const char* groupName) {
    struct group* gr = getgrnam(groupName);
    if (!gr) return true;  // group absent → not a blocker

    struct passwd* pw = getpwuid(getuid());
    if (!pw) return false;

    if (pw->pw_gid == gr->gr_gid) return true;

    for (char** m = gr->gr_mem; m && *m; ++m)
        if (strcmp(*m, pw->pw_name) == 0) return true;

    return false;
}

QString PermissionChecker::serialGroup() {
    return groupExists("dialout") ? "dialout" : "uucp";
}

PermStatus PermissionChecker::check() {
    PermStatus s;
    s.serialPortGroup = userInGroup("dialout") || userInGroup("uucp");
    s.canSudoHelper   = QFile::exists("/usr/local/bin/socketspy-can-setup")
                     && QFile::exists("/etc/sudoers.d/socketspy-can");
    s.udevRules       = QFile::exists("/etc/udev/rules.d/99-socketspy-can.rules");
    s.plugdevExists   = groupExists("plugdev");
    s.plugdevGroup    = s.plugdevExists ? userInGroup("plugdev") : true;
    return s;
}

bool PermissionChecker::addUserToGroup(const QString& group) {
    const QString user = QProcessEnvironment::systemEnvironment().value("USER");
    if (user.isEmpty()) return false;
    QProcess p;
    p.start("pkexec", {"usermod", "-aG", group, user});
    p.waitForFinished(30'000);
    return p.exitCode() == 0;
}

} // namespace socketspy::gui
