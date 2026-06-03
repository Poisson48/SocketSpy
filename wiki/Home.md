# SocketSpy Wiki

> **Language / Langue:** [English](#english) · [Français](#français)

---

<a name="english"></a>
## English

**SocketSpy** is a 100% local Linux CAN bus analysis platform built on SocketCAN.
No telemetry, no network calls, no cloud — everything runs on your machine.
It is part of the **[ECU Studio Suite](https://github.com/Poisson48/ecu_studio_suite)**,
a Qt6 automotive software suite. SocketSpy handles **live CAN monitoring and
decoding**, while its sibling **ECU Studio** handles **ECU reprogramming and map
editing**. Together they close the loop: ECU Studio flashes a map, SocketSpy
verifies on the live bus that the change took effect.

- **Repository:** https://github.com/Poisson48/SocketSpy
- **Website / docs:** https://poisson48.github.io/SocketSpy/
- **License:** GPL-3.0
- **Current version:** v0.8.7
- **Platform:** Linux x86_64 / aarch64 / armv7l, kernel ≥ 5.11 with SocketCAN

### What can it do?

| Area | Highlights |
|------|-----------|
| **Live monitor** | Real-time scrolling table, 2000-row buffer, search by ID, pause/clear, copy-to-clipboard |
| **Decode** | Load a `.dbc` file; signals decoded inline with engineering units |
| **Graphs** | Up to 8 live traces on a rolling 10-second QChart window |
| **Transmit** | Send standard / extended / FD frames (DLC 0–15, up to 64 bytes) |
| **Protocols** | CANopen, J1939, ISO-TP, UDS, OBD-II, NMEA 2000 decoders |
| **Lua scripting** | Sandboxed Lua 5.4 engine with in-app editor, Run/Stop and console |
| **Simulator** | Generate CAN traffic without hardware; built-in vehicle profiles |
| **Fuzzer** | Random / incremental / bit-flip frame injection |
| **UDS** | ISO 14229 tester + a full ISO-TP + UDS ECU simulator |
| **Export** | Vector-compatible BLF v2 and asammdf/MATLAB MDF4 v4.10 |
| **Capture diff** | Diff two captures by ID; byte-level delta |
| **Multi-bus** | Run two SocketCAN interfaces in parallel |
| **Signal Detective** | Auto-classify CAN IDs by rate/type; wiggle test |
| **MCP server** | JSON-RPC 2.0 over stdio / TCP (127.0.0.1) for AI / automation |

### Navigation

| Page | What's inside |
|------|--------------|
| **[Getting Started](Getting-Started.md)** | Build from source, virtual CAN setup, real hardware adapters |
| **[Features](Features.md)** | Every panel explained, with maturity (Proven / Beta / Incoming) |
| **[API & MCP](Api-MCP.md)** | The JSON-RPC / MCP server, its tools, the local transport, and the ECU Studio live-verify interconnection |
| **[Scripting](Scripting.md)** | The Lua 5.4 engine, the `can` / `log` / `time` / `bit` / `hex` APIs |
| **[FAQ](FAQ.md)** | Common questions, permissions, troubleshooting |

### Maturity legend

Throughout this wiki, features are tagged:

- **Proven** — verified, shipped, stable.
- **Beta** — works but not yet broadly proven (new panels, the flash→verify loop).
- **Incoming** — on the roadmap, not yet shipped.

---

<a name="français"></a>
## Français

**SocketSpy** est une plateforme 100% locale d'analyse du bus CAN sous Linux,
bâtie sur SocketCAN. Aucune télémétrie, aucun appel réseau, aucun cloud — tout
s'exécute sur votre machine. Elle fait partie de l'**[ECU Studio Suite](https://github.com/Poisson48/ecu_studio_suite)**,
une suite logicielle automobile en Qt6. SocketSpy gère la **surveillance et le
décodage CAN en direct**, tandis que son application sœur **ECU Studio** gère la
**reprogrammation d'ECU et l'édition de cartographies**. Ensemble, elles bouclent
le cycle : ECU Studio flashe une cartographie, SocketSpy vérifie sur le bus en
direct que le changement a bien pris effet.

- **Dépôt :** https://github.com/Poisson48/SocketSpy
- **Site / docs :** https://poisson48.github.io/SocketSpy/
- **Licence :** GPL-3.0
- **Version actuelle :** v0.8.7
- **Plateforme :** Linux x86_64 / aarch64 / armv7l, noyau ≥ 5.11 avec SocketCAN

### Que fait-il ?

| Domaine | Points clés |
|---------|-------------|
| **Monitor en direct** | Table défilante temps réel, buffer de 2000 lignes, recherche par ID, pause/effacement, copier-coller |
| **Décodage** | Chargez un fichier `.dbc` ; signaux décodés en ligne avec unités physiques |
| **Graphes** | Jusqu'à 8 courbes en direct sur une fenêtre QChart glissante de 10 s |
| **Émission** | Envoi de trames standard / étendues / FD (DLC 0–15, jusqu'à 64 octets) |
| **Protocoles** | Décodeurs CANopen, J1939, ISO-TP, UDS, OBD-II, NMEA 2000 |
| **Scripting Lua** | Moteur Lua 5.4 en bac à sable, éditeur intégré, Run/Stop et console |
| **Simulateur** | Génère du trafic CAN sans matériel ; profils véhicules intégrés |
| **Fuzzer** | Injection de trames aléatoires / incrémentales / bit-flip |
| **UDS** | Testeur ISO 14229 + simulateur d'ECU UDS complet (ISO-TP + UDS) |
| **Export** | BLF v2 compatible Vector et MDF4 v4.10 asammdf/MATLAB |
| **Diff de captures** | Compare deux captures par ID ; delta au niveau octet |
| **Multi-bus** | Exécute deux interfaces SocketCAN en parallèle |
| **Signal Detective** | Auto-classe les IDs CAN par cadence/type ; test « wiggle » |
| **Serveur MCP** | JSON-RPC 2.0 via stdio / TCP (127.0.0.1) pour l'IA / l'automatisation |

### Navigation

| Page | Contenu |
|------|---------|
| **[Prise en main](Getting-Started.md)** | Compilation, configuration CAN virtuel, adaptateurs matériels |
| **[Fonctionnalités](Features.md)** | Chaque panneau expliqué, avec maturité (Éprouvé / Bêta / À venir) |
| **[API & MCP](Api-MCP.md)** | Le serveur JSON-RPC / MCP, ses outils, le transport local et l'interconnexion de vérification en direct avec ECU Studio |
| **[Scripting](Scripting.md)** | Le moteur Lua 5.4, les API `can` / `log` / `time` / `bit` / `hex` |
| **[FAQ](FAQ.md)** | Questions fréquentes, permissions, dépannage |

### Légende de maturité

Dans ce wiki, les fonctionnalités sont étiquetées :

- **Éprouvé** — vérifié, livré, stable.
- **Bêta** — fonctionnel mais pas encore largement éprouvé (nouveaux panneaux, boucle flash→vérif).
- **À venir** — dans la feuille de route, pas encore livré.

---

### Publishing this wiki to GitHub / Publier ce wiki sur GitHub

**EN —** These pages live in the `wiki/` directory of the worktree. GitHub serves
wikis from a separate `*.wiki.git` repository, so they are published like this:

```bash
# Clone the wiki repo (create the first page once via the GitHub UI to initialise it)
git clone https://github.com/Poisson48/SocketSpy.wiki.git
cp wiki/*.md SocketSpy.wiki/
cd SocketSpy.wiki
git add . && git commit -m "Publish bilingual wiki" && git push
```

GitHub uses `Home.md` as the wiki landing page and links pages by filename
(`[Features](Features)` — no `.md` needed on the live wiki, but the `.md` links
used here also resolve correctly).

**FR —** Ces pages se trouvent dans le répertoire `wiki/` du worktree. GitHub
sert les wikis depuis un dépôt distinct `*.wiki.git`, donc on les publie ainsi :

```bash
# Cloner le dépôt wiki (créez d'abord une page via l'UI GitHub pour l'initialiser)
git clone https://github.com/Poisson48/SocketSpy.wiki.git
cp wiki/*.md SocketSpy.wiki/
cd SocketSpy.wiki
git add . && git commit -m "Publication du wiki bilingue" && git push
```

GitHub utilise `Home.md` comme page d'accueil et relie les pages par nom de
fichier (`[Features](Features)` — l'extension `.md` est optionnelle sur le wiki
en ligne, mais les liens `.md` utilisés ici fonctionnent aussi).
