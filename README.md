![SilphMod — UGPhone Edition](docs/assets/banner.svg)

# SilphMod

[Español](spanish.md)

Local automation for the original Pokémon GO app on a **rooted UGPhone instance running Android 10**. It catches nearby Pokémon, spins PokéStops, transfers new catches according to your filters, and keeps your item bag within the limits you choose.

Configure the features, press **Start service**, and follow the progress through the notification. **Stop** and **Export logs** let you end the session and review its results. The interface, notifications, and visual assets are in English.

**Edition:** `1.5.0-ugphone` · **License for original code:** [MIT](LICENSE) · **Reference environment:** UGPhone Android 10, ARM64. **THIS EDITION HAS NOT YET BEEN TESTED ON OTHER ANDROID VERSIONS OR OUTSIDE UGPHONE**. Broader compatibility is planned.

[Features](#features) · [Usage](#usage) · [Architecture](#architecture) · [Building](#building) · [Status and upcoming improvements](#status-and-upcoming-improvements)

## How it works

[![Watch SilphMod in action — 1:13](docs/assets/demo-preview.png)](docs/assets/test%40KernelX-debug_01.mp4?raw=true)

## Compatibility

| Component | Profile for this edition |
| --- | --- |
| Observed environment | UGPhone instance running Android **10 / SDK 29**, with `su` permission |
| Architecture | **ARM64**, 64-bit pointers |
| Game | Original Pokémon GO **0.431.0**, `versionCode 2026092402` |
| Game engine | Unity **6000.3.21f1**, IL2CPP |
| APK | `minSdk 29`, `targetSdk 36` |
| Optional Error 12 treatment | Limited to **SDK 29** in this edition |
| Installed package identity | `dev.rootcatch.ugphonetest`, retained to update previous installations |

Compatibility refers to the **Android version of the instance**; the reports do not identify the UGPhone client version. Later Android versions, other ROMs, physical phones, and different game profiles require independent validation.

## Features

| Feature | Current behavior |
| --- | --- |
| Auto-catch | Selects the eligible target closest to the current position and checks its existence and distance again before throwing. |
| PokéStops | Sends spin requests for targets within the range configured in the engine and counts successful responses. |
| Transfers | Only Pokémon caught by **this SilphMod session**, with a verified capture response and exact ID. |
| Keep shiny / 15/15/15 | Two independent filters. Meeting either enabled filter prevents that Pokémon from being transferred. |
| Item bag cleanup | Removes excess quantities of selected item types **from the start** and maintains those limits throughout the session. It does not need to wait until the bag is full. |
| Fix error 12 | Location treatment inside the game process, optional and tied to the session. |
| Statistics | Catches, flees, spins, transfers, discarded items, XP, Stardust, and time. |
| Logs | Manual export of up to three recent sessions, with a file-size and SHA-256 index. |

There is no built-in route planner or joystick: SilphMod works with the position the game receives. It also does not include worldwide maps, spawn databases, or a shiny prediction service.

### Transfers without touching your existing collection

The transfer queue starts empty. A Pokémon enters it only when a capture request issued by this session receives a valid, successful response containing its storage ID. Finding an ID in the collection **is not enough** to authorize its transfer.

In addition to the selected filters, favorites, buddies, eggs, deployed Pokémon, fusions, special classes, and records whose protection information cannot be validated are preserved. Disabling **Keep shiny** or **Keep 15/15/15** allows those new catches to be transferred if no other protection applies.

Transfers are disabled by default and **have no total per-session quota**. The engine receives `maxTransfers = 0`; old preferences do not restore a limit. Authorizations are not carried over between sessions, accounts, or restarts.

![Actual transfer authorization code](docs/assets/session-transfer.png)

*This is an excerpt from [session-transfers.cjs](engine/src/session-transfers.cjs), generated from the repository's code.*

### Item bag: per-type limits, not indiscriminate deletion

An enabled checkbox allows excess items of that type to be discarded; the number specifies how many to keep. An unchecked type is not discarded. For example:

```text
Potion=50,SuperPotion=50,Revive=30,NanabBerry=30
```

With 83 Potions and a minimum of 50, the plan allows 33 to be discarded. Each request discards at most 50 units, **with no total per-session quota**. The engine checks the response and inventory synchronization before continuing.

There are **33 selectable types**, including three common Poké Balls, four potions, two revives, five berries, and other permitted consumables. The catalog contains another **17 protected types**; Master Balls, Safari Balls, Premier Balls, passes, incubators, and unknown items are not authorized for discarding. You can review the IDs and restrictions in [ItemCatalog.java](app/src/main/java/dev/rootcatch/nativeengine/ItemCatalog.java) and [item-catalog.cjs](engine/src/item-catalog.cjs).

Cleanup is disabled by default. The engine receives `maxDiscardedItems = 0`, regardless of previous preferences. It can discard items already in the bag, according to the selected types and minimum quantities. An ambiguous response or a known rejection pauses cleanup without repeating the request; the other features can continue.

## Usage

Before starting, make sure you have a properly configured GPS joystick app.

1. Install the APK from the releases section, open it, and grant root and notification permissions if requested. Keep your joystick configured and running. Do not run other automation tools or injectors at the same time.
2. Select **Catches**, **Spin PokéStops**, transfer filters, and **Items to keep**. Transfers and discards have no total per-session maximum: there is no limit on the total number of items you have configured for discarding.
3. Set a **walking route** in your GPS joystick app, or simply use that app's joystick to move around the map. A **walking route** is recommended for more effective automation over longer periods.
4. If your instance needs the location treatment (UGPHONE), enable **Fix error 12** and press **Start service**. Stay in SilphMod during preparation; opening the game may take a few seconds. The app may close only the game once and reopen it to install the treatment early. It does not clear the cache, storage, or joystick settings.

   If you press **Start service** and the game process does not open, this can happen and is not a reason to panic. Wait for the error logs to be saved, then press **Start service** again. Repeat as needed until the game opens and loads the map.

5. Wait for **Active**. The map, player, and references are checked automatically before any actions are sent. With the location checkbox disabled, open the game yourself and wait for the map before starting the service.
6. Press **Stop service** or **Stop** in the notification. If you want to save an error report, wait for **Saving logs…** to finish and use **Export logs**. The ZIP is saved to `Downloads/SilphMod/`.

The service has no 30/60-second timer or fixed hourly limit. To stop it, open the app's notification and press Stop, or close Pokémon GO and return to the mod's app.

Switching apps pauses actions; losing the game process, switching accounts, or an extended loss of the controller ends the session. Opening the notification shade may pause actions if Android gives it focus: **this edition does not yet distinguish that panel from other instances of focus loss**.

If Pokémon storage fills up, catches are paused and the status is reported; SilphMod does not delete your existing collection to make room. When the item bag is full and there are no authorized excess items, it does not discard other items on its own. Stopping cannot undo an operation that has already been sent.

## Architecture

![SilphMod architecture](docs/assets/architecture.svg)

SilphMod has two parts that cooperate through a local pipe:

- **Android / Java:** interface, configuration, foreground service, root permission, process identity, focus, control, notification, and export.
- **Agent / TypeScript and CommonJS:** IL2CPP service resolution, target selection, game requests, response reading, filters, and reference lifecycle.

The agent uses objects and methods from the original process; the game client still serializes its requests and communicates with its server. SilphMod has no `INTERNET` permission or its own spawn API, but **the game needs a connection**: this is not an offline mode.

### Startup and continuity

When the location option is enabled, the agent prepares the JNI adapter first and then loads the Java/IL2CPP bridges, after a minimum of eight seconds and once the required libraries are present. That does not establish that the map is ready: the scene, account, signatures, 64-bit GC references, and stable samples are still validated over approximately ten seconds.

Activation uses a single-use authorization tied to the process, session, and player. There is no disconnect/reinjection between validation and farming. Actions run on the Unity thread; retained objects are released when the session ends.

The controller sends heartbeats every **500 ms** on a thread independent of focus queries and log writing. Only a positive focus reading no older than **1,500 ms** can authorize actions. Authorization for new actions expires after **2,200 ms**; a brief delay can recover within the same connection, while a continuous loss of control exceeding **10 seconds** ends the session. **The game is not automatically restarted after a failure.**

### Captures and game calls

Reverse engineering was used to identify contracts, structures, and signatures for the supported profile. The engine resolves classes and services at runtime and validates what it will use; it does not distribute the game's library or decompiled metadata.

| Operation | Entry point used in the 0.431.0 profile |
| --- | --- |
| Resolve services | `Zenject.Internal.ZenUtilInternal.GetAllSceneContexts` and `DiContainer.TryResolve(System.Type)` |
| Read position | `Niantic.Holoholo.Avatar.IMapAvatar.get_Location`, `Niantic.LatLng` structure |
| Read targets | `IMapPlaceDirectoryService.get_AllPokemon` / `get_ActivePokestops` |
| Open encounter | `MapPokemon.SendEncounterRequest()` |
| Throw | `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto)` |
| Spin | `IRpcHandler.SendRpc<FortSearchProto, FortSearchOutProto>`; operation ID `101` |
| Recycle items | `IRpcHandler.SendRpc<RecycleItemProto, RecycleItemOutProto>`; ID `137` |
| Transfer | `IPokemonBag.ReleasePokemon(pokemon)`, followed by reconciliation by ID |

The engine's distance thresholds are **40 m for selecting catches** and **80 m for spins**; these are local thresholds, not a promise that the server will accept an action. The target is revalidated before each throw, which is important when the joystick is following a route.

The current throw enables a hit and spin, with `ReticleSize = 0.5`. **There is not yet an Excellent/Great throw selector**, and no specific reward classification is guaranteed. Ball selection uses the first available ball in the order Poké Ball → Great Ball → Ultra Ball.

Future updates will introduce expanded features and throw options. Catches always use curveball throws.

Only one action request can be pending at a time. The **250–3000 ms** interval, initially **300 ms** in the interface, separates requests; it does not mean one catch every 300 ms. Responses, checks, target availability, and reconciliation also take time.

```ts
// Engine excerpt: open the encounter and retain its promise.
reserve(
  'encounter',
  object(pokemon.method<Obj>('SendEncounterRequest', 0).invoke()),
  nearestId,
  [pokemon]
);
```

*Simplified excerpt from [agent.ts](engine/src/agent.ts): the full file includes checks, traces, and error handling around the call.*

### Transfers and discards: confirm before counting

64-bit identifiers remain exact strings or `UInt64` values, never JavaScript numbers that could lose precision. Authorization combines an issued request, a recent successful response, the correct class, the captured ID, and the current session.

The item bag policy is independent of animations or whether the bag visually appears full:

```js
// Actual excerpt from engine/src/policy.cjs
const count = counts[name];
if (!Number.isInteger(count) || count <= keep) continue;
return {
  name, item: ITEMS[name], count: Math.min(count - keep, maxBatch),
  before: count, keep
};
```

A transfer does not increment the counter merely because it was requested: the Pokémon must also disappear from the inventory after the accepted response. Unknown results do not authorize retries of irreversible operations.

### Location and Error 12: the checkbox's actual scope

The adapter uses the JNI ABI to identify `android.location.Location.isFromMockProvider()` and, if present, `isMock()`. It treats only the boolean responses corresponding to those methods inside **the process it connected to**. It does not modify coordinates, `Location` objects, AppOps, Google services, SELinux, or system modules.

This is a temporary, session-bound treatment, not a permanent device repair or a universal solution for every cause of Error 12. When the session stops and the hooks are removed, the error may return. This feature's implementation is in [engine/location](engine/location).

### XP and Stardust

Rewards from successful **catches and spins in this session** are counted: `CaptureScoreProto.Exp`, `Stardust`, temporary/mega evolution rewards, and `FortSearchOutProto.XpAwarded`, `Loot`, and `BonusLoot`, according to the validated contract.

The account balance is not subtracted, and rewards are not estimated by multiplying catches by a constant. Amounts are not multiplied again by bonuses the game may already have applied. An increasing sequence prevents duplicates; Android totals use `long`. `—` indicates an unavailable reading, and `+` a partial total with missing rewards.

## Current operational evidence

The report reviewed on **October 7, 2026** documents a **1.3.1-ugphone** session with approximately **2 h 6 min 31 s of activity**, calculated using the controller's monotonic clock up to its stop request.

| Confirmed result | Total |
| --- | ---: |
| Catches | 1,365 |
| Flees | 34 |
| Spins | 532 |
| Transfers | 1,362 |
| Item units discarded | 634 |
| XP | 416,800 |
| Stardust | 360,650 |
| Recorded unreadable reward receipts | 0 |

File sizes and SHA-256 hashes for **60 indexed files** were verified, with no discrepancies. The session ended because **the game process identity changed**. The logs are rotated and do not retain every historical event.

## Building

Requirements: JDK **17 or later**, Android SDK **36**, Node.js **22 or later**, and Python **3.11 or later**. Android Studio can open the repository root. Configure `JAVA_HOME` and `ANDROID_HOME`, or create your own `local.properties` with `sdk.dir`; that file is not published.

```powershell
cd engine
npm.cmd ci
npm.cmd run typecheck
npm.cmd test
cd ..
python tools/prepare_native.py
.\gradlew.bat :app:testDebugUnitTest :app:lintRelease :app:assembleRelease
```

On Linux/macOS, use `npm` and `bash ./gradlew` instead of `npm.cmd` and `gradlew.bat`. Native preparation downloads the official ARM64 **Frida 17.22.1** executable, verifies the compressed archive, binary, size, and ELF format, and does not run it on the computer. It also accepts `--archive /path/to/official-archive.xz` for offline preparation.

Gradle rebuilds the agent from source and generates its expected SHA-256 during the build. You do not need to edit a constant manually or copy files from another project. The native binary is verified again before building.

Without a signing configuration, the release output is **unsigned**. To sign it, provide `SILPH_KEYSTORE`, `SILPH_STORE_PASSWORD`, `SILPH_KEY_ALIAS`, and `SILPH_KEY_PASSWORD` through the environment. Keys remain outside the repository; updating an existing installation requires the same certificate.

Android output: `app/build/outputs/apk/release/`. The APK, `node_modules`, caches, logs, keys, and downloaded runtime are excluded from Git.

### Checks

Node tests exercise policies and contracts with synthetic objects, as well as the actual bundled agent with simulated bridges. JVM tests cover configuration, control, focus, export, and startup. **They do not replace native execution on Android.** Bash syntax tests are explicitly skipped if Bash is unavailable.

The audit for this version is in [build-1.5.0.json](docs/validation/build-1.5.0.json). The changes focus on translation and presentation; capture calls, filters, reconciliation, and control deadlines are preserved.

Local validation: **252 Node tests passed**, **123 JVM tests passed and 2 were skipped** due to Windows symbolic-link restrictions, a successful TypeScript check, and **0 lint issues**. Agent integrity and the APK signature are also verified. Testing this version on UGPhone is still pending.

To audit a signed APK against its assets, permissions, signature, JVM tests, and lint:

```powershell
python tools/verify_apk.py app/build/outputs/apk/release/app-release.apk
```

To prepare a source folder and ZIP without build files, use `tools/package_source.py --directory ../SilphMod-public --zip ../SilphMod-source.zip`. It refuses to overwrite existing distributions and excludes native binaries, keys, installed dependencies, and logs. Visual assets are included; the code image can be regenerated with `tools/render_code.py --font /path/to/a/monospaced-font.ttf` and Pillow.

## Logs and privacy

Export is local and manual. It includes final state, configuration, engine events, health, startup, accessible Android sources, logcat, and crash reports, with time/size limits and rotation. It does not collect all memory or preserve every line of a session indefinitely. Old sessions are not deleted automatically: check the available storage.

The ZIP files **do not add redaction** and may include coordinates, identifiers, and messages from other processes. A coordinate coming from a joystick does not make the other data public. Review reports before sharing them. This repository includes only selected aggregate statistics for documentation.

## Structure

```text
SilphMod/
├── app/                  # A single Android application
├── engine/
│   ├── src/              # Engine, policies, and session control
│   ├── location/         # JNI adapter
│   ├── test/             # Engine tests
│   └── location-test/    # Adapter tests
├── docs/
│   ├── assets/           # Identity, architecture, and code excerpt
│   └── validation/       # Aggregate evidence and build audit
├── tools/                # Native preparation and source distribution
├── gradle/               # Build wrapper
├── LICENSE
├── NOTICE
└── README.md
```

## Status and upcoming improvements

**1.5.0 includes the interface and visual assets** and removes total transfer and discard quotas. It retains the engine from the previous standalone distribution.

It is worth noting that the **engine**, not SilphMod itself, was tested on multiple Android versions with positive results. This does not mean it works correctly on every device: compatibility issues and native `SIGSEGV / SEGV_ACCERR` errors with calls in `libNianticLabsPlugin.so` still exist and will continue to be investigated.

The next phase is to measure response and reconciliation times, focus/control latency, and logging overhead. Distinguishing the notification shade from other apps and expanding the device matrix also remain pending.

A better interface for selecting items to discard is planned, along with options to choose throw quality and the ball selection priority.

Another pending improvement is to expand the catalog of Pokémon configured to be kept, identifying them by background, Dynamax status, nundo IVs (0/0/0), and other attributes.

SilphMod does not remove server restrictions, cooldowns, or account risks. Transfers and discards are irreversible. Automation may violate the game's terms; use the project with an understanding of those limits. Using it on accounts you consider valuable is not recommended.

[MIT](LICENSE) license. Dependencies retain their notices in [NOTICE](NOTICE) and their licenses within the assets. Independent project, with no affiliation with the game's rights holders.
