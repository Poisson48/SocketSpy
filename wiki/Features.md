# Features — Fonctionnalités

> **Language / Langue:** [English](#english) · [Français](#français)
> **Navigation:** [Home](Home.md) · [Getting Started](Getting-Started.md) · [API & MCP](Api-MCP.md) · [Scripting](Scripting.md) · [FAQ](FAQ.md)

**Maturity legend / Légende de maturité:**
**Proven / Éprouvé** = verified, shipped, stable · **Beta / Bêta** = works but not yet broadly proven · **Incoming / À venir** = roadmap.

---

<a name="english"></a>
## English

SocketSpy replaces the old 22-tab scroll bar with an **icon sidebar**. Drag any
panel — or right-click → **Detach** — to float it as an independent window;
**Redock** snaps it back. This makes it easy to watch the Monitor and a Graph
side by side.

### Live frame monitor — Proven
<a name="monitor"></a>

Real-time scrolling table of every frame on the bus.

- 2000-row ring buffer, search/filter by CAN ID, pause and clear.
- **Copy to clipboard** — `Ctrl+C` or right-click on any table to copy a single
  cell, the selected rows, or the whole table as TSV.
- When a second bus is open (see Multi-bus), a **Bus** column distinguishes
  frame sources.

### DBC signal decode — Proven
<a name="dbc"></a>

Load a `.dbc` file and signals are decoded **inline** with engineering units.

- Full **round-trip lossless parser**: `VERSION`, `BU_`, `BO_`, `SG_`, `VAL_`, `CM_`.
- A built-in **DBC builder** panel lets you construct messages and signals with a
  visual bit grid and value tables — useful when you have no manufacturer DBC.

### Signal graphs — Proven
<a name="graphs"></a>

Plot up to **8 live traces** on a rolling **10-second** QChart window. Pick any
decoded signal; auto-scaling keeps the trace in frame. Ideal for watching RPM,
speed, temperatures, or any analog quantity change in real time.

### Frame transmit — Proven
<a name="transmit"></a>

Send **standard / extended / FD** frames. DLC 0–15 (up to 64 bytes for CAN FD),
with input validation on the ID and payload fields.

### Interface selector — Proven

Auto-detects all SocketCAN interfaces and lets you **hot-swap** without
restarting. Hit **↺** to refresh after plugging in an adapter.

### Protocol decoders — Proven
<a name="protocols"></a>

A live in-app **Protocols** tab decodes higher-level traffic. Supported stacks:

| Protocol | Decoded |
|----------|---------|
| **CANopen** | NMT, SDO, PDO, EMCY, heartbeat |
| **J1939** | PGN / SPN extraction |
| **ISO-TP** (ISO 15765-2) | Multi-frame transport reassembly |
| **UDS** (ISO 14229) | Service requests/responses |
| **OBD-II** | Standard PIDs |
| **NMEA 2000** | Marine PGNs |

These same stacks back the UDS tester, the simulator, and the MCP CANopen tools.

### Lua scripting — Proven
<a name="lua"></a>

In-app editor with **Run / Stop** and a console, driven by a **sandboxed Lua 5.4**
engine with a watchdog. Scripts get `can`, `log`, `time`, `bit` and `hex` tables.
See the dedicated **[Scripting](Scripting.md)** page for the full API and examples.

### CAN simulator — Proven
<a name="simulator"></a>

Generate realistic CAN traffic **without hardware**. Use the built-in vehicle
profiles, or create custom ones from the UI — handy for demos, DBC authoring, and
testing decoders. Pairs naturally with virtual CAN (`vcan0`).

### Frame fuzzer — Proven
<a name="fuzzer"></a>

Send **random**, **incremental**, or **bit-flip** CAN frames at a configurable
interval, with a per-run frame counter. Useful for robustness testing and for
provoking ECU reactions during reverse engineering.

### UDS tester — Proven
<a name="uds-tester"></a>

A full **ISO 14229 + ISO 15765-2** client session:

- **Read DTC** (service 0x19) and **Clear DTC** (0x14).
- **Read ECU Info** via 0x22 (VIN, serial, current session).
- Live session indicator.

### UDS ECU simulator — Beta
<a name="uds-sim"></a>

A full **ISO-TP server + UDS responder** so you can test UDS clients with no real
hardware. Implements services
**0x10 / 0x11 / 0x14 / 0x19 / 0x22 / 0x27 / 0x28 / 0x2E / 0x31 / 0x3E**, including
**SecurityAccess** seed/key (0x27) and configurable **DIDs** and **DTCs**.

### BLF / MDF4 export — Proven
<a name="blf-mdf4"></a>

- **File → Export BLF…** — Vector CANalyzer-compatible **BLF v2**.
- **File → Export MDF4…** — **MDF4 v4.10** (asammdf / MATLAB).

Take captures straight into the standard automotive tooling.

### Capture diff — Proven
<a name="capture-diff"></a>

Compare two captures (`.log` or `.csv`) side by side:

- Diff by CAN ID: **A only / B only / Changed / Same**.
- **Byte-level delta**, with filterable results.

Workflow for reverse engineering: capture a baseline, trigger a function on the
vehicle, capture again, diff to find the changed signals.

### Multi-bus monitor — Proven
<a name="multi-bus"></a>

**Tools → "+ Add Second Bus…"** opens a second SocketCAN interface in parallel.
A **Bus** column in the Monitor tells the two sources apart — useful for gateways
and cross-bus correlation.

### Signal Detective — Beta
<a name="signal-detective"></a>

A heuristic (no LLM) reverse-engineering assistant under the **Detect** panel,
with two tabs:

- **Classify** — auto-classify every observed CAN ID by **update rate**
  (slow / medium / fast) and **signal type** (digital / analog / constant / counter).
- **Wiggle test** — captures a baseline, then a snapshot after a physical action
  (e.g. press a button), and ranks the **top 30 changed signals** by score.

### Other niceties — Proven

- **Welcome screen** — GIMP-style floating dialog on first launch with recent
  projects, resource links and a "don't show again" preference. Reopen any time
  via **Help → Welcome screen…**.
- **French / English UI** — full Qt i18n via `.ts` files; the language choice
  persists across sessions.
- **Secure updates** — Ed25519-signed release manifest; atomic AppImage install
  with rollback; bundled TLS for HTTPS update checks.
- **io_uring capture** — high-throughput capture pipeline with hardware
  timestamps.

### Flash → verify loop (with ECU Studio) — Beta

The flagship suite interconnection: **ECU Studio** flashes an ECU map, then
**SocketSpy** verifies *live on the CAN bus* that the change took effect at the
right operating point. This is driven through the **[MCP server](Api-MCP.md)** —
see that page for the wiring.

---

<a name="français"></a>
## Français

SocketSpy remplace l'ancienne barre à 22 onglets par une **barre latérale à
icônes**. Glissez n'importe quel panneau — ou clic droit → **Détacher** — pour le
faire flotter en fenêtre indépendante ; **Redocker** le réintègre. Pratique pour
surveiller le Monitor et un graphe côte à côte.

### Monitor de trames en direct — Éprouvé

Table défilante temps réel de chaque trame du bus.

- Buffer circulaire de 2000 lignes, recherche/filtre par ID CAN, pause et effacement.
- **Copier-coller** — `Ctrl+C` ou clic droit sur toute table pour copier une
  cellule, les lignes sélectionnées, ou toute la table en TSV.
- Avec un second bus ouvert (voir Multi-bus), une colonne **Bus** distingue les
  sources de trames.

### Décodage de signaux DBC — Éprouvé

Chargez un fichier `.dbc` : les signaux sont décodés **en ligne** avec unités.

- **Parser sans perte aller-retour** : `VERSION`, `BU_`, `BO_`, `SG_`, `VAL_`, `CM_`.
- Un panneau **DBC builder** intégré permet de construire messages et signaux via
  une grille de bits visuelle et des tables de valeurs — utile sans DBC constructeur.

### Graphes de signaux — Éprouvé

Tracez jusqu'à **8 courbes en direct** sur une fenêtre QChart glissante de
**10 secondes**. L'auto-échelle garde la courbe cadrée. Idéal pour suivre régime,
vitesse, températures ou toute grandeur analogique en temps réel.

### Émission de trames — Éprouvé

Envoi de trames **standard / étendues / FD**. DLC 0–15 (jusqu'à 64 octets en
CAN FD), avec validation des champs ID et payload.

### Sélecteur d'interface — Éprouvé

Détecte automatiquement toutes les interfaces SocketCAN et permet le **hot-swap**
sans redémarrage. Cliquez **↺** pour rafraîchir après branchement.

### Décodeurs de protocoles — Éprouvé

Un onglet **Protocols** intégré décode le trafic de plus haut niveau :

| Protocole | Décodé |
|-----------|--------|
| **CANopen** | NMT, SDO, PDO, EMCY, heartbeat |
| **J1939** | extraction PGN / SPN |
| **ISO-TP** (ISO 15765-2) | réassemblage transport multi-trames |
| **UDS** (ISO 14229) | requêtes/réponses de service |
| **OBD-II** | PIDs standard |
| **NMEA 2000** | PGNs maritimes |

Ces piles servent aussi au testeur UDS, au simulateur et aux outils CANopen MCP.

### Scripting Lua — Éprouvé

Éditeur intégré avec **Run / Stop** et console, propulsé par un moteur **Lua 5.4
en bac à sable** avec watchdog. Les scripts disposent des tables `can`, `log`,
`time`, `bit` et `hex`. Voir la page dédiée **[Scripting](Scripting.md)**.

### Simulateur CAN — Éprouvé

Génère un trafic CAN réaliste **sans matériel**. Utilisez les profils véhicules
intégrés ou créez les vôtres depuis l'UI — pratique pour les démos, la création
de DBC et le test de décodeurs. Se marie bien avec le CAN virtuel (`vcan0`).

### Fuzzer de trames — Éprouvé

Envoi de trames **aléatoires**, **incrémentales** ou **bit-flip** à intervalle
configurable, avec compteur de trames par run. Utile pour les tests de robustesse
et pour provoquer des réactions d'ECU lors du reverse.

### Testeur UDS — Éprouvé

Une session client **ISO 14229 + ISO 15765-2** complète :

- **Lire DTC** (service 0x19) et **Effacer DTC** (0x14).
- **Lire infos ECU** via 0x22 (VIN, numéro de série, session courante).
- Indicateur de session en direct.

### Simulateur d'ECU UDS — Bêta

Un **serveur ISO-TP + répondeur UDS** complet pour tester des clients UDS sans
matériel réel. Implémente les services
**0x10 / 0x11 / 0x14 / 0x19 / 0x22 / 0x27 / 0x28 / 0x2E / 0x31 / 0x3E**, dont le
**SecurityAccess** seed/key (0x27) et des **DIDs** et **DTCs** configurables.

### Export BLF / MDF4 — Éprouvé

- **File → Export BLF…** — **BLF v2** compatible Vector CANalyzer.
- **File → Export MDF4…** — **MDF4 v4.10** (asammdf / MATLAB).

Emmenez vos captures directement dans l'outillage automobile standard.

### Diff de captures — Éprouvé

Comparez deux captures (`.log` ou `.csv`) côte à côte :

- Diff par ID CAN : **A seul / B seul / Changé / Identique**.
- **Delta au niveau octet**, résultats filtrables.

Flux de reverse : capturez une référence, déclenchez une fonction du véhicule,
recapturez, puis diff pour trouver les signaux qui ont changé.

### Monitor multi-bus — Éprouvé

**Tools → « + Add Second Bus… »** ouvre une seconde interface SocketCAN en
parallèle. Une colonne **Bus** dans le Monitor distingue les deux sources —
utile pour les passerelles et la corrélation inter-bus.

### Signal Detective — Bêta

Un assistant de reverse heuristique (sans LLM) dans le panneau **Detect**, à deux
onglets :

- **Classify** — auto-classe chaque ID CAN observé par **cadence**
  (lent / moyen / rapide) et **type de signal** (digital / analogique / constant / compteur).
- **Wiggle test** — capture une référence, puis un instantané après une action
  physique (ex. appui bouton), et classe les **30 signaux les plus modifiés** par score.

### Autres atouts — Éprouvé

- **Écran d'accueil** — dialogue flottant style GIMP au premier lancement :
  projets récents, liens ressources et préférence « ne plus afficher ». Rouvrable
  via **Help → Welcome screen…**.
- **UI Français / Anglais** — i18n Qt complète via fichiers `.ts` ; le choix de
  langue persiste entre sessions.
- **Mises à jour sécurisées** — manifeste de version signé Ed25519 ; installation
  AppImage atomique avec rollback ; TLS embarqué pour les vérifs HTTPS.
- **Capture io_uring** — pipeline de capture haut débit avec timestamps matériels.

### Boucle flash → vérif (avec ECU Studio) — Bêta

L'interconnexion phare de la suite : **ECU Studio** flashe une cartographie
d'ECU, puis **SocketSpy** vérifie *en direct sur le bus CAN* que le changement a
pris effet au bon point de fonctionnement. Cela passe par le **[serveur MCP](Api-MCP.md)** —
voir cette page pour le câblage.
