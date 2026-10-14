![SilphMod — Root Android](docs/assets/banner.svg)

# SilphMod

**[English](README.md) · Español**

Automatización local para Pokémon GO original en **Android con root**, tanto en UGPhone como en teléfonos físicos ARM64. Captura Pokémon cercanos, gira Poképaradas, recoge investigaciones de campo, transfiere nuevas capturas según tus filtros y mantiene la mochila dentro de los límites que elijas.

Configuras las funciones, pulsas **Start service** y sigues el progreso desde la notificación. **Stop** y **Export logs** permiten detener la sesión y revisar sus resultados. La interfaz, las notificaciones y los recursos visuales están en inglés.

**Edición:** `1.9.0` · [MIT](LICENSE)

**Entornos de referencia:** UGPhone Android 10, Motorola Edge 50 Fusion con Android 15 y Redmi Note 14 Pro+ 5G con Android 16; todos ARM64 con root.

<details>
<summary>Contenido</summary>

- [Cómo funciona](#cómo-funciona)
- [Compatibilidad](#compatibilidad)
- [Funciones](#funciones)
  - [Only catch: escoger qué Pokémon capturar](#only-catch-escoger-qué-pokémon-capturar)
  - [Transferir sin tocar la colección anterior](#transferir-sin-tocar-la-colección-anterior)
  - [Mochila: límites por tipo, no borrado indiscriminado](#mochila-límites-por-tipo-no-borrado-indiscriminado)
  - [Límites de captura y giros](#límites-de-captura-y-giros)
- [Uso](#uso)
- [Arquitectura](#arquitectura)
  - [Inicio y continuidad](#inicio-y-continuidad)
  - [Recuperación automática del mapa](#recuperación-automática-del-mapa)
  - [Capturas y llamadas del juego](#capturas-y-llamadas-del-juego)
  - [Alcance y prioridad de captura](#alcance-y-prioridad-de-captura)
  - [Throw quality](#throw-quality-del-contrato-il2cpp-al-lanzamiento)
  - [Bayas](#bayas-confirmar-el-efecto-antes-de-lanzar)
  - [Interrupciones y continuidad del mapa](#interrupciones-y-continuidad-del-mapa)
  - [Investigaciones de campo](#investigaciones-de-campo)
  - [Remove buddy](#remove-buddy)
  - [Inventory tools](#inventory-tools)
  - [Transferencias y borrado](#transferencias-y-borrado)
  - [Ubicación y Error 12](#ubicación-y-error-12)
  - [Contador XP y polvo estelar](#contador-xp-y-polvo-estelar)
- [Compilación](#compilación)
- [Registros y privacidad](#registros-y-privacidad)
- [Estructura](#estructura)
- [Estado y próximas mejoras](#estado-y-próximas-mejoras)

</details>

## Cómo funciona

![SilphMod en funcionamiento — 1:13](docs/assets/demo.gif)

## Compatibilidad

| Componente | Perfil de esta edición |
| --- | --- |
| Sistema | Android **10 o posterior** y permiso `su` |
| Entornos de referencia | UGPhone Android **10 / SDK 29**; teléfonos físicos Android **15 / SDK 35** y **16 / SDK 36** |
| Arquitectura | **ARM64**, punteros de 64 bits |
| Juego | Pokémon GO original **0.433.0**, `versionCode 2026100702` |
| Motor del juego | Unity **6000.3.22f1**, IL2CPP |
| APK | `minSdk 29`, `targetSdk 36` |
| Tratamiento opcional de Error 12 | Contratos JNI de `Location` resueltos según la versión del sistema |
| Conexión | Selección automática del arranque y del runtime según el SDK, dentro de una misma APK |
| Identidad instalada | `dev.rootcatch.ugphonetest`, conservada para actualizar instalaciones anteriores |

La aplicación conserva una sola interfaz y un solo motor Farmer. El arranque se adapta al **Android del dispositivo o de la instancia**, sin que el usuario tenga que elegir una APK diferente para cada entorno.

## Funciones

| Función | Comportamiento actual |
| --- | --- |
| Autocaptura | Selecciona el objetivo elegible más cercano dentro del alcance leído del juego, hasta **50 m**, y vuelve a comprobar su existencia y distancia antes de lanzar. Reintenta mientras el encuentro siga siendo válido; si desaparece o sale del alcance, pasa al siguiente objetivo. |
| Only catch | Selección de una o varias especies con miniaturas, búsqueda y orden de Pokédex. Sin selección, captura todas las especies elegibles. Las recompensas de investigación de campo se capturan independientemente de este filtro. |
| Throw quality | Selecciona **Normal**, **Nice**, **Great** o **Excellent**. Los lanzamientos mantienen el giro de bola curva. |
| Bayas | Usa la primera baya disponible según la selección y prioridad configuradas, confirma su aplicación y continúa con el lanzamiento. Mantiene los límites de Item limits. |
| Poképaradas | Envía giros para objetivos dentro del alcance configurado en el motor y contabiliza respuestas exitosas. |
| Límites de sesión | Máximos configurables de capturas y giros, inicialmente **4.800** y **2.400**, aplicados a las estadísticas acumuladas. Alcanzar cualquiera detiene el farmeo, cierra Pokémon GO y guarda los registros. |
| Interrupciones | Manejo automático de medallas, subida de nivel, recompensas de XP, resumen del incienso diario, eclosiones, aviso de velocidad y aviso de Stamp Rally. Permanece activo durante el farmeo. |
| Investigaciones de campo | Reclama recompensas disponibles, captura sus encuentros y descarta tareas incompatibles o de tipo desconocido. Conserva las investigaciones especiales, temporales y el espacio de escaneo AR. |
| Remove buddy | Retira el compañero al iniciar, sin transferirlo ni volver a asignarlo al terminar. Desactivado inicialmente. |
| Transferencias | Solo Pokémon capturados por **esta sesión de SilphMod**, con respuesta de captura acreditada e ID exacto. |
| Conservar shiny / 15/15/15 | Dos filtros independientes. Cumplir cualquiera de los filtros activos impide transferir ese Pokémon. |
| Conservar Dynamax / eclosionados / fondo | Nuevos filtros de protección para las capturas autorizadas por el servicio. No recorren ni modifican la colección anterior. |
| Conservar shiny + fondo | Protege los Pokémon que cumplen ambas condiciones. Se combina con los demás filtros de conservación. |
| Conservar encuentros de investigación | **Keep field research encounters** protege sus capturas. Si se desactiva, se aplican los mismos filtros shiny, IV y demás protecciones que a las capturas salvajes. |
| Show IV names | Muestra temporalmente el porcentaje entero y los IV en la lista del inventario, sin renombrar los Pokémon de la cuenta. |
| Extended manual transfer | Amplía la selección manual múltiple de Pokémon especiales desde el inventario del juego. Es independiente de las transferencias automáticas. |
| Limpiar mochila | Elimina excedentes de los tipos seleccionados **desde el inicio**, y mantiene esos límites durante la sesión. No necesita esperar a que la mochila se llene. |
| Solucionar error 12 | Tratamiento de ubicación dentro del proceso del juego, opcional y vinculado a la sesión. |
| Estadísticas | Capturas, huidas, giros, transferencias, objetos eliminados, XP, polvo estelar y tiempo activo acumulados. Se conservan al detener, volver a iniciar y recuperar el mapa. |
| Clear statistics | Restablece los contadores únicamente al pulsarlo con el servicio detenido. Se encuentra debajo de las estadísticas. |
| Recuperación del mapa | Tras cinco minutos de lecturas válidas sin Pokémon dentro del alcance efectivo, reinicia el juego y vuelve a conectar el motor. Hasta **10 intentos de arranque por recuperación**, cancelables con Stop. Se desactiva al seleccionar especies en **Only catch**. |
| Barra de notificaciones | Las capturas y los giros continúan al desplegarla si Pokémon GO sigue siendo la app subyacente y la pantalla está desbloqueada. |
| Registros | Exportación manual de hasta tres sesiones recientes, con índice de tamaños y SHA-256. |

No hay ruta o joystick integrado: SilphMod trabaja con tu aplicación GPS JoyStick (desarrollado por The App Ninjas). No uses algún otro joistick que tenga un distinto nombre de paquete que el anterior mencionado.

### Only catch: escoger qué Pokémon capturar

**Only catch** permite escoger una o varias especies para las siguientes sesiones. El selector utiliza un catálogo local con miniaturas, orden de Pokédex y búsqueda por nombre o número. La selección queda guardada; si la dejas vacía, el farmer vuelve a capturar todas las especies elegibles.

El filtro se aplica antes de abrir un encuentro salvaje y se vuelve a comprobar antes de usar una baya o lanzar. Seleccionar una especie no modifica sus IV ni condiciona los filtros de conservación: estos se aplican después de capturar.

```js
allowsPokemon(readId, source = 'WILD') {
  return source === 'FIELD_RESEARCH' || !this.active || this.allows(readId());
}
```

*Fragmento de [catch-selection.cjs](engine/src/catch-selection.cjs). Las capturas de investigación de campo no quedan bloqueadas por Only catch.*

Cuando hay especies seleccionadas, no se activa la recuperación por cinco minutos sin Pokémon: el farmer puede esperar a que aparezcan esas especies sin reiniciar el juego. **Stop**, los límites acumulados y el control de conexión siguen funcionando de la misma forma.

### Transferir sin tocar la colección anterior

La cola de transferencias nace vacía. Un Pokémon entra únicamente cuando una petición de captura emitida por esta sesión obtiene una respuesta válida y exitosa con su ID de almacenamiento. Encontrar un ID en la colección **no basta** para autorizar su transferencia.

Además de los filtros elegidos, se conservan favoritos, compañeros, huevos, Pokémon desplegados, fusiones, clases especiales y registros cuya información de protección no puede validarse. Desmarcar **Keep shiny** o **Keep 15/15/15** permite transferir esas nuevas capturas si no existe otra protección.

La versión **1.7.0** añade **Keep Dynamax**, **Keep hatched**, **Keep background** y **Keep shiny + background**. Las tres primeras vienen activadas inicialmente; la combinación shiny + fondo viene desactivada. Cumplir cualquiera de los filtros marcados conserva el Pokémon. Dentro de **Keep shiny + background**, en cambio, deben cumplirse las dos condiciones a la vez.

Si marcas **Keep shiny** junto con **Keep shiny + background**, se conservan todos los shiny, tengan fondo o no. Si marcas **Keep background** junto con la combinación, se conservan todos los que tengan fondo. Para conservar únicamente shiny con fondo mediante estos filtros, deja marcada solo la combinación y desmarca los demás filtros opcionales. El motor sigue capturando los otros Pokémon y los transfiere si no existe otra protección.

La protección de eclosionados se consulta en el registro autorizado para transferencia; no convierte las eclosiones en nuevas autorizaciones ni añade Pokémon de la colección anterior a la cola.

**Keep field research encounters**, activado inicialmente, añade la conservación por origen. Si se desactiva, las capturas de investigación se transfieren cuando no cumplen ninguna de las demás protecciones activas. Por ejemplo, con **Keep shiny** marcado se conservan los shiny tanto salvajes como de investigación; la combinación **Keep shiny + background** exige ambas características en los dos orígenes.

Las transferencias están desactivadas inicialmente y **no tienen cuota total por sesión**. El motor recibe `maxTransfers = 0`; las preferencias antiguas no restauran un límite. No se heredan autorizaciones entre sesiones, cuentas ni reinicios.

![Fragmento de la autorización de transferencias](docs/assets/session-transfer.png)

*Este es un fragmento del archivo [session-transfers.cjs](engine/src/session-transfers.cjs).*

### Mochila: límites por tipo, no borrado indiscriminado

Una casilla habilitada permite eliminar el excedente de ese tipo; el número indica cuánto conservar. Un tipo desmarcado no se elimina. Por ejemplo:

```text
Potion=50,SuperPotion=50,Revive=30,NanabBerry=30
```

Con 83 pociones y un mínimo de 50, el plan permite eliminar 33. Cada petición elimina como máximo 50 unidades, **sin cuota total por sesión**. El motor comprueba la respuesta y la sincronización del inventario antes de continuar.

Hay **33 tipos seleccionables**, incluyendo tres Poké Balls comunes, cuatro pociones, dos revivires, cinco bayas y otros consumibles permitidos. El catálogo contiene otros **17 tipos protegidos**; Master Ball, Safari Ball, Premier Ball, pases, incubadoras y objetos desconocidos no se autorizan para descarte. Puedes revisar los IDs y restricciones en [ItemCatalog.java](app/src/main/java/dev/rootcatch/nativeengine/ItemCatalog.java) y [item-catalog.cjs](engine/src/item-catalog.cjs).

La limpieza está desactivada inicialmente. El motor recibe `maxDiscardedItems = 0`, independientemente de las preferencias anteriores. Puede borrar objetos que ya estaban en la mochila, conforme a los tipos y mínimos seleccionados. Una respuesta ambigua o un rechazo conocido pausa la limpieza sin repetir la petición; las demás funciones pueden continuar.

### Límites de captura y giros

En **Engine settings → Session limits**, **Maximum catches** comienza en **4800** y **Maximum PokéStop spins** en **2400**. Ambos aceptan un máximo positivo y conservan el valor elegido para las siguientes sesiones.

Los contadores se conservan entre inicios del servicio y recuperaciones automáticas. Solo consumen el límite las respuestas exitosas de las acciones emitidas por SilphMod: no los intentos fallidos, las huidas ni las capturas manuales. Son máximos sobre las estadísticas locales acumuladas, no contadores diarios de la cuenta.

Si las capturas o los giros acumulados ya alcanzan su máximo, **Start service** queda bloqueado y muestra **Capture limit reached** o **Spin limit reached**. Para continuar, aumenta los límites por encima de ambos totales o utiliza **Clear statistics** con el servicio detenido. Detener el farmer no borra los resultados; limpiar estadísticas tampoco borra los logs.

Al alcanzar cualquiera de los dos máximos, no se envían más acciones. El agente retira sus hooks, libera sus referencias y confirma la parada; después Android verifica la identidad del proceso, cierra Pokémon GO y guarda los registros con el motivo y los resultados finales. **Export logs** permite recuperar el ZIP. El cierre no borra caché, almacenamiento ni configuración del juego.

## Uso

Antes de todo, recuerda que debes de disponer de una aplicación GPS Joistick correctamente configurada.

1. Instala el APK desde la sección de lanzamientos, abrela y concede permiso root y notificaciones (En caso te lo pida). Mantén tu joystick configurado y funcionando. No ejecutes otros automatismos o inyectores simultáneamente.
2. Selecciona **Catches**, **Only catch**, **Throw quality**, **Use berries**, **Berry priority**, **Remove buddy**, **Spin PokéStops**, filtros de transferencia e **Items to keep**, según lo que quieras utilizar. En **Inventory tools** puedes activar los nombres por IV y la transferencia manual ampliada. Ajusta los máximos de capturas y giros en **Engine settings**. Las transferencias y los descartes no tienen un máximo total propio por sesión, es decir, eliminan una cantidad ilimitada de los objetos que hayas configurado para descartar mientras la sesión permanezca activa.
3. Coloca una **ruta de recorrido** en tu aplicación GPS joistick o simplemente usa el Joistick de esta app para movilizarte por el mapa. Recuerda que la **ruta de recorrido** es recomendada para mayor efectividad del mod en mayor tiempo.
4. Si tu dispositivo o instancia necesita el tratamiento de ubicación, marca **Fix error 12** y pulsa **Start service**. SilphMod abre Pokémon GO y prepara la conexión correspondiente al sistema. El arranque puede cerrar únicamente el juego y volver a abrirlo para instalar el tratamiento temprano; no borra caché, almacenamiento ni ajustes del joystick. Si activaste **Remove buddy**, el estado **Removing buddy…** acompaña su retirada y el reinicio necesario para continuar sin compañero.

5. Espera **Active**. Hay una comprobación automática de mapa, jugador y referencias antes de enviar acciones.
6. Pulsa **Stop service** o **Stop** en la notificación cuando desees terminar la sesión. Y solo en caso de querer guardar el reporte de errores, espera a que termine **Saving logs…** y utiliza **Export logs**. El ZIP se guarda en `Descargas/SilphMod/`.

El servicio no tiene un temporizador de 30/60 segundos ni un límite fijo de horas. Para detenerlo, debes abrir la notificación de la app y darle en detener o en su defecto cerrar Pokémon Go y volver a la app del Mod.

Cambiar de app pausa las acciones; perder el proceso, cambiar de cuenta o perder prolongadamente el controlador termina la sesión. Abrir la barra de notificaciones permite revisar las estadísticas sin pausar capturas ni giros, mientras el juego siga debajo del panel y la pantalla esté desbloqueada. Al cambiar a otra app o bloquear la pantalla, las acciones se pausan. (Recuerda que en UGPHONE el juego puede funcionar en segundo plano si cierras la instancia, pero no si cierras Pokémon Go o SilphMod).

Si el almacenamiento Pokémon se llena, se pausan las capturas y se informa el estado; SilphMod no borra la colección anterior para hacer espacio. Con la mochila llena y sin excedentes autorizados, tampoco elimina otros objetos por su cuenta. Detener no puede revertir una operación que ya fue enviada.

## Arquitectura

![Arquitectura de SilphMod](docs/assets/architecture.svg)

SilphMod tiene dos partes que cooperan mediante una tubería local:

- **Android / Java:** interfaz, configuración, servicio en primer plano, permiso root, identidad del proceso, foco, control, notificación y exportación.
- **Agente / TypeScript y CommonJS:** resolución de servicios IL2CPP, selección de objetivos, solicitudes del juego, lectura de respuestas, filtros y ciclo de vida de referencias.

El agente utiliza objetos y métodos del proceso original; el cliente del juego sigue siendo quien serializa sus solicitudes y se comunica con su servidor. SilphMod no tiene permiso `INTERNET` ni una API propia de apariciones, pero **el juego necesita conexión**: esto no es un modo offline.

### Inicio y continuidad

Cuando la opción de ubicación está activa, el agente prepara primero el adaptador JNI y carga después los puentes Java/IL2CPP, tras un mínimo de ocho segundos y la presencia de las bibliotecas requeridas. Eso no acredita que el mapa esté listo: aún se validan escena, cuenta, firmas, referencias GC de 64 bits y muestras estables durante aproximadamente diez segundos.

La activación usa una autorización de un solo uso vinculada a proceso, sesión y jugador. No hay una desconexión/reinyección entre la validación y el farmer. Las acciones se ejecutan desde el hilo Unity; los objetos retenidos se liberan al terminar.

El controlador envía latidos cada **500 ms** en un hilo independiente de consultas de foco y escritura de registros. Solo una lectura positiva de foco de hasta **1.500 ms** puede autorizar acciones; el panel de notificaciones se admite cuando el juego permanece como actividad subyacente. El permiso para nuevas acciones caduca a los **2.200 ms**; una demora breve puede recuperarse en la misma conexión, mientras una pérdida continua de control superior a **10 segundos** termina la sesión. La recuperación automática atiende el mapa vacío descrito a continuación y el reinicio necesario después de retirar el compañero.

En Android 10 se utiliza la conexión al proceso con Frida **17.22.1**. En Android posteriores, el arranque temprano utiliza Frida **17.23.3** y el adaptador nativo propio. La estrategia se selecciona por SDK; la configuración del Farmer y sus políticas son las mismas. El foco se consulta mediante Java en Android 10 y mediante el adaptador JNI de actividad en los sistemas posteriores.

### Recuperación automática del mapa

El farmer observa la presencia de Pokémon cercanos cada **cinco segundos**. Si acumula **cinco minutos continuos de lecturas válidas sin Pokémon dentro del alcance efectivo de captura**, detiene el agente, confirma la liberación de referencias y del tratamiento de ubicación, cierra únicamente el proceso verificado de Pokémon GO y guarda los registros. Después abre el juego y conecta una nueva instancia del motor con la misma configuración y cuenta.

Esta función permanece activa durante el farmeo general y no requiere marcar una opción. Una lectura con Pokémon reinicia el contador, aunque esos objetivos estén temporalmente en espera. Las muestras incompletas, la falta de lecturas, cambiar de app o bloquear la pantalla no se interpretan como un mapa vacío. Tampoco basta con quedarse sin Poké Balls o llenar el almacenamiento. **Only catch** desactiva únicamente esta recuperación mientras haya especies seleccionadas.

Cada recuperación admite **hasta diez intentos de arranque en total, contando el primero**. Si el motor llega a **Active**, no se realizan los intentos restantes y continúa el farmeo. Si el juego se cierra durante ese arranque o la conexión no termina de activarse, puede reintentarse tras confirmar que el inyector anterior terminó y el juego está cerrado. Las esperas entre intentos aumentan de **5 a 30 segundos**; no hay inyectores simultáneos. Al agotar los diez intentos, el servicio se detiene, conserva las estadísticas y guarda los registros. Una recuperación posterior, tras volver a farmear y acumular otros cinco minutos de mapa vacío, dispone de un nuevo máximo de diez intentos.

**Stop service** y **Stop** cancelan tanto el farmeo como cualquier reintento pendiente. Las estadísticas no se reinician y los límites siguen contando lo ya acumulado. La nueva conexión valida nuevamente al jugador y no recupera la cola de transferencias del proceso anterior.

### Capturas y llamadas del juego

La ingeniería inversa se utilizó para identificar contratos, estructuras y firmas del perfil admitido. El motor resuelve clases y servicios en ejecución y valida lo que va a utilizar; no distribuye la biblioteca del juego ni su metadata decompilada.

| Operación | Entrada utilizada en el perfil 0.433.0 |
| --- | --- |
| Resolver servicios | `Zenject.Internal.ZenUtilInternal.GetAllSceneContexts` y `DiContainer.TryResolve(System.Type)` |
| Leer posición | `Niantic.Holoholo.Avatar.IMapAvatar.get_Location`, estructura `Niantic.LatLng` |
| Leer objetivos | `IMapPlaceDirectoryService.get_AllPokemon` / `get_ActivePokestops` |
| Leer alcance | `IGlobalSettings.get_Settings()` → `GlobalSettingsProto.MapSettings.EncounterRangeMeters` |
| Abrir encuentro | `MapPokemon.SendEncounterRequest()` |
| Aplicar baya | `IItemBag.UseItemForEncounter(Item, MapPokemon)` y respuesta `UseItemEncounterOutProto` |
| Lanzar | `MapPokemon.TryCapture(PokeballThrow, ARPlusEncounterValuesProto)` |
| Girar | `IRpcHandler.SendRpc<FortSearchProto, FortSearchOutProto>`; ID de operación `101` |
| Reciclar objetos | `IRpcHandler.SendRpc<RecycleItemProto, RecycleItemOutProto>`; ID `137` |
| Transferir | `IPokemonBag.ReleasePokemon(pokemon)`, seguido de reconciliación por ID |
| Investigaciones de campo | `IQuestService` e `IQuestRpcService.CompleteQuest`, `RemoveQuest` y `BeginQuestEncounter` |

El motor utiliza el alcance de encuentro leído del juego, hasta **50 m para seleccionar capturas**, y mantiene **80 m para giros**. El objetivo se vuelve a validar antes de cada lanzamiento, algo importante cuando el joystick está siguiendo una ruta.

El lanzamiento activa impacto y giro y aplica el perfil elegido en **Throw quality**. **Great** conserva el valor anterior, `ReticleSize = 0.5`, y sigue siendo la opción inicial. La elección de bola utiliza la primera disponible en el orden Poké Ball → Super Ball → Ultra Ball.

Solo hay una petición de acción pendiente. **Action interval** permite **100–3000 ms**, inicialmente **150 ms** en la interfaz. Separa envíos; no significa una captura cada 150 ms. Respuestas, comprobaciones, disponibilidad de objetivos y reconciliación también consumen tiempo.

```ts
// Fragmento del motor: abrir el encuentro y retener su promesa.
reserve(
  'encounter',
  object(pokemon.method<Obj>('SendEncounterRequest', 0).invoke()),
  nearestId,
  [pokemon]
);
```

*Fragmento simplificado de [agent.ts](engine/src/agent.ts): el archivo completo incluye comprobaciones, trazas y gestión de errores alrededor de la llamada.*

### Alcance y prioridad de captura

La ingeniería inversa permitió distinguir los campos de visibilidad y encuentro de `MapSettingsProto`. SilphMod lee **EncounterRangeMeters** para decidir qué objetivos puede seleccionar; no utiliza **PokemonVisibleRange** como si fuera un radio de captura ni modifica los ajustes del juego.

La lectura se actualiza cada **30 segundos** en el hilo Unity. El alcance efectivo admite valores positivos de hasta **50 m**; si la lectura no está disponible, es inválida o supera los **65 segundos** de antigüedad, vuelve al umbral anterior de **40 m**. Las bonificaciones de efectos aventura no se suman a este valor.

```js
const meters=sample.base.values?.EncounterRangeMeters;
if(typeof meters!=='number'||!Number.isFinite(meters)||meters<=0)
  return fallback('invalid_encounter_setting');
if(meters>MAX_VALIDATED_METERS)return fallback('outside_validated_range');
```

*Fragmento de [capture-planning.cjs](engine/src/capture-planning.cjs), con `MAX_VALIDATED_METERS = 50`.*

El mismo alcance se aplica al seleccionar, antes de abrir el encuentro, antes de usar una baya o lanzar y al observar la presencia de Pokémon para la recuperación del mapa. Si la ruta aleja al personaje del objetivo, el motor lo deja pendiente y continúa buscando otros encuentros válidos.

Cuando ya hay un encuentro abierto, se da prioridad a su baya y lanzamiento antes de despachar la transferencia de la captura anterior. Esta prioridad puede aplazar la cola hasta **tres segundos**; después se atiende su entrada más antigua. Se conserva una sola solicitud pendiente y la confirmación individual de las transferencias.

```js
const age=now-oldestOwnedAt;
return Number.isFinite(age)&&age>=0&&age<MAX_TRANSFER_DEFERRAL_MS;
```

*Decisión de prioridad en [capture-planning.cjs](engine/src/capture-planning.cjs), con `MAX_TRANSFER_DEFERRAL_MS = 3000`.*

El escaneo sin acciones se espacia **250 ms** y se evitan lecturas repetidas de inventario durante esa espera. **Action interval** mantiene su rango de **100–3000 ms**: la optimización cambia el orden del trabajo, no envía capturas en paralelo ni elimina la espera por las respuestas del juego.

### Throw quality: del contrato IL2CPP al lanzamiento

El contrato de `Niantic.Holoholo.Encounter.PokeballThrow` distingue los grados **Normal = 1**, **Nice = 2**, **Great = 3** y **Excellent = 4**. Se pudieron identificar los campos `ReticleSize`, `HitBullseye`, `Missed` y `Spinning`, y la firma que recibe esa estructura en `MapPokemon.TryCapture`.

La calidad no se aplica con un toque sobre la pantalla ni escribiendo una etiqueta de recompensa. SilphMod prepara la estructura de lanzamiento que utiliza el juego, con un tamaño de retícula específico para cada perfil:

```js
const PROFILES = Object.freeze({
  NORMAL: Object.freeze({grade: 1, reticleSize: 1.1, hitBullseye: true}),
  NICE: Object.freeze({grade: 2, reticleSize: 0.85, hitBullseye: true}),
  GREAT: Object.freeze({grade: 3, reticleSize: 0.5, hitBullseye: true}),
  EXCELLENT: Object.freeze({grade: 4, reticleSize: 0.05, hitBullseye: true})
});
```

*[throw-profile.cjs](engine/src/throw-profile.cjs). `grade` identifica el perfil en las trazas; no se escribe como campo de la estructura del juego.*

El motor crea `PokeballThrow`, aplica esos campos y pasa la estructura a la llamada de captura:

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

*Dos fragmentos de [agent.ts](engine/src/agent.ts). Entre ellos se comprueban el control activo y el ID del encuentro, y se registran la petición y el intento.*

El contador suma las recompensas recibidas del juego, no una cantidad fija asociada al selector de lanzamiento.

### Bayas: confirmar el efecto antes de lanzar

se identificó `PokemonEncounterResponse.InitialActiveItem`, el método de dos argumentos `IItemBag.UseItemForEncounter` y los campos `Status` y `ActiveItem` de `UseItemEncounterOutProto`. El agente valida esos contratos y utiliza el servicio del juego.

Se pueden seleccionar las cinco bayas y ordenar su prioridad:

| Baya | Identificador del contrato |
| --- | ---: |
| Frambu · `RazzBerry` | 701 |
| Latano · `NanabBerry` | 703 |
| Piña · `PinapBerry` | 705 |
| Frambu dorada · `GoldenRazzBerry` | 706 |
| Piña plateada · `GoldenPinapBerry` | 708 |

La prioridad inicial es **Frambu → Piña → Latano**; las dos bayas especiales se seleccionan expresamente. Se utiliza la primera elegida con unidades disponibles, no varias bayas seguidas para un mismo efecto activo.

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

*Fragmentos de [agent.ts](engine/src/agent.ts). Entre ambos se comprueba la autorización de control activo.*

El motor retiene la promesa y espera su respuesta antes de lanzar. Para este contrato, **Status = 0** corresponde a la aplicación de la baya; no se confunde con el código de éxito de una captura. La política comprueba también el objeto activo y solo entonces incrementa el contador:

```ts
const status=number(result.field('Status').value), activeItem=number(result.field('ActiveItem').value);
const verdict=berryCycle.settle(status,activeItem,p.item);
if(verdict.consumed)stats.berriesUsed++;
```

*Código de [agent.ts](engine/src/agent.ts), con la decisión en [berry-policy.cjs](engine/src/berry-policy.cjs).*

Si ya existe una baya activa, no se aplica otra. Cuando la respuesta de captura indica que el Pokémon salió de la bola, el ciclo permite volver a aplicar una baya antes del siguiente intento. Un rechazo de la baya no provoca una cadena de reintentos; la captura puede continuar sin ella. La existencia y distancia del objetivo se vuelven a comprobar antes de actuar.

No se añade una espera fija para la animación de la baya: se conserva **Action interval** y una sola petición pendiente. Usar bayas añade la solicitud y su respuesta reales, por lo que sí consume tiempo de comunicación. **Item limits** continúa eliminando los excedentes seleccionados; la captura usa las unidades que queden. La XP sigue proviniendo del recibo de captura, no de un bono inventado por haber aplicado una baya.

### Interrupciones y continuidad del mapa

El manejo de interrupciones permanece **siempre activo** durante el farmeo. Observa la apertura y visibilidad de pantallas compatibles y utiliza `Dismiss()`, `Click()` o `LuckyBonusSkipButtonClicked()` según el controlador. Incluye medallas, subida de nivel, XP, resumen del incienso diario, aviso de velocidad y el aviso de nueva colección de sellos de **Stamp Rally**.

Las eclosiones tienen además un adaptador en `MapExploreState`: identifica la firma que recibe `HatchedPokemonsData` y omite la presentación antes de que se abra la escena. La obtención del Pokémon y las respuestas de recompensa siguen a cargo del juego. Si una pantalla de eclosión ya está abierta, se atiende mediante su controlador compatible.

Las acciones se ejecutan en el hilo Unity, con el mismo control de sesión y foco que el farmer. No hay sondeo continuo de todas las ventanas: únicamente se conserva una cola limitada cuando aparece una pantalla reconocida. Las pantallas ajenas a esa lista no se cierran automáticamente.

Los registros `spin_scan` explican la selección de Poképaradas, su distancia, enfriamiento, mochila llena y límite de recorrido de la lista cargada. Esta observación reutiliza el escaneo existente y no añade solicitudes al juego ni modifica el radio o los enfriamientos.

### Investigaciones de campo

Las investigaciones de campo se gestionan **siempre** durante el Farmer, incluidas las que llegan al girar Poképaradas. El motor consulta `IQuestService`, reclama las tareas que el juego permite canjear y procesa las recompensas de objetos, bayas, XP, polvo estelar o encuentros que reciba.

El ámbito se identifica con **QuestContext = 2** y sus encuentros con **EncounterType = 6**. Las investigaciones especiales y temporales quedan fuera de ese ámbito. Las tareas de campo incompatibles —como incursiones o intercambios— y las de tipo o condición desconocidos se eliminan para dejar espacio a otras; se conserva el espacio de escaneo AR.

```js
if (!task || task.context !== FIELD_CONTEXT || typeof task.id !== 'string' || !task.id.trim())
  return {action: 'keep', reason: 'outside_field_research'};
if (task.redeemable === true) return {action: 'claim', reason: 'game_can_redeem'};
```

*Fragmento de [field-research.cjs](engine/src/field-research.cjs). El contrato de servicios y respuestas se resuelve en [field-research.ts](engine/src/field-research.ts).*

Los encuentros de recompensa usan el ciclo de bayas, calidad de tiro y captura del Farmer. Se procesan aunque su especie no esté seleccionada en **Only catch**. Después, **Keep field research encounters** decide si se conservan por origen; con esa opción desactivada se aplican las demás protecciones de transferencia.

### Remove buddy

**Remove buddy** viene desactivado. Al marcarlo, el motor consulta si hay compañero y solicita retirarlo con identificador Pokémon **0**. Una respuesta confirmada da paso al reinicio del juego para continuar el Farmer sin compañero. Si ya no había compañero, continúa directamente.

Se utilizan los contratos `BuddyMapProto` y `SetBuddyPokemonProto`, mediante `IRpcHandler.SendRpc`, con las operaciones **1350** y **152** del perfil del juego. La retirada se realiza una vez por inicio del servicio; no se repite en cada recuperación del mapa.

Retirarlo no lo transfiere ni modifica sus datos de inventario. Al terminar no se vuelve a asignar: puedes colocarlo nuevamente desde el juego cuando quieras. La coordinación de este flujo está en [buddy-removal.cjs](engine/src/buddy-removal.cjs).

### Inventory tools

**Show IV names**, activado inicialmente, cambia temporalmente el texto mostrado en las filas del inventario. Por ejemplo, un Pokémon con IV **15/14/13** se muestra como **93% 15/14/13**. El porcentaje es entero y el nombre guardado en la cuenta no cambia.

```js
return `${Math.floor(((data.attack+data.defense+data.stamina)/45)*100)}% ${data.attack}/${data.defense}/${data.stamina}`;
```

*Formato utilizado en [inventory-tools.cjs](engine/src/inventory-tools.cjs). Se aplica durante la sesión y se retira al detener el motor.*

**Extended manual transfer**, desactivado inicialmente, amplía la selección múltiple del inventario para Pokémon especiales, incluidos shiny y determinadas clases legendarias. La selección y la confirmación las hace el usuario desde el juego. Favoritos, compañeros y otras protecciones de inventario siguen excluidos; esta función no añade Pokémon anteriores a la cola de transferencias automáticas.

### Transferencias y borrado

Los identificadores de 64 bits permanecen como cadenas exactas o valores `UInt64`, nunca como números JavaScript susceptibles de perder precisión. La autorización combina petición emitida, respuesta reciente y exitosa, clase correcta, ID capturado y sesión actual.

La política de mochila es independiente de la animación o de que visualmente parezca llena:

```js
// Fragmento de engine/src/policy.cjs
const count = counts[name];
if (!Number.isInteger(count) || count <= keep) continue;
return {
  name, item: ITEMS[name], count: Math.min(count - keep, maxBatch),
  before: count, keep
};
```

Una transferencia no incrementa el contador únicamente por haber sido solicitada: también debe desaparecer del inventario tras la respuesta aceptada. Los resultados desconocidos no autorizan reintentos irreversibles.

### Ubicación y Error 12

El adaptador usa la ABI JNI para identificar `android.location.Location.isFromMockProvider()` y, si existe, `isMock()`. Trata únicamente las respuestas booleanas correspondientes a esos métodos dentro del **proceso al que se conectó**. No modifica coordenadas, objetos `Location`, AppOps, servicios de Google, SELinux ni módulos del sistema.

El tratamiento se instala al iniciar con **Fix error 12** y se retira al detener el servicio. Resuelve los métodos disponibles según el SDK del proceso, incluida la variante moderna `isMock()`, y comparte el mismo ciclo de vida en UGPhone y teléfonos físicos. La implementación está en [engine/location](engine/location).

### Contador XP y polvo estelar

Se contabilizan las recompensas de respuestas exitosas de **capturas, giros e investigaciones de campo emitidos por SilphMod**: `CaptureScoreProto.Exp`, `Stardust`, recompensas de evolución temporal/mega, `FortSearchOutProto.XpAwarded`, `Loot`, `BonusLoot` y `CompleteQuestOutProto.Rewards` según el contrato validado. Los totales se conservan al detener o recuperar el servicio y solo se restablecen con **Clear statistics**.

No se resta el saldo de la cuenta ni se estiman premios multiplicando capturas por una constante. No se vuelven a multiplicar los importes por bonos que ya pudiera haber aplicado el juego. Una secuencia creciente evita duplicados; los totales Android usan `long`. `—` indica lectura no disponible y `+` un total parcial con recompensas faltantes.

## Compilación

Requisitos: JDK **17 o superior**, Android SDK **36**, Android NDK **r28c / 28.2.13676358**, Node.js **22 o superior** y Python **3.11 o superior**. Android Studio puede abrir la raíz del repositorio. Configura `JAVA_HOME` y `ANDROID_HOME`, o crea tu propio `local.properties` con `sdk.dir`.

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

La preparación nativa descarga los ejecutables oficiales ARM64 de **Frida 17.22.1** y **17.23.3**, verifica archivo comprimido, binario, tamaño y formato ELF, y no los ejecuta en el ordenador. También acepta `--archive /ruta/al/archivo-oficial.xz` junto al backend correspondiente para una preparación offline. El adaptador de carga se compila desde [native](native) con el NDK indicado; [build_loader.ps1](tools/build_loader.ps1) contiene sus opciones de compilación para Windows.

Gradle reconstruye el agente desde el código fuente y genera su SHA-256 esperado durante la compilación. No necesitas editar una constante a mano ni copiar archivos desde otro proyecto. El binario nativo se verifica de nuevo antes de compilar.

Sin configuración de firma, la salida release es **unsigned**. Para firmarla, proporciona por entorno `SILPH_KEYSTORE`, `SILPH_STORE_PASSWORD`, `SILPH_KEY_ALIAS` y `SILPH_KEY_PASSWORD`. Las claves quedan fuera del repositorio; actualizar una instalación existente requiere el mismo certificado.

Salida Android: `app/build/outputs/apk/release/`. El APK, `node_modules`, caches, logs, claves y runtime descargado están excluidos de Git.

## Registros y privacidad

La exportación es local y manual. Incluye estado final, configuración, eventos del motor, salud, arranque, fuentes Android accesibles, logcat y reportes de cierre, con límites de tiempo/tamaño y rotación. No equivale a recoger toda la memoria ni a conservar cada línea de una sesión indefinidamente. Las sesiones antiguas no se eliminan automáticamente: revisa el espacio disponible.

Los ZIP **no añaden censura** y pueden incluir coordenadas, identificadores y mensajes de otros procesos.

## Estructura

```text
SilphMod/
├── app/                  # Una sola aplicación Android
├── engine/
│   ├── src/              # Motor, políticas y control de sesión
│   └── location/         # Adaptador JNI
├── docs/
│   └── assets/           # Identidad, arquitectura y fragmento de código
├── native/               # Adaptador de carga ARM64
├── tools/                # Preparación nativa y catálogo de imágenes
├── gradle/               # Wrapper de compilación
├── LICENSE
├── NOTICE
└── README.md
```

## Estado y próximas mejoras

**1.6.0 incorpora calidad de lanzamiento seleccionable**, uso de bayas por prioridad, manejo selectivo de interrupciones, límites de sesión de capturas y giros, continuidad al desplegar las notificaciones y un intervalo de acciones de **100–3000 ms**, con valor inicial de **150 ms**. Las transferencias y los descartes mantienen sus cuotas totales propias ilimitadas; la sesión completa finaliza al alcanzar cualquiera de los máximos de captura o giros.

**1.7.0 añade recuperación automática del mapa con hasta diez intentos de arranque**, estadísticas persistentes con borrado manual, bloqueo del inicio cuando se han alcanzado los límites acumulados y filtros de conservación para Dynamax, eclosionados, fondos y la combinación shiny + fondo.

**1.9.0 reúne el Farmer en una sola APK para UGPhone y teléfonos físicos con root**, incorpora **Only catch**, investigaciones de campo automáticas, **Remove buddy**, nombres temporales por IV y transferencia manual ampliada. Añade el alcance de encuentro leído del juego de hasta **50 m**, prioridad acotada para terminar capturas activas, menor repetición de lecturas y mejoras en la exportación de registros. El manejo de interrupciones incluye la presentación de eclosiones, el aviso de velocidad y el aviso de Stamp Rally.

La siguiente fase es seguir optimizando la continuidad del Farmer y ampliar la compatibilidad entre dispositivos.

Se buscará agregar la configuración del orden jerárquico del uso de pokebolas. No la de por defecto: 1. Pokeball; 2. Superball; 3. Ultraball

Algo pendiente también es seguir ampliando el catálogo de pokemones configurados para no transferir, por ejemplo con nundos (0/0/0).

Licencia [MIT](LICENSE). Las dependencias conservan sus avisos en [NOTICE](NOTICE) y sus licencias dentro de los assets. Proyecto independiente, sin afiliación con los titulares del juego.

Take care, little guy.