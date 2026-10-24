![SilphMod — Root Android](docs/assets/banner.svg)

# SilphMod

**English · [Español](spanish.md)**

Local automation for the original Pokémon GO app on **rooted Android**, both on UGPhone and on physical ARM64 phones. It catches nearby Pokémon, spins PokéStops, collects field research, transfers new catches according to your filters, and keeps the item bag within your chosen limits.
Kecleon mode (BETA) is implemented in this version, *but is not yet final*

Configure the features, press **Start service**, and follow progress from the notification. **Stop** and **Export logs** let you stop the session and review its results. The interface, notifications, and visual resources are in English.

**Edition:** `2.0.15` · [MIT](LICENSE)

**Platform:** Android 10 or later, ARM64, and root. The same APK for UGPhone and physical phones.

<details>
<summary>Contents</summary>

- [How it works](#how-it-works)
- [Compatibility](#compatibility)
- [Features](#features)
  - [Kecleon (BETA)](#kecleon-beta)
  - [Only catch: choose which Pokémon to catch](#only-catch-choose-which-pokémon-to-catch)
  - [Transfer without touching the existing collection](#transfer-without-touching-the-existing-collection)
  - [Item bag: per-type limits, not indiscriminate deletion](#item-bag-per-type-limits-not-indiscriminate-deletion)
  - [Catch and spin limits](#catch-and-spin-limits)
- [Usage](#usage)
- [Architecture](#architecture)
  - [Startup and continuity](#startup-and-continuity)
  - [Integrity and control channel](#integrity-and-control-channel)
  - [Automatic map recovery](#automatic-map-recovery)
  - [Catches and game calls](#catches-and-game-calls)
  - [Catch range and priority](#catch-range-and-priority)
  - [Throw quality](#throw-quality)
  - [Berries](#berries-confirm-the-effect-before-throwing)
  - [Interruptions and map continuity](#interruptions-and-map-continuity)
  - [Field research](#field-research)
  - [Remove buddy](#remove-buddy)
  - [Inventory tools](#inventory-tools)
  - [Game tools: gifts and temporary name](#game-tools-gifts-and-temporary-name)
  - [Transfers and deletion](#transfers-and-deletion)
  - [Location and Error 12](#location-and-error-12)
  - [XP and Stardust counters](#xp-and-stardust-counters)
- [Build](#build)
- [Logs and privacy](#logs-and-privacy)
- [Structure](#structure)
- [Status and upcoming improvements](#status-and-upcoming-improvements)

</details>

## How it works

![SilphMod in action — 0:25](docs/assets/demo.gif)

## Compatibility

| Component | Profile for this edition |
| --- | --- |
| System | Android **10 or later** and `su` permission |
| Environments | UGPhone and rooted physical ARM64 phones |
| Architecture | **ARM64**, 64-bit pointers |
| Game | Original Pokémon GO **0.433.1**, `versionCode 2026101502` |
| Game engine | Unity **6000.3.22f1**, IL2CPP |
| APK | `minSdk 29`, `targetSdk 36` |
| Optional Error 12 treatment | JNI `Location` contracts resolved according to the system version |
| Connection | Adaptive selection between four startup routes and reuse of the one that worked for the environment |
| Installed identity | `dev.rootcatch.ugphonetest`, retained to update existing installations |

The app brings together **Farmer** and **Kecleon (BETA)**, with independent execution and separate statistics. Startup adapts to the **Android version of the device or instance**.

## Features

| Feature | Current behavior |
| --- | --- |
| Autocatch | Selects the nearest eligible target within **50 m**, and checks its existence and distance again before throwing. Retries while the encounter remains valid; if it disappears or goes out of range, moves on to the next target. |
| Only catch | Select one or more species with thumbnails, search, and Pokédex ordering. With no selection, catches all eligible species. Field research rewards are caught regardless of this filter. |
| Throw quality | Choose **Normal**, **Nice**, **Great**, or **Excellent**. Throws retain the curveball spin. |
| Berries | Uses the first available berry according to the configured selection and priority, confirms its application, and proceeds with the throw. Respects Item limits. |
| PokéStops | Sends spins for targets within the range configured in the engine and counts successful responses. |
| Session limits | Configurable catch and spin maximums, initially **4,800** and **2,400**, applied to accumulated statistics. Reaching either stops farming, closes Pokémon GO, and saves the logs. |
| Interruptions | Automatic handling of medals, level-ups, XP rewards, the Daily Adventure Incense summary, egg hatches, the speed warning, and the Stamp Rally notice. Remains active during farming. |
| Field research | Claims available rewards, catches their encounters, and discards incompatible tasks or tasks of an unknown type. Preserves special and timed research and the AR scanning slot. |
| Remove buddy | Removes the buddy at startup, without transferring it or reassigning it at the end. Disabled by default. |
| Transfers | Only Pokémon caught during **this SilphMod session**, with a verified catch response and exact ID. |
| Keep shiny / 15/15/15 | Two independent filters. Meeting any active filter prevents that Pokémon from being transferred. |
| Keep Dynamax / hatched / background | Additional protection filters for catches authorized by the service. They do not scan or modify the existing collection. |
| Keep shiny + background | Protects Pokémon that meet both conditions. Combines with the other keep filters. |
| Keep research encounters | **Keep field research encounters** protects those catches. When disabled, the same shiny, IV, and other protections used for wild catches apply. |
| Show IV names | Temporarily displays the whole-number percentage and IVs in the inventory list, without renaming the account's Pokémon. |
| Extended manual transfer | Expands manual multi-selection of special Pokémon from the game's inventory. Independent of automatic transfers. |
| Fast gift opening | Speeds up manual gift opening, the display of its items, and local waits in that sequence. Enabled by default in **Game tools**. |
| Hide trainer name | Temporarily displays **SilphMod** in the HUD, your own profile, and the friends header. The account name does not change. Enabled by default in **Game tools**. |
| Clean item bag | Deletes excess quantities of selected types **from the start**, and maintains those limits throughout the session. Does not need to wait for the bag to fill up. |
| Kecleon (BETA) | Automatic candidate search and capture, with filters and per-account history. |
| Fix error 12 | Location treatment within the game process, optional and tied to the session. |
| Statistics | Accumulated catches, flees, spins, transfers, items deleted, XP, Stardust, and active time. Preserved when stopping, restarting, and recovering the map. |
| Clear statistics | Resets only the counters of the open mode, with the service stopped. In Kecleon, also clears its candidate history. Located below the statistics. |
| Map recovery | After five minutes of valid readings with no spawns in the map directory, restarts the game and reconnects the engine. Up to **10 startup attempts per recovery**, cancelable with Stop. Disabled when species are selected in **Only catch**. |
| Notification panel | Catches and spins continue while it is pulled down if Pokémon GO remains the underlying app and the screen is unlocked. This also applies with the item bag, Pokémon inventory, and profile open. |
| Integrity and control | Component verification before startup and a local channel bound to the process and session, with credentials excluded from arguments and new logs. |
| Logs | Manual export of up to three recent sessions to a local ZIP. |

Farmer uses the route you configure in your joystick. Kecleon requires the public integration of **GPS JoyStick by The App Ninjas** for its automatic jumps.

### Kecleon (BETA)

This mode visits candidates from **Moonani**, moves with GPS JoyStick, looks for the corresponding PokéStop, and checks its active incident. It resolves `fort_id`, incident, and `encounter_id` from the game before opening the encounter.

Map loading is accompanied by bounded **eight-meter** movements and spaced readings. A timeout is recorded as **unverified**, not as proof that there was no Kecleon there. History separates accounts and avoids reprocessing candidates that have already been resolved.

The **Only catch 15/15/15**, **Only catch 0/0/0**, **Only catch shiny**, and **Only catch shiny + 15/15/15** filters combine with “OR.” The last option requires both shiny status and perfect IVs. With no filters, all valid Kecleon found are caught.

**Maximum encounters** limits accumulated encounters. When the maximum is reached or pending candidates run out, the game closes and the reason is displayed. **Clear statistics** clears its counters and candidate history. Farmer retains its own results.

Kecleon shares throw quality, berries, item bag limits, and inventory and presentation tools. Its statistics show **Encounters**, **Catches**, **Shinies**, **Items deleted**, **XP**, and **Stardust**. It does not spin PokéStops or transfer its collection.

### Only catch: choose which Pokémon to catch

**Only catch** lets you choose one or more species for subsequent sessions. The selector uses a local catalog with thumbnails, Pokédex ordering, and search by name or number. The selection is saved; if you leave it empty, Farmer resumes catching all eligible species.

The filter applies before opening a wild encounter and is checked again before using a berry or throwing. Selecting a species does not modify its IVs or determine the keep filters: those apply after the catch.

The selector prepares a species mask that the engine checks before opening the encounter. Research catches use a separate source and are not blocked by **Only catch**.

When species are selected, recovery after five minutes without Pokémon is not activated: Farmer can wait for those species to appear without restarting the game. **Stop**, accumulated limits, and connection control continue to work in the same way.

### Transfer without touching the existing collection

The transfer queue starts empty. A Pokémon enters it only when a catch request issued by this session obtains a valid, successful response with its storage ID. Finding an ID in the collection **is not enough** to authorize its transfer.

In addition to the selected filters, favorites, buddies, eggs, deployed Pokémon, fusions, special classes, and records whose protection information cannot be validated are retained. Unchecking **Keep shiny** or **Keep 15/15/15** allows those new catches to be transferred if no other protection applies.

The options include **Keep Dynamax**, **Keep hatched**, **Keep background**, and **Keep shiny + background**. The first three are enabled by default; the shiny + background combination is disabled. Meeting any checked filter retains the Pokémon. Within **Keep shiny + background**, however, both conditions must be met at the same time.

If you check **Keep shiny** together with **Keep shiny + background**, all shiny Pokémon are retained, with or without a background. If you check **Keep background** along with the combination, all Pokémon with a background are retained. To retain only shiny Pokémon with a background through these filters, leave only the combination checked and uncheck the other optional filters. The engine continues catching the other Pokémon and transfers them if no other protection applies.

Hatched protection is checked on the record authorized for transfer; it does not turn hatches into new authorizations or add Pokémon from the existing collection to the queue.

**Keep field research encounters**, enabled by default, adds retention by source. When disabled, research catches are transferred when they do not meet any other active protection. For example, with **Keep shiny** checked, both wild and research shiny Pokémon are retained; the **Keep shiny + background** combination requires both characteristics for both sources.

Transfers are disabled by default and **have no total per-session quota**. Old transfer quotas are not restored. Authorizations are not inherited between sessions, accounts, or restarts.

Authorization is implemented in [core.c](native/core.c), [farmer-actions.c](native/farmer-actions.c), and [transfer-policy.c](native/transfer-policy.c).

### Item bag: per-type limits, not indiscriminate deletion

An enabled checkbox allows the excess of that type to be deleted; the number indicates how many to keep. An unchecked type is not deleted. For example:

```text
Potion=50,SuperPotion=50,Revive=30,NanabBerry=30
```

With 83 Potions and a minimum of 50, the plan allows 33 to be deleted. Each request deletes at most 50 units, **with no total per-session quota**. The engine checks the response and inventory synchronization before continuing.

There are **33 selectable types**, including three common Poké Balls, four Potions, two Revives, five berries, and other allowed consumables. The catalog contains another **17 protected types**; Master Balls, Safari Balls, Premier Balls, passes, incubators, and unknown items are not authorized for deletion. You can review the IDs and restrictions in [ItemCatalog.java](app/src/main/java/dev/rootcatch/nativeengine/ItemCatalog.java).

Cleaning is disabled by default. There is no total quota for discarded items. It can delete items already in the bag, according to the selected types and minimums. An ambiguous response or a known rejection pauses cleaning without repeating the request; the other features can continue.

### Catch and spin limits

In **Engine settings → Session limits**, **Maximum catches** starts at **4800** and **Maximum PokéStop spins** at **2400**. Both accept a positive maximum and retain the chosen value for subsequent sessions.

Counters are preserved between service starts and automatic recoveries. Only successful responses to actions issued by SilphMod count toward the limit: not failed attempts, flees, or manual catches. These are maximums applied to accumulated local statistics.

If accumulated catches or spins have already reached their maximum, **Start service** is blocked and displays **Capture limit reached** or **Spin limit reached**. To continue, increase the limits above both totals or use **Clear statistics** with the service stopped. Stopping Farmer does not erase results; clearing statistics does not erase logs either.

When either maximum is reached, no more actions are sent. The agent removes its hooks, releases its references, and confirms the stop; Android then verifies the process identity, closes Pokémon GO, and saves the logs with the reason and final results. **Export logs** lets you retrieve the ZIP. Closing does not clear the game's cache, storage, or settings.

## Usage

First, remember that you need a correctly configured GPS joystick app.

1. Install the APK from the releases section, open it, and grant root and notification permissions (if requested). Keep your joystick configured and working. Do not run other automation tools or injectors at the same time. Set SilphMod's battery mode to unrestricted.

2. Select **Catches**, **Only catch**, **Throw quality**, **Use berries**, **Berry priority**, **Remove buddy**, **Spin PokéStops**, transfer filters, and **Items to keep**, according to what you want to use. In **Inventory tools**, you can enable IV names and extended manual transfer. In **Game tools**, you can choose **Fast gift opening** and **Hide trainer name**. Adjust the catch and spin maximums in **Engine settings**. Transfers and discards have no total maximum of their own per session; in other words, they delete an unlimited number of the items you have configured for deletion while the session remains active.

3. Set a **walking route** in your GPS joystick app, or simply use that app's joystick to move around the map. Remember that a **walking route** is recommended for greater effectiveness over longer periods.

4. If your device or instance needs location treatment, check **Fix error 12** and press **Start service**. SilphMod opens Pokémon GO and prepares the connection appropriate for the system. Startup may close only the game and reopen it to install the early treatment; it does not clear cache, storage, or joystick settings. If you enabled **Remove buddy**, the **Removing buddy…** status accompanies its removal and the restart needed to continue without a buddy.

5. Wait for **Active**. The map, player, and references are checked automatically before sending actions.

6. Press **Stop service** or **Stop** in the notification whenever you want to end the session. Only if you want to save an error report, wait for **Saving logs…** to finish and use **Export logs**. The ZIP is saved in `Descargas/SilphMod/`.

The service has no 30/60-second timer or fixed limit on hours. To stop it, open the app's notification and press Stop, or close Pokémon GO and return to the mod's app.

Switching apps pauses actions; losing the process, changing accounts, or losing the controller for a prolonged period ends the session. Opening the notification panel lets you review statistics without pausing catches or spins, while the game remains underneath the panel and the screen is unlocked. Switching to another app or locking the screen pauses actions. (Remember that on UGPHONE, the game can run in the background if you close the instance, but not if you close Pokémon GO or SilphMod.)

Within the game, Farmer can continue when opening the item bag, the Pokémon inventory list, or your own profile. Selecting multiple Pokémon or entering a screen outside this flow pauses actions until you return to a compatible view.

If Pokémon storage fills up, catches pause and the status is reported; SilphMod does not delete the existing collection to make room. With a full item bag and no authorized excess to delete, it does not delete other items on its own either. Stopping cannot reverse an operation that has already been sent.

## Architecture

![SilphMod architecture](docs/assets/architecture.svg)

SilphMod has two parts that cooperate through a local control channel:

- **Android / Java:** interface, settings, foreground service, root permission, process identity, focus, notification, Kecleon candidates, and export.
- **Engine / C:** IL2CPP service resolution, action scheduling on the Unity thread, response reading, inventory policies, and reference lifecycles.

The engine uses objects and methods from the original process; the game client continues serializing its requests and communicating with its server. Kecleon mode also queries its candidate source over HTTPS. Account and Pokémon identifiers are not sent to that source.

### Startup and continuity

**Start service** opens Pokémon GO and checks the version, libraries, process, and its start times. When **Fix error 12** is enabled, it prepares the location adapter first. Access to IL2CPP objects waits for the game libraries to be present and for the **20-second** stability period to be met.

Startup has four combinations: **ATTACH / FD**, **ATTACH / PATH**, **SEIZE / FD**, and **SEIZE / PATH**. They change the connection mechanism and how the library is opened, not the engine or its policies. The route that worked is associated with the device and game profile. There is a maximum of **ten attempts**, with confirmation that the previous attempt has exited before continuing.

Execution is bound to the process, session, and account. Heartbeats and focus checks are separate from log writing and feed network operations. The scheduler executes actions on the Unity thread and maintains only one pending request.

Focus detection distinguishes Pokémon GO underneath, notifications, and system overlays. **scrcpy**, screenshots, and recording are not treated as app switches by themselves. Opening another app or locking the screen pauses actions; **Stop** cancels the service.

Farmer maintains its cycle from compatible item bag, Pokémon inventory, and profile views. The engine checks the active screen before acting and respects manual multi-selection. The same process, session, and focus checks continue to apply outside the map.

### Integrity and control channel

The **SHA-256** hashes of native components are checked before startup. Reading requires regular files, canonical paths, and permissions without group or other write access; it rejects symbolic links and changes in size or identity during verification.

The service and engine communicate through a local **AF_UNIX** socket, checking process credentials, session identity, and message sequence. Keys are delivered to the controller through a **private pipe**, not as process arguments. A connection that does not authenticate within **five seconds** is closed; that deadline does not limit an already authenticated session.

New logs exclude the control key. Restoration, disconnection, and resource-release checks remain in place at shutdown. The implementation is split between [NativeComponentIntegrity.java](app/src/main/java/dev/silphmod/nativecheck/NativeComponentIntegrity.java), [control.c](native/control.c), [control-client.c](native/control-client.c), and [LogPrivacy.java](app/src/main/java/dev/silphmod/nativecheck/LogPrivacy.java).

### Automatic map recovery

Farmer checks the number of spawns in the map directory once per second. If it accumulates **five continuous minutes of valid readings with no spawns**, it waits until there is no pending action, stops the engine, and confirms the release of references and location treatment. It then closes only the verified Pokémon GO process, opens the game, and reconnects with the same configuration and account.

This feature remains active during general farming and does not require a checkbox. A reading with Pokémon resets the counter. Invalid samples, prolonged lack of readings, pauses, or screen locking do not count as an empty map. **Only catch** disables this recovery while any species are selected.

```c
if (count) { s->empty = 0; return false; }
if (!s->empty) { s->since = now; s->empty = 1; }
return now - s->since >= UINT64_C(300000000000);
```

*Excerpt from [recovery-policy.c](native/recovery-policy.c), using a monotonic clock in nanoseconds.*

Each recovery has **up to ten startup attempts**. **Stop** also cancels retries. Statistics and their accumulated limits are preserved; the new connection validates the player again and does not inherit the previous process's transfer queue.

### Catches and game calls

Reverse engineering was used to identify contracts, structures, and signatures for the supported profile. The engine resolves classes and services at runtime and validates what it will use; it does not distribute the game library or its decompiled metadata.

| Operation | Entry used in profile 0.433.1 |
| --- | --- |
| Resolve services | `Zenject.Internal.ZenUtilInternal.GetAllSceneContexts` and `DiContainer.TryResolve(System.Type)` |
| Read position | `Niantic.Holoholo.Avatar.IMapAvatar.get_Location`, `Niantic.LatLng` structure |
| Read targets | `IMapPlaceDirectoryService.get_AllPokemon` / `get_ActivePokestops` |
| Open encounter | `MapPokemon.SendEncounterRequest()` |
| Apply berry | `IItemBag.UseItemForEncounter(Item, MapPokemon)` and `UseItemEncounterOutProto` response |
| Throw | `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto)` |
| Spin | `IRpcHandler.SendRpc<FortSearchProto, FortSearchOutProto>`; operation ID `101` |
| Recycle items | `IRpcHandler.SendRpc<RecycleItemProto, RecycleItemOutProto>`; ID `137` |
| Transfer | `IPokemonBag.ReleasePokemon(pokemon)`, followed by reconciliation by ID |
| Field research | `IQuestService` and `IQuestRpcService.CompleteQuest`, `RemoveQuest`, and `BeginQuestEncounter` |
| Manual gifts | `OpenGiftGuiController.OnOpenClick()`, gift animators, and local presentation sequences |
| Temporary name | Text in `PlayerHudGuiController`, `PlayerProfilePage`, and `FriendsListHeaderCellView` |

The engine maintains **50 m for wild catches** and **80 m for spins**. It checks ID, existence, and distance again before acting, which matters when the joystick follows a route.

Ball selection uses the first available in the order Poké Ball → Great Ball → Ultra Ball. **Throw quality** lets you choose the profile, and **Curve ball** controls the spin.

**Action interval** supports **100–3000 ms**, with an initial value of **100 ms**. It spaces operations; it does not mean a catch completes in that time. Responses and inventory reconciliation retain their own cycle.

With the interval at **100 ms**, pending catch phases are checked at **33 ms** from the game's original update. Once that operation finishes, the configured interval resumes; higher values retain their cadence. The scheduler can chain up to **three internal steps** when the previous phase finishes, without sending simultaneous requests or performing a transfer ahead of catch confirmation.

```c
status = silph_action_call(s->read, permit, SILPH_ACTION_CAPTURE,
    s->target.object, args, 2, &promise);
```

*Actual call from [farmer-actions.c](native/farmer-actions.c). Arguments and the session permit are validated before invoking the game method.*

### Catch range and priority

Seeing a Pokémon on the map and being able to catch it are different states. The engine selects targets within **50 m** and validates the encounter again before using berries or throwing. If the route takes it out of range, it continues with other valid targets.

Scanning, catch responses, research, cleaning, and transfers share a scheduler. The entire inventory is not reread while a response is pending. Transfers retain their individual authorization and never execute in parallel with another engine operation.

Field and method resolution uses bounded caches of validated metadata. Inventory values and game responses are read again when appropriate; the cache does not retain Pokémon or authorizations from another session. Research, spins, and cleaning retain their turn before the next target is selected.

Coordination is in [farmer-session.c](native/farmer-session.c); target selection and reading are in [game-read.c](native/game-read.c).

### Throw quality

The `Niantic.Holoholo.Encounter.PokeballThrow` contract distinguishes **Normal = 1**, **Nice = 2**, **Great = 3**, and **Excellent = 4**. The engine prepares the throw structure with `ReticleSize`, `HitBullseye`, `Missed`, and `Spinning`, and passes it to `MapPokemon.TryCapture`.

```c
const float profiles[] = {.5f, 1.1f, .85f, .5f, .05f};
status = silph_action_capture_data(s->read, permit, (int32_t)ball,
    profiles[s->options.quality], s->options.quality != 0,
    s->options.curve != 0, &data, &ar);
```

*Excerpt from [farmer-actions.c](native/farmer-actions.c). Indices 1–4 correspond to the selector; the initial option is **Excellent**.*

[managed-read.c](native/managed-read.c) creates and validates the objects received by the call. XP comes from the game's actual response, not a fixed amount associated with the selector.

### Berries: confirm the effect before throwing

`PokemonEncounterResponse.InitialActiveItem`, the two-argument method `IItemBag.UseItemForEncounter`, and the `Status` and `ActiveItem` fields of `UseItemEncounterOutProto` were identified. The agent validates those contracts and uses the game's service.

All five berries can be selected and their priority arranged:

| Berry | Contract identifier |
| --- | ---: |
| Razz · `RazzBerry` | 701 |
| Nanab · `NanabBerry` | 703 |
| Pinap · `PinapBerry` | 705 |
| Golden Razz · `GoldenRazzBerry` | 706 |
| Silver Pinap · `GoldenPinapBerry` | 708 |

The first selected berry with available units is used, not multiple berries for the same active effect. The initial priority is **Razz → Pinap → Nanab**; the two special berries must be selected explicitly.

```c
SilphReadArgument arguments[] = {
    {.kind = SILPH_READ_ENUM32, .value.bits = (uint32_t)berry},
    {.kind = SILPH_READ_REFERENCE, .value.reference = s->target.object}
};
if (!status) {
    s->metrics.berry_requests++;
    status = silph_action_call(s->read, permit, SILPH_ACTION_BERRY,
        bag, arguments, 2, &pending);
}
```

*Excerpt from [farmer-actions.c](native/farmer-actions.c). The `UseItemEncounterOutProto` response is validated before throwing.*

If a berry is already active, another is not applied. After the Pokémon breaks out of the ball, a berry can be used again before the next attempt. A rejection does not trigger a chain of retries; the catch can continue without it.

No berry animation is awaited. **Item limits** continues deleting the selected excess, and catching uses the remaining units. XP comes from the catch receipt, not an assumed bonus for applying a berry.

### Interruptions and map continuity

Interruption handling remains **always active** during farming. It observes compatible screens opening and becoming visible and uses `Dismiss()`, `Click()`, or `LuckyBonusSkipButtonClicked()` according to the controller. It includes medals, level-ups, XP, the Daily Adventure Incense summary, the speed warning, and the **Stamp Rally** new stamp collection notice.

Egg hatches also have an adapter in `MapExploreState`: it identifies the signature that receives `HatchedPokemonsData` and skips presentation before the scene opens. Obtaining the Pokémon and reward responses remain the game's responsibility. If a hatch screen is already open, it is handled through its compatible controller.

Actions execute on the Unity thread, with the same session and focus control as Farmer. There is no continuous polling of all windows: only a bounded queue is retained when a recognized screen appears. Screens outside that list are not closed automatically.

The presentation queue is bounded, and components are retained through controlled references. When the engine closes, hooks are removed and those references are released. Implementation: [presentation-tools.c](native/presentation-tools.c).

### Field research

Field research is **always** managed during Farmer, including tasks received when spinning PokéStops. The engine queries `IQuestService`, claims tasks that the game allows to be redeemed, and processes the item, berry, XP, Stardust, or encounter rewards it receives.

The scope is identified by **QuestContext = 2**, and its encounters by **EncounterType = 6**. Special and timed research remain outside that scope. Incompatible field tasks—such as raids or trades—and tasks with an unknown type or condition are removed to make room for others; the AR scanning slot is preserved.

```c
if (context != 2 || redeemable > 1 || count > 32 ||
    (count && !conditions) || transfers > 1 || berries > 1)
    return SILPH_RESEARCH_KEEP;
if (redeemable) return SILPH_RESEARCH_CLAIM_TASK;
if (status != 1 || type == 46) return SILPH_RESEARCH_KEEP;
```

*Excerpt from [research-policy.c](native/research-policy.c). [research-actions.c](native/research-actions.c) coordinates calls and their responses.*

Reward encounters use Farmer's berry, throw-quality, and catch cycle. They are processed even if their species is not selected in **Only catch**. Afterward, **Keep field research encounters** determines whether they are retained by source; with that option disabled, the other transfer protections apply.

### Remove buddy

**Remove buddy** is disabled by default. When checked, the engine queries whether a buddy is present and requests its removal with Pokémon identifier **0**. A confirmed response leads to a game restart to continue Farmer without a buddy. If there was no buddy, it continues directly.

The `BuddyMapProto` and `SetBuddyPokemonProto` contracts are used through `IRpcHandler.SendRpc`, with operations **1350** and **152** from the game profile. Removal happens once per service start; it is not repeated during every map recovery.

Removing it does not transfer it or modify its inventory data. It is not reassigned at the end: you can set it again from the game whenever you want. Coordination of this flow is in [buddy-actions.c](native/buddy-actions.c).

### Inventory tools

**Show IV names**, enabled by default, temporarily changes the text displayed in inventory rows. For example, a Pokémon with **15/14/13** IVs is displayed as **93% 15/14/13**. The percentage is a whole number, and the name saved on the account does not change.

```c
int n = snprintf(out, capacity, "%d%% %d/%d/%d",
    (a + d + s) * 100 / 45, a, d, s);
```

*Format from [visual-tools.c](native/visual-tools.c). Applied during the session, without modifying the saved name.*

**Extended manual transfer**, disabled by default, expands inventory multi-selection for special Pokémon, including shiny Pokémon and certain legendary classes. Selection and confirmation are performed by the user from the game. Favorites, buddies, and other inventory protections remain excluded; this feature does not add existing Pokémon to the automatic transfer queue.

### Game tools: gifts and temporary name

**Fast gift opening**, enabled by default, speeds up gifts you open manually from the game while the service is active. It reduces the opening animation duration to **50 ms** and accelerates the gift and reward animators. The original request, its responses, and item delivery remain the game's responsibility.

This made it possible to identify `OpenGiftGuiController.giftOpenAnimationLength`, its `guiAnimator`, the `GiftBox` animator, and the creation of `MajorRewardSequenceView` within gift sequences. The engine applies changes only to those components and restores their values when the gift closes or the session stops.

Only local presentation waits in the two opening sequences are shortened, when the object is exactly `Niantic.Zeppelin.Scheduler.Wait` and its duration is within the supported range. Network and resource waits remain the game's responsibility.

The treatment is in [presentation-tools.c](native/presentation-tools.c). The game's global clock keeps its usual speed.

**Hide trainer name**, enabled by default, replaces the player's visible text in the map HUD, their own profile, and the friends header. It uses the `UnityEngine.UI.Text` components of those screens; it does not rename the account or modify the identity data used by the game.

The HUD is obtained from the currently running map state. Previous text is retained during the session and restored when stopping, on the Unity thread. References are limited to **48 components** and released when the engine closes; destroyed components are removed to avoid accumulating them while navigating between screens.

### Transfers and deletion

Identifiers remain **`uint64_t`** in C and 64-bit values in the Android controller. Authorization combines an issued request, a recent successful response, the correct class, the caught ID, and the current session.

```c
if (!input || silph_transfer_evaluate(input, sizeof(*input), &verdict)
    || verdict.decision != SILPH_VERDICT_CANDIDATE
    || !silph_core_authorize(scope, input->owned_id, encounter)
    || !silph_schedule_permitted(permit, true))
    return SILPH_MANAGED_STATE;
```

*Authorization gate in [managed-read.c](native/managed-read.c). Transfer calls do not accept IDs found only by scanning the inventory.*

A transfer is not counted merely because it was requested: the response is checked and the ID is reconciled against the inventory. Unknown results do not authorize irreversible retries. Item deletion uses a separate policy for types and quantities.

### Location and Error 12

The adapter uses the JNI ABI to identify `android.location.Location.isFromMockProvider()` and, if present, `isMock()`. It treats only the boolean responses corresponding to those methods within the **process it connected to**.

The treatment is installed when starting with **Fix error 12** and removed when the service stops. It resolves available methods according to the process's SDK, including the modern `isMock()` variant, and shares the same lifecycle on UGPhone and physical phones. The implementation is in [location-tools.c](native/location-tools.c).

### XP and Stardust counters

Rewards from successful responses to **catches, spins, and field research issued by SilphMod** are counted: `CaptureScoreProto.Exp`, `Stardust`, temporary/Mega Evolution rewards, `FortSearchOutProto.XpAwarded`, `Loot`, `BonusLoot`, and `CompleteQuestOutProto.Rewards`, according to the validated contract. Totals are preserved when stopping or recovering the service and reset only with **Clear statistics**.

The account balance is not subtracted, and rewards are not estimated by multiplying catches by a constant. Amounts are not multiplied again by bonuses the game may already have applied. An increasing sequence prevents duplicates; Android totals use `long`. `—` indicates an unavailable reading, and `+` indicates a partial total with missing rewards.

## Build

Requirements: JDK **17 or later**, Android SDK **36**, Android NDK **r28c / 28.2.13676358**, Git, and PowerShell. Android Studio can open the repository root. Configure `JAVA_HOME` and `ANDROID_HOME`, or create your own `local.properties` with `sdk.dir`.

```powershell
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
$env:ANDROID_HOME = "C:\Users\usuario\AppData\Local\Android\Sdk"
.\gradlew.bat :app:assembleRelease
```

Gradle compiles the C components from [native](native) through [build_native.ps1](tools/build_native.ps1). The hooking backend is obtained from a pinned ShadowHook revision and kept outside the publishable code, in `.dependencies/`. You can specify existing installations with `SILPH_NDK` and `SILPH_SHADOWHOOK`.

The build generates the **eight native `.so` components** and their SHA-256 hashes. The service verifies them again before loading the engine. These files are built from C and the hooking dependency. Gradle includes them in the APK from `app/src/main/jniLibs/arm64-v8a/`.

Without signing configuration, the release output is **unsigned**. To sign it, provide `SILPH_KEYSTORE`, `SILPH_STORE_PASSWORD`, `SILPH_KEY_ALIAS`, and `SILPH_KEY_PASSWORD`. Keys remain outside the repository; updating an existing installation uses the same certificate.

Android output: `app/build/outputs/apk/release/`. APKs, generated components, caches, logs, and keys are excluded from Git.

## Logs and privacy

Export is local and manual. It includes up to **three recent sessions** and build identification. It records operational settings, process status, startup routes, responses, counters, and resource release. Each log has a maximum size of **8 MB**. Newly saved events filter out the control-channel key.

Files are saved in the app's private storage. **Export logs** creates a ZIP in `Descargas/SilphMod/`. Logs may contain coordinates and device identifiers; they do not include sensitive and/or confidential data.

## Structure

```text
SilphMod/
├── app/                  # Interface, service, settings, and resources
├── native/               # C engine, startup, and policies
│   └── standalone/       # Process connection and control
├── docs/assets/          # Identity and visual resources
├── tools/                # Native build and resource provenance
├── gradle/               # Build wrapper
├── LICENSE
├── NOTICE
├── spanish.md
└── README.md
```

## Status and upcoming improvements

**1.6.0 introduces selectable throw quality**, berry use by priority, selective interruption handling, catch and spin session limits, continuity with the notification panel open, and an action interval of **100–3000 ms**, initially **150 ms**. Transfers and discards retain their own unlimited total quotas; the entire session ends when either the catch or spin maximum is reached.

**1.7.0 adds automatic map recovery with up to ten startup attempts**, persistent statistics with manual clearing, startup blocking when accumulated limits have been reached, and keep filters for Dynamax, hatched Pokémon, backgrounds, and the shiny + background combination.

**1.9.0 brings Farmer together in a single APK for UGPhone and rooted physical phones**, introducing **Only catch**, automatic field research, **Remove buddy**, temporary IV names, and extended manual transfer. It adds the game's observed encounter range of up to **50 m**, bounded priority for finishing active catches, less repeated reading, and log export improvements. Interruption handling includes hatch presentation, the speed warning, and the Stamp Rally notice.

**1.9.3 updates the profile to Pokémon GO 0.433.1**, adds **Fast gift opening** to speed up manual gift opening, and **Hide trainer name** to temporarily display SilphMod instead of the player's name. It integrates the adaptive injector with alternative routes and reuse of the connection that worked for that environment.

**2.0.0 introduces Kecleon (BETA)** as a separate mode, with candidates, bounded movements, catch filters, and per-account history. Farmer retains independent settings and results.

The engine moves to **C**, with scheduling on the Unity thread, controlled-lifetime references, and fewer repeated readings while waiting for responses. Startup combines four alternative routes in a single APK. Field research, inventory tools, fast gift opening, buddy removal, and recovery after five minutes of an empty map are integrated.

**2.0.15 strengthens native component integrity and the control channel**. Session credentials are delivered through a private pipe, unauthenticated connections expire, and diagnostic logs exclude the control key. Startup routes and catch and transfer policies are preserved.

Farmer adds continuity in compatible game menus, more frequent catch-phase checks with **Action interval** at **100 ms**, and bounded metadata caches. Berries, research, spins, cleaning, and transfers retain coordination with a single pending operation.

The next phase is to continue optimizing continuity in both modes and expand compatibility across devices.

Configuration of the priority order for Poké Ball use is planned, rather than the default: 1. Poké Ball; 2. Great Ball; 3. Ultra Ball.

Another pending improvement is to keep expanding the catalog of Pokémon that can be configured not to transfer, for example nundos (0/0/0).

[MIT](LICENSE) license. Dependencies retain their notices in [NOTICE](NOTICE) and their licenses within the assets. An independent, personal project, with no affiliation to the game's rights holders or other parties.

Take care, little guy.
