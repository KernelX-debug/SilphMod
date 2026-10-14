![SilphMod — Root Android](docs/assets/banner.svg)

# SilphMod

**English · [Español](spanish.md)**

Local automation for the original Pokémon GO on **rooted Android**, both in UGPhone and on physical ARM64 phones. It catches nearby Pokémon, spins PokéStops, collects field research rewards, transfers new catches according to your filters, and keeps the item bag within your chosen limits.

Configure the features, press **Start service**, and follow the progress from the notification. **Stop** and **Export logs** let you end the session and review its results. The interface, notifications, and visual assets are in English.

**Edition:** `1.9.0` · [MIT](LICENSE)

**Reference environments:** UGPhone Android 10, Motorola Edge 50 Fusion running Android 15, and Redmi Note 14 Pro+ 5G running Android 16; all rooted ARM64 devices.

<details>
<summary>Contents</summary>

- [How it works](#how-it-works)
- [Compatibility](#compatibility)
- [Features](#features)
  - [Only catch: choose which Pokémon to catch](#only-catch-choose-which-pokémon-to-catch)
  - [Transfers without touching the existing collection](#transfers-without-touching-the-existing-collection)
  - [Item bag: per-type limits, not indiscriminate deletion](#item-bag-per-type-limits-not-indiscriminate-deletion)
  - [Catch and spin limits](#catch-and-spin-limits)
- [Usage](#usage)
- [Architecture](#architecture)
  - [Startup and continuity](#startup-and-continuity)
  - [Automatic map recovery](#automatic-map-recovery)
  - [Captures and game calls](#captures-and-game-calls)
  - [Capture range and priority](#capture-range-and-priority)
  - [Throw quality](#throw-quality-from-the-il2cpp-contract-to-the-throw)
  - [Berries](#berries-confirm-the-effect-before-throwing)
  - [Interruptions and map continuity](#interruptions-and-map-continuity)
  - [Field research](#field-research)
  - [Remove buddy](#remove-buddy)
  - [Inventory tools](#inventory-tools)
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
| System | Android **10 or later** with `su` permission |
| Reference environments | UGPhone Android **10 / SDK 29**; physical phones running Android **15 / SDK 35** and **16 / SDK 36** |
| Architecture | **ARM64**, 64-bit pointers |
| Game | Original Pokémon GO **0.433.0**, `versionCode 2026100702` |
| Game engine | Unity **6000.3.22f1**, IL2CPP |
| APK | `minSdk 29`, `targetSdk 36` |
| Optional Error 12 treatment | JNI `Location` contracts resolved according to the system version |
| Connection | Automatic startup and runtime selection based on the SDK, within the same APK |
| Installed package identity | `dev.rootcatch.ugphonetest`, retained to update previous installations |

The application keeps a single interface and a single Farmer engine. Startup adapts to the **Android version of the device or instance**, without requiring the user to choose a different APK for each environment.

## Features

| Feature | Current behavior |
| --- | --- |
| Auto-catch | Selects the nearest eligible target within the range read from the game, up to **50 m**, and rechecks its existence and distance before throwing. Retries while the encounter remains valid; if the target disappears or moves out of range, it moves on to the next target. |
| Only catch | Select one or more species with thumbnails, search, and Pokédex ordering. With no selection, it catches every eligible species. Field research reward encounters are caught regardless of this filter. |
| Throw quality | Choose **Normal**, **Nice**, **Great**, or **Excellent**. Throws retain the curveball spin. |
| Berries | Uses the first available berry according to the configured selection and priority, confirms its application, and continues with the throw. Preserves Item limits. |
| PokéStops | Sends spin requests for targets within the range configured in the engine and counts successful responses. |
| Session limits | Configurable catch and spin maximums, initially **4,800** and **2,400**, applied to accumulated statistics. Reaching either one stops farming, closes Pokémon GO, and saves the logs. |
| Interruptions | Automatic handling of medals, level-ups, XP rewards, the Daily Adventure Incense recap, egg hatches, the speed warning, and the Stamp Rally notice. Remains active while farming. |
| Field research | Claims available rewards, catches their encounters, and discards incompatible tasks or tasks of unknown types. Preserves Special Research, Timed Research, and the AR scanning slot. |
| Remove buddy | Removes the buddy at startup without transferring it or reassigning it when the session ends. Disabled by default. |
| Transfers | Only Pokémon caught by **this SilphMod session**, with a verified capture response and exact ID. |
| Keep shiny / 15/15/15 | Two independent filters. Matching any enabled filter prevents that Pokémon from being transferred. |
| Keep Dynamax / hatched / background | Additional protection filters for catches authorized by the service. They do not scan or modify the existing collection. |
| Keep shiny + background | Protects Pokémon that meet both conditions. Combines with the other retention filters. |
| Keep field research encounters | **Keep field research encounters** protects these catches. If disabled, the same shiny, IV, and other protection filters used for wild catches apply. |
| Show IV names | Temporarily displays the integer percentage and IVs in the inventory list without renaming Pokémon in the account. |
| Extended manual transfer | Expands manual multi-selection of special Pokémon from the game's inventory. Independent of automatic transfers. |
| Item bag cleanup | Removes surplus units of selected item types **from startup** and maintains those limits throughout the session. It does not need to wait for the bag to fill up. |
| Fix error 12 | Optional, session-bound location treatment inside the game process. |
| Statistics | Accumulated catches, escapes, spins, transfers, discarded items, XP, Stardust, and active time. Preserved when stopping, restarting, and recovering the map. |
| Clear statistics | Resets counters only when pressed with the service stopped. Located below the statistics. |
| Map recovery | After five minutes of valid readings with no Pokémon within the effective range, restarts the game and reconnects the engine. Up to **10 startup attempts per recovery**, cancelable with Stop. Disabled when species are selected in **Only catch**. |
| Notification shade | Captures and spins continue while it is expanded if Pokémon GO remains the underlying app and the screen is unlocked. |
| Logs | Manual export of up to three recent sessions, with a size and SHA-256 index. |

There is no built-in route or joystick: SilphMod works with your GPS JoyStick application (developed by The App Ninjas). Do not use another joystick with a package name different from that application.

### Only catch: choose which Pokémon to catch

**Only catch** lets you choose one or more species for subsequent sessions. The selector uses a local catalog with thumbnails, Pokédex ordering, and search by name or number. Your selection is saved; leaving it empty makes the farmer catch every eligible species again.

The filter is applied before opening a wild encounter and checked again before using a berry or throwing. Selecting a species does not change its IVs or determine the retention filters: those are applied after capture.

```js
allowsPokemon(readId, source = 'WILD') {
  return source === 'FIELD_RESEARCH' || !this.active || this.allows(readId());
}
```

*Excerpt from [catch-selection.cjs](engine/src/catch-selection.cjs). Field research catches are not blocked by Only catch.*

When species are selected, recovery after five minutes without Pokémon is disabled: the farmer can wait for those species to appear without restarting the game. **Stop**, accumulated limits, and connection control continue to work in the same way.

### Transfers without touching the existing collection

The transfer queue starts empty. A Pokémon enters it only when a capture request issued by this session receives a valid, successful response containing its storage ID. Finding an ID in the collection is **not enough** to authorize its transfer.

In addition to the chosen filters, favorites, buddies, eggs, deployed Pokémon, fusions, special classes, and records whose protection information cannot be validated are preserved. Unchecking **Keep shiny** or **Keep 15/15/15** allows those new catches to be transferred if no other protection applies.

Version **1.7.0** adds **Keep Dynamax**, **Keep hatched**, **Keep background**, and **Keep shiny + background**. The first three are enabled by default; the shiny + background combination is disabled. Matching any checked filter preserves the Pokémon. Within **Keep shiny + background**, however, both conditions must be met at the same time.

If you check **Keep shiny** together with **Keep shiny + background**, all shiny Pokémon are preserved, whether they have a background or not. If you check **Keep background** together with the combination, all Pokémon with a background are preserved. To keep only shiny Pokémon with a background through these filters, leave only the combination checked and uncheck the other optional filters. The engine continues catching other Pokémon and transfers them if no other protection applies.

Hatched protection is checked on the record authorized for transfer; it does not turn egg hatches into new authorizations or add Pokémon from the existing collection to the queue.

**Keep field research encounters**, enabled by default, adds protection based on origin. If disabled, research catches are transferred when they match none of the other enabled protections. For example, checking **Keep shiny** preserves shiny Pokémon from both wild and research encounters; **Keep shiny + background** requires both traits for either origin.

Transfers are disabled by default and have **no total per-session quota**. The engine receives `maxTransfers = 0`; old preferences do not restore a limit. Authorizations are not inherited across sessions, accounts, or restarts.

![Transfer authorization excerpt](docs/assets/session-transfer.png)

*This is an excerpt from [session-transfers.cjs](engine/src/session-transfers.cjs).*

### Item bag: per-type limits, not indiscriminate deletion

An enabled checkbox allows surplus units of that type to be discarded; the number specifies how many to keep. An unchecked type is not discarded. For example:

```text
Potion=50,SuperPotion=50,Revive=30,NanabBerry=30
```

With 83 Potions and a minimum of 50, the plan allows 33 to be discarded. Each request discards at most 50 units, with **no total per-session quota**. The engine checks the response and inventory synchronization before continuing.

There are **33 selectable types**, including three ordinary Poké Balls, four Potions, two Revives, five berries, and other permitted consumables. The catalog contains another **17 protected types**; Master Balls, Safari Balls, Premier Balls, passes, incubators, and unknown items are not authorized for discarding. You can review IDs and restrictions in [ItemCatalog.java](app/src/main/java/dev/rootcatch/nativeengine/ItemCatalog.java) and [item-catalog.cjs](engine/src/item-catalog.cjs).

Cleanup is disabled by default. The engine receives `maxDiscardedItems = 0`, regardless of previous preferences. It can discard items already in the bag, according to the selected types and minimums. An ambiguous response or known rejection pauses cleanup without repeating the request; other features can continue.

### Catch and spin limits

In **Engine settings → Session limits**, **Maximum catches** starts at **4800** and **Maximum PokéStop spins** at **2400**. Both accept a positive maximum and save the chosen value for subsequent sessions.

Counters are preserved across service starts and automatic recoveries. Only successful responses to actions issued by SilphMod count toward the limit: failed attempts, escapes, and manual catches do not. These are maximums applied to accumulated local statistics, not daily account counters.

If accumulated catches or spins have already reached their maximum, **Start service** is blocked and displays **Capture limit reached** or **Spin limit reached**. To continue, raise the limits above both totals or use **Clear statistics** with the service stopped. Stopping the farmer does not erase results; clearing statistics does not erase logs either.

When either maximum is reached, no more actions are sent. The agent removes its hooks, releases its references, and confirms the stop; Android then verifies the process identity, closes Pokémon GO, and saves the logs with the reason and final results. **Export logs** lets you retrieve the ZIP. Closing the game does not erase its cache, storage, or configuration.

## Usage

Before anything else, remember that you need a properly configured GPS joystick application.

1. Install the APK from the releases section, open it, and grant root and notification permissions if requested. Keep your joystick configured and running. Do not run other automation tools or injectors simultaneously.
2. Select **Catches**, **Only catch**, **Throw quality**, **Use berries**, **Berry priority**, **Remove buddy**, **Spin PokéStops**, transfer filters, and **Items to keep**, according to the features you want to use. Under **Inventory tools**, you can enable IV names and extended manual transfer. Adjust the catch and spin maximums in **Engine settings**. Transfers and discards have no separate total per-session maximum: they remove an unlimited number of the items you have configured for discarding while the session remains active.
3. Set up a **walking route** in your GPS joystick application, or simply use that app's joystick to move around the map. A **walking route** is recommended for more effective use of the mod over longer periods.
4. If your device or instance needs location treatment, check **Fix error 12** and press **Start service**. SilphMod opens Pokémon GO and prepares the connection appropriate for the system. Startup may close only the game and reopen it to install the early treatment; it does not erase cache, storage, or joystick settings. If you enabled **Remove buddy**, the **Removing buddy…** status accompanies its removal and the restart needed to continue without a buddy.

5. Wait for **Active**. The map, player, and references are checked automatically before actions are sent.
6. Press **Stop service** or **Stop** in the notification whenever you want to end the session. Only if you want to save an error report, wait for **Saving logs…** to finish and use **Export logs**. The ZIP is saved under `Downloads/SilphMod/`.

The service has no 30/60-second timer or fixed limit in hours. To stop it, open the app's notification and press Stop, or close Pokémon GO and return to the mod's app.

Switching apps pauses actions; losing the process, changing accounts, or losing the controller for an extended period ends the session. Opening the notification shade lets you check statistics without pausing captures or spins, as long as the game remains beneath the panel and the screen is unlocked. Switching to another app or locking the screen pauses actions. (Remember that in UGPHONE the game can keep running in the background if you close the instance, but not if you close Pokémon GO or SilphMod.)

If Pokémon storage fills up, captures pause and the status is reported; SilphMod does not delete the existing collection to make room. Likewise, when the item bag is full and there is no authorized surplus, it does not discard other items on its own. Stopping cannot undo an operation that has already been sent.

## Architecture

![SilphMod architecture](docs/assets/architecture.svg)

SilphMod has two parts that cooperate through a local pipe:

- **Android / Java:** interface, configuration, foreground service, root permission, process identity, focus, control, notification, and export.
- **Agent / TypeScript and CommonJS:** IL2CPP service resolution, target selection, game requests, response reading, filters, and reference lifecycle.

The agent uses objects and methods from the original process; the game client remains responsible for serializing its requests and communicating with its server. SilphMod has no `INTERNET` permission or its own spawn API, but **the game needs a connection**: this is not an offline mode.

### Startup and continuity

When the location option is enabled, the agent prepares the JNI adapter first and then loads the Java/IL2CPP bridges, after a minimum of eight seconds and once the required libraries are present. This does not establish that the map is ready: the scene, account, signatures, 64-bit GC references, and stable samples over approximately ten seconds still need to be validated.

Activation uses a one-time authorization bound to the process, session, and player. There is no disconnection/reinjection between validation and the farmer. Actions run on the Unity thread; retained objects are released when the session ends.

The controller sends heartbeats every **500 ms** on a thread independent of focus queries and log writing. Only a positive focus reading no older than **1,500 ms** can authorize actions; the notification shade is accepted when the game remains the underlying activity. Permission for new actions expires after **2,200 ms**; a brief delay can recover within the same connection, while continuous loss of control for more than **10 seconds** ends the session. Automatic recovery handles the empty-map condition described below and the restart needed after removing the buddy.

Android 10 uses process attachment with Frida **17.22.1**. On later Android versions, early startup uses Frida **17.23.3** and the project's native adapter. The strategy is selected by SDK; the Farmer configuration and policies remain the same. Focus is queried through Java on Android 10 and through the activity JNI adapter on later systems.

### Automatic map recovery

The farmer observes nearby Pokémon presence every **five seconds**. If it accumulates **five continuous minutes of valid readings with no Pokémon within the effective capture range**, it stops the agent, confirms the release of references and location treatment, closes only the verified Pokémon GO process, and saves the logs. It then opens the game and connects a new engine instance with the same configuration and account.

This feature remains active during general farming and does not require enabling an option. A reading containing Pokémon resets the counter, even if those targets are temporarily waiting. Incomplete samples, missing readings, switching apps, or locking the screen are not interpreted as an empty map. Running out of Poké Balls or filling storage is not enough either. **Only catch** disables only this recovery while species are selected.

Each recovery allows **up to ten startup attempts in total, including the first**. If the engine reaches **Active**, the remaining attempts are skipped and farming continues. If the game closes during startup or the connection does not finish activating, it may retry after confirming that the previous injector has exited and the game is closed. Delays between attempts increase from **5 to 30 seconds**; there are no simultaneous injectors. After all ten attempts are exhausted, the service stops, preserves statistics, and saves the logs. A later recovery, after farming resumes and another five minutes of an empty map accumulate, receives a new maximum of ten attempts.

**Stop service** and **Stop** cancel both farming and any pending retry. Statistics are not reset, and limits continue counting everything already accumulated. The new connection validates the player again and does not restore the previous process's transfer queue.

### Captures and game calls

Reverse engineering was used to identify contracts, structures, and signatures for the supported profile. The engine resolves classes and services at runtime and validates what it is about to use; it does not distribute the game's library or its decompiled metadata.

| Operation | Entry used in the 0.433.0 profile |
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

The engine uses the encounter range read from the game, up to **50 m for selecting captures**, and maintains **80 m for spins**. The target is revalidated before every throw, which matters when the joystick is following a route.

The throw enables impact and spin and applies the profile selected in **Throw quality**. **Great** retains the previous value, `ReticleSize = 0.5`, and remains the default. Ball selection uses the first available in the order Poké Ball → Great Ball → Ultra Ball.

There is only one pending action request. **Action interval** allows **100–3000 ms**, initially **150 ms** in the interface. It spaces outgoing requests; it does not mean one capture every 150 ms. Responses, checks, target availability, and reconciliation also take time.

```ts
// Engine excerpt: open the encounter and retain its promise.
reserve(
  'encounter',
  object(pokemon.method<Obj>('SendEncounterRequest', 0).invoke()),
  nearestId,
  [pokemon]
);
```

*Simplified excerpt from [agent.ts](engine/src/agent.ts): the complete file includes checks, traces, and error handling around the call.*

### Capture range and priority

Reverse engineering made it possible to distinguish the visibility and encounter fields in `MapSettingsProto`. SilphMod reads **EncounterRangeMeters** to decide which targets it can select; it does not treat **PokemonVisibleRange** as a capture radius or modify the game's settings.

The reading is updated every **30 seconds** on the Unity thread. The effective range accepts positive values up to **50 m**; if the reading is unavailable, invalid, or more than **65 seconds** old, it falls back to the previous **40 m** threshold. Adventure Effect bonuses are not added to this value.

```js
const meters=sample.base.values?.EncounterRangeMeters;
if(typeof meters!=='number'||!Number.isFinite(meters)||meters<=0)
  return fallback('invalid_encounter_setting');
if(meters>MAX_VALIDATED_METERS)return fallback('outside_validated_range');
```

*Excerpt from [capture-planning.cjs](engine/src/capture-planning.cjs), with `MAX_VALIDATED_METERS = 50`.*

The same range applies during selection, before opening the encounter, before using a berry or throwing, and when observing Pokémon presence for map recovery. If the route moves the trainer away from the target, the engine leaves it pending and continues looking for other valid encounters.

When an encounter is already open, its berry and throw take priority over dispatching the previous catch's transfer. This priority can defer the queue for up to **three seconds**; its oldest entry is then processed. A single pending request and individual transfer confirmations are preserved.

```js
const age=now-oldestOwnedAt;
return Number.isFinite(age)&&age>=0&&age<MAX_TRANSFER_DEFERRAL_MS;
```

*Priority decision in [capture-planning.cjs](engine/src/capture-planning.cjs), with `MAX_TRANSFER_DEFERRAL_MS = 3000`.*

Scans without actions are spaced **250 ms** apart, and repeated inventory readings are avoided during that wait. **Action interval** retains its **100–3000 ms** range: the optimization changes the order of work; it neither sends captures in parallel nor removes the wait for game responses.

### Throw quality: from the IL2CPP contract to the throw

The `Niantic.Holoholo.Encounter.PokeballThrow` contract distinguishes the grades **Normal = 1**, **Nice = 2**, **Great = 3**, and **Excellent = 4**. The fields `ReticleSize`, `HitBullseye`, `Missed`, and `Spinning`, as well as the signature accepting this structure in `MapPokemon.TryCapture`, were identified.

Quality is not applied by tapping the screen or writing a reward label. SilphMod prepares the throw structure used by the game, with a specific reticle size for each profile:

```js
const PROFILES = Object.freeze({
  NORMAL: Object.freeze({grade: 1, reticleSize: 1.1, hitBullseye: true}),
  NICE: Object.freeze({grade: 2, reticleSize: 0.85, hitBullseye: true}),
  GREAT: Object.freeze({grade: 3, reticleSize: 0.5, hitBullseye: true}),
  EXCELLENT: Object.freeze({grade: 4, reticleSize: 0.05, hitBullseye: true})
});
```

*[throw-profile.cjs](engine/src/throw-profile.cjs). `grade` identifies the profile in traces; it is not written as a field in the game's structure.*

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

The counter adds the rewards received from the game, not a fixed amount associated with the throw selector.

### Berries: confirm the effect before throwing

`PokemonEncounterResponse.InitialActiveItem`, the two-argument method `IItemBag.UseItemForEncounter`, and the `Status` and `ActiveItem` fields of `UseItemEncounterOutProto` were identified. The agent validates those contracts and uses the game's service.

All five berries can be selected and prioritized:

| Berry | Contract identifier |
| --- | ---: |
| Razz · `RazzBerry` | 701 |
| Nanab · `NanabBerry` | 703 |
| Pinap · `PinapBerry` | 705 |
| Golden Razz · `GoldenRazzBerry` | 706 |
| Silver Pinap · `GoldenPinapBerry` | 708 |

The default priority is **Razz → Pinap → Nanab**; the two special berries must be explicitly selected. The first selected berry with available units is used, rather than several berries in a row for the same active effect.

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

*Excerpts from [agent.ts](engine/src/agent.ts). Active control authorization is checked between them.*

The engine retains the promise and waits for its response before throwing. For this contract, **Status = 0** means the berry was applied; it is not confused with the success code for a capture. The policy also checks the active item and only then increments the counter:

```ts
const status=number(result.field('Status').value), activeItem=number(result.field('ActiveItem').value);
const verdict=berryCycle.settle(status,activeItem,p.item);
if(verdict.consumed)stats.berriesUsed++;
```

*Code from [agent.ts](engine/src/agent.ts), with the decision in [berry-policy.cjs](engine/src/berry-policy.cjs).*

If a berry is already active, another is not applied. When the capture response indicates that the Pokémon broke out of the ball, the cycle allows a berry to be applied again before the next attempt. A rejected berry does not trigger a chain of retries; the capture can continue without it. The target's existence and distance are checked again before acting.

No fixed wait is added for the berry animation: **Action interval** and a single pending request are preserved. Using berries adds the real request and response, so it does take communication time. **Item limits** continues removing the selected surplus; captures use the remaining units. XP still comes from the capture receipt, not from an invented bonus for applying a berry.

### Interruptions and map continuity

Interruption handling remains **always active** during farming. It observes the opening and visibility of supported screens and uses `Dismiss()`, `Click()`, or `LuckyBonusSkipButtonClicked()` according to the controller. This includes medals, level-ups, XP, the Daily Adventure Incense recap, the speed warning, and the new stamp collection notice for **Stamp Rally**.

Egg hatches also have an adapter in `MapExploreState`: it identifies the signature accepting `HatchedPokemonsData` and skips the presentation before the scene opens. Obtaining the Pokémon and handling reward responses remain the game's responsibility. If a hatch screen is already open, its supported controller handles it.

Actions run on the Unity thread, with the same session and focus control as the farmer. There is no continuous polling of every window: only a bounded queue is retained when a recognized screen appears. Screens outside that list are not closed automatically.

The `spin_scan` logs explain PokéStop selection, distance, cooldown, full item bag status, and the traversal limit of the loaded list. This observation reuses the existing scan and does not add game requests or change the radius or cooldowns.

### Field research

Field research is **always** managed while the Farmer runs, including tasks received by spinning PokéStops. The engine queries `IQuestService`, claims tasks that the game allows to be redeemed, and processes any item, berry, XP, Stardust, or encounter rewards it receives.

The scope is identified by **QuestContext = 2**, and its encounters by **EncounterType = 6**. Special Research and Timed Research remain outside that scope. Incompatible field tasks—such as raids or trades—and tasks of unknown types or conditions are removed to make room for others; the AR scanning slot is preserved.

```js
if (!task || task.context !== FIELD_CONTEXT || typeof task.id !== 'string' || !task.id.trim())
  return {action: 'keep', reason: 'outside_field_research'};
if (task.redeemable === true) return {action: 'claim', reason: 'game_can_redeem'};
```

*Excerpt from [field-research.cjs](engine/src/field-research.cjs). The service and response contract is resolved in [field-research.ts](engine/src/field-research.ts).*

Reward encounters use the Farmer's berry, throw quality, and capture cycle. They are processed even if their species is not selected in **Only catch**. Afterwards, **Keep field research encounters** determines whether they are kept based on origin; with that option disabled, the remaining transfer protections apply.

### Remove buddy

**Remove buddy** is disabled by default. When checked, the engine queries whether a buddy is assigned and requests its removal using Pokémon ID **0**. A confirmed response triggers a game restart so the Farmer can continue without a buddy. If no buddy was assigned, it continues directly.

The `BuddyMapProto` and `SetBuddyPokemonProto` contracts are used through `IRpcHandler.SendRpc`, with operations **1350** and **152** from the game profile. Removal happens once per service start; it is not repeated on every map recovery.

Removing the buddy does not transfer it or modify its inventory data. It is not reassigned at the end: you can assign it again from the game whenever you want. This flow is coordinated in [buddy-removal.cjs](engine/src/buddy-removal.cjs).

### Inventory tools

**Show IV names**, enabled by default, temporarily changes the text displayed in inventory rows. For example, a Pokémon with IVs **15/14/13** appears as **93% 15/14/13**. The percentage is an integer, and the name saved in the account does not change.

```js
return `${Math.floor(((data.attack+data.defense+data.stamina)/45)*100)}% ${data.attack}/${data.defense}/${data.stamina}`;
```

*Format used in [inventory-tools.cjs](engine/src/inventory-tools.cjs). Applied during the session and removed when the engine stops.*

**Extended manual transfer**, disabled by default, expands inventory multi-selection for special Pokémon, including shiny Pokémon and certain legendary classes. The user selects and confirms them from the game. Favorites, buddies, and other inventory protections remain excluded; this feature does not add existing Pokémon to the automatic transfer queue.

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

A transfer does not increment the counter just because it was requested: the Pokémon must also disappear from the inventory after the accepted response. Unknown outcomes do not authorize irreversible retries.

### Location and Error 12

The adapter uses the JNI ABI to identify `android.location.Location.isFromMockProvider()` and, when available, `isMock()`. It treats only the boolean responses from those methods within the **process to which it connected**. It does not modify coordinates, `Location` objects, AppOps, Google services, SELinux, or system modules.

The treatment is installed when starting with **Fix error 12** and removed when the service stops. It resolves available methods according to the process SDK, including the modern `isMock()` variant, and shares the same lifecycle in UGPhone and on physical phones. The implementation is in [engine/location](engine/location).

### XP and Stardust counters

Rewards are counted from successful responses to **captures, spins, and field research issued by SilphMod**: `CaptureScoreProto.Exp`, `Stardust`, temporary/mega evolution rewards, `FortSearchOutProto.XpAwarded`, `Loot`, `BonusLoot`, and `CompleteQuestOutProto.Rewards`, according to the validated contract. Totals are preserved when the service stops or recovers and reset only with **Clear statistics**.

The account balance is not subtracted, and rewards are not estimated by multiplying catches by a constant. Amounts are not multiplied again by bonuses the game may already have applied. An increasing sequence prevents duplicates; Android totals use `long`. `—` indicates an unavailable reading, and `+` a partial total with missing rewards.

## Building

Requirements: JDK **17 or later**, Android SDK **36**, Android NDK **r28c / 28.2.13676358**, Node.js **22 or later**, and Python **3.11 or later**. Android Studio can open the repository root. Configure `JAVA_HOME` and `ANDROID_HOME`, or create your own `local.properties` with `sdk.dir`.

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

Native preparation downloads the official ARM64 executables for **Frida 17.22.1** and **17.23.3**, verifies the compressed archive, binary, size, and ELF format, and does not run them on the computer. It also accepts `--archive /path/to/official-file.xz` with the corresponding backend for offline preparation. The loader adapter is compiled from [native](native) with the specified NDK; [build_loader.ps1](tools/build_loader.ps1) contains its Windows compilation options.

Gradle rebuilds the agent from source and generates its expected SHA-256 during compilation. You do not need to edit a constant manually or copy files from another project. The native binary is verified again before building.

Without signing configuration, the release output is **unsigned**. To sign it, provide `SILPH_KEYSTORE`, `SILPH_STORE_PASSWORD`, `SILPH_KEY_ALIAS`, and `SILPH_KEY_PASSWORD` through the environment. Keys remain outside the repository; updating an existing installation requires the same certificate.

Android output: `app/build/outputs/apk/release/`. The APK, `node_modules`, caches, logs, keys, and downloaded runtime are excluded from Git.

## Logs and privacy

Export is local and manual. It includes final status, configuration, engine events, health, startup, accessible Android sources, logcat, and crash reports, with time/size limits and rotation. It does not collect all memory or preserve every line of a session indefinitely. Old sessions are not deleted automatically: check available storage.

The ZIP files **do not add redaction** and may include coordinates, identifiers, and messages from other processes.

## Structure

```text
SilphMod/
├── app/                  # A single Android application
├── engine/
│   ├── src/              # Engine, policies, and session control
│   └── location/         # JNI adapter
├── docs/
│   └── assets/           # Identity, architecture, and code excerpt
├── native/               # ARM64 loader adapter
├── tools/                # Native preparation and image catalog
├── gradle/               # Build wrapper
├── LICENSE
├── NOTICE
└── README.md
```

## Status and upcoming improvements

**1.6.0 introduces selectable throw quality**, prioritized berry use, selective interruption handling, session catch and spin limits, continuity with the notification shade expanded, and an action interval of **100–3000 ms**, with a default of **150 ms**. Transfers and discards retain their own unlimited total quotas; the entire session ends when either the catch or spin maximum is reached.

**1.7.0 adds automatic map recovery with up to ten startup attempts**, persistent statistics with manual clearing, startup blocking when accumulated limits have been reached, and retention filters for Dynamax, hatched Pokémon, backgrounds, and the shiny + background combination.

**1.9.0 brings the Farmer together in a single APK for UGPhone and rooted physical phones**, adding **Only catch**, automatic field research, **Remove buddy**, temporary IV names, and extended manual transfer. It adds the encounter range read from the game up to **50 m**, bounded priority for finishing active captures, fewer repeated readings, and improvements to log export. Interruption handling includes egg hatch presentations, the speed warning, and the Stamp Rally notice.

The next phase is to continue optimizing Farmer continuity and broaden device compatibility.

Configurable Poké Ball priority is planned, instead of the default order: 1. Poké Ball; 2. Great Ball; 3. Ultra Ball.

Another pending improvement is to keep expanding the catalog of Pokémon configured to be preserved, for example nundos (0/0/0).

Licensed under [MIT](LICENSE). Dependencies retain their notices in [NOTICE](NOTICE) and their licenses within the assets. An independent project, unaffiliated with the game's rightsholders.

Take care, little guy.
