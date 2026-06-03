# API & MCP Server — Serveur API & MCP

> **Language / Langue:** [English](#english) · [Français](#français)
> **Navigation:** [Home](Home.md) · [Getting Started](Getting-Started.md) · [Features](Features.md) · [Scripting](Scripting.md) · [FAQ](FAQ.md)

---

<a name="english"></a>
## English

SocketSpy ships a **Model Context Protocol (MCP)** server so an AI assistant
(Claude Desktop, or any MCP client) — or any JSON-RPC automation — can drive the
CAN bus through a clean, typed tool interface. It is the mechanism behind the
**ECU Studio flash → live verify loop**.

> **Privacy first.** All traffic stays local. The TCP transport binds **only to
> `127.0.0.1`**, and this is enforced *in code*, not merely in config. SocketSpy
> makes no outbound network calls.

### Two ways to run the server

1. **Standalone binary `socketspy-mcp`** — built when `spdlog` + `cpp-httplib`
   are present. Speaks JSON-RPC 2.0:

   ```bash
   socketspy-mcp --stdio          # default — for Claude Desktop and stdio clients
   socketspy-mcp --tcp 7891       # TCP, bound to 127.0.0.1 only (default port 7891)
   ```

2. **From the GUI — the MCP panel.** Open the **MCP** panel in the sidebar
   (the ⚙ icon, "MCP Server"). Choose **TCP** (with a port spinbox) or **Stdio**,
   press **Start**, and watch the live console. The panel launches the bundled
   `socketspy-mcp` binary next to the app and shows its stdout/stderr.

### Transport — JSON-RPC 2.0

The server implements the standard MCP handshake and tool calls:

| Method | Purpose |
|--------|---------|
| `initialize` | Capability handshake |
| `tools/list` | Enumerate available tools and their JSON-Schema |
| `tools/call` | Invoke a tool with parameters |

- **stdio**: line-delimited JSON-RPC on stdin/stdout — the transport Claude
  Desktop uses.
- **TCP**: same protocol over a socket bound to `127.0.0.1:<port>` (default 7891).

### Connecting Claude Desktop

Add SocketSpy to your MCP client config
(`~/.config/Claude/claude_desktop_config.json` on Linux):

```json
{
  "mcpServers": {
    "socketspy": {
      "command": "/path/to/socketspy-mcp",
      "args": ["--stdio"]
    }
  }
}
```

Point `command` at the `socketspy-mcp` binary (it sits next to the `socketspy`
GUI binary in the build/AppImage). Restart Claude Desktop; the SocketSpy tools
appear in the tool list.

### The 11 tools

| Tool | What it does |
|------|--------------|
| `can_monitor` | Capture frames from an interface for a duration. Returns frame objects (timestamp, ID, DLC, hex). Optional ID filter and DBC decode. |
| `can_send` | Transmit a frame once, or periodically (`periodic_ms`). Periodic sends return a handle to cancel with `can_stop`. |
| `can_stop` | Cancel a periodic transmission by handle; also stops an active monitor session. |
| `can_decode` | Decode a raw frame against a loaded DBC. Returns the message name and an array of decoded signals with units. |
| `can_replay` | Replay a captured log onto a live interface. Supports candump `.log` and SocketSpy native; `speed` multiplier (1.0 = real-time). |
| `can_script` | Run a Lua script in the SocketSpy engine. Returns captured stdout, final result, and errors. See [Scripting](Scripting.md). |
| `canopen_sdo_read` | Read one Object Dictionary entry from a CANopen node via SDO (expedited + segmented). |
| `canopen_sdo_write` | Write an Object Dictionary entry via SDO. `type` chooses encoding (uint8/int16/uint32/string…). |
| `canopen_scan` | Scan the bus for active CANopen nodes (NMT guarding / heartbeat). Returns node IDs, NMT states, identity. |
| `can_diff` | Compare two capture sessions and report byte-level differences. The reverse-engineering workhorse. |
| `get_stats` | Live bus statistics: frame rate, bus load %, error frames, unique IDs, ring buffer utilisation. |

#### Example: `can_send` input schema

```json
{
  "type": "object",
  "properties": {
    "iface":       { "type": "string",  "description": "CAN interface, e.g. vcan0 or can0" },
    "id":          { "type": "string",  "description": "CAN frame ID in hex, e.g. '0x123'" },
    "data":        { "type": "string",  "description": "Payload as hex bytes, e.g. 'DEADBEEF01020304'" },
    "periodic_ms": { "type": "integer", "description": "If set, repeat at this interval (ms)", "minimum": 1 }
  },
  "required": ["iface", "id", "data"]
}
```

`can_monitor` requires `{ iface, duration_ms }` and accepts an optional hex
`id` filter and a `decode` boolean. Every tool advertises its full JSON-Schema
through `tools/list`, so a client discovers parameters automatically.

### How the ECU Studio interconnection drives a live signal verify

The flagship suite loop ties the two apps together (this loop is **Beta**):

1. **ECU Studio flashes a map** — e.g. it raises the boost-pressure target at a
   given RPM / load operating point and writes the patched ROM to the ECU.
2. **ECU Studio (or you) asks SocketSpy to verify** through the MCP server, for
   example:
   - `can_send` to command the engine/ECU to the **right operating point**
     (the RPM/load cell that was modified), then
   - `can_monitor` (with `decode: true` and a loaded DBC) — or `can_decode` on
     a specific frame — to read back the **live signal** (e.g. actual boost,
     injected fuel) at that point.
3. **`can_diff` / `get_stats`** confirm the change: diff a pre-flash baseline
   capture against the post-flash capture, or watch the live signal cross the new
   target. SocketSpy reports back whether the flashed value took effect *at the
   right operating point* — closing the loop that ECU Studio opened.

Because everything is JSON-RPC over a local `127.0.0.1` transport, ECU Studio (or
an AI agent orchestrating both) can run this verification programmatically, with
no manual scope reading.

> **Tip.** For a no-hardware dry run, point both apps at `vcan0` and use the
> SocketSpy **Simulator** to play the role of the ECU while you exercise the loop.

---

<a name="français"></a>
## Français

SocketSpy embarque un serveur **Model Context Protocol (MCP)** pour qu'un
assistant IA (Claude Desktop, ou tout client MCP) — ou n'importe quelle
automatisation JSON-RPC — puisse piloter le bus CAN via une interface d'outils
propre et typée. C'est le mécanisme derrière la **boucle flash → vérif en direct
d'ECU Studio**.

> **Confidentialité d'abord.** Tout le trafic reste local. Le transport TCP
> n'écoute **que sur `127.0.0.1`**, ce qui est imposé *dans le code*, pas
> seulement en config. SocketSpy ne fait aucun appel réseau sortant.

### Deux façons de lancer le serveur

1. **Binaire autonome `socketspy-mcp`** — compilé quand `spdlog` + `cpp-httplib`
   sont présents. Parle JSON-RPC 2.0 :

   ```bash
   socketspy-mcp --stdio          # défaut — pour Claude Desktop et clients stdio
   socketspy-mcp --tcp 7891       # TCP, lié à 127.0.0.1 uniquement (port 7891 par défaut)
   ```

2. **Depuis l'interface — le panneau MCP.** Ouvrez le panneau **MCP** dans la
   barre latérale (icône ⚙, « MCP Server »). Choisissez **TCP** (avec un sélecteur
   de port) ou **Stdio**, cliquez **Start** et observez la console en direct. Le
   panneau lance le binaire `socketspy-mcp` situé à côté de l'application et
   affiche sa sortie stdout/stderr.

### Transport — JSON-RPC 2.0

Le serveur implémente la poignée de main MCP standard et les appels d'outils :

| Méthode | Rôle |
|---------|------|
| `initialize` | Négociation des capacités |
| `tools/list` | Énumère les outils disponibles et leur JSON-Schema |
| `tools/call` | Invoque un outil avec ses paramètres |

- **stdio** : JSON-RPC délimité par lignes sur stdin/stdout — le transport
  qu'utilise Claude Desktop.
- **TCP** : même protocole sur une socket liée à `127.0.0.1:<port>` (défaut 7891).

### Connecter Claude Desktop

Ajoutez SocketSpy à la config de votre client MCP
(`~/.config/Claude/claude_desktop_config.json` sous Linux) :

```json
{
  "mcpServers": {
    "socketspy": {
      "command": "/chemin/vers/socketspy-mcp",
      "args": ["--stdio"]
    }
  }
}
```

Pointez `command` vers le binaire `socketspy-mcp` (situé à côté du binaire GUI
`socketspy` dans le build/l'AppImage). Redémarrez Claude Desktop ; les outils
SocketSpy apparaissent dans la liste.

### Les 11 outils

| Outil | Rôle |
|-------|------|
| `can_monitor` | Capture des trames d'une interface pendant une durée. Retourne des objets trame (timestamp, ID, DLC, hex). Filtre ID et décodage DBC optionnels. |
| `can_send` | Émet une trame une fois ou périodiquement (`periodic_ms`). L'envoi périodique retourne un handle annulable via `can_stop`. |
| `can_stop` | Annule une émission périodique par handle ; arrête aussi une session de monitoring active. |
| `can_decode` | Décode une trame brute avec un DBC chargé. Retourne le nom du message et un tableau de signaux décodés avec unités. |
| `can_replay` | Rejoue un log capturé sur une interface. Supporte candump `.log` et le format natif ; multiplicateur `speed` (1.0 = temps réel). |
| `can_script` | Exécute un script Lua dans le moteur SocketSpy. Retourne stdout, résultat final et erreurs. Voir [Scripting](Scripting.md). |
| `canopen_sdo_read` | Lit une entrée du dictionnaire d'objets d'un nœud CANopen via SDO (expedited + segmenté). |
| `canopen_sdo_write` | Écrit une entrée du dictionnaire d'objets via SDO. `type` choisit l'encodage (uint8/int16/uint32/string…). |
| `canopen_scan` | Scanne le bus pour les nœuds CANopen actifs (guarding NMT / heartbeat). Retourne IDs de nœuds, états NMT, identité. |
| `can_diff` | Compare deux sessions de capture et signale les différences au niveau octet. Le cheval de bataille du reverse. |
| `get_stats` | Statistiques de bus en direct : cadence, charge bus %, trames d'erreur, IDs uniques, utilisation du ring buffer. |

#### Exemple : schéma d'entrée de `can_send`

```json
{
  "type": "object",
  "properties": {
    "iface":       { "type": "string",  "description": "Interface CAN, ex. vcan0 ou can0" },
    "id":          { "type": "string",  "description": "ID de trame CAN en hex, ex. '0x123'" },
    "data":        { "type": "string",  "description": "Payload en octets hex, ex. 'DEADBEEF01020304'" },
    "periodic_ms": { "type": "integer", "description": "Si défini, répète à cet intervalle (ms)", "minimum": 1 }
  },
  "required": ["iface", "id", "data"]
}
```

`can_monitor` requiert `{ iface, duration_ms }` et accepte un filtre `id` hex
optionnel et un booléen `decode`. Chaque outil expose son JSON-Schema complet via
`tools/list`, donc un client découvre les paramètres automatiquement.

### Comment l'interconnexion ECU Studio pilote une vérification de signal en direct

La boucle phare de la suite relie les deux applications (cette boucle est **Bêta**) :

1. **ECU Studio flashe une cartographie** — par ex. il relève la cible de
   pression de suralimentation à un point de fonctionnement régime/charge donné
   et écrit le ROM patché dans l'ECU.
2. **ECU Studio (ou vous) demande à SocketSpy de vérifier** via le serveur MCP,
   par exemple :
   - `can_send` pour amener le moteur/ECU au **bon point de fonctionnement**
     (la cellule régime/charge modifiée), puis
   - `can_monitor` (avec `decode: true` et un DBC chargé) — ou `can_decode` sur
     une trame précise — pour relire le **signal en direct** (ex. boost réel,
     carburant injecté) à ce point.
3. **`can_diff` / `get_stats`** confirment le changement : on compare une capture
   de référence pré-flash à la capture post-flash, ou on observe le signal en
   direct franchir la nouvelle cible. SocketSpy rapporte si la valeur flashée a
   pris effet *au bon point de fonctionnement* — fermant la boucle ouverte par
   ECU Studio.

Comme tout passe en JSON-RPC sur un transport local `127.0.0.1`, ECU Studio (ou
un agent IA orchestrant les deux) peut exécuter cette vérification de façon
programmatique, sans lecture manuelle à l'oscilloscope.

> **Astuce.** Pour un essai sans matériel, pointez les deux applications sur
> `vcan0` et utilisez le **Simulateur** SocketSpy pour jouer le rôle de l'ECU
> pendant que vous exercez la boucle.
