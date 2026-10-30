![SilphMod — Root Android](docs/assets/banner.svg)

# SilphMod

**English · [Español](spanish.md)**

Local automation for the original Pokémon GO on **rooted Android**, both on UGPhone and on physical ARM64 phones. It catches nearby Pokémon, spins PokéStops, collects field, weekly, special and event research, claims GO Pass rewards, transfers new catches according to your filters and keeps the bag within the limits you choose.
The **Kecleon (BETA)**, **AutoRocket (BETA)** and **Event tricks** modes have their own settings, notification and statistics.

You configure the features, press **Start service** and follow the progress from the notification. **Stop** and **Export logs** let you stop the session and review its results. The interface, notifications and visual assets are in English.

**Edition:** `2.3.15` · [MIT](LICENSE)

**Platform:** Android 10 or later, ARM64 and root. One APK for both UGPhone and physical phones.

<details>
<summary>Contents</summary>

- [How it works](#how-it-works)
- [Compatibility](#compatibility)
- [Features](#features)
  - [Kecleon (BETA)](#kecleon-beta)
  - [AutoRocket (BETA)](#autorocket-beta)
  - [Event tricks](#event-tricks)
  - [Only catch: choosing which Pokémon to catch](#only-catch-choosing-which-pokémon-to-catch)
  - [Select Pokémon to keep](#select-pokémon-to-keep)
  - [Buddy](#buddy)
  - [Transfer without touching the previous collection](#transfer-without-touching-the-previous-collection)
  - [Bag: limits per type, not indiscriminate deletion](#bag-limits-per-type-not-indiscriminate-deletion)
  - [Catch and spin limits](#catch-and-spin-limits)
- [Usage](#usage)
- [Architecture](#architecture)
  - [Startup and continuity](#startup-and-continuity)
  - [Integrity and control channel](#integrity-and-control-channel)
  - [Automatic map recovery](#automatic-map-recovery)
  - [Catches and game calls](#catches-and-game-calls)
  - [Capture range and priority](#capture-range-and-priority)
  - [Incense and event actions](#incense-and-event-actions)
  - [Throw quality](#throw-quality)
  - [Berries](#berries-confirm-the-effect-before-throwing)
  - [Interruptions and map continuity](#interruptions-and-map-continuity)
  - [Field research and weekly achievement](#field-research-and-weekly-achievement)
  - [Special and event research](#special-and-event-research)
  - [GO Pass](#go-pass)
  - [Remove buddy](#remove-buddy)
  - [Inventory tools](#inventory-tools)
  - [Game tools: gifts and temporary name](#game-tools-gifts-and-temporary-name)
  - [Transfers and deletion](#transfers-and-deletion)
  - [Location and Error 12](#location-and-error-12)
  - [XP and Stardust counter](#xp-and-stardust-counter)
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
| Environments | UGPhone and rooted ARM64 physical phones |
| Architecture | **ARM64**, 64-bit pointers |
| Game | Original Pokémon GO **0.435.1**, `versionCode 2026102802` |
| Game engine | Unity **6000.3.22f1**, IL2CPP |
| APK | `minSdk 29`, `targetSdk 36` |
| Optional Error 12 handling | JNI `Location` contracts resolved according to the system version |
| Connection | Adaptive selection among four startup routes, reusing the one that worked for the environment |
| Installed identity | `dev.rootcatch.ugphonetest`, kept so earlier installations can be updated |

The app brings together **Farmer**, **Kecleon (BETA)**, **AutoRocket (BETA)** and **Event tricks**, with independent execution and separate statistics. Startup adapts to the **Android of the device or instance**.

## Features

| Feature | Current behavior |
| --- | --- |
| Auto-catch | Selects the nearest eligible target within **50 m**, and checks again that it exists and its distance before throwing. Retries while the encounter remains valid; if it disappears or goes out of range, moves on to the next target. |
| Incense | Catches the spawns of regular incense and daily incense, also when their durations stack. Keeps the Farmer's species filters, berries and transfer protections. |
| Automatic event actions | Interacts with Halloween boxes within **50 m**, collects their rewards and catches the Pokémon they deliver. Initially enabled in Farmer. |
| Only catch | Selection of one or more species with thumbnails, search and Pokédex order. With no selection, it catches all eligible species. Field research, weekly achievement, special, event and GO Pass encounters are caught regardless of this filter. |
| Select Pokémon to keep | Keeps the selected species and forms when the Farmer catches them, even if they do not meet other keep filters. Does not change the Only catch selection. |
| Buddy | **Feed buddy** feeds it according to the berry priority and **Mega Evolve buddy** uses the lowest-cost mega evolution available. Detects buddy changes during the session. |
| Throw quality | Selects **Normal**, **Nice**, **Great** or **Excellent**. Throws keep the curveball spin. |
| Berries | Uses the first available berry according to the configured selection and priority, confirms it was applied and continues with the throw. Maintains the Item limits. |
| PokéStops | Sends spins for targets within the range configured in the engine and counts successful responses. |
| Session limits | Configurable maximums of catches and spins, initially **4,800** and **2,400**, applied to the accumulated statistics. Reaching either one stops farming, closes Pokémon GO and saves the logs. |
| Interruptions | Automatic closing of medals, level-ups, XP rewards, the daily incense summary, hatches, the speed warning, the Stamp Rally notice, friendship level-ups and Willow research dialogs. Prepared from engine start. |
| Field and weekly research | Claims available rewards, catches their encounters and collects the pending weekly achievement before continuing with field tasks. Discards incompatible tasks, including walking kilometers and earning Candy with the buddy. Keeps the AR scan slot; discarding is limited to field tasks. |
| Special and event research | Claims the available rewards from tasks and stages, and catches their encounters. Keeps incomplete tasks and leaves branch choices to the user. |
| GO Pass | Claims the unlocked rewards of the current pass and catches their encounters. Items, XP and Stardust are counted according to the game's responses. |
| Remove buddy | Removes the buddy at start, without transferring it or reassigning it at the end. Initially disabled. |
| Transfers | Only Pokémon caught by **this SilphMod session**, with a credited catch response and exact ID. |
| Keep shiny / 15/15/15 | Two independent filters. Meeting any of the active filters prevents that Pokémon from being transferred. |
| Keep Dynamax / hatched / background | New protection filters for catches authorized by the service. They do not scan or modify the previous collection. |
| Keep shiny + background | Protects Pokémon that meet both conditions. Combined with the other keep filters. |
| Keep research encounters | **Keep field research encounters** protects catches from field research, the weekly achievement, special research and event research. If disabled, the shiny, IV and other global protections still apply. |
| Keep GO Pass encounters | **Keep GO Pass encounters**, initially enabled, protects the pass catches by their origin. Legendaries remain protected even with this box unchecked. |
| Show IV names | Temporarily shows the whole-number percentage and the IVs in the inventory. **15/15/15** appear in red in the list and on their detail screen, without renaming the account's Pokémon. |
| Extended manual transfer | Extends the manual multi-select of special Pokémon from the game's inventory. Independent from automatic transfers. |
| Fast gift opening | Speeds up manual gift opening, the presentation of its items and the local waits of that sequence. Initially enabled in **Game tools**. |
| Auto-open gifts / Auto-send gifts | Opens and sends gifts to available friends during the session, without stopping the Farmer. With a full bag, opening requests conversion to Stardust. Both options are initially disabled. |
| Hide trainer name | Temporarily shows **SilphMod** on the HUD, own profile and friends header from the moment its components connect at engine start. The account name does not change. Initially enabled in **Game tools**. |
| Clean bag | Deletes surpluses of the selected types **from the start**, and maintains those limits during the session. It does not need to wait for the bag to fill up. |
| Kecleon (BETA) | Automatic search and catching of candidates, with filters and per-account history. This is still under development. |
| AutoRocket (BETA) | Goes through regular Grunt candidates only (for now), validates the incident, battles, catches according to the filters and heals the team between battles. Includes recommended team, two shields and a return location for catches (Alan Method). |
| Event tricks | For now, collects rewards from the event boxes and repeats confirmed Candy collections, prioritizing Rare Candy. Has independent statistics and does not perform catches or spins. |
| Fix error 12 | Location handling inside the game process, optional and tied to the session. |
| Statistics | Accumulated catches, flees, spins, transfers, deleted items, XP, Stardust and active time. Kept when stopping, restarting and recovering the map. |
| Clear statistics | Resets only the counters of the open mode, with the service stopped. In Kecleon it also clears its candidate history; in AutoRocket it deletes the history and the return location. Found below the statistics. |
| Map recovery | After five minutes of valid readings with no spawns in the map directory, restarts the game and reconnects the engine. Up to **10 startup attempts per recovery**, cancelable with Stop. Disabled when species are selected in **Only catch**. |
| Notification bar | Catches and spins continue when it is expanded if Pokémon GO remains the underlying app and the screen is unlocked. It also continues in the bag, the Pokémon inventory, the individual Pokémon detail screen, the own profile, the shop and the Pokédex. Informational notices of mission and reward progress allow it to continue while the map remains visible. |
| Integrity and control | Component verification before startup and a local channel bound to the process and the session, with credentials kept out of arguments and new logs. |
| Connection recovery | Retries recoverable native channel disconnections, keeps statistics and re-validates the process and the account. Retries respect Stop and the session limits. |
| Logs | Manual export of up to three recent sessions in a local ZIP. |

Farmer uses the route you configure in your joystick. Kecleon and AutoRocket need the public integration of **GPS JoyStick by The App Ninjas** to make their automatic jumps.

### Kecleon (BETA)

This mode goes through **Moonani** candidates, moves with GPS JoyStick, looks for the corresponding PokéStop and checks its active incident. It resolves `fort_id`, incident and `encounter_id` from the game before opening the encounter.

The HTML is processed row by row and candidates are stored in a temporary queue inside the app's private storage, without loading the whole page into memory. The list can grow without the previous caps on size, rows or candidates. Entries are offered one by one to the engine, with the same per-account history to avoid repeats. When the session is stopped, the temporary queue is deleted.

Map loading is accompanied by bounded movements of **eight meters** and spaced-out readings. A timeout is saved as **unverified**, not as proof that there was no Kecleon there. The history keeps accounts separate and avoids processing already-resolved candidates again.

The filters **Only catch 15/15/15**, **Only catch 0/0/0**, **Only catch shiny** and **Only catch shiny + 15/15/15** are combined with “OR”. The last option requires both shiny and perfect IVs at the same time. With no filters, all valid Kecleon found are caught.

**Maximum encounters** limits its accumulated encounters. On reaching the maximum or exhausting the pending candidates, the game is closed and the reason is shown. **Clear statistics** deletes its counters and the candidate history. Farmer keeps its own results.

Kecleon shares throw quality, berries, bag limits and inventory and presentation tools. Its statistics show **Encounters**, **Catches**, **Shinies**, **Items deleted**, **XP** and **Stardust**. It does not spin PokéStops or transfer the collection.

### AutoRocket (BETA)

This mode goes through regular Grunt candidates from **NYCPokeMap**, checks their appearance and disappearance times and validates in the game the PokéStop and its active incident. The feed refreshes every **30 seconds**. If it finds a PokéStop without the expected Grunt or with a Kecleon, it moves on to the next candidate. This edition works with regular Grunts for now.

You can donate, as thanks for the great work of the creator of that free map: https://www.paypal.com/paypalme/SGPokeMap/10USD

Map loading uses bounded movements of **eight meters** and spaced-out readings. Battle entry, attacks, shields and catching are performed through the game's services. The entry and result dialogs, the battle presentation and the obtaining of Purification Gems are skipped.

**Use recommended team**, initially enabled, prepares the recommended team for each battle. When disabled, it uses the user's selection. It checks the team's health before starting, uses both shields at the first opportunities and throws the available Charged Attacks, including the second Charged Attack. If it loses, it closes the defeat screens and continues with the team's recovery.

Between battles it heals and revives only the three Pokémon used. **Potion priority** and **Revive priority** let you choose the order in which items are used. It uses as many applications as needed until the team is fully recovered. If items are missing, it checks whether it can prepare a viable team before starting another battle.

**Catch all**, initially enabled, catches all valid Shadow encounters. When disabled, it lets you combine **Only catch 15/15/15**, **Only catch 0/0/0**, **Only catch shiny**, **Only catch shiny + 15/15/15** and **Only catch 13/13/13 or above**. Filters are combined with “OR”; shiny + 15/15/15 requires both conditions, and 13/13/13 or above requires each IV to be at least 13.

In **Engine settings**, **Last cooldown location** lets you specify return coordinates in order to catch from the first accepted encounter. If the field is empty, it uses the location of the first confirmed Rocket catch of that session. After each battle, it returns to that point before catching the encounters that meet the filters. An encounter discarded by the filters does not set that location.

**Maximum Rockets** limits accumulated completed battles and defaults to **700**. The statistics show **Rockets**, **Catches**, **Shinies**, **Items deleted**, **XP** and **Stardust**. **Clear statistics** resets those counters, the candidate history and the return location. Throw quality, berries and bag limits remain in the shared configuration.

Encounters are caught without opening the camera. If the game presents the detail screen of the newly caught Pokémon or a size record, only the screen associated with that catch is closed. **Stop** cancels pending actions and leaving for another app pauses the mode; scrcpy, screenshots and recording keep the same focus handling as the Farmer. AutoRocket keeps its catches and does not transfer the collection.

### Event tricks

This mode is dedicated to collecting rewards from the event boxes while you follow your joystick route. In this edition it works with the Halloween boxes, with species Candy, Rare Candy. Pokémon encounters are left uncaught; it does not spin PokéStops or run the Farmer's auxiliary tasks.

After a confirmed collection with Candy, it processes the box again while it remains available. When a response delivers Rare Candy, that box gets preference in the following collections. It also keeps room to discover new boxes and continue with species Candy.

It works with the boxes known to the game, without adding its own distance filter. Quantities are counted from confirmed responses. The contents of a box are known when it is opened; rejected or unconfirmed responses keep their wait before continuing.

The statistics show **Boxes opened**, **Pokémon Candy**, **Rare Candy / XL**, **Candy XL**, **XP** and **Stardust**. Repeated collections count as new operations on the same box. **Clear statistics** resets only this mode's counters.

Collection continues in compatible menus and with the notification bar expanded. Leaving for another app or locking the screen pauses the mode, and it resumes on returning to the game. It includes **Fix error 12**, **Hide trainer name** and log export.

### Only catch: choosing which Pokémon to catch

**Only catch** lets you choose one or more species for the following sessions. The selector uses a local catalog with thumbnails, Pokédex order and search by name or number. The selection is saved; if you leave it empty, the farmer goes back to catching all eligible species.

The catalog includes Alolan and Galarian regional forms, in addition to the regular species. It adds Minior, Mimikyu and Bramblin. Minior's general option lets you catch its spawns regardless of the color revealed after catching it.

The filter is applied before opening a wild encounter and is checked again before using a berry or throwing. Selecting a species does not modify its IVs or condition the keep filters: these are applied after catching.

The selector prepares a species mask that the engine consults before opening the encounter. Field research, weekly achievement, special, event and GO Pass catches use separate reward origins and are not blocked by **Only catch**.

When species are selected, the five-minute recovery for no Pokémon is not activated: the farmer can wait for those species to appear without restarting the game. **Stop**, the accumulated limits and connection control keep working the same way.

### Select Pokémon to keep

This selection appears below **Keep GO Pass encounters**, within the Farmer's transfer options. It uses the same catalog, thumbnails, search and Pokédex order as **Only catch**, including regional forms.

Selected Pokémon are kept when the Farmer catches them, regardless of whether they meet the other keep filters. **Only catch** still decides which wild spawns are caught; this selection does not force catching excluded species.

Initially it keeps **Galarian Articuno**, **Galarian Zapdos**, **Galarian Moltres**, **Gimmighoul** and **Meltan**. It also applies to incense catches and to box encounters when the game delivers them. The other global transfer protections remain active.

### Buddy

The Farmer's **Buddy** section includes **Feed buddy**, its own **Berry priority** and **Mega Evolve buddy**. Both boxes are initially disabled.

Feeding starts when the service starts if the buddy needs it and is checked again during the session. It uses the available berries in the chosen order, detects if you change buddy and respects the bag limits. If there are no berries, it waits until they are available again while the Farmer continues.

Mega evolution checks the current buddy's options and chooses the lowest-cost one available. It can use Mega Energy if needed; if the energy is not enough or the buddy has no valid option, farming continues.

If there is no buddy or **Remove buddy** is checked, these actions are not performed.

### Transfer without touching the previous collection

The transfer queue starts empty. A Pokémon enters only when a catch request issued by this session gets a valid, successful response with its storage ID. Finding an ID in the collection is **not enough** to authorize its transfer.

In addition to the chosen filters, favorites, buddies, eggs, deployed Pokémon, fusions, legendary, mythical or singular Pokémon, Ultra Beasts and records whose protection information cannot be validated are kept. Unchecking **Keep shiny** or **Keep 15/15/15** allows those new catches to be transferred if there is no other protection.

**Keep Dynamax**, **Keep hatched**, **Keep background** and **Keep shiny + background** are included. The first three are initially enabled; the shiny + background combination is initially disabled. Meeting any of the checked filters keeps the Pokémon. Within **Keep shiny + background**, however, both conditions must be met at once.

If you check **Keep shiny** together with **Keep shiny + background**, all shinies are kept, with or without a background. If you check **Keep background** together with the combination, all those with a background are kept. To keep only shinies with a background using these filters, leave only the combination checked and uncheck the other optional filters. The engine still catches the other Pokémon and transfers them if there is no other protection.

The hatched protection is looked up in the record authorized for transfer; it does not turn hatches into new authorizations or add Pokémon from the previous collection to the queue.

**Keep field research encounters**, initially enabled, keeps by origin the catches from field research, the weekly achievement, special research and events. **Keep GO Pass encounters**, also initially enabled, does the same with the pass catches. Disabling one of these options allows the catches from that origin to be transferred when they meet no other protection.

**Keep shiny**, **Keep 15/15/15**, **Keep shiny + background** and the other global filters apply to all origins: wild, field, weekly, special, event and GO Pass. Filters are combined with “OR”: meeting one is enough to keep the Pokémon. Legendary, mythical or singular Pokémon and Ultra Beasts remain protected in all origins, even if their keep boxes are unchecked. Common encounters can be transferred according to the remaining filters.

Transfers are initially disabled and have **no total quota per session**. Old transfer quotas are not recovered. Authorizations are not inherited between sessions, accounts or restarts.

Authorization is implemented in [core.c](native/core.c), [farmer-actions.c](native/farmer-actions.c) and [transfer-policy.c](native/transfer-policy.c).

### Bag: limits per type, not indiscriminate deletion

An enabled box allows the surplus of that type to be deleted; the number indicates how much to keep. An unchecked type is not deleted. For example:

```text
Potion=50,SuperPotion=50,Revive=30,NanabBerry=30
```

With 83 potions and a minimum of 50, the plan allows deleting 33. Each request deletes at most 50 units, **with no total quota per session**. The engine checks the response and the inventory synchronization before continuing.

There are **33 selectable types**, including three regular Poké Balls, four potions, two revives, five berries and other allowed consumables. The catalog contains another **17 protected types**; Master Ball, Safari Ball, Premier Ball, passes, incubators and unknown items are not authorized for discarding. You can review the IDs and restrictions in [ItemCatalog.java](app/src/main/java/dev/rootcatch/nativeengine/ItemCatalog.java).

Cleanup is initially disabled. No total quota of discarded items is applied. It can delete items that were already in the bag, according to the selected types and minimums. An ambiguous response or a known rejection pauses the cleanup without repeating the request; the other features can continue.

### Catch and spin limits

In **Engine settings → Session limits**, **Maximum catches** starts at **4800** and **Maximum PokéStop spins** at **2400**. Both accept a positive maximum and keep the chosen value for the following sessions.

The counters are kept between service starts and automatic recoveries. Only successful responses to actions issued by SilphMod consume the limit: not failed attempts, flees or manual catches. They are maximums over the accumulated local statistics.

If the accumulated catches or spins already reach their maximum, **Start service** is blocked and shows **Capture limit reached** or **Spin limit reached**. To continue, raise the limits above both totals or use **Clear statistics** with the service stopped. Stopping the farmer does not delete the results; clearing statistics does not delete the logs either.

When either of the two maximums is reached, no more actions are sent. The agent removes its hooks, releases its references and confirms the stop; then Android verifies the process identity, closes Pokémon GO and saves the logs with the reason and the final results. **Export logs** lets you retrieve the ZIP. Closing does not delete the game's cache, storage or settings.

## Usage

Before anything else, remember that you must have a correctly configured GPS joystick app.

1. Install the APK from the releases section, open it and grant root permission and notifications (if it asks for them). Keep your joystick configured and running. Do not run other automations or injectors at the same time. Set SilphMod to unrestricted battery mode.

2. Select **Catches**, **Only catch**, **Throw quality**, **Use berries**, **Berry priority**, **Remove buddy**, **Spin PokéStops**, transfer filters and **Items to keep**, according to what you want to use. In Farmer, **Automatic event actions** controls the interactions with the event boxes. In AutoRocket, choose **Catch all** or its filters, **Use recommended team**, **Maximum Rockets** and, if you want, **Last cooldown location**. In **Inventory tools** you can enable IV names and extended manual transfer. In **Game tools** you can choose **Fast gift opening**, **Auto-open gifts**, **Auto-send gifts** and **Hide trainer name**. The **Keep field research encounters** and **Keep GO Pass encounters** filters let you keep the catches from those rewards. Adjust the catch and spin maximums in **Engine settings**. Transfers and discards have no total maximum of their own per session, that is, they delete an unlimited amount of the items you have configured to discard while the session remains active.

3. Set a **travel route** in your GPS joystick app or simply use this app's joystick to move around the map. Remember that the **travel route** is recommended for greater effectiveness of the mod over a longer time.

4. If your device or instance needs the location handling, check **Fix error 12** and press **Start service**. SilphMod opens Pokémon GO and prepares the connection appropriate to the system. Startup may close only the game and open it again to install the early handling; it does not delete cache, storage or joystick settings. If you enabled **Remove buddy**, the **Removing buddy…** status accompanies its removal and the restart needed to continue without a buddy.

5. Wait for **Active**. There is an automatic check of the map, player and references before actions are sent.

6. Press **Stop service** or **Stop** in the notification when you want to end the session. And only if you want to save the error report, wait for **Saving logs…** to finish and use **Export logs**. The ZIP is saved in `Downloads/SilphMod/`.

The service has no 30/60-second timer or fixed limit of hours. To stop it, you must open the app's notification and tap stop or, failing that, close Pokémon GO and return to the Mod app.

Switching apps pauses the actions; losing the process, switching accounts or a prolonged loss of the controller ends the session. Opening the notification bar lets you review the statistics without pausing catches or spins, as long as the game remains underneath the panel and the screen is unlocked. When switching to another app or locking the screen, actions are paused. (Remember that on UGPHONE the game can run in the background if you close the instance, but not if you close Pokémon GO or SilphMod).

Inside the game, the Farmer can continue when opening the bag, the Pokémon inventory list, the individual Pokémon detail screen, the own profile, the shop or the Pokédex. Selecting several Pokémon or entering a screen that is not part of this flow pauses the actions until you return to a compatible view.

If Pokémon storage fills up, catches are paused and the state is reported; SilphMod does not delete the previous collection to make room. With a full bag and no authorized surplus, it does not delete other items on its own either. Stopping cannot revert an operation that has already been sent.

## Architecture

![SilphMod architecture](docs/assets/architecture.svg)

SilphMod has two parts that cooperate through a local control channel:

- **Android / Java:** interface, configuration, foreground service, root permission, process identity, focus, notification, Kecleon candidates and export.
- **Engine / C:** IL2CPP service resolution, action scheduling on the Unity thread, response reading, inventory policies and reference lifecycle.

The engine uses objects and methods of the original process; the game client keeps serializing its requests and communicating with its server. The Kecleon and AutoRocket modes also query their candidate sources over HTTPS. Account and Pokémon identifiers are not sent to that source.

### Startup and continuity

**Start service** opens Pokémon GO and checks version, libraries, process and their startup times. When **Fix error 12** is active, it prepares the location adapter first. Access to IL2CPP objects waits until the game's libraries are present and the **20-second** stability period is met.

Startup offers four combinations: **ATTACH / FD**, **ATTACH / PATH**, **SEIZE / FD** and **SEIZE / PATH**. They change the connection mechanism and how the library is opened, not the engine or its policies. The route that worked is tied to the device and the game profile. There is a maximum of **ten attempts**, with confirmation that the previous attempt has exited before continuing.

Execution is bound to the process, the session and the account. Heartbeats and the focus query are kept separate from log writing and from the feed's network operations. The scheduler runs actions on the Unity thread and coordinates their pending requests. Event tricks uses a bounded queue to collect several boxes without blocking the rest of the interface.

Focus reading distinguishes the underlying Pokémon GO, notifications and system overlays. **scrcpy**, screenshots and recording are not interpreted on their own as an app change. Opening another application or locking the screen pauses the actions; **Stop** cancels the service.

The Farmer maintains its cycle from the compatible views of the bag, Pokémon inventory, individual detail screen, own profile, shop and Pokédex. The engine checks the active screen before acting and respects manual multi-select. The same process, session and focus checks continue to apply outside the map.

Recoverable native channel disconnections are handled with up to **ten consecutive attempts**, according to the process state and the recorded cause of its exit. Each new connection keeps the statistics, re-validates the player's identity and starts with no previous transfer authorizations or pending requests to repeat. **Stop** and the accumulated limits cancel the recovery.

### Integrity and control channel

Before startup, the **SHA-256** fingerprints of the native components are checked. The reading requires regular files, canonical paths and permissions without group or other write access; it rejects symbolic links and changes in size or identity during verification.

The service and the engine communicate through a local **AF_UNIX** socket, with checks of process credentials, session identity and message sequence. Keys are delivered to the controller through a **private pipe**, not as process arguments. A connection that does not authenticate within **five seconds** is closed; that deadline does not limit an already authenticated session.

New logs exclude the control key. On finishing, the restoration, disconnection and resource-release checks are kept. The implementation is spread across [NativeComponentIntegrity.java](app/src/main/java/dev/silphmod/nativecheck/NativeComponentIntegrity.java), [control.c](native/control.c), [control-client.c](native/control-client.c) and [LogPrivacy.java](app/src/main/java/dev/silphmod/nativecheck/LogPrivacy.java).

### Automatic map recovery

The Farmer queries the number of spawns in the map directory once per second. If it accumulates **five continuous minutes of valid readings without spawns**, it waits until no action is pending, stops the engine and confirms the release of references and of the location handling. Then it closes only the verified Pokémon GO process, opens the game and reconnects with the same configuration and account.

This feature stays active during general farming and does not require a checkbox. A reading with Pokémon resets the counter. Invalid samples, a prolonged lack of readings, pauses or screen lock do not count as an empty map. **Only catch** disables this recovery while species are selected.

```c
if (count) { s->empty = 0; return false; }
if (!s->empty) { s->since = now; s->empty = 1; }
return now - s->since >= UINT64_C(300000000000);
```

*Excerpt from [recovery-policy.c](native/recovery-policy.c), with a monotonic clock in nanoseconds.*

Each recovery has **up to ten startup attempts**. **Stop** also cancels the retries. The statistics and their accumulated limits are kept; the new connection validates the player again and does not inherit the previous process's transfer queue.

### Catches and game calls

Reverse engineering was used to identify contracts, structures and signatures of the supported profile. The engine resolves classes and services at runtime and validates what it is going to use; it does not distribute the game's library or its decompiled metadata.

| Operation | Entry point used in profile 0.435.1 |
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
| Field research | `IQuestService` and `IQuestRpcService.CompleteQuest`, `RemoveQuest` and `BeginQuestEncounter` |
| Special and event research | `IQuestService.get_QuestsProto`, `QuestProtoExt.CanRedeem` and `IQuestRpcService.CompleteQuest` with stage and subtask ID |
| Weekly achievement | `IQuestStampCardService`, `IQuestRpcService.CompleteStampCard` and pending encounter of type `5` |
| GO Pass | `IEventPassService` and `ClaimEventPassRewardsRequestProto`; operations `3034` and `3035` |
| Automatic gifts | Friends services, gift inventory and the game's open or send responses |
| Manual gifts | `OpenGiftGuiController.OnOpenClick()`, gift animators and local presentation sequences |
| Temporary name | Texts of `PlayerHudGuiController`, `PlayerProfilePage` and `FriendsListHeaderCellView` |
| Event boxes | `DynamicTappablesSession`, `TAPPABLE_TYPE_BOX` controllers and `ProcessTappableOutProto` response |
| Rocket battles | Incident services, team preparation and synchronized battle state |

The engine keeps **50 m for wild catches** and **80 m for spins**. It re-checks ID, existence and distance before acting, which matters when the joystick follows a route.

Ball choice uses the first one available in the order Poké Ball → Great Ball → Ultra Ball. **Throw quality** lets you choose the profile and **Curve ball** controls the spin.

**Action interval** accepts **100–3000 ms**, with an initial value of **100 ms**. It separates operations; it is not equivalent to completing a catch in that time. Responses and inventory reconciliation keep their own cycle.

With the interval at **100 ms**, pending catch phases are checked every **33 ms** from the game's original update. When that operation finishes, the configured interval is restored; larger values keep their cadence. The scheduler can chain up to **three internal steps** when the previous phase finishes, without sending simultaneous requests or moving a transfer ahead of the catch confirmation.

```c
status = silph_action_call(s->read, permit, SILPH_ACTION_CAPTURE,
    s->target.object, args, 2, &promise);
```

*Actual call from [farmer-actions.c](native/farmer-actions.c). The arguments and the session permit are validated before invoking the game method.*

Catch responses distinguish successful result, ball escape, flee, missed throw, catch error and item replacement. A missed throw can continue if the encounter is still valid. A known result without a confirmed catch allows that target to be deferred and the engine to continue with others; the counters and the transfer authorization are only updated with a recent, successful response with a valid ID.

### Capture range and priority

Seeing a Pokémon on the map and being able to catch it are different states. The engine selects targets within **50 m** and re-validates the encounter before using berries or throwing. If the route leaves it out of range, it continues with other valid targets.

Scanning, catch responses, research, cleanup and transfers share a scheduler. Re-reading the whole inventory is avoided while a response is pending. Transfers keep their individual authorization and are never executed in parallel with another engine operation.

Field and method resolution uses bounded caches of validated metadata. Inventory values and game responses are re-read when appropriate; the cache does not keep Pokémon or authorizations from another session. Research, spins and cleanup keep their turn before the next target is chosen.

Coordination is in [farmer-session.c](native/farmer-session.c); target selection and reading, in [game-read.c](native/game-read.c).

A temporary failure when requesting the encounter or a target that momentarily goes out of range can be checked again after **15 seconds**. Uncertain catch or transfer responses keep their independent handling and do not generate automatic resends of that operation.

### Incense and event actions

The Farmer includes regular and daily incense spawns in its target selection. It reads the spawn type and uses the corresponding encounter, with the same **Only catch**, berry and throw quality filters. The cycle queries the game's active spawns, without depending on a fixed incense duration.

**Automatic event actions**, initially enabled, adds automatic interaction with the Halloween boxes. It selects visible, current boxes within **50 m**, identifies each one by its map data and avoids repeating the interaction. It collects item or Candy rewards and hands Pokémon encounters to the Farmer's catcher, regardless of **Only catch**.

Box encounters keep the catch check and the global transfer protections. Their identification persists for a bounded interval after a confirmed catch so that a delayed camera opening cannot leave the Farmer stuck. Encounters that the user opens manually follow their normal flow.

Box scanning runs every **two seconds**, when the Farmer is available on the map. It skips repeated reads of boxes already processed, releases temporary references and maintains coordination with catches, berries, spins, research and transfers. The specific action of this box adapts to the active event that appears in the game.

Implementation: [event-actions.c](native/event-actions.c), [event-view.c](native/event-view.c) and [farmer-actions.c](native/farmer-actions.c).

### Throw quality

The `Niantic.Holoholo.Encounter.PokeballThrow` contract distinguishes **Normal = 1**, **Nice = 2**, **Great = 3** and **Excellent = 4**. The engine prepares the throw structure with `ReticleSize`, `HitBullseye`, `Missed` and `Spinning`, and passes it to `MapPokemon.TryCapture`.

```c
const float profiles[] = {.5f, 1.1f, .85f, .5f, .05f};
status = silph_action_capture_data(s->read, permit, (int32_t)ball,
    profiles[s->options.quality], s->options.quality != 0,
    s->options.curve != 0, &data, &ar);
```

*Excerpt from [farmer-actions.c](native/farmer-actions.c). Indices 1–4 correspond to the selector; the initial option is **Excellent**.*

[managed-read.c](native/managed-read.c) creates and validates the objects the call receives. XP is obtained from the game's actual response, not from a fixed amount associated with the selector.

### Berries: confirm the effect before throwing

`PokemonEncounterResponse.InitialActiveItem`, the two-argument method `IItemBag.UseItemForEncounter` and the `Status` and `ActiveItem` fields of `UseItemEncounterOutProto` were identified. The agent validates those contracts and uses the game's service.

All five berries can be selected and their priority ordered:

| Berry | Contract identifier |
| --- | ---: |
| Razz Berry · `RazzBerry` | 701 |
| Nanab Berry · `NanabBerry` | 703 |
| Pinap Berry · `PinapBerry` | 705 |
| Golden Razz Berry · `GoldenRazzBerry` | 706 |
| Silver Pinap Berry · `GoldenPinapBerry` | 708 |

The first chosen one with available units is used, not several berries for the same active effect. The initial priority is **Razz Berry → Pinap Berry → Nanab Berry**; the two special berries are selected explicitly.

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

If a berry is already active, another one is not applied. After the ball breaks out, a berry can be used again before the next attempt. A rejection does not trigger a chain of retries; the catch can continue without it.

No berry animation is awaited. **Item limits** keeps deleting the selected surpluses and the catch uses the remaining units. XP comes from the catch receipt, not from an assumed bonus for having applied a berry.

### Interruptions and map continuity

Interruption handling is prepared **from engine start** and stays always active during farming. It observes the opening and visibility of compatible screens and uses `Dismiss()`, `Click()` or `LuckyBonusSkipButtonClicked()` depending on the controller. It includes medals, level-ups, XP, the daily incense summary, the speed warning, friendship level-ups and the notice of a new stamp collection in **Stamp Rally**. Willow's research dialogs are skipped through `QuestDialogGuiController.ClickSkip()`, using the game's own skip action.

Medals are recognized both when their progress increases and when a new category is received. The handling includes the gym badge controllers, Vivillon sub-medals and avatar customization rewards. It also checks screens that are already open and retries closing them for a bounded interval while the controller remains available.

Hatches also have an adapter in `MapExploreState`: it identifies the signature that receives `HatchedPokemonsData` and skips the presentation before the scene opens. Obtaining the Pokémon and the reward responses remain the game's responsibility. If a hatch screen is already open, it is handled through its compatible controller. The hatch result screen is closed when that flow finishes; one that you open manually from the inventory stays open until you close it.

The Farmer continues during informational notices of mission progress and reward lists when the map is still active and visible. Manual multi-select, manual encounters and windows that require interaction keep their pauses.

Actions run on the Unity thread, with the same session and focus control as the farmer. There is no continuous polling of all windows: only a limited queue is kept when a recognized screen appears. Screens outside that list are not closed automatically.

The presentation queue is limited and the components are kept through controlled references. When the engine closes, the hooks are removed and those references are released. Implementation: [presentation-tools.c](native/presentation-tools.c).

### Field research and weekly achievement

Field research is handled **always** during the Farmer, including the research that arrives from spinning PokéStops. The engine queries `IQuestService`, claims the tasks the game allows to be redeemed and processes the rewards of items, berries, XP, Stardust or encounters that it receives.

The scope is identified by **QuestContext = 2** and its encounters by **EncounterType = 6**. Discarding is limited to this scope; special and event research follow their own claim flow. Incompatible field tasks, such as raids, trades, exploring kilometers and earning Candy with the buddy, and those of unknown type or condition are deleted to make room for others; the AR scan slot is kept. An already completed task is claimed before its discard is considered.

```c
if (context != 2 || redeemable > 1 || count > 32 ||
    (count && !conditions) || transfers > 1 || berries > 1)
    return SILPH_RESEARCH_KEEP;
if (redeemable) return SILPH_RESEARCH_CLAIM_TASK;
if (status != 1 || type == 46) return SILPH_RESEARCH_KEEP;
```

*Excerpt from [research-policy.c](native/research-policy.c). [research-actions.c](native/research-actions.c) coordinates the calls and their responses.*

If there is a pending **weekly research achievement**, the engine handles it before continuing with the field tasks. It queries the seven-stamp card through `IQuestStampCardService`, requests `CompleteStampCard` when it is available and retrieves its pending encounter, identified by **EncounterType = 5**.

Field and weekly encounters use the Farmer's berry, throw quality and catch cycle. They are processed even if their species is not selected in **Only catch**. **Keep field research encounters** protects both origins; with that option disabled, **Keep shiny**, **Keep 15/15/15**, **Keep shiny + background** and the other global protections still apply.

Responses are queried in phases, without a blocking wait by the Farmer. The encounter is handed to the catch cycle when the previous operation has finished and is identified by its ID to avoid processing it twice.

### Special and event research

Special and event research is collected automatically during the Farmer. The engine reads the tasks and subtasks of each stage from `IQuestService.get_QuestsProto` and queries `QuestProtoExt.CanRedeem` on each individual task. It claims only the rewards that the game allows to be received through `IQuestRpcService.CompleteQuest`, with the stage ID and the subtask ID where applicable.

Incomplete tasks are kept. Rewards that require choosing a branch are left pending the user's choice. Discarding incompatible tasks remains limited to field research.

```c
SilphReadArgument args[] = {
    {.kind = SILPH_READ_REFERENCE, .value.reference = parent},
    {.kind = SILPH_READ_REFERENCE, .value.reference = child}
};
status = silph_research_invoke(s->read, permit,
    SILPH_RESEARCH_STORY_CLAIM, s->rpc, args, 2, &result);
```

*Call from [research-extra.inc](native/research-extra.inc). [research-bridge.inc](native/research-bridge.inc) validates the relationship between stage and subtask before invoking the service.*

Special encounters use **EncounterType = 4** and temporary ones **EncounterType = 10**; the custom research type **23** is also supported. They are opened through `BeginQuestEncounter` and reuse the Farmer's berries, throw quality and catch checks, regardless of **Only catch**.

**Keep field research encounters** includes field, weekly achievement, special and event research. The global shiny, IV and background protections also apply to these encounters. Legendary, mythical or singular Pokémon and Ultra Beasts remain protected even if keeping by origin is disabled.

Reads are distributed in bounded blocks and claims keep a single pending response. The engine identifies each task and stage to avoid duplicate claims, processes the items and counts XP and Stardust according to the game's responses. Willow's dialogs are skipped through their **Skip** action.

### GO Pass

The Farmer queries the **current GO Pass** and claims the rewards unlocked according to the account's rank. It reads the free and paid tracks that the account already has available; the engine uses the pass's current data, so it does not depend on a fixed monthly list of rewards.

Items, XP and Stardust are claimed through `ClaimEventPassRewardsRequestProto` and their `ClaimEventPassRewardsResponseProto` responses. Encounters are requested by pass, track and rank; they then move into the shared berry, throw quality and catch cycle. The **Only catch** filter does not block these rewards.

**Keep GO Pass encounters**, initially enabled in **Transfer new catches**, keeps the pass's Pokémon. If you disable it, common encounters can be transferred when they meet no other protection. **Legendaries are always kept**, as are the other protected special classes. The shiny, 15/15/15 and shiny + background filters keep applying independently of that box.

The origin is obtained from the original reward, **EncounterType = 25**, and is kept alongside the encounter ID throughout the catch. The label of the object used to present the encounter does not replace that origin.

```c
s->reward_source = type == 25 ? SILPH_SOURCE_GO_PASS : SILPH_SOURCE_RESEARCH;
```

*Assignment from [research-actions.c](native/research-actions.c). [pass-actions.c](native/pass-actions.c) coordinates the pass and [transfer-policy.c](native/transfer-policy.c) applies the protections.*

The pass query and requests advance in phases while the Farmer continues. At most one pending encounter reward is kept to hand to the catch cycle, with spaced-out reads and retries. XP and Stardust are added to the statistics only from rewards confirmed by the game.

### Remove buddy

**Remove buddy** comes disabled. When checked, the engine queries whether there is a buddy and requests its removal with Pokémon identifier **0**. A confirmed response leads to a game restart to continue the Farmer without a buddy. If there was no buddy anymore, it continues directly.

The `BuddyMapProto` and `SetBuddyPokemonProto` contracts are used, through `IRpcHandler.SendRpc`, with operations **1350** and **152** of the game profile. The removal is performed once per service start; it is not repeated on every map recovery.

Removing it does not transfer it or modify its inventory data. It is not assigned again at the end: the user can set it again from the game. The coordination of this flow is in [buddy-actions.c](native/buddy-actions.c).

### Inventory tools

**Show IV names**, initially enabled, temporarily changes the text displayed in the inventory rows. For example, a Pokémon with IVs **15/14/13** is displayed as **93% 15/14/13**. The percentage is a whole number and the name saved in the account does not change.

**15/15/15** are displayed as **100% 15/15/15** in red, both in the list and on their detail screen. The color is updated when the inventory rows are reused and is restored when the engine stops.

```c
int n = snprintf(out, capacity, "%d%% %d/%d/%d",
    (a + d + s) * 100 / 45, a, d, s);
```

*Format from [visual-tools.c](native/visual-tools.c). Applied during the session, without modifying the saved name.*

**Extended manual transfer**, initially disabled, extends the inventory's multi-select for special Pokémon, including shinies and certain legendary classes. The selection and the confirmation are done by the user from the game. Favorites, buddies and other inventory protections remain excluded; this feature does not add previous Pokémon to the automatic transfer queue.

### Game tools: gifts and temporary name

**Fast gift opening**, initially enabled, speeds up the gifts you open manually from the game while the service is active. It reduces the opening animation duration to **50 ms** and speeds up the animators of the gift and its rewards. The original request, its responses and the delivery of the items remain the game's responsibility.

Thanks to all this, it was possible to identify `OpenGiftGuiController.giftOpenAnimationLength`, its `guiAnimator`, the `GiftBox` animator and the creation of `MajorRewardSequenceView` within the gift sequences. The engine applies the changes only to those components and restores their values when the gift is closed or the session stops.

Only the local presentation waits of the two opening sequences are shortened, when the object is exactly `Niantic.Zeppelin.Scheduler.Wait` and its duration is within the supported range. Network and resource waits remain the game's responsibility.

The handling is in [presentation-tools.c](native/presentation-tools.c). The game's global clock keeps its usual speed.

**Auto-open gifts** and **Auto-send gifts** are independent options, initially disabled. They go through the available friends during the session and query the list again periodically; they are not limited to service start. They open the gifts the account can receive and send the ones it has available to eligible friends. With a full bag, opening requests conversion to Stardust. The game's responses determine the result and the opening or sending limits.

The automatic flow uses a single pending request, keeps the identifiers already processed and spaces out the review of friends. It advances alongside the Farmer without opening the friends list or stopping catches and spins. The friendship level-up window is closed through its controller. Implementation: [gift-actions.c](native/gift-actions.c).

**Hide trainer name**, initially enabled, replaces the player's visible text on the map HUD, the own profile and the friends header. It uses the `UnityEngine.UI.Text` components of those screens; it does not rename the user's account or modify the identity data the game uses.

The name handling is prepared when the engine starts and applied as soon as the HUD and profile components are available, without waiting for the first catch. The HUD is obtained from the map state that is running. The previous texts are kept during the session and restored on stopping, on the Unity thread. References are limited to **48 components** and are released when the engine closes; destroyed components are removed so they do not accumulate when navigating between screens.

### Transfers and deletion

Identifiers are kept as **`uint64_t`** in C and as 64-bit values in the Android controller. The authorization combines issued request, recent successful response, correct class, caught ID and current session.

```c
if (!input || silph_transfer_evaluate(input, sizeof(*input), &verdict)
    || verdict.decision != SILPH_VERDICT_CANDIDATE
    || !silph_core_authorize(scope, input->owned_id, encounter)
    || !silph_schedule_permitted(permit, true))
    return SILPH_MANAGED_STATE;
```

*Authorization gate in [managed-read.c](native/managed-read.c). Transfer calls do not accept IDs found solely by walking the inventory.*

A transfer is not counted merely because it was requested: the response is checked and the ID is reconciled in the inventory. Unknown results do not authorize irreversible retries. Item deletion uses a separate policy of types and quantities.

### Location and Error 12

The adapter uses the JNI ABI to identify `android.location.Location.isFromMockProvider()` and, if it exists, `isMock()`. It handles only the boolean responses corresponding to those methods within the **process it connected to**.

The handling is installed at startup with **Fix error 12** and removed when the service stops. It resolves the available methods according to the process's SDK, including the modern `isMock()` variant, and shares the same lifecycle on UGPhone and physical phones. The implementation is in [location-tools.c](native/location-tools.c).

### XP and Stardust counter

The rewards from successful responses to **catches, spins, field research, weekly achievement, special research, event research, GO Pass and gifts issued by SilphMod** are counted: `CaptureScoreProto.Exp`, `Stardust`, temporary/mega evolution rewards, `FortSearchOutProto.XpAwarded`, `Loot`, `BonusLoot`, `CompleteQuestOutProto.Rewards`, the weekly card's rewards and `ClaimEventPassRewardsResponseProto.RewardsGranted`, in addition to the Stardust confirmed when opening gifts, according to the validated contract. Totals are kept when the service is stopped or recovered and are only reset with **Clear statistics**.

The account balance is not subtracted and rewards are not estimated by multiplying catches by a constant. Amounts are not multiplied again by bonuses the game may have already applied. An increasing sequence prevents duplicates; Android totals use `long`. `—` indicates that a reading is unavailable and `+` a partial total with missing rewards.

## Build

Requirements: JDK **17 or later**, Android SDK **36**, Android NDK **r28c / 28.2.13676358**, Git and PowerShell. Android Studio can open the repository root. Set `JAVA_HOME` and `ANDROID_HOME`, or create your own `local.properties` with `sdk.dir`.

```powershell
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
$env:ANDROID_HOME = "C:\Users\user\AppData\Local\Android\Sdk"
.\gradlew.bat :app:assembleRelease
```

Gradle builds the C components from [native](native) through [build_native.ps1](tools/build_native.ps1). The hook backend is obtained from a pinned ShadowHook revision and kept outside the publishable code, in `.dependencies/`. You can point to existing installations with `SILPH_NDK` and `SILPH_SHADOWHOOK`.

The build generates the **twelve native `.so` components** and their SHA-256 fingerprints. The service verifies them again before loading the engine. These files are built from C and the hook dependency. Gradle bundles them into the APK from `app/src/main/jniLibs/arm64-v8a/`.

Without a signing configuration, the release output is **unsigned**. To sign it, provide `SILPH_KEYSTORE`, `SILPH_STORE_PASSWORD`, `SILPH_KEY_ALIAS` and `SILPH_KEY_PASSWORD`. The keys stay out of the repository; updating an existing installation uses the same certificate.

## Logs and privacy

The export is local and manual. It includes up to **three recent sessions** and the build identification. It records operational configuration, process state, startup routes, responses, counters and resource release. Each log has a maximum of **8 MB**. Newly saved events filter out the control channel key.

Files are stored in the app's private space. **Export logs** creates a ZIP in `Downloads/SilphMod/`. Logs may contain coordinates and device identifiers, but do not include sensitive and/or confidential data.

## Structure

```text
SilphMod/
├── app/                  # Interface, service, configuration and resources
├── native/               # C engine, startup and policies
│   └── standalone/       # Connection and process control
├── docs/assets/          # Identity and visual assets
├── tools/                # Native build and asset provenance
├── gradle/               # Build wrapper
├── LICENSE
├── NOTICE
├── spanish.md
└── README.md
```

## Status and upcoming improvements

**1.6.0 introduces selectable throw quality**, priority-based berry use, selective interruption handling, session catch and spin limits, continuity when expanding the notifications and an action interval of **100–3000 ms**, with an initial value of **150 ms**. Transfers and discards keep their own unlimited total quotas; the full session ends upon reaching either the catch or the spin maximum.

**1.7.0 adds automatic map recovery with up to ten startup attempts**, persistent statistics with manual clearing, blocking of startup when the accumulated limits have been reached and keep filters for Dynamax, hatched, backgrounds and the shiny + background combination.

**1.9.0 brings the Farmer together in a single APK for UGPhone and rooted physical phones**, incorporates **Only catch**, automatic field research, **Remove buddy**, temporary names by IV and expanded manual transfer. It adds the encounter range read from the game of up to **50 m**, bounded priority to finish active catches, fewer repeated reads and improvements in log export. Interruption handling includes the hatch presentation, the speed warning and the Stamp Rally notice.

**1.9.3 updates the profile to Pokémon GO 0.433.1**, adds **Fast gift opening** to speed up manual gift opening and **Hide trainer name** to temporarily show SilphMod instead of the player's name. It integrates the adaptive injector with alternative routes and reuse of the connection that worked for that environment.

**2.0.0 introduces Kecleon (BETA)** as a separate mode, with candidates, bounded movements, catch filters and per-account history. Farmer keeps its independent configuration and results.

The engine moves to **C**, with scheduling on the Unity thread, references with controlled lifetime and fewer repeated reads while waiting for responses. Startup brings together four alternative routes in a single APK. Field research, inventory tools, fast gift opening, buddy removal and recovery after five minutes of an empty map are integrated.

**2.0.15 strengthens the integrity of the native components and the control channel**. Session credentials are delivered through a private pipe, unauthenticated connections expire and diagnostic logs exclude the control key. The startup routes and the catch and transfer policies are kept.

The Farmer adds continuity in the game's compatible menus, more frequent checking of the catch phases with **Action interval** at **100 ms** and bounded metadata caches. Berries, research, spins, cleanup and transfers keep the coordination of a single pending operation.

**2.1.0 extends the Farmer's continuity** to the individual Pokémon detail screen, the shop and the Pokédex. It adds automatic opening and sending of gifts during the session, opening with a full bag by converting to Stardust and closing of friendship windows. The temporary name and scene skipping are prepared from the start of the engine; the closing of medals covers their various controllers and already-open screens.

**2.1.1 adds the weekly research achievement and the GO Pass rewards** to the catch cycle. It adds **Keep GO Pass encounters**, extends **Keep field research encounters** to the weekly achievement and applies the global keep filters to all origins. The pass's legendaries remain protected. The field tasks of exploring kilometers and earning Candy with the buddy are discarded along with the other incompatible ones. This may vary depending on the events active in the game, so keep an eye on it.

**2.1.4 adds the rewards of special and event research**, from both their tasks and their stages, and integrates them into the Farmer's catch cycle. **Keep field research encounters** also includes these origins, with the same global protections. Willow's dialogs are skipped, the protection of legendary, mythical or singular Pokémon and Ultra Beasts is kept, and incomplete tasks and the user's branch decisions are maintained.

This edition also improves recovery from native channel disconnections and the handling of catch responses without confirmation. It keeps the statistics, the startup routes and the individual authorization of transfers.

**2.2.11 adds AutoRocket (BETA)**, with regular Grunt candidates, recommended team, automatic attacks and shields, catching by filters, return to the configured location and team healing between battles. It adds **Only catch 13/13/13 or above**, **Maximum Rockets** and per-account history.
The Farmer adds regular and daily incense spawns and **Automatic event actions** to collect the Halloween boxes and catch their encounters. It improves continuity during informational notices of missions and rewards, avoids late camera openings from the boxes and shortens the wait after temporary encounter failures.

**2.3.8 adds Event tricks**, an independent mode to collect Candy from the event boxes, repeat confirmed collections and give preference to Rare Candy boxes. It includes its own notification and statistics, with continuity in compatible menus and the usual pauses when leaving the game.

The Farmer adds **Select Pokémon to keep**, regional forms in both selectors and an updated catalog with Minior, Mimikyu and Bramblin. The **Buddy** section allows feeding and mega evolving the buddy. **15/15/15** are identified in red and the temporary name is prepared from startup.

AutoRocket adds configurable Potion and Revive priorities, and shortens the transition from dialogs and rewards to the catch. The validation of presentation objects and the preservation of arguments when executing game calls are reinforced.

**2.3.15 updates compatibility for Pokémon GO 0.435.1** in Farmer, Kecleon, AutoRocket and Event tricks. Game references, services and preparation of the temporary name at startup have been adapted.

AutoRocket fixes engine startup before opening the Grunt and preserves team selection, shields, filtered catches, the return location and healing between battles.

Startup expands the checks for older kernels and keeps its four connection routes. Event tricks logs reduce repetitive entries and preserve state changes and the final session summary. Rare Candy priority, collection settings and transfer protections are maintained.

The next phase is to keep optimizing the continuity of the four modes and expand compatibility across devices.

We will look to add a setting for the hierarchical order in which Poké Balls are used. Not the default one: 1. Poké Ball; 2. Great Ball; 3. Ultra Ball

New improvements and modes are coming to SilphMod, stay tuned.

Another pending item is to keep expanding the catalog of Pokémon configured not to be transferred, for example with nundos (0/0/0) or rank 1 (PvP).

License [MIT](LICENSE). Dependencies keep their notices in [NOTICE](NOTICE) and their licenses within the assets. Independent, personal project, with no affiliation to the game's rights holders or to any third parties.

Take care, little guy.
