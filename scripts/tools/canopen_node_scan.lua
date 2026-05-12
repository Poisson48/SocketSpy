-- canopen_node_scan.lua
-- Scans a CAN bus for CANopen nodes using NMT heartbeat and boot-up frames.
-- Usage: run in SocketSpy scripting engine with can interface set.

local iface      = "vcan0"
local timeout_ms = 5000
local found_nodes = {}

log.print("Scanning for CANopen nodes on " .. iface .. " ...")

local h = can.open(iface)
if not h then
    log.print("ERROR: cannot open " .. iface)
    return
end

-- Filter for heartbeat range: 0x701..0x77F
can.filter(h, {{id = 0x700, mask = 0x780}})

local deadline = time.now_us() + timeout_ms * 1000
while time.now_us() < deadline do
    local frame = can.recv(h, 50)
    if frame then
        local node_id = frame.id - 0x700
        if node_id >= 1 and node_id <= 127 then
            if not found_nodes[node_id] then
                found_nodes[node_id] = frame.data[1]
                local state = frame.data[1]
                log.print(string.format("Node 0x%02X  state=0x%02X", node_id, state))
            end
        end
    end
end

can.close(h)

local count = 0
for _ in pairs(found_nodes) do count = count + 1 end
log.print(string.format("Scan complete. Found %d node(s).", count))
