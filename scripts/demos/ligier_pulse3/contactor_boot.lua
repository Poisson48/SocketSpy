-- contactor_boot.lua
-- Boots the contactor on the Ligier Pulse 3 via CANopen NMT + PDO handshake.
-- Known CAN IDs: NMT 0x000, PDO 0x226, SDO 0x580/0x600.

local iface      = "vcan0"
local bms_node   = 0x02
local motor_node = 0x03  -- Curtis 1232e

log.print("Ligier Pulse 3 — Contactor Boot Sequence")

local h = can.open(iface)
if not h then
    log.print("ERROR: cannot open " .. iface)
    return
end

-- Step 1: Put both nodes into pre-operational
log.print("Step 1: NMT → Pre-Operational (broadcast)")
nmt.send(h, 0x80, 0)  -- broadcast pre-op
time.sleep_ms(100)

-- Step 2: Configure TPDO mapping on BMS (SDO)
log.print("Step 2: Configure BMS TPDO (PDO 0x226)")
sdo.write(h, bms_node, 0x1800, 0x01, 0x226, "uint32")
sdo.write(h, bms_node, 0x1800, 0x02, 0xFF,  "uint8")   -- event-driven

-- Step 3: Start both nodes
log.print("Step 3: NMT → Operational")
nmt.send(h, 0x01, bms_node)
nmt.send(h, 0x01, motor_node)
time.sleep_ms(200)

-- Step 4: Send contactor enable via PDO 0x226
log.print("Step 4: PDO 0x226 — contactor enable")
pdo.send(h, 0x226, {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00})

-- Step 5: Monitor for contactor acknowledgement
log.print("Step 5: Waiting for contactor ack ...")
can.filter(h, {{id = 0x226, mask = 0x7FF}})
local deadline = time.now_us() + 3000 * 1000
local acked = false
while time.now_us() < deadline do
    local f = can.recv(h, 50)
    if f and f.id == 0x226 and f.data[1] == 0x11 then
        acked = true
        break
    end
end

if acked then
    log.print("Contactor CLOSED — drive enabled")
else
    log.print("WARNING: no contactor ack within 3s")
end

can.close(h)
log.print("Sequence complete.")
