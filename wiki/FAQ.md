# FAQ — Foire aux questions

> **Language / Langue:** [English](#english) · [Français](#français)
> **Navigation:** [Home](Home.md) · [Getting Started](Getting-Started.md) · [Features](Features.md) · [API & MCP](Api-MCP.md) · [Scripting](Scripting.md)

---

<a name="english"></a>
## English

### Does SocketSpy send any data to the cloud?

No. SocketSpy is **100% local** — no telemetry, no analytics, no network calls.
The only outbound traffic is the optional, signed **update check** over HTTPS,
which you trigger manually. The MCP TCP transport binds **only to `127.0.0.1`**,
enforced in code.

### Which OS / kernel do I need?

Linux with **SocketCAN**. The AppImage needs kernel ≥ 5.4; building from source
targets kernel 5.11+. Works on x86_64, aarch64 and armv7l (Raspberry Pi).

### Do I need a CAN adapter to try it?

No. Run `bash scripts/dev/setup_vcan.sh` to create a virtual interface (`vcan0`),
then use the built-in **[Simulator](Features.md#simulator)** to generate traffic.
See **[Getting Started](Getting-Started.md)**.

### Which CAN adapters are supported?

Anything the Linux SocketCAN subsystem recognises: CANable 2.0 / Pro and Seeed
USB-CAN (`gs_usb`), PEAK PCAN-USB (`peak_usb`), Kvaser (`kvaser_usb`), Ixxat
(`ixxat_usb2can` / `ems_usb`), and more. Full table in **[Getting Started](Getting-Started.md)**.

### The app opens but shows no frames. What's wrong?

1. Check the interface is up: `ip link show can0` (or `vcan0`).
2. Confirm raw traffic outside the app: `candump can0` (from `can-utils`).
3. Make sure you selected the right interface in the toolbar; hit **↺** to refresh.
4. For a virtual bus, inject a test frame: `cansend vcan0 123#DEADBEEF`.

### "Permission denied" on can0?

Use **Tools → "Configure CAN permissions (one-time)"** in the app, or run
`sudo usermod -aG dialout $USER` and log out/in.

### Build fails — `Qt6Charts not found` / `lua5.4 not found` / `nlohmann_json not found`.

Install the matching dev package: `qt6-charts-dev`, `liblua5.4-dev`,
`nlohmann-json3-dev`. The easiest path is `bash scripts/dev/install_deps.sh`,
which auto-detects your distro. CMake must be **3.28+**.

### How do I get the MCP / `socketspy-mcp` binary?

It builds when the optional `libspdlog-dev` and `libcpp-httplib-dev` packages are
present at configure time. Then launch it standalone (`socketspy-mcp --stdio` /
`--tcp <port>`) or from the GUI's **MCP** panel. See **[API & MCP](Api-MCP.md)**.

### Can Claude (or another AI) drive SocketSpy?

Yes — that's what the MCP server is for. Point Claude Desktop at `socketspy-mcp`
and it gets 11 typed tools (`can_monitor`, `can_send`, `can_decode`, …). All
traffic stays on `127.0.0.1`. See **[API & MCP](Api-MCP.md)**.

### What's the relationship with ECU Studio?

Both are part of the **[ECU Studio Suite](https://github.com/Poisson48/ecu_studio_suite)**.
ECU Studio reprograms ECUs and edits maps; SocketSpy monitors the live bus. The
flagship loop: ECU Studio flashes a map, then SocketSpy verifies on the CAN bus
that the change took effect at the right operating point — driven over MCP. See
the **[flash → verify loop](Api-MCP.md#english)**.

### Is my Lua script safe to run?

Yes. The engine is **sandboxed Lua 5.4** with a **watchdog**: dangerous
standard-library access is blocked, runaway scripts are stopped, and a script can
only reach the CAN bus and the local files you allow. See **[Scripting](Scripting.md)**.

### What export formats are supported?

Vector CANalyzer-compatible **BLF v2** and asammdf/MATLAB **MDF4 v4.10**, via
**File → Export BLF… / Export MDF4…**. Captures also load from / save to candump
`.log` and `.csv`.

### What license is SocketSpy under?

**GPL-3.0.** Source: https://github.com/Poisson48/SocketSpy.

### Where do I report bugs or request features?

Open an issue at https://github.com/Poisson48/SocketSpy/issues.

---

<a name="français"></a>
## Français

### SocketSpy envoie-t-il des données dans le cloud ?

Non. SocketSpy est **100% local** — aucune télémétrie, aucune analytique, aucun
appel réseau. Le seul trafic sortant est la **vérification de mise à jour**
optionnelle et signée en HTTPS, que vous déclenchez manuellement. Le transport
TCP du MCP n'écoute **que sur `127.0.0.1`**, imposé dans le code.

### Quel OS / noyau faut-il ?

Linux avec **SocketCAN**. L'AppImage nécessite un noyau ≥ 5.4 ; la compilation
depuis les sources vise le noyau 5.11+. Fonctionne sur x86_64, aarch64 et armv7l
(Raspberry Pi).

### Faut-il un adaptateur CAN pour essayer ?

Non. Lancez `bash scripts/dev/setup_vcan.sh` pour créer une interface virtuelle
(`vcan0`), puis utilisez le **[Simulateur](Features.md#simulator)** intégré pour
générer du trafic. Voir **[Prise en main](Getting-Started.md)**.

### Quels adaptateurs CAN sont pris en charge ?

Tout ce que le sous-système Linux SocketCAN reconnaît : CANable 2.0 / Pro et
Seeed USB-CAN (`gs_usb`), PEAK PCAN-USB (`peak_usb`), Kvaser (`kvaser_usb`),
Ixxat (`ixxat_usb2can` / `ems_usb`), et plus. Table complète dans
**[Prise en main](Getting-Started.md)**.

### L'application s'ouvre mais n'affiche aucune trame. Pourquoi ?

1. Vérifiez que l'interface est active : `ip link show can0` (ou `vcan0`).
2. Confirmez le trafic brut hors application : `candump can0` (de `can-utils`).
3. Vérifiez d'avoir sélectionné la bonne interface dans la barre d'outils ;
   cliquez **↺** pour rafraîchir.
4. Pour un bus virtuel, injectez une trame de test : `cansend vcan0 123#DEADBEEF`.

### « Permission denied » sur can0 ?

Utilisez **Tools → « Configure CAN permissions (one-time) »** dans l'app, ou
lancez `sudo usermod -aG dialout $USER` puis déconnexion/reconnexion.

### La compilation échoue — `Qt6Charts not found` / `lua5.4 not found` / `nlohmann_json not found`.

Installez le paquet dev correspondant : `qt6-charts-dev`, `liblua5.4-dev`,
`nlohmann-json3-dev`. Le plus simple est `bash scripts/dev/install_deps.sh`, qui
détecte votre distribution. CMake doit être en **3.28+**.

### Comment obtenir le binaire MCP / `socketspy-mcp` ?

Il se compile quand les paquets optionnels `libspdlog-dev` et
`libcpp-httplib-dev` sont présents à la configuration. Lancez-le ensuite en
autonome (`socketspy-mcp --stdio` / `--tcp <port>`) ou depuis le panneau **MCP**
de l'interface. Voir **[API & MCP](Api-MCP.md)**.

### Claude (ou une autre IA) peut-il piloter SocketSpy ?

Oui — c'est le rôle du serveur MCP. Pointez Claude Desktop vers `socketspy-mcp`
et il obtient 11 outils typés (`can_monitor`, `can_send`, `can_decode`, …). Tout
le trafic reste sur `127.0.0.1`. Voir **[API & MCP](Api-MCP.md)**.

### Quel est le lien avec ECU Studio ?

Les deux font partie de l'**[ECU Studio Suite](https://github.com/Poisson48/ecu_studio_suite)**.
ECU Studio reprogramme les ECU et édite les cartographies ; SocketSpy surveille le
bus en direct. La boucle phare : ECU Studio flashe une cartographie, puis
SocketSpy vérifie sur le bus CAN que le changement a pris effet au bon point de
fonctionnement — piloté via MCP. Voir la **[boucle flash → vérif](Api-MCP.md#français)**.

### Mon script Lua est-il sûr à exécuter ?

Oui. Le moteur est en **Lua 5.4 bac à sable** avec **watchdog** : l'accès aux
fonctions dangereuses est bloqué, les scripts emballés sont arrêtés, et un script
ne peut atteindre que le bus CAN et les fichiers locaux autorisés. Voir
**[Scripting](Scripting.md)**.

### Quels formats d'export sont supportés ?

**BLF v2** compatible Vector CANalyzer et **MDF4 v4.10** asammdf/MATLAB, via
**File → Export BLF… / Export MDF4…**. Les captures se chargent/sauvent aussi en
candump `.log` et `.csv`.

### Sous quelle licence est SocketSpy ?

**GPL-3.0.** Source : https://github.com/Poisson48/SocketSpy.

### Où signaler un bug ou demander une fonctionnalité ?

Ouvrez une issue sur https://github.com/Poisson48/SocketSpy/issues.
