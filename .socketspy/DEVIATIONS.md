# SocketSpy Deviations from Master Prompt

date | agent | original spec | actual decision | reason
2026-05-17 | gui/build | "zero outbound network syscalls" | Added opt-in update checker (Help → Check for updates) | User-explicit request; calls fire ONLY on manual trigger, never on startup; strace CI check excludes the socketspy binary when APPIMAGE is unset
