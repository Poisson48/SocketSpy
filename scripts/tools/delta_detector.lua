-- delta_detector.lua
-- Captures a baseline, then captures an active session,
-- reports which IDs changed between the two.
-- Usage: set iface, baseline_ms, active_ms before running.

local iface       = "vcan0"
local baseline_ms = 3000
local active_ms   = 3000

local function capture(duration_ms)
    local h = can.open(iface)
    if not h then return nil end
    local snapshot = {}
    local deadline = time.now_us() + duration_ms * 1000
    while time.now_us() < deadline do
        local f = can.recv(h, 10)
        if f then
            snapshot[f.id] = f.data
        end
    end
    can.close(h)
    return snapshot
end

log.print("Capturing baseline for " .. baseline_ms .. "ms ...")
local baseline = capture(baseline_ms)
if not baseline then
    log.print("ERROR: cannot open " .. iface)
    return
end

log.print("Capturing active session for " .. active_ms .. "ms ...")
log.print("(Trigger your event now)")
local active = capture(active_ms)

log.print("\n=== Delta Report ===")
local changed = 0

for id, data in pairs(active) do
    local base = baseline[id]
    if not base then
        log.print(string.format("NEW  0x%03X  %s", id, hex.from_bytes(data)))
        changed = changed + 1
    else
        local diff = false
        for i = 1, #data do
            if data[i] ~= base[i] then
                diff = true
                break
            end
        end
        if diff then
            log.print(string.format("CHNG 0x%03X  was:%s  now:%s",
                id, hex.from_bytes(base), hex.from_bytes(data)))
            changed = changed + 1
        end
    end
end

for id in pairs(baseline) do
    if not active[id] then
        log.print(string.format("GONE 0x%03X", id))
        changed = changed + 1
    end
end

log.print(string.format("Total changes: %d", changed))
