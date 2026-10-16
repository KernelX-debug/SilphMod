![SilphMod — Root Android](docs/assets/banner.svg)

# SilphMod

**English · [Español](spanish.md)**

Local automation for the original Pokémon GO on **rooted Android**, both in UGPhone and on physical ARM64 phones. It catches nearby Pokémon, spins PokéStops, collects field research rewards, transfers new catches according to your filters, and keeps your item bag within the limits you choose.

Configure the features, press **Start service**, and follow progress from the notification. **Stop** and **Export logs** let you end the session and review its results. The interface, notifications, and visual assets are in English.

**Version:** `1.9.3` · [MIT](LICENSE)

**Reference environments:** UGPhone Android 10, Motorola Edge 50 Fusion with Android 15, and Redmi Note 14 Pro+ 5G with Android 16; all ARM64 with root.

<details>
<summary>Contents</summary>

- [How it works](#how-it-works)
- [Compatibility](#compatibility)
- [Features](#features)
  - [Only catch: choose which Pokémon to catch](#only-catch-choose-which-pokémon-to-catch)
  - [Transfer without touching your existing collection](#transfer-without-touching-your-existing-collection)
  - [Item bag: per-type limits, not indiscriminate deletion](#item-bag-per-type-limits-not-indiscriminate-deletion)
  - [Catch and spin limits](#catch-and-spin-limits)
- [Usage](#usage)
- [Architecture](#architecture)
  - [Startup and continuity](#startup-and-continuity)
  - [Automatic map recovery](#automatic-map-recovery)
  - [Catches and game calls](#catches-and-game-calls)
  - [Range and catch priority](#range-and-catch-priority)
  - [Throw quality](#throw-quality)
  - [Berries](#berries-confirm-the-effect-before-throwing)
  - [Interruptions and map continuity](#interruptions-and-map-continuity)
  - [Field research](#field-research)
  - [Remove buddy](#remove-buddy)
  - [Inventory tools](#inventory-tools)
  - [Game tools: gifts and a temporary name](#game-tools-gifts-and-a-temporary-name)
  - [Transfers and deletion](#transfers-and-deletion)
  - [Location and Error 12](#location-and-error-12)
  - [XP and Stardust counters](#xp-and-stardust-counters)
- [Building](#building)
- [Logs and privacy](#logs-and-privacy)
- [Structure](#structure)
- [Status and upcoming improvements](#status-and-upcoming-improvements)

</details>

## How it works

![SilphMod in action — 1:01](docs/assets/demo.gif)

## Compatibility

| Component | Profile for this version |
| --- | --- |
| System | Android **10 or later** with `su` permission |
| Reference environments | UGPhone Android **10 / SDK 29**; physical phones with Android **15 / SDK 35** and **16 / SDK 36** |
| Architecture | **ARM64**, 64-bit pointers |
| Game | Original Pokémon GO **0.433.1**, `versionCode 2026101502` |
| Game engine | Unity **6000.3.22f1**, IL2CPP |
| APK | `minSdk 29`, `targetSdk 36` |
| Optional Error 12 treatment | JNI `Location` contracts resolved according to the system version |
| Connection | Adaptive injector with automatic startup and runtime selection based on the SDK and environment capabilities, within a single APK |
| Installed identity | `dev.rootcatch.ugphonetest`, retained to update earlier installations |

The application keeps a single interface and a single Farmer engine. Startup adapts to the **Android version of the device or instance**, without requiring the user to choose a different APK for each environment.

## Features

| Feature | Current behavior |
| --- | --- |
| Autocatch | Selects the nearest eligible target within the range read from the game, up to **50 m**, and checks its existence and distance again before throwing. Retries while the encounter remains valid; if it disappears or moves out of range, proceeds to the next target. |
| Only catch | Select one or more species with thumbnails, search, and Pokédex ordering. With no selection, catches all eligible species. Field research reward encounters are caught regardless of this filter. |
| Throw quality | Choose **Normal**, **Nice**, **Great**, or **Excellent**. Throws retain the curveball spin. |
| Berries | Uses the first available berry according to the configured selection and priority, confirms its application, and continues with the throw. Retains Item limits. |
| PokéStops | Sends spins for targets within the range configured in the engine and counts successful responses. |
| Session limits | Configurable maximum catches and spins, initially **4,800** and **2,400**, applied to accumulated statistics. Reaching either stops farming, closes Pokémon GO, and saves the logs. |
| Interruptions | Automatically handles medals, trainer level-ups, XP rewards, the Daily Adventure Incense recap, hatches, the speed warning, and the Stamp Rally notice. Remains active during farming. |
| Field research | Claims available rewards, catches their encounters, and discards incompatible tasks or tasks of an unknown type. Preserves special research, timed research, and the AR scanning slot. |
| Remove buddy | Removes the buddy at startup, without transferring it or reassigning it when the session ends. Initially disabled. |
| Transfers | Only Pokémon caught by **this SilphMod session**, with a confirmed catch response and an exact ID. |
| Keep shiny / 15/15/15 | Two independent filters. Meeting any enabled filter prevents that Pokémon from being transferred. |
| Keep Dynamax / hatched / background | New protection filters for catches authorized by the service. They do not scan or modify the existing collection. |
| Keep shiny + background | Protects Pokémon that meet both conditions. Combines with the other keep filters. |
| Keep field research encounters | **Keep field research encounters** protects these catches. If disabled, the same shiny, IV, and other protection filters used for wild catches apply. |
| Show IV names | Temporarily displays the whole-number percentage and IVs in the inventory list, without renaming the Pokémon on the account. |
| Extended manual transfer | Expands manual multi-selection of special Pokémon from the game's inventory. Independent of automatic transfers. |
| Fast gift opening | Speeds up manual gift opening, the presentation of its items, and local waits in that sequence. Initially enabled in **Game tools**. |
| Hide trainer name | Temporarily displays **SilphMod** in the HUD, your own profile, and the friends header. The account name does not change. Initially enabled in **Game tools**. |
| Clean item bag | Deletes surplus items of the selected types **from startup**, and maintains those limits throughout the session. Does not need to wait for the bag to fill up. |
| Fix error 12 | Location treatment inside the game process, optional and tied to the session. |
| Statistics | Accumulated catches, flees, spins, transfers, deleted items, XP, Stardust, and active time. Preserved when stopping, restarting, and recovering the map. |
| Clear statistics | Resets counters only when pressed with the service stopped. Located below the statistics. |
| Map recovery | After five minutes of valid readings without Pokémon within range, restarts the game and reconnects the engine. Up to **10 startup attempts per recovery**, cancellable with Stop. Disabled when species are selected in **Only catch**. |
| Notification shade | Catches and spins continue when it is expanded if Pokémon GO remains the underlying app and the screen is unlocked. |
| Logs | Manual export of up to three recent sessions, with a size and SHA-256 index. |

There is no built-in route or joystick: SilphMod works with your GPS JoyStick app (developed by The App Ninjas). Do not use another joystick with a different package name from the one mentioned above.

### Only catch: choose which Pokémon to catch

**Only catch** lets you choose one or more species for subsequent sessions. The selector uses a local catalog with thumbnails, Pokédex ordering, and search by name or number. Your selection is saved; if you leave it empty, the farmer returns to catching all eligible species.

The filter is applied before opening a wild encounter and checked again before using a berry or throwing. Selecting a species does not modify its IVs or affect the keep filters: those are applied after the catch.

```js
allowsPokemon(readId, source = 'WILD') {
  return source === 'FIELD_RESEARCH' || !this.active || this.allows(readId());
}
```

*Excerpt from [catch-selection.cjs](engine/src/catch-selection.cjs). Field research catches are not blocked by Only catch.*

When species are selected, recovery after five minutes without Pokémon is not triggered: the farmer can wait for those species to appear without restarting the game. **Stop**, accumulated limits, and connection control keep working in the same way.

### Transfer without touching your existing collection

The transfer queue starts empty. A Pokémon is added only when a catch request sent by this session receives a valid, successful response containing its storage ID. Finding an ID in the collection is **not enough** to authorize its transfer.

In addition to your selected filters, favorites, buddies, eggs, deployed Pokémon, fusions, special classes, and records whose protection information cannot be validated are preserved. Unchecking **Keep shiny** or **Keep 15/15/15** allows those new catches to be transferred if no other protection applies.

Version **1.7.0** adds **Keep Dynamax**, **Keep hatched**, **Keep background**, and **Keep shiny + background**. The first three are initially enabled; the shiny + background combination is disabled. Meeting any checked filter keeps the Pokémon. Within **Keep shiny + background**, however, both conditions must be met at the same time.

If you check **Keep shiny** together with **Keep shiny + background**, all shiny Pokémon are kept, whether they have a background or not. If you check **Keep background** together with the combination, all Pokémon with a background are kept. To keep only shiny Pokémon with a background through these filters, leave only the combination checked and uncheck the other optional filters. The engine continues catching other Pokémon and transfers them if no other protection applies.

Hatched protection is checked in the record authorized for transfer; it does not turn hatches into new authorizations or add Pokémon from the existing collection to the queue.

**Keep field research encounters**, initially enabled, adds protection based on origin. If disabled, research catches are transferred when they do not meet any other active protection. For example, with **Keep shiny** checked, both wild and research shiny Pokémon are kept; **Keep shiny + background** requires both characteristics for either origin.

Transfers are initially disabled and **have no total per-session quota**. The engine receives `maxTransfers = 0`; old preferences do not restore a limit. Authorizations are not inherited between sessions, accounts, or restarts.

![Transfer authorization excerpt](docs/assets/session-transfer.png)

*This is an excerpt from [session-transfers.cjs](engine/src/session-transfers.cjs).*

### Item bag: per-type limits, not indiscriminate deletion

An enabled checkbox allows the surplus of that type to be deleted; the number indicates how many to keep. An unchecked type is not deleted. For example:

```text
Potion=50,SuperPotion=50,Revive=30,NanabBerry=30
```

With 83 Potions and a minimum of 50, the plan allows 33 to be deleted. Each request deletes at most 50 units, **with no total per-session quota**. The engine checks the response and inventory synchronization before continuing.

There are **33 selectable types**, including three common Poké Balls, four Potions, two Revives, five Berries, and other permitted consumables. The catalog contains another **17 protected types**; Master Ball, Safari Ball, Premier Ball, passes, incubators, and unknown items are not authorized for deletion. You can review the IDs and restrictions in [ItemCatalog.java](app/src/main/java/dev/rootcatch/nativeengine/ItemCatalog.java) and [item-catalog.cjs](engine/src/item-catalog.cjs).

Cleaning is initially disabled. The engine receives `maxDiscardedItems = 0`, regardless of previous preferences. It can delete items already in the bag, according to your selected types and minimums. An ambiguous response or a known rejection pauses cleaning without repeating the request; the other features can continue.

### Catch and spin limits

In **Engine settings → Session limits**, **Maximum catches** starts at **4800** and **Maximum PokéStop spins** at **2400**. Both accept a positive maximum and retain your chosen value for subsequent sessions.

Counters are preserved between service starts and automatic recoveries. Only successful responses to actions sent by SilphMod count toward the limit: failed attempts, flees, and manual catches do not. These are maximums applied to accumulated local statistics, not daily account counters.

If accumulated catches or spins have already reached their maximum, **Start service** is blocked and displays **Capture limit reached** or **Spin limit reached**. To continue, raise the limits above both totals or use **Clear statistics** with the service stopped. Stopping the farmer does not erase results; clearing statistics does not erase logs either.

When either maximum is reached, no further actions are sent. The agent removes its hooks, releases its references, and confirms the stop; Android then verifies the process identity, closes Pokémon GO, and saves the logs with the reason and final results. **Export logs** retrieves the ZIP. Closing the game does not erase its cache, storage, or settings.

## Usage

First of all, remember that you need a correctly configured GPS Joystick app.

1. Install the APK from the releases section, open it, and grant root and notification permissions (if requested). Keep your joystick configured and running. Do not run other automation tools or injectors at the same time.
2. Select **Catches**, **Only catch**, **Throw quality**, **Use berries**, **Berry priority**, **Remove buddy**, **Spin PokéStops**, transfer filters, and **Items to keep**, according to what you want to use. In **Inventory tools**, you can enable IV names and extended manual transfer. In **Game tools**, you can choose **Fast gift opening** and **Hide trainer name**. Adjust maximum catches and spins in **Engine settings**. Transfers and deletions have no separate total maximum per session, meaning they delete an unlimited number of the items you configured for deletion while the session remains active.
3. Set a **route** in your GPS Joystick app or simply use that app's joystick to move around the map. Remember that a **route** is recommended for greater mod effectiveness over longer periods.
4. If your device or instance needs location treatment, check **Fix error 12** and press **Start service**. SilphMod opens Pokémon GO and prepares the connection appropriate for the system. Startup may close only the game and reopen it to install early treatment; it does not erase the cache, storage, or joystick settings. If you enabled **Remove buddy**, the **Removing buddy…** status accompanies its removal and the restart required to continue without a buddy.

5. Wait for **Active**. The map, player, and references are checked automatically before actions are sent.
6. Press **Stop service** or **Stop** in the notification whenever you want to end the session. Only if you want to save an error report, wait for **Saving logs…** to finish and use **Export logs**. The ZIP is saved in `Downloads/SilphMod/`.

The service has no 30/60-second timer or fixed limit in hours. To stop it, open the app's notification and press Stop, or alternatively close Pokémon GO and return to the mod app.

Switching apps pauses actions; losing the process, changing accounts, or losing the controller for an extended period ends the session. Opening the notification shade lets you check statistics without pausing catches or spins, while the game remains beneath the panel and the screen is unlocked. Switching to another app or locking the screen pauses actions. (Remember that in UGPHONE, the game can run in the background if you close the instance, but not if you close Pokémon GO or SilphMod.)

If Pokémon storage fills up, catches pause and the status is reported; SilphMod does not delete your existing collection to make space. With a full item bag and no authorized surplus, it does not delete other items on its own either. Stopping cannot undo an operation that has already been sent.

## Architecture

![SilphMod architecture](docs/assets/architecture.svg)

SilphMod has two parts that cooperate through a local pipe:

- **Android / Java:** interface, configuration, foreground service, root permission, process identity, focus, control, notification, and export.
- **Agent / TypeScript and CommonJS:** IL2CPP service resolution, target selection, game requests, response reading, filters, and reference lifecycle.

The agent uses objects and methods from the original process; the game client remains responsible for serializing its requests and communicating with its server. SilphMod has no `INTERNET` permission or its own spawn API, but **the game needs a connection**: this is not an offline mode.

### Startup and continuity

When the location option is enabled, the agent first prepares the JNI adapter and then loads the Java/IL2CPP bridges, after a minimum of eight seconds and once the required libraries are present. That does not establish that the map is ready: the scene, account, signatures, 64-bit GC references, and stable samples are still validated for approximately ten seconds.

Activation uses a single-use authorization tied to the process, session, and player. There is no disconnection/reinjection between validation and the farmer. Actions run on the Unity thread; retained objects are released when the session ends.

The controller sends heartbeats every **500 ms** on a thread independent of focus queries and log writing. Only a positive focus reading no older than **1,500 ms** can authorize actions; the notification shade is accepted when the game remains the underlying activity. Permission for new actions expires after **2,200 ms**; a brief delay can recover within the same connection, while a continuous loss of control exceeding **10 seconds** ends the session. Automatic recovery handles the empty map described below and the restart required after removing the buddy.

The adaptive injector selects the connection according to the SDK, memory page size, root context, and SELinux state. On Android 10, it can use process attachment with Frida **17.22.1** or direct attachment with **17.23.3**, depending on the environment. On later Android versions, it prioritizes early startup with **17.23.3** and the native adapter. If a connection fails in a recoverable way, it can use an alternative route within the same APK.

The route that works is associated with the device configuration, game version, runtimes, and **Fix error 12** state. The next startup can reuse it when that environment has not changed. Retries confirm that the previous injector has exited and the process has closed before reconnecting; the startup maximum is **ten attempts**. The Farmer configuration and its policies are the same on every route.

Focus is queried through Java on Android 10 and through the JNI activity adapter on later systems. The reading distinguishes the underlying game, the notification shade, and the screen that is actually focused when using **scrcpy** or the system recorder. Mirroring or recording the screen is not, by itself, interpreted as switching apps; opening another application still pauses actions.

### Automatic map recovery

The farmer checks for nearby Pokémon every **five seconds**. If it accumulates **five continuous minutes of valid readings without Pokémon within effective catch range**, it stops the agent, confirms the release of references and location treatment, closes only the verified Pokémon GO process, and saves the logs. It then opens the game and connects a new engine instance with the same configuration and account.

This feature remains active during general farming and does not require a checkbox. A reading with Pokémon resets the counter, even if those targets are temporarily waiting. Incomplete samples, missing readings, switching apps, or locking the screen are not interpreted as an empty map. Running out of Poké Balls or filling storage is not enough either. **Only catch** disables only this recovery while species are selected.

Each recovery allows **up to ten startup attempts in total, including the first**. If the engine reaches **Active**, the remaining attempts are not made and farming continues. If the game closes during startup or the connection does not finish activating, another attempt can be made after confirming that the previous injector has ended and the game is closed. Waits between attempts increase from **5 to 30 seconds**; there are no simultaneous injectors. After all ten attempts are exhausted, the service stops, preserves statistics, and saves the logs. A later recovery, after farming resumes and another five minutes of an empty map accumulate, has a new maximum of ten attempts.

**Stop service** and **Stop** cancel both farming and any pending retry. Statistics are not reset, and limits continue counting what has already accumulated. The new connection validates the player again and does not recover the previous process's transfer queue.

### Catches and game calls

Reverse engineering was used to identify contracts, structures, and signatures for the supported profile. The engine resolves classes and services at runtime and validates what it will use; it does not distribute the game library or its decompiled metadata.

| Operation | Entry used in the 0.433.1 profile |
| --- | --- |
| Resolve services | `Zenject.Internal.ZenUtilInternal.GetAllSceneContexts` and `DiContainer.TryResolve(System.Type)` |
| Read position | `Niantic.Holoholo.Avatar.IMapAvatar.get_Location`, `Niantic.LatLng` structure |
| Read targets | `IMapPlaceDirectoryService.get_AllPokemon` / `get_ActivePokestops` |
| Read range | `IGlobalSettings.get_Settings()` → `GlobalSettingsProto.MapSettings.EncounterRangeMeters` |
| Open encounter | `MapPokemon.SendEncounterRequest()` |
| Apply berry | `IItemBag.UseItemForEncounter(Item, MapPokemon)` and `UseItemEncounterOutProto` response |
| Throw | `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto)` |
| Spin | `IRpcHandler.SendRpc<FortSearchProto, FortSearchOutProto>`; operation ID `101` |
| Recycle items | `IRpcHandler.SendRpc<RecycleItemProto, RecycleItemOutProto>`; ID `137` |
| Transfer | `IPokemonBag.ReleasePokemon(pokemon)`, followed by reconciliation by ID |
| Field research | `IQuestService` and `IQuestRpcService.CompleteQuest`, `RemoveQuest`, and `BeginQuestEncounter` |
| Manual gifts | `OpenGiftGuiController.OnOpenClick()`, gift animators, and local presentation sequences |
| Temporary name | Text in `PlayerHudGuiController`, `PlayerProfilePage`, and `FriendsListHeaderCellView` |

The engine uses the encounter range read from the game, up to **50 m for catch selection**, and retains **80 m for spins**. The target is validated again before each throw, which matters when the joystick is following a route.

The throw enables impact and spin and applies the profile selected in **Throw quality**. **Great** retains the previous value, `ReticleSize = 0.5`, and remains the initial option. Ball selection uses the first available in the order Poké Ball → Great Ball → Ultra Ball.

Only one action request is pending at a time. **Action interval** allows **100–3000 ms**, initially **150 ms** in the interface. It separates sends; it does not mean one catch every 150 ms. Responses, checks, target availability, and reconciliation also take time.

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

### Range and catch priority

Reverse engineering made it possible to distinguish the visibility and encounter fields in `MapSettingsProto`. SilphMod reads **EncounterRangeMeters** to decide which targets it can select; it does not use **PokemonVisibleRange** as a catch radius or modify game settings.

The reading is refreshed every **30 seconds** on the Unity thread. Effective range accepts positive values up to **50 m**; if the reading is unavailable, invalid, or more than **65 seconds** old, it returns to the previous **40 m** threshold. Adventure Effect bonuses are not added to this value.

```js
const meters=sample.base.values?.EncounterRangeMeters;
if(typeof meters!=='number'||!Number.isFinite(meters)||meters<=0)
  return fallback('invalid_encounter_setting');
if(meters>MAX_VALIDATED_METERS)return fallback('outside_validated_range');
```

*Excerpt from [capture-planning.cjs](engine/src/capture-planning.cjs), with `MAX_VALIDATED_METERS = 50`.*

The same range is applied during selection, before opening the encounter, before using a berry or throwing, and when checking for Pokémon for map recovery. If the route moves the player away from the target, the engine leaves it pending and continues looking for other valid encounters.

When an encounter is already open, its berry and throw take priority over dispatching the previous catch's transfer. This priority can defer the queue for up to **three seconds**; the oldest entry is then handled. A single pending request and individual transfer confirmation are preserved.

```js
const age=now-oldestOwnedAt;
return Number.isFinite(age)&&age>=0&&age<MAX_TRANSFER_DEFERRAL_MS;
```

*Priority decision in [capture-planning.cjs](engine/src/capture-planning.cjs), with `MAX_TRANSFER_DEFERRAL_MS = 3000`.*

Scanning without actions is spaced **250 ms** apart, and repeated inventory readings are avoided during that wait. **Action interval** keeps its **100–3000 ms** range: the optimization changes the order of work, does not send catches in parallel, and does not eliminate the wait for game responses.

### Throw quality:

The `Niantic.Holoholo.Encounter.PokeballThrow` contract distinguishes **Normal = 1**, **Nice = 2**, **Great = 3**, and **Excellent = 4**. The fields `ReticleSize`, `HitBullseye`, `Missed`, and `Spinning`, and the signature that receives this structure in `MapPokemon.TryCapture`, were identified.

Quality is not applied by tapping the screen or writing a reward label. SilphMod prepares the throw structure used by the game, with a specific reticle size for each profile:

```js
const PROFILES = Object.freeze({
  NORMAL: Object.freeze({grade: 1, reticleSize: 1.1, hitBullseye: true}),
  NICE: Object.freeze({grade: 2, reticleSize: 0.85, hitBullseye: true}),
  GREAT: Object.freeze({grade: 3, reticleSize: 0.5, hitBullseye: true}),
  EXCELLENT: Object.freeze({grade: 4, reticleSize: 0.05, hitBullseye: true})
});
```

*[throw-profile.cjs](engine/src/throw-profile.cjs). `grade` identifies the profile in traces; it is not written as a field in the game structure.*

The engine creates `PokeballThrow`, applies these fields, and passes the structure to the catch call:

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

The counter adds rewards received from the game, not a fixed amount associated with the throw selector.

### Berries: confirm the effect before throwing

`PokemonEncounterResponse.InitialActiveItem`, the two-argument method `IItemBag.UseItemForEncounter`, and the `Status` and `ActiveItem` fields of `UseItemEncounterOutProto` were identified. The agent validates these contracts and uses the game's service.

You can select the five berries and order their priority:

| Berry | Contract identifier |
| --- | ---: |
| Razz · `RazzBerry` | 701 |
| Nanab · `NanabBerry` | 703 |
| Pinap · `PinapBerry` | 705 |
| Golden Razz · `GoldenRazzBerry` | 706 |
| Silver Pinap · `GoldenPinapBerry` | 708 |

The initial priority is **Razz → Pinap → Nanab**; the two special berries are explicitly selected. The first selected berry with available units is used, not several berries in a row for the same active effect.

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

The engine retains the promise and waits for its response before throwing. For this contract, **Status = 0** corresponds to applying the berry; it is not confused with a catch's success code. The policy also checks the active item, and only then increments the counter:

```ts
const status=number(result.field('Status').value), activeItem=number(result.field('ActiveItem').value);
const verdict=berryCycle.settle(status,activeItem,p.item);
if(verdict.consumed)stats.berriesUsed++;
```

*Code from [agent.ts](engine/src/agent.ts), with the decision in [berry-policy.cjs](engine/src/berry-policy.cjs).*

If a berry is already active, another is not applied. When the catch response indicates that the Pokémon broke out of the ball, the cycle allows a berry to be applied again before the next attempt. A rejected berry does not trigger a chain of retries; the catch can continue without it. The target's existence and distance are checked again before acting.

No fixed wait is added for the berry animation: **Action interval** and a single pending request are preserved. Using berries adds the real request and its response, so it does take communication time. **Item limits** continues deleting the selected surplus; catches use the remaining units. XP still comes from the catch receipt, not from an invented bonus for applying a berry.

### Interruptions and map continuity

Interruption handling remains **always active** during farming. It observes the opening and visibility of compatible screens and uses `Dismiss()`, `Click()`, or `LuckyBonusSkipButtonClicked()` according to the controller. It includes medals, trainer level-ups, XP, the Daily Adventure Incense recap, the speed warning, and the **Stamp Rally** new stamp collection notice.

Hatches also have an adapter in `MapExploreState`: it identifies the signature that receives `HatchedPokemonsData` and skips the presentation before the scene opens. Obtaining the Pokémon and reward responses remain the game's responsibility. If a hatch screen is already open, its compatible controller handles it.

Actions run on the Unity thread, with the same session and focus control as the farmer. There is no continuous polling of every window: only a bounded queue is retained when a recognized screen appears. Screens outside that list are not closed automatically.

The `spin_scan` logs explain PokéStop selection, distance, cooldown, a full bag, and the traversal limit of the loaded list. This observation reuses the existing scan and does not add game requests or modify range or cooldowns.

### Field research

Field research is **always** handled during the Farmer, including tasks received from spinning PokéStops. The engine queries `IQuestService`, claims tasks that the game allows to be redeemed, and processes the item, Berry, XP, Stardust, or encounter rewards it receives.

The scope is identified by **QuestContext = 2** and its encounters by **EncounterType = 6**. Special and timed research remain outside that scope. Incompatible field tasks—such as raids or trades—and tasks of an unknown type or condition are deleted to make space for others; the AR scanning slot is preserved.

```js
if (!task || task.context !== FIELD_CONTEXT || typeof task.id !== 'string' || !task.id.trim())
  return {action: 'keep', reason: 'outside_field_research'};
if (task.redeemable === true) return {action: 'claim', reason: 'game_can_redeem'};
```

*Excerpt from [field-research.cjs](engine/src/field-research.cjs). The service and response contract is resolved in [field-research.ts](engine/src/field-research.ts).*

Reward encounters use the Farmer's berry, throw quality, and catch cycle. They are processed even if their species is not selected in **Only catch**. Afterwards, **Keep field research encounters** decides whether to keep them based on origin; with that option disabled, the other transfer protections apply.

### Remove buddy

**Remove buddy** is initially disabled. When checked, the engine queries whether a buddy is assigned and requests its removal with Pokémon identifier **0**. A confirmed response leads to a game restart to continue the Farmer without a buddy. If there was no buddy, it continues directly.

The `BuddyMapProto` and `SetBuddyPokemonProto` contracts are used through `IRpcHandler.SendRpc`, with operations **1350** and **152** in the game profile. Removal runs once per service start; it is not repeated on every map recovery.

Removing the buddy does not transfer it or modify its inventory data. It is not reassigned when the session ends: you can assign it again from the game whenever you want. This flow is coordinated in [buddy-removal.cjs](engine/src/buddy-removal.cjs).

### Inventory tools

**Show IV names**, initially enabled, temporarily changes the text displayed in inventory rows. For example, a Pokémon with **15/14/13** IVs is displayed as **93% 15/14/13**. The percentage is a whole number, and the name saved on the account does not change.

```js
return `${Math.floor(((data.attack+data.defense+data.stamina)/45)*100)}% ${data.attack}/${data.defense}/${data.stamina}`;
```

*Format used in [inventory-tools.cjs](engine/src/inventory-tools.cjs). Applied during the session and removed when the engine stops.*

**Extended manual transfer**, initially disabled, expands inventory multi-selection for special Pokémon, including shiny Pokémon and certain legendary classes. The user selects and confirms from within the game. Favorites, buddies, and other inventory protections remain excluded; this feature does not add existing Pokémon to the automatic transfer queue.

### Game tools: gifts and a temporary name

**Fast gift opening**, initially enabled, speeds up gifts you open manually from the game while the service is active. It reduces the opening animation duration to **50 ms** and speeds up the animators for the gift and its rewards. The original request, its responses, and item delivery remain the game's responsibility.

All of this made it possible to identify `OpenGiftGuiController.giftOpenAnimationLength`, its `guiAnimator`, the `GiftBox` animator, and the creation of `MajorRewardSequenceView` inside gift sequences. The engine applies changes only to those components and restores their values when the gift closes or the session stops.

```js
const LABEL='SilphMod',MAX_RECORDS=48,ANIMATION_SPEED=12,OPEN_SECONDS=0.05;
```

*Values used in [game-tools.cjs](engine/src/game-tools.cjs). The game's global clock keeps its usual speed.*

Simple presentation waits produced by the two opening sequences are also shortened. The engine checks that the object is exactly `Niantic.Zeppelin.Scheduler.Wait` before changing its duration:

```js
if(!runtime.valid(current)||current.class.fullName!==WAIT)return;
const duration=current.field('<SleepFor>k__BackingField');
const seconds=duration.value;
if(!Number.isFinite(seconds)||seconds<0||seconds>5)return;
if(seconds>OPEN_SECONDS+0.001){duration.value=OPEN_SECONDS;giftWaits++;savedWaitSeconds+=seconds-OPEN_SECONDS;}
```

*Excerpt from [game-tools.cjs](engine/src/game-tools.cjs). Network waits, resource loading, and pending conditions retain their behavior.*

**Hide trainer name**, initially enabled, replaces the player's visible text in the map HUD, your own profile, and the friends header. It uses the `UnityEngine.UI.Text` components on those screens; it does not rename the account or modify the identity data used by the game.

The HUD is obtained from the map state that is currently running. Previous text is retained during the session and restored when it stops, on the Unity thread. References have a limit of **48 components** and are released when the engine closes; destroyed components are removed so they do not accumulate as you navigate between screens.

### Transfers and deletion

64-bit identifiers remain exact strings or `UInt64` values, never JavaScript numbers that could lose precision. Authorization combines the sent request, a recent successful response, the correct class, the caught ID, and the current session.

The item bag policy is independent of the animation or whether it visually appears full:

```js
// Excerpt from engine/src/policy.cjs
const count = counts[name];
if (!Number.isInteger(count) || count <= keep) continue;
return {
  name, item: ITEMS[name], count: Math.min(count - keep, maxBatch),
  before: count, keep
};
```

A transfer does not increment the counter merely because it was requested: the Pokémon must also disappear from the inventory after the accepted response. Unknown results do not authorize irreversible retries.

### Location and Error 12

The adapter uses the JNI ABI to identify `android.location.Location.isFromMockProvider()` and, if present, `isMock()`. It treats only the boolean responses corresponding to those methods within the **process it connected to**.

Treatment is installed when starting with **Fix error 12** and removed when the service stops. It resolves the available methods according to the process SDK, including the modern `isMock()` variant, and shares the same lifecycle in UGPhone and on physical phones. The implementation is in [engine/location](engine/location).

### XP and Stardust counters

Rewards are counted from successful responses to **catches, spins, and field research sent by SilphMod**: `CaptureScoreProto.Exp`, `Stardust`, temporary evolution/Mega rewards, `FortSearchOutProto.XpAwarded`, `Loot`, `BonusLoot`, and `CompleteQuestOutProto.Rewards`, according to the validated contract. Totals are preserved when stopping or recovering the service and are reset only with **Clear statistics**.

The account balance is not subtracted, and rewards are not estimated by multiplying catches by a constant. Amounts are not multiplied again by bonuses the game may already have applied. An increasing sequence prevents duplicates; Android totals use `long`. `—` indicates an unavailable reading, and `+` a partial total with missing rewards.

## Building

Requirements: JDK **17 or later**, Android SDK **36**, Android NDK **r28c / 28.2.13676358**, Node.js **22 or later**, and Python **3.11 or later**. Android Studio can open the repository root. Set `JAVA_HOME` and `ANDROID_HOME`, or create your own `local.properties` with `sdk.dir`.

```powershell
cd engine
npm.cmd ci
npm.cmd run typecheck
cd ..
python tools/prepare_native.py
python tools/prepare_native.py --backend spawn
.\tools\build_loader.ps1 -Ndk "$env:ANDROID_HOME\ndk\28.2.13676358"
.\gradlew.bat :app:assembleRelease
```

Native preparation downloads the official ARM64 executables for **Frida 17.22.1** and **17.23.3**, verifies the compressed archive, binary, size, and ELF format, and does not execute them on the computer. It also accepts `--archive /path/to/official-file.xz` with the corresponding backend for offline preparation. The loader adapter is compiled from [native](native) with the specified NDK; [build_loader.ps1](tools/build_loader.ps1) contains its Windows compilation options.

Gradle rebuilds the agent from source and generates its expected SHA-256 during the build. You do not need to edit a constant manually or copy files from another project. The native binary is verified again before building.

Without signing configuration, the release output is **unsigned**. To sign it, provide `SILPH_KEYSTORE`, `SILPH_STORE_PASSWORD`, `SILPH_KEY_ALIAS`, and `SILPH_KEY_PASSWORD` through the environment. Keys remain outside the repository; updating an existing installation requires the same certificate.

Android output: `app/build/outputs/apk/release/`. The APK, `node_modules`, caches, logs, keys, and downloaded runtime are excluded from Git.

## Logs and privacy

Export is local and manual. It includes the final state, configuration, engine events, health, startup, accessible Android sources, logcat, and crash reports, with time/size limits and rotation. It does not amount to collecting all memory or retaining every line of a session indefinitely. Old sessions are not deleted automatically: check the available space.

ZIP files **do not add redaction** and may include coordinates, identifiers, and messages from other processes.

## Structure

```text
SilphMod/
├── app/                  # Single Android application
├── engine/
│   ├── src/              # Engine, policies, and session control
│   └── location/         # JNI adapter
├── docs/
│   └── assets/           # Branding, architecture, and code excerpt
├── native/               # ARM64 loader adapter
├── tools/                # Native preparation and image catalog
├── gradle/               # Build wrapper
├── LICENSE
├── NOTICE
└── README.md
```

## Status and upcoming improvements

**1.6.0 introduces selectable throw quality**, priority-based berry use, selective interruption handling, session catch and spin limits, continuity when expanding notifications, and a **100–3000 ms** action interval, initially **150 ms**. Transfers and discards retain their own unlimited total quotas; the complete session ends when either the maximum catches or spins is reached.

**1.7.0 adds automatic map recovery with up to ten startup attempts**, persistent statistics with manual clearing, startup blocking when accumulated limits have been reached, and keep filters for Dynamax, hatched Pokémon, backgrounds, and the shiny + background combination.

**1.9.0 unifies the Farmer in a single APK for UGPhone and physical rooted phones**, introducing **Only catch**, automatic field research, **Remove buddy**, temporary IV names, and extended manual transfer. It adds encounter range read from the game of up to **50 m**, bounded priority to finish active catches, fewer repeated readings, and improvements to log export. Interruption handling includes hatch presentation, the speed warning, and the Stamp Rally notice.

**1.9.3 updates the profile to Pokémon GO 0.433.1**, adds **Fast gift opening** to speed up manual gift opening, and **Hide trainer name** to temporarily display SilphMod instead of the player's name. It integrates the adaptive injector with alternative routes and reuse of the connection that worked for that environment.

The next phase is to keep optimizing Farmer continuity and expand compatibility across devices.

There are plans to add configuration for Poké Ball priority order, rather than the default: 1. Poké Ball; 2. Great Ball; 3. Ultra Ball.

Another pending improvement is to keep expanding the catalog of Pokémon configured not to be transferred, for example nundos (0/0/0).

[MIT](LICENSE) license. Dependencies retain their notices in [NOTICE](NOTICE) and their licenses within the assets. An independent, personal project, not affiliated with the game's rights holders or other third parties.

Take care, little guy.
