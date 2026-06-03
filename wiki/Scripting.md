# Scripting (Lua) — Scripting (Lua)

> **Language / Langue:** [English](#english) · [Français](#français)
> **Navigation:** [Home](Home.md) · [Getting Started](Getting-Started.md) · [Features](Features.md) · [API & MCP](Api-MCP.md) · [FAQ](FAQ.md)

---

<a name="english"></a>
## English

SocketSpy embeds a **sandboxed Lua 5.4** engine. Open the **Scripts** panel, write
or load a script, and press **Run** (or **Stop**); output appears in the in-app
console. A **watchdog** stops runaway scripts, and the sandbox blocks dangerous
standard-library calls so a script can only touch the CAN bus and local files you
allow — no arbitrary OS access, no network.

You can also run a script remotely through the MCP tool
**[`can_script`](Api-MCP.md)**, which returns the captured stdout, the final
result, and any errors.

### The API tables

Scripts get these global tables injected:

#### `can` — bus access

| Function | Description |
|----------|-------------|
| `can.open(iface)` | Open an interface (e.g. `"vcan0"`). Returns a handle, or `nil` on failure. |
| `can.close(h)` | Close a handle. |
| `can.send(h, frame)` | Transmit a frame. Returns a boolean. |
| `can.recv(h, timeout_ms)` | Receive one frame within the timeout. Returns a frame table or `nil`. |
| `can.filter(h, rules)` | Install hardware filters, e.g. `{{id = 0x700, mask = 0x780}}`. |
| `can.periodic(h, frame, interval_ms)` | Start a periodic send; returns a handle. |
| `can.stop_periodic(ph)` | Stop a periodic send. |
| `can.set_fd(h, enabled)` | Toggle CAN FD on the handle. |

A **frame** table looks like `{ id = 0x123, data = { 0xDE, 0xAD, ... }, dlc = n }`
(`data` is 1-indexed, Lua-style).

#### `log` — output

| Function | Description |
|----------|-------------|
| `log.print(msg)` | Print to the console (and the engine output buffer). |
| `log.csv(path, headers, row)` | Append a CSV row to a **local** file. No network. |

#### `time` — timing

| Function | Description |
|----------|-------------|
| `time.now_us()` | Monotonic microsecond timestamp (integer). |
| `time.sleep_ms(n)` | Sleep for `n` milliseconds. |

#### `bit` / `hex` — bit & hex helpers

| Function | Description |
|----------|-------------|
| `bit.extract(byte, start, len)` | Extract `len` bits from `byte` starting at `start`. |
| `bit.insert(byte, start, len, val)` | Insert `val` into `byte` at the given bit field. |
| `hex.to_bytes(str)` | Hex string → byte table. |
| `hex.from_bytes(table)` | Byte table → hex string. |

#### Protocol helpers — `sdo` / `nmt` / `pdo` / `isotp`

Higher-level protocol tables are also available for CANopen and ISO-TP work
(`sdo.read` / `sdo.write`, `nmt.send` / `nmt.wait_boot`, `pdo.send` /
`pdo.subscribe`, `isotp.send` / `isotp.recv` / `isotp.request`).

### Example: scan for CANopen nodes

This is `scripts/tools/canopen_node_scan.lua`, shipped with SocketSpy:

```lua
-- Scans a CAN bus for CANopen nodes via NMT heartbeat / boot-up frames.
local iface      = "vcan0"
local timeout_ms = 5000
local found_nodes = {}

log.print("Scanning for CANopen nodes on " .. iface .. " ...")

local h = can.open(iface)
if not h then
    log.print("ERROR: cannot open " .. iface)
    return
end

-- Heartbeat range 0x701..0x77F
can.filter(h, {{id = 0x700, mask = 0x780}})

local deadline = time.now_us() + timeout_ms * 1000
while time.now_us() < deadline do
    local frame = can.recv(h, 50)
    if frame then
        local node_id = frame.id - 0x700
        if node_id >= 1 and node_id <= 127 and not found_nodes[node_id] then
            found_nodes[node_id] = frame.data[1]
            log.print(string.format("Node 0x%02X  state=0x%02X", node_id, frame.data[1]))
        end
    end
end

can.close(h)

local count = 0
for _ in pairs(found_nodes) do count = count + 1 end
log.print(string.format("Scan complete. Found %d node(s).", count))
```

### Scripts shipped with SocketSpy

Run these from the **Scripts** panel (not the command line):

| Script | What it does |
|--------|--------------|
| `tools/canopen_node_scan.lua` | Scan CANopen heartbeats (0x701–0x77F), list live nodes |
| `tools/delta_detector.lua` | Detect signal changes vs a baseline |
| `demos/ligier_pulse3/battery_scan.lua` | Read Ligier Pulse 3 BMS cell voltages via SDO |
| `demos/ligier_pulse3/contactor_boot.lua` | NMT + PDO contactor boot sequence |

> **Note.** The standalone `scripting/` library module ships these APIs as stubs
> that print "not connected to core" until wired; inside the **Scripts panel** of
> the GUI the same tables are connected to the live bus, exactly as the shipped
> scripts above demonstrate.

---

<a name="français"></a>
## Français

SocketSpy embarque un moteur **Lua 5.4 en bac à sable**. Ouvrez le panneau
**Scripts**, écrivez ou chargez un script, puis cliquez **Run** (ou **Stop**) ;
la sortie apparaît dans la console intégrée. Un **watchdog** arrête les scripts
emballés, et le bac à sable bloque les appels dangereux de la bibliothèque
standard : un script ne peut toucher que le bus CAN et les fichiers locaux que
vous autorisez — aucun accès OS arbitraire, aucun réseau.

Vous pouvez aussi exécuter un script à distance via l'outil MCP
**[`can_script`](Api-MCP.md)**, qui retourne la sortie stdout capturée, le
résultat final et les éventuelles erreurs.

### Les tables d'API

Ces tables globales sont injectées dans les scripts :

#### `can` — accès au bus

| Fonction | Description |
|----------|-------------|
| `can.open(iface)` | Ouvre une interface (ex. `"vcan0"`). Retourne un handle, ou `nil` en cas d'échec. |
| `can.close(h)` | Ferme un handle. |
| `can.send(h, frame)` | Émet une trame. Retourne un booléen. |
| `can.recv(h, timeout_ms)` | Reçoit une trame dans le délai imparti. Retourne une table trame ou `nil`. |
| `can.filter(h, rules)` | Installe des filtres, ex. `{{id = 0x700, mask = 0x780}}`. |
| `can.periodic(h, frame, interval_ms)` | Démarre un envoi périodique ; retourne un handle. |
| `can.stop_periodic(ph)` | Arrête un envoi périodique. |
| `can.set_fd(h, enabled)` | Active/désactive CAN FD sur le handle. |

Une table **frame** ressemble à `{ id = 0x123, data = { 0xDE, 0xAD, ... }, dlc = n }`
(`data` est indexé à partir de 1, à la mode Lua).

#### `log` — sortie

| Fonction | Description |
|----------|-------------|
| `log.print(msg)` | Affiche dans la console (et le buffer de sortie du moteur). |
| `log.csv(path, headers, row)` | Ajoute une ligne CSV à un fichier **local**. Aucun réseau. |

#### `time` — temps

| Fonction | Description |
|----------|-------------|
| `time.now_us()` | Timestamp monotone en microsecondes (entier). |
| `time.sleep_ms(n)` | Met en pause `n` millisecondes. |

#### `bit` / `hex` — bits & hexa

| Fonction | Description |
|----------|-------------|
| `bit.extract(byte, start, len)` | Extrait `len` bits de `byte` à partir de `start`. |
| `bit.insert(byte, start, len, val)` | Insère `val` dans `byte` au champ de bits indiqué. |
| `hex.to_bytes(str)` | Chaîne hex → table d'octets. |
| `hex.from_bytes(table)` | Table d'octets → chaîne hex. |

#### Aides protocole — `sdo` / `nmt` / `pdo` / `isotp`

Des tables de protocole de plus haut niveau sont également disponibles pour le
travail CANopen et ISO-TP (`sdo.read` / `sdo.write`, `nmt.send` /
`nmt.wait_boot`, `pdo.send` / `pdo.subscribe`, `isotp.send` / `isotp.recv` /
`isotp.request`).

### Exemple : scan de nœuds CANopen

Voici `scripts/tools/canopen_node_scan.lua`, livré avec SocketSpy :

```lua
-- Scanne un bus CAN pour les nœuds CANopen via trames heartbeat / boot-up NMT.
local iface      = "vcan0"
local timeout_ms = 5000
local found_nodes = {}

log.print("Scanning for CANopen nodes on " .. iface .. " ...")

local h = can.open(iface)
if not h then
    log.print("ERROR: cannot open " .. iface)
    return
end

-- Plage heartbeat 0x701..0x77F
can.filter(h, {{id = 0x700, mask = 0x780}})

local deadline = time.now_us() + timeout_ms * 1000
while time.now_us() < deadline do
    local frame = can.recv(h, 50)
    if frame then
        local node_id = frame.id - 0x700
        if node_id >= 1 and node_id <= 127 and not found_nodes[node_id] then
            found_nodes[node_id] = frame.data[1]
            log.print(string.format("Node 0x%02X  state=0x%02X", node_id, frame.data[1]))
        end
    end
end

can.close(h)

local count = 0
for _ in pairs(found_nodes) do count = count + 1 end
log.print(string.format("Scan complete. Found %d node(s).", count))
```

### Scripts livrés avec SocketSpy

Lancez-les depuis le panneau **Scripts** (pas la ligne de commande) :

| Script | Rôle |
|--------|------|
| `tools/canopen_node_scan.lua` | Scan des heartbeats CANopen (0x701–0x77F), liste les nœuds actifs |
| `tools/delta_detector.lua` | Détecte les changements de signaux vs une référence |
| `demos/ligier_pulse3/battery_scan.lua` | Lit les tensions de cellules BMS du Ligier Pulse 3 via SDO |
| `demos/ligier_pulse3/contactor_boot.lua` | Séquence de boot du contacteur NMT + PDO |

> **Note.** Le module bibliothèque autonome `scripting/` livre ces API comme des
> stubs affichant « not connected to core » tant qu'ils ne sont pas câblés ; dans
> le **panneau Scripts** de l'interface, ces mêmes tables sont connectées au bus
> en direct, exactement comme le montrent les scripts ci-dessus.
