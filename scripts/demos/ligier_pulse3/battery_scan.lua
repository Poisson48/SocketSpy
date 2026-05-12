-- battery_scan.lua
-- Scans the E4V LiFePO4 48V battery pack on the Ligier Pulse 3 electric microcar.
-- CANopen node, expected at node ID 0x01 or 0x02.
-- Reads cell voltages via SDO from index 0x6000+cell.

local iface   = "vcan0"
local node_id = 0x02   -- E4V BMS node

log.print("Ligier Pulse 3 — Battery Scan")
log.print("Interface: " .. iface .. "  Node: 0x" .. string.format("%02X", node_id))

local h = can.open(iface)
if not h then
    log.print("ERROR: cannot open " .. iface)
    return
end

-- Send NMT reset to wake up the BMS node
nmt.send(h, 0x81, node_id)
time.sleep_ms(500)

-- Wait for boot-up heartbeat
local booted = nmt.wait_boot(h, node_id, 3000)
if not booted then
    log.print("WARNING: node 0x" .. string.format("%02X", node_id) .. " did not boot")
end

-- Read pack voltage (SDO 0x2100, sub 0x01)
local pack_v, err = sdo.read(h, node_id, 0x2100, 0x01)
if pack_v then
    log.print(string.format("Pack voltage : %.2f V", pack_v / 1000.0))
else
    log.print("Pack voltage : read failed (" .. tostring(err) .. ")")
end

-- Read cell count (SDO 0x6000, sub 0x00 = number of cells)
local cell_count = sdo.read(h, node_id, 0x6000, 0x00) or 15
log.print("Cell count   : " .. tostring(cell_count))

-- Read each cell voltage
local total = 0
for cell = 1, cell_count do
    local v = sdo.read(h, node_id, 0x6000, cell)
    if v then
        total = total + v
        log.print(string.format("  Cell %2d: %d mV", cell, v))
    else
        log.print(string.format("  Cell %2d: read failed", cell))
    end
end

log.print(string.format("Sum of cells : %d mV", total))
can.close(h)
log.print("Scan complete.")
