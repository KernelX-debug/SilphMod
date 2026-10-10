![SilphMod — UGPhone Edition](docs/assets/banner.svg)

# SilphMod

**English · [Español](spanish.md)**

Local automation for the original Pokémon GO in a **rooted UGPhone instance running Android 10**. It catches nearby Pokémon, spins PokéStops, transfers new catches according to your filters, and keeps the item bag within your chosen limits.

Configure the features, press **Start service**, and follow the progress from the notification. **Stop** and **Export logs** let you end the session and review its results. The interface, notifications, and visual assets are in English.

**Edition:** `1.7.0-ugphone` · [MIT](LICENSE)

**Reference environment:** UGPhone Android 10, ARM64.

> **NOT YET TESTED ON OTHER ANDROID VERSIONS OR OUTSIDE UGPHONE**. Broader compatibility will be pursued in future updates.

<details>
<summary>Contents</summary>

- [How it works](#how-it-works)
- [Compatibility](#compatibility)
- [Features](#features)
  - [Transfers without touching the existing collection](#transfers-without-touching-the-existing-collection)
  - [Item bag: per-type limits, not indiscriminate deletion](#item-bag-per-type-limits-not-indiscriminate-deletion)
  - [Catch and spin limits](#catch-and-spin-limits)
- [Usage](#usage)
- [Architecture](#architecture)
  - [Startup and continuity](#startup-and-continuity)
  - [Automatic map recovery](#automatic-map-recovery)
  - [Captures and game calls](#captures-and-game-calls)
  - [Throw quality](#throw-quality-from-the-il2cpp-contract-to-the-throw)
  - [Berries](#berries-confirm-the-effect-before-throwing)
  - [Interruptions and map continuity](#interruptions-and-map-continuity)
  - [Transfers and discards](#transfers-and-discards)
  - [Location and Error 12](#location-and-error-12)
  - [XP and Stardust counters](#xp-and-stardust-counters)
- [Building](#building)
- [Logs and privacy](#logs-and-privacy)
- [Structure](#structure)
- [Status and upcoming improvements](#status-and-upcoming-improvements)

</details>

## How it works

![SilphMod in action — 1:13](docs/assets/demo.gif)

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

Compatibility refers to the **Android version of the instance**. Later Android versions, other ROMs, physical phones, and different game profiles require independent validation.

## Features

| Feature | Current behavior |
| --- | --- |
| Auto-catch | Selects the eligible target closest to the current position and checks its existence and distance again before throwing. Retries while the encounter remains valid; if it disappears or moves out of range, it proceeds to the next target. |
| Throw quality | Choose **Normal**, **Nice**, **Great**, or **Excellent**. Throws retain the curveball spin. |
| Berries | Uses the first available berry according to the configured selection and priority, confirms its application, and proceeds with the throw. Preserves the limits set in Item limits. |
| PokéStops | Sends spin requests for targets within the range configured in the engine and counts successful responses. |
| Session limits | Configurable catch and spin maximums, initially **4,800** and **2,400**, applied to accumulated statistics. Reaching either stops farming, closes Pokémon GO, and saves the logs. |
| Skip interruptions | Selectively handles medals, trainer level-ups, XP rewards, the Daily Adventure Incense recap, and egg hatches through the actions provided by their controllers. |
| Transfers | Only Pokémon caught by **this SilphMod session**, with a verified capture response and exact ID. |
| Keep shiny / 15/15/15 | Two independent filters. Meeting either enabled filter prevents that Pokémon from being transferred. |
| Keep Dynamax / hatched / background | New protection filters for catches authorized by the service. They do not scan or modify the existing collection. |
| Keep shiny + background | Protects Pokémon that meet both conditions. Combines with the other retention filters. |
| Item bag cleanup | Removes excess quantities of selected item types **from the start** and maintains those limits throughout the session. It does not need to wait until the bag is full. |
| Fix error 12 | Location treatment inside the game process, optional and tied to the session. |
| Statistics | Accumulated catches, flees, spins, transfers, discarded items, XP, Stardust, and active time. Preserved when stopping, restarting, and recovering the map. |
| Clear statistics | Resets the counters only when pressed with the service stopped. Located below the statistics. |
| Map recovery | After five minutes of valid readings with no Pokémon within **40 m**, restarts the game and reconnects the engine. Up to **10 startup attempts per recovery**, cancelable with Stop. |
| Notification shade | Catches and spins continue while it is expanded, provided Pokémon GO remains the underlying app and the screen is unlocked. |
| Logs | Manual export of up to three recent sessions, with a file-size and SHA-256 index. |

There is no built-in route planner or joystick: SilphMod works with a mock-location joystick app.

### Transfers without touching the existing collection

The transfer queue starts empty. A Pokémon enters it only when a capture request issued by this session receives a valid, successful response containing its storage ID. Finding an ID in the collection **is not enough** to authorize its transfer.

In addition to the selected filters, favorites, buddies, eggs, deployed Pokémon, fusions, special classes, and records whose protection information cannot be validated are preserved. Disabling **Keep shiny** or **Keep 15/15/15** allows those new catches to be transferred if no other protection applies.

Version **1.7.0** adds **Keep Dynamax**, **Keep hatched**, **Keep background**, and **Keep shiny + background**. The first three are enabled by default; the shiny + background combination is disabled. Meeting any enabled filter keeps the Pokémon. Within **Keep shiny + background**, however, both conditions must be met at the same time.

If you enable **Keep shiny** alongside **Keep shiny + background**, all shiny Pokémon are kept, with or without a background. If you enable **Keep background** alongside the combination, all Pokémon with a background are kept. To keep only shiny Pokémon with a background through these filters, leave only the combination enabled and disable the other optional filters. The engine still catches the other Pokémon and transfers them if no other protection applies.

Hatched protection is checked on the record authorized for transfer; it does not turn hatches into new authorizations or add Pokémon from the existing collection to the queue.

Transfers are disabled by default and **have no total per-session quota**. The engine receives `maxTransfers = 0`; old preferences do not restore a limit. Authorizations are not carried over between sessions, accounts, or restarts.

![Transfer authorization excerpt](docs/assets/session-transfer.png)

*This is an excerpt from [session-transfers.cjs](engine/src/session-transfers.cjs).*

### Item bag: per-type limits, not indiscriminate deletion

An enabled checkbox allows excess items of that type to be discarded; the number specifies how many to keep. An unchecked type is not discarded. For example:

```text
Potion=50,SuperPotion=50,Revive=30,NanabBerry=30
```

With 83 Potions and a minimum of 50, the plan allows 33 to be discarded. Each request discards at most 50 units, **with no total per-session quota**. The engine checks the response and inventory synchronization before continuing.

There are **33 selectable types**, including three common Poké Balls, four potions, two revives, five berries, and other permitted consumables. The catalog contains another **17 protected types**; Master Balls, Safari Balls, Premier Balls, passes, incubators, and unknown items are not authorized for discarding. You can review the IDs and restrictions in [ItemCatalog.java](app/src/main/java/dev/rootcatch/nativeengine/ItemCatalog.java) and [item-catalog.cjs](engine/src/item-catalog.cjs).

Cleanup is disabled by default. The engine receives `maxDiscardedItems = 0`, regardless of previous preferences. It can discard items already in the bag, according to the selected types and minimum quantities. An ambiguous response or a known rejection pauses cleanup without repeating the request; the other features can continue.

### Catch and spin limits

In **Engine settings → Session limits**, **Maximum catches** starts at **4800** and **Maximum PokéStop spins** at **2400**. These values can be changed. Both accept a positive maximum and retain the selected value for subsequent sessions.

The counters are preserved between service starts and automatic recoveries. Only successful responses to actions issued by SilphMod count toward the limits: failed attempts, flees, and manual catches do not. These are maximums applied to accumulated local statistics, not daily account counters.

If accumulated catches or spins have already reached their maximum, **Start service** is blocked and shows **Capture limit reached** or **Spin limit reached**. To continue, raise the limits above both totals or use **Clear statistics** with the service stopped. Stopping the farmer does not erase the results; clearing statistics does not erase the logs either.

When either maximum is reached, no further actions are sent. The agent removes its hooks, releases its references, and confirms that it has stopped; Android then verifies the process identity, closes Pokémon GO, and saves the logs with the reason and final results. **Export logs** lets you retrieve the ZIP. Closing the game does not clear its cache, storage, or configuration.

## Usage

Before starting, make sure you have a properly configured GPS joystick app.

1. Install the APK from the releases section, open it, and grant root and notification permissions if requested. Keep your joystick configured and running. Do not run other automation tools or injectors at the same time.
2. Select **Catches**, **Throw quality**, **Use berries**, **Berry priority**, **Spin PokéStops**, transfer filters, and **Item limits**. Adjust the catch and spin maximums in **Engine settings**. Transfers and discards have no separate total per-session maximum: they can discard an unlimited total quantity of the items you have configured for discarding while the session remains active.
3. Set a **walking route** in your GPS joystick app, or simply use that app's joystick to move around the map. A **walking route** is recommended for more effective automation over longer periods.
4. If your instance needs the location treatment (UGPHONE), enable **Fix error 12** and press **Start service**. Stay in SilphMod during preparation; opening the game may take a few seconds. The app may close only the game once and reopen it to install the treatment early. It does not clear the cache, storage, or joystick settings.

   If you press **Start service**, wait for the game to open, and it closes on the loading screen before reaching the map, this can happen and is not a reason to panic. Wait for the error logs to be saved, then press **Start service** again. Repeat as needed until the game opens and loads the map.

5. Wait for **Active**. The map, player, and references are checked automatically before any actions are sent.
6. Press **Stop service** or **Stop** in the notification when you want to end the session. Only if you want to save an error report, wait for **Saving logs…** to finish and use **Export logs**. The ZIP is saved to `Downloads/SilphMod/`.

The service has no 30/60-second timer or fixed hourly limit. To stop it, open the app's notification and press Stop, or close Pokémon GO and return to the mod's app.

Switching apps pauses actions; losing the game process, switching accounts, or an extended loss of the controller ends the session. Opening the notification shade lets you check statistics without pausing catches or spins, as long as the game remains underneath the panel and the screen is unlocked. Switching to another app or locking the screen pauses actions. (In UGPHONE, the game can keep running in the background if you close the instance, but not if you close Pokémon GO or SilphMod.)

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

The controller sends heartbeats every **500 ms** on a thread independent of focus queries and log writing. Only a positive focus reading no older than **1,500 ms** can authorize actions; the notification shade is allowed when the game remains the underlying activity. Authorization for new actions expires after **2,200 ms**; a brief delay can recover within the same connection, while a continuous loss of control exceeding **10 seconds** ends the session. Automatic recovery is reserved for the empty-map case described below; it does not indiscriminately restart after any failure.

### Automatic map recovery

The farmer observes the presence of nearby Pokémon every **five seconds**. If it accumulates **five continuous minutes of valid readings with no Pokémon within 40 m**, it stops the agent, confirms the release of references and the location treatment, closes only the verified Pokémon GO process, and saves the logs. It then opens the game and connects a new engine instance with the same configuration and account.

This feature stays active while farming and does not require enabling an option. A reading with Pokémon resets the timer, even if those targets are temporarily on hold. Incomplete samples, missing readings, switching apps, or locking the screen are not interpreted as an empty map. Running out of Poké Balls or filling storage is not enough to trigger it either.

Each recovery allows **up to ten startup attempts in total, counting the first**. If the engine reaches **Active**, the remaining attempts are not performed and farming continues. If the game closes during that startup or the connection does not finish activating, it can retry after confirming that the previous injector has exited and the game is closed. The waits between attempts increase from **5 to 30 seconds**; injectors never run simultaneously. After all ten attempts are exhausted, the service stops, preserves the statistics, and saves the logs. A later recovery, after farming resumes and another five minutes of empty-map readings accumulate, gets a new maximum of ten attempts.

**Stop service** and **Stop** cancel both farming and any pending retry. Statistics are not reset, and the limits continue to count the accumulated totals. The new connection validates the player again and does not restore the previous process's transfer queue.

### Captures and game calls

Reverse engineering was used to identify contracts, structures, and signatures for the supported profile. The engine resolves classes and services at runtime and validates what it will use; it does not distribute the game's library or decompiled metadata.

| Operation | Entry point used in the 0.431.0 profile |
| --- | --- |
| Resolve services | `Zenject.Internal.ZenUtilInternal.GetAllSceneContexts` and `DiContainer.TryResolve(System.Type)` |
| Read position | `Niantic.Holoholo.Avatar.IMapAvatar.get_Location`, `Niantic.LatLng` structure |
| Read targets | `IMapPlaceDirectoryService.get_AllPokemon` / `get_ActivePokestops` |
| Open encounter | `MapPokemon.SendEncounterRequest()` |
| Apply berry | `IItemBag.UseItemForEncounter(Item, MapPokemon)` and the `UseItemEncounterOutProto` response |
| Throw | `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto)` |
| Spin | `IRpcHandler.SendRpc<FortSearchProto, FortSearchOutProto>`; operation ID `101` |
| Recycle items | `IRpcHandler.SendRpc<RecycleItemProto, RecycleItemOutProto>`; ID `137` |
| Transfer | `IPokemonBag.ReleasePokemon(pokemon)`, followed by reconciliation by ID |

The engine's distance thresholds are **40 m for selecting catches** and **80 m for spins**; these are local thresholds, not a promise that the server will accept an action. The target is revalidated before each throw, which is important when the joystick is following a route.

The throw enables a hit and spin and applies the selected **Throw quality** profile. **Great** retains the previous value, `ReticleSize = 0.5`, and remains the default. Ball selection uses the first available ball in the order Poké Ball → Great Ball → Ultra Ball.

Only one action request can be pending at a time. **Action interval** allows **100–3000 ms**, initially **150 ms** in the interface. It separates requests; it does not mean one catch every 150 ms. Responses, checks, target availability, and reconciliation also take time.

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

### Throw quality: from the IL2CPP contract to the throw

The `Niantic.Holoholo.Encounter.PokeballThrow` contract distinguishes the grades **Normal = 1**, **Nice = 2**, **Great = 3**, and **Excellent = 4**. The `ReticleSize`, `HitBullseye`, `Missed`, and `Spinning` fields were identified, along with the signature that receives this structure in `MapPokemon.TryCapture`.

Quality is not applied by tapping the screen or writing a reward label. SilphMod prepares the throw structure used by the game, with a specific reticle size for each profile:

```js
const PROFILES = Object.freeze({
  NORMAL: Object.freeze({grade: 1, reticleSize: 1.1, hitBullseye: true}),
  NICE: Object.freeze({grade: 2, reticleSize: 0.85, hitBullseye: true}),
  GREAT: Object.freeze({grade: 3, reticleSize: 0.5, hitBullseye: true}),
  EXCELLENT: Object.freeze({grade: 4, reticleSize: 0.05, hitBullseye: true})
});
```

*[throw-profile.cjs](engine/src/throw-profile.cjs). `grade` identifies the profile in the traces; it is not written as a field in the game's structure.*

The engine creates `PokeballThrow`, applies those fields, and passes the structure to the capture call:

```ts
const data=klass('holo-game','Niantic.Holoholo.Encounter.PokeballThrow').method<Il2Cpp.ValueType>('MissedThrow',1).invoke(item);
const profile=throwProfile.profile(config.throwQuality);
data.field('Missed').value=false;data.field('HitBullseye').value=profile.hitBullseye;
data.field('Spinning').value=true;data.field('ReticleSize').value=profile.reticleSize;
const ar=proto('ARPlusEncounterValuesProto');
```

```ts
const promise=object(observed('TryCapture',()=>pokemon.method<Obj>('TryCapture',2).invoke(data,ar)),'MapPokemon.TryCapture');
```

*Two excerpts from [agent.ts](engine/src/agent.ts). Between them, active control and the encounter ID are checked, and the request and attempt are recorded.*

On **October 8, 2026**, the logs show **Excellent** requests, `ReticleSize = 0.05`, and curveballs, followed by capture receipts with `Exp = [100, 1000, 20, 50]`: **1,170 XP** awarded by the game. The session recorded **33 catches and 10 spins**; during one of the intervals with the notification shade open, **10 catches and 2 spins** were confirmed. The counter adds the rewards received, not a fixed amount associated with the selector.

### Berries: confirm the effect before throwing

`PokemonEncounterResponse.InitialActiveItem`, the two-argument `IItemBag.UseItemForEncounter` method, and the `Status` and `ActiveItem` fields of `UseItemEncounterOutProto` were identified. The agent validates those contracts and uses the game's service.

All five berries can be selected and prioritized:

| Berry | Contract identifier |
| --- | ---: |
| Razz Berry · `RazzBerry` | 701 |
| Nanab Berry · `NanabBerry` | 703 |
| Pinap Berry · `PinapBerry` | 705 |
| Golden Razz Berry · `GoldenRazzBerry` | 706 |
| Silver Pinap Berry · `GoldenPinapBerry` | 708 |

The default priority is **Razz → Pinap → Nanab**; the two special berries must be explicitly selected. The first selected berry with available units is used, rather than applying multiple berries in succession for the same active effect.

```ts
const name=berryCycle.choose(config.berryOrder,counts);
```

```ts
const berryItem=klass('holo-protos','Holoholo.Rpc.Item').field(name).value;
const id=pokemon.method<UInt64>('get_EncounterId',0).invoke().toString();
enterStage('capture:UseItemForEncounter');
berryCycle.request();
const promise=object(observed('UseItemForEncounter',()=>findService('Niantic.Holoholo.IItemBag')
  .method<Obj>('UseItemForEncounter',2).invoke(berryItem,pokemon)),'IItemBag.UseItemForEncounter');
emit('berry_requested',{id,name,item:berryPolicy.ITEMS[name],before:counts[name],attempt:throws+1});
reserve('berry',promise,id,[pokemon],{item:berryPolicy.ITEMS[name]});return true;
```

*Excerpts from [agent.ts](engine/src/agent.ts). Active-control authorization is checked between them.*

The engine retains the promise and waits for its response before throwing. For this contract, **Status = 0** means the berry was applied; it is not confused with the success code for a capture. The policy also checks the active item and only then increments the counter:

```ts
const status=number(result.field('Status').value), activeItem=number(result.field('ActiveItem').value);
const verdict=berryCycle.settle(status,activeItem,p.item);
if(verdict.consumed)stats.berriesUsed++;
```

*Code from [agent.ts](engine/src/agent.ts), with the decision in [berry-policy.cjs](engine/src/berry-policy.cjs).*

If a berry is already active, another is not applied. When the capture response indicates that the Pokémon broke out of the ball, the cycle allows another berry to be applied before the next attempt. A rejected berry does not trigger a chain of retries; the capture can continue without it. The target's existence and distance are checked again before acting.

No fixed wait is added for the berry animation: **Action interval** and a single pending request are preserved. Using berries adds the actual request and response, so it does take communication time. **Item limits** continues to discard the selected excess items; captures use the remaining units. XP still comes from the capture receipt, not from an invented bonus for applying a berry.

### Interruptions and map continuity

**Skip interruptions**, enabled by default, observes when supported screens finish opening. It preserves the original opening behavior and uses `Dismiss()`, `Click()`, or `LuckyBonusSkipButtonClicked()` according to the validated controller. For egg hatches, the normal sequence is started with `Click()` and its subsequent scenes are handled; responses from the service that processes eggs or rewards are not suppressed.

Actions run on the Unity thread, with the same session and focus control as farming. There is no continuous polling of every window: only a bounded queue is retained when a recognized screen appears. Screens outside that list are not closed automatically.

The `spin_scan` logs explain PokéStop selection, distance, cooldown, a full item bag, and the traversal limit of the loaded list. This observation reuses the existing scan and does not add game requests or change the radius or cooldowns.

### Transfers and discards

64-bit identifiers remain exact strings or `UInt64` values, never JavaScript numbers that could lose precision. Authorization combines an issued request, a recent successful response, the correct class, the captured ID, and the current session.

The item bag policy is independent of animations or whether the bag visually appears full:

```js
// Excerpt from engine/src/policy.cjs
const count = counts[name];
if (!Number.isInteger(count) || count <= keep) continue;
return {
  name, item: ITEMS[name], count: Math.min(count - keep, maxBatch),
  before: count, keep
};
```

A transfer does not increment the counter merely because it was requested: the Pokémon must also disappear from the inventory after the accepted response. Unknown results do not authorize retries of irreversible operations.

### Location and Error 12

The adapter uses the JNI ABI to identify `android.location.Location.isFromMockProvider()` and, if present, `isMock()`. It treats only the boolean responses corresponding to those methods inside **the process it connected to**. It does not modify coordinates, `Location` objects, AppOps, Google services, SELinux, or system modules.

This is a temporary, session-bound treatment, not a permanent device repair or a universal solution for every cause of Error 12. When the session stops and the hooks are removed, the error may return. This feature's implementation is in [engine/location](engine/location).

This option is intended to work on **Android 10 - SDK 29**. Higher or lower Android versions will not work with **Fix Error 12**. Work is underway to expand this feature.

### XP and Stardust counters

Rewards from successful **catches and spins issued by SilphMod** are counted: `CaptureScoreProto.Exp`, `Stardust`, temporary/mega evolution rewards, and `FortSearchOutProto.XpAwarded`, `Loot`, and `BonusLoot`, according to the validated contract. Totals are preserved when stopping or recovering the service and are reset only with **Clear statistics**.

The account balance is not subtracted, and rewards are not estimated by multiplying catches by a constant. Amounts are not multiplied again by bonuses the game may already have applied. An increasing sequence prevents duplicates; Android totals use `long`. `—` indicates an unavailable reading, and `+` a partial total with missing rewards.

## Building

Requirements: JDK **17 or later**, Android SDK **36**, Node.js **22 or later**, and Python **3.11 or later**. Android Studio can open the repository root. Configure `JAVA_HOME` and `ANDROID_HOME`, or create your own `local.properties` with `sdk.dir`; that file is not published.

```powershell
cd engine
npm.cmd ci
npm.cmd run typecheck
cd ..
python tools/prepare_native.py
.\gradlew.bat :app:assembleRelease
```

On Linux/macOS, use `npm` and `bash ./gradlew` instead of `npm.cmd` and `gradlew.bat`. Native preparation downloads the official ARM64 **Frida 17.22.1** executable, verifies the compressed archive, binary, size, and ELF format, and does not run it on the computer. It also accepts `--archive /path/to/official-archive.xz` for offline preparation.

Gradle rebuilds the agent from source and generates its expected SHA-256 during the build. You do not need to edit a constant manually or copy files from another project. The native binary is verified again before building.

Without a signing configuration, the release output is **unsigned**. To sign it, provide `SILPH_KEYSTORE`, `SILPH_STORE_PASSWORD`, `SILPH_KEY_ALIAS`, and `SILPH_KEY_PASSWORD` through the environment. Keys remain outside the repository; updating an existing installation requires the same certificate.

Android output: `app/build/outputs/apk/release/`. The APK, `node_modules`, caches, logs, keys, and downloaded runtime are excluded from Git.

## Logs and privacy

Export is local and manual. It includes final state, configuration, engine events, health, startup, accessible Android sources, logcat, and crash reports, with time/size limits and rotation. It does not collect all memory or preserve every line of a session indefinitely. Old sessions are not deleted automatically: check the available storage.

The ZIP files **do not add redaction** and may include coordinates, identifiers, and messages from other processes. A coordinate coming from a joystick does not make the other data public. Review reports before sharing them. This repository includes only selected aggregate statistics for documentation.

## Structure

```text
SilphMod/
├── app/                  # A single Android application
├── engine/
│   ├── src/              # Engine, policies, and session control
│   └── location/         # JNI adapter
├── docs/
│   └── assets/           # Identity, architecture, and code excerpt
├── tools/                # Native preparation
├── gradle/               # Build wrapper
├── LICENSE
├── NOTICE
└── README.md
```

## Status and upcoming improvements

**1.6.0 introduces selectable throw quality**, prioritized berry use, selective interruption handling, session catch and spin limits, continuity while the notification shade is expanded, and an action interval of **100–3000 ms**, initially **150 ms**. Transfers and discards retain their own unlimited total quotas; the entire session ends when either the catch or spin maximum is reached.

**1.7.0 adds automatic map recovery with up to ten startup attempts**, persistent statistics with manual clearing, startup blocking when accumulated limits have been reached, and retention filters for Dynamax, hatched Pokémon, backgrounds, and the shiny + background combination.

It is worth noting that the **engine**, not SilphMod itself, was tested on multiple Android versions with positive results. This does not mean it works correctly on every device: compatibility issues and native `SIGSEGV / SEGV_ACCERR` errors with calls in `libNianticLabsPlugin.so` still exist and will continue to be investigated.

The next phase is to measure response and reconciliation times, focus/control latency, and logging overhead, as well as expand the device matrix.

A configurable Poké Ball priority order is planned, rather than the default: 1. Poké Ball; 2. Great Ball; 3. Ultra Ball.

Another pending improvement is to continue expanding the catalog of Pokémon configured to be kept, for example with nundo IVs (0/0/0).

SilphMod does not remove server restrictions, cooldowns, or account risks. Transfers and discards are irreversible. Automation may violate the game's terms; use the project with an understanding of those limits. Using it on accounts you consider valuable is not recommended.

[MIT](LICENSE) license. Dependencies retain their notices in [NOTICE](NOTICE) and their licenses within the assets. Independent project, with no affiliation with the game's rights holders.
