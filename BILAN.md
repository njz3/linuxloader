# Bilan des jeux Linux de racecab1 sur le loader Windows

Rempli au fur et à mesure, jeu par jeu (demande du 22/10). Chaque jeu est copié seul depuis racecab1, débarrassé des
fichiers TeknoParrot et FFB Plugin, essayé, puis mis à la Corbeille. Build MinGW ; essais en mode agent (fenêtre
cachée, entrées injectées, retour de force lu dans le modèle de la carte servo), puis à travers vJoy (fenêtre visible).
Depuis le 22/10 au matin, le loader demande le GPU dédié des portables à deux GPU (`NvOptimusEnablement`) : ce PC a une
GeForce RTX 3050 Laptop à côté de l'Intel UHD, et les essais tournent sur la GeForce.

Légende : ✅ vérifié · ⚠️ partiel ou non vérifié · ❌ ne marche pas · ⏳ pas encore essayé · — sans objet

## Inventaire du partage de racecab1 (23/10)

`\\racecab1\C\racecab\Games` : 65 dossiers. Chacun parcouru sur 4 niveaux (dossiers de données exclus) : en-tête
des fichiers sans extension, `.elf`, `.bin`, `.out` lu (ELF ou non), fichiers `.exe` comptés. **18 jeux Linux**, les
47 autres sont des jeux Windows (le projet windowsloader). Les deux ELF de Daytona Championship USA sont des programmes
annexes (`Sega\assets\rpi-billboard`, une mise à jour), pas le jeu.

| Dossier sur racecab1 | Système | Programme | Dans le loader | État |
|:--|:--|:--|:--|:--|
| Outrun 2 SP SDX - SEGA Lindbergh | Lindbergh | `disk0/Jennifer/Jennifer` | `OUTRUN_2_SP_SDX_*` | ✅ partie, FFB RS422, menu de service |
| Hummer Extreme - SEGA Lindbergh | Lindbergh | `disk0/hummer_Master.elf` | `HUMMER_EXTREME_SBST` | ✅ partie, FFB RS422 (vibrations), menu de service ; dump en 2560×1440 |
| Sega Race TV - SEGA Lindbergh | Lindbergh | `drive.elf` | `SEGA_RACE_TV_SBPF` | ✅ partie complète, FFB RS422, menu de service |
| R-Tuned Ultimate Street Racing - SEGA Lindbergh | Lindbergh | `dsr` (et variantes TeknoParrot) | `R_TUNED_SBQW` | ✅ course entière, FFB RS422, menu de test, vJoy ; MSVC : plus de plantage |
| Initial D 4 - SEGA Lindbergh | Lindbergh | `id4.elf` | `INITIALD_4_SBML_REVB` | ✅ partie, FFB (par défaut), menu de service, vJoy |
| Initial D 4 Export (Fr Traduction) - SEGA Lindbergh | Lindbergh | `id4.elf` (Rev D) | `INITIALD_4_EXP_SBNK_REVD` | ✅ partie, FFB, menu de service, vJoy |
| Initial D 5 Export - SEGA Lindbergh | Lindbergh | `id5.exe` (ELF) | `INITIALD_5_EXP20_SBTS` | ✅ partie (HUD complet), FFB, menu de test, vJoy ; MinGW et MSVC |
| Dead Heat - Namco ES1 | Namco ES1 | `a.elf` | `DEAD_HEAT_US` | ✅ course (MinGW et MSVC), FFB (Kickback), menu de test, vJoy |
| Maximum Heat 3D - Namco ES1 | Namco ES1 | `a.elf` | `MAXIMUM_HEAT_3D` | ✅ partie, FFB, menu de test, vJoy |
| Nirin - Namco ES1 | Namco ES1 | `exec/a.elf` | `NIRIN` | ✅ course, menu de test, vJoy |
| Dead Heat Riders - Namco N2 | Namco N2 | `a.elf` | `DEAD_HEAT_RIDERS` | ✅ course, FFB, menu de test, vJoy |
| Wangan Midnight Maximum Tune 3 (Export) - Namco N2 | Namco N2 | `main` | `WANGAN_MIDNIGHT_MT3` | ✅ course (MinGW et MSVC), FFB, menu de test, son, lecteur-imprimante de cartes (carte neuve écrite puis relue), vJoy |
| Wangan Midnight Maximum Tune 3DX+ (Japan) - Namco N2 | Namco N2 | `main` | `WANGAN_MIDNIGHT_MT3DX` | ✅ course (MinGW et MSVC), FFB, menu de test, vJoy, cartes (neuve, lue, écrite et imprimée en fin de partie), jeu sans lecteur |
| Cruisn Blast 1.25 - Raw Thrills PC based | Raw Thrills (Linux) | `game` | — | ❌ non fait |
| Cruisn Blast 2.31 - Raw Thrills PC based | Raw Thrills (Linux) | `game` | — | ❌ abandonné |
| Gaelco Championship Tuning Race - Gaelco PC Based | Gaelco PC | image disque, `.../salidas/car` | `GAELCO_TUNING_RACE` | ✅ course, FFB, menu Test, son émulé, vJoy |
| Ring Riders - Gaelco PC Based | Gaelco PC | image disque, `.../salidas/gameport` | `RING_RIDERS` | ✅ course, FFB, menu TEST, son émulé, vJoy |
| Tokyo Cop - Gaelko PC Based | Gaelco PC | image disque, `.../salidas/gameport` | `TOKYO_COP` | ✅ poursuite, FFB, menu de test, son émulé, vJoy |

Bilan (23/10, nuit) : 16 des 18 jeux Linux se jouent entièrement (course, FFB, menu de test, vJoy), Maximum Tune 3
et 3DX+ avec leurs cartes, R-Tuned, Initial D 4 Japon, Initial D 5 et Dead Heat compris ; restent les deux Cruis'n
Blast (non faits). Les courses de Maximum Tune 3 et 3DX+ en MSVC, dernière
limite connue de cette build, sont corrigées le 24/10. Dans `gameData.c` (l'état que montre le sélecteur), Dead Heat Riders,
Nirin, Maximum Tune 3 et 3DX+ sont en `WORKING`.

Jeux Windows (47) : Taito Type X, X+ et X2 (Battle Gear 4 et ses deux Tuned 2.8, Chase H.Q. 2, D1GP Arcade, Valve
Limit R, Wacky Races) ; SEGA Ringedge, NU, Ringwide, ALLS, Europa-R et PC (Initial D 6 AA, 7 AAX, 8 Infinity,
Initial D Arcade Stage Zero 1.31 et 2.30, K.O. Drive, Let's Go Safari, Sonic & Sega All Stars Racing, Sega World
Drivers Championship, Sega Rally 3 en 9 éditions et Revo, Daytona Championship USA et sa New Season Edition, Ford
Racing Full Blown, Showdown, Storm Racer G) ; Namco ES3, ES3X et ES3B (Mario Kart Arcade GP DX 1.10 et 1.18,
Maximum Tune 5, 5DX+, 6) ; Raw Thrills avant Cruis'n Blast (Batman, Cars, Dirty Drivin, H2 Overdrive) ; Konami (GTI
Club Super Mini Fiesta, Road Fighters 3D) ; autres (Crazy Ride, CrazySpeed, Frenzy Express, Storm Rider, Wasteland
Racers 2071).

## Cartes réelles et modes d'affichage (23/10)

- **Passthrough série** (Tab > Options du jeu > Cartes) : carte moteur du volant, carte de mouvement d'OutRun SP
  SDX et carte JVS, chacune « Émulée » ou « Vraie carte sur COMn » ; sous Windows le port COM est réglé comme le
  jeu règle son port Linux. Vérifié sur OutRun SDX avec un faux boîtier de l'autre côté d'une paire virtuelle
  (COM10/COM11) : la carte moteur et le JVS dialoguent, les pièces s'affichent. Pas encore essayé avec de vraies
  cartes.
- **Description des jeux** : carte d'E/S des contrôles et carte moteur de chaque jeu (sélecteur, Options du jeu).
- **Affichage** : `BORDERLESS` et `BORDERLESS_STRETCH` (plein écran fenêtré) ; `FULLSCREEN` change désormais la
  résolution de l'écran pour celle du jeu.

## Synthèse par jeu et par fonction

| Jeu | Démarrage | Affichage | Contrôles | vJoy | Retour de force | Menu de service | Remarques |
|:--|:--|:--|:--|:--|:--|:--|:--|
| Initial D 4 Export FR (Rev D) | ✅ | ✅ 1360×768 plein cadre | ✅ pièces, Start, volant, gaz, frein | ✅ partie menée par vJoy, retour de force reçu | ✅ couple 84, amortisseur 86, vibrations 85 | ✅ menu complet, navigation, test des sorties | menus et démo en français |
| Maximum Heat 3D | ✅ contrôle du link passé (borne seule) | ✅ 1360×768 | ✅ pièces, menus à l'accélérateur, volant, frein, vue | ✅ partie menée par vJoy, retour de force reçu | ✅ ressort, amortisseur, couple (carte Kickback) | ✅ menu de test, test des E/S, retour au jeu | caméra coupée (pas de /dev/video0) |
| Dead Heat Riders | ✅ (environ 70 s : réseau, caméra absente) | ✅ 1360×768 (MinGW et MSVC) | ✅ pièces, Nitro (DECIDE), Vue, Enter, guidon, accélérateur, frein (contact) ; test des commandes juste | ✅ course menée par vJoy (pièces, Nitro, guidon, accélérateur, frein), retour de force reçu | ✅ carte Kickback : ressort, amortisseur, couple | ✅ menu de test, I/O TEST, SWITCH TEST | photos absentes (pas de caméra) |
| Nirin | ✅ | ✅ 1360×768 (MinGW et MSVC) | ✅ pièces, OK (View), Select (pencher), guidon, poignée, frein, shift up/down ; test des commandes juste | ✅ course menée par vJoy | — (pas de moteur) | ✅ menu de test, I/O TEST, SWITCH TEST |  |
| Wangan Midnight Maximum Tune 3 (Export) | ✅ contrôles HANDLE et CARD passés (CARD R/W OFF dans le menu de test) ; réseau absent (borne seule) | ✅ 640×480 : pilote NVIDIA intégré au programme remplacé par la fenêtre du loader (MinGW et MSVC) | ✅ pièces, accélérateur (menus), volant, frein, View Change, Intrude Change, levier en H à 6 vitesses depuis Gear Up/Down ; SWITCH TEST juste | ⏳ | ✅ carte Kickback : ressort, amortisseur, couple | ✅ menu de test, GAME OPTIONS, I/O TEST, SWITCH TEST | son par OpenAL Soft (capture WAV) ; cartes : lecteur-imprimante pas encore émulé |
| Ring Riders | ✅ | ✅ sur GPU NVIDIA (blanc sur Intel : register combiners) | ✅ pièces, Start, guidon (sens corrigé), poignée, frein | ✅ course menée par vJoy (pièce, Start, Service, guidon, poignée, frein), retour de force reçu | ✅ moteur du guidon (rappel au centre, sens corrigé) et vibreur | ✅ menu TEST, Input/Output, mire | son émulé (carte son Gaelco) ; plateforme de mouvement absente (« MOTION SYSTEM ERROR LOG ») |
| Tokyo Cop | ✅ | ✅ sur GPU NVIDIA | ✅ pièces, klaxon (Start), volant (sens corrigé), accélérateur, frein (contact, nouveau), sirène, vue | ✅ poursuite menée par vJoy (pièce, klaxon, volant, accélérateur, sirène, frein), retour de force reçu | ✅ moteur du volant (rappel au centre, sens corrigé) | ✅ menu TEST (code de touche 0x14), Controls Test, mire | son émulé ; « MOTION SYSTEM ERROR! » (siège mobile absent) |
| Gaelco Championship Tuning Race | ✅ (dérouleur C++ du jeu) | ✅ 720×540 natif | ✅ pièces, Start, volant, accélérateur, frein, vue, palettes (boîte manuelle) | ✅ course menée par vJoy (pièce, Start, volant, accélérateur, frein, vue), retour de force reçu | ✅ moteur du volant (rappel au centre) | ✅ menu Test, Controls Test (toutes les commandes affichées justes) | son émulé ; plateforme de mouvement absente ; « Control de produccion » (écran d'usine) fait quitter le jeu |
| Dead Heat (non-régression) | ✅ (link d'une borne seule, environ 60 s) | ✅ sur GPU NVIDIA | ✅ course | ✅ partie menée par vJoy, retour de force reçu | ✅ ressort, amortisseur, couple | ✅ menu de test, I/O TEST | revérifié le 23/10 (MinGW et MSVC) ; patch du PCB ID retiré |

## Bilan par fonction (22/10)

- **Démarrage** : les 8 jeux démarrent et se jouent (ID4 FR, Maximum Heat 3D, Dead Heat, Dead Heat Riders, Nirin,
  Ring Riders, Tokyo Cop, Gaelco Tuning) ; les clés de Dead Heat Riders et de Nirin sont répondues par le loader
  (demande du 22/10 au soir).
- **Affichage** : chaque jeu à sa résolution native (1360×768 pour Namco et ID4, 640×480 Ring Riders et Tokyo Cop,
  720×540 Gaelco Tuning), mise à l'échelle par le loader ; ASPECT_RATIO aligné sur windowsloader ; GPU dédié demandé
  (indispensable aux jeux Gaelco, qui dessinent avec les register combiners NVIDIA).
- **Contrôles** : tous vérifiés en mode agent (pièces, Start, volant, pédales, boutons propres à chaque jeu) et, pour
  les jeux Gaelco, dans leur propre écran de test des commandes. Corrigé en route : le sens du volant des trois jeux
  Gaelco, le frein de Tokyo Cop (un contact), ses boutons sirène et vue, la touche fantôme du port 0x42.
- **Retour de force** : chaque carte du jeu est convertie en trames de la carte servo Sega pour le modèle FFB du
  loader (ID4 : `_sSCI::send` ; Namco : carte Kickback ; Gaelco : sorties moteur 4 et 5 et vibreur). Le sens du rappel
  est vérifié par le signe du couple pour les jeux Gaelco.
- **vJoy** : les cinq jeux qui démarrent sont joués de bout en bout par vJoy, le retour de force reçu par vJoy (Ring
  Riders 3 574 forces constantes et 71 vibrations en course, Tokyo Cop 900, Tuning 1 909).
- **Menu de service** : ouvert et parcouru pour ID4 FR, Maximum Heat 3D, Ring Riders, Tokyo Cop (depuis aujourd'hui)
  et Gaelco Tuning ; Dead Heat aussi depuis le 23/10.
- **Son** : non vérifié pour ID4 et les jeux Namco (essais sans écoute). Jeux Gaelco : leur carte son est émulée
  (`hardware/gaelco/gaelcoSound.c`, ci-dessous) ; la musique de Tuning capturée est identique à son échantillon
  (corrélation 1,000, niveau attendu), Ring Riders et Tokyo Cop jouent du début à la fin.
- **Mouvement** : les plateformes et sièges mobiles Gaelco sont absents (messages « MOTION SYSTEM ERROR », sans gêne
  pour le jeu).

## Détail par jeu

### Initial D 4 Export FR (Rev D, SBNK)
- **Démarrage** : l'écran « Card Stocker Cleaning » venait du `sram.bin` neuf (compteur du stocker à 0) : les
  vérifications de nettoyage renvoient 0, comme pour ID5.
- **Affichage** : `initialD.ini` de TeknoParrot (1920×1080, hors des limites du jeu) donnait une image de 800×1080
  coupée ; la résolution est maintenant imposée, en jeu (taille de la fenêtre) et dans le programme de test
  (1360×768).
- **Contrôles** (mode agent) : pièces, Start, menus (licence, constructeur, modèle, couleur, boîte, mode, circuit,
  rival), course : départ, accélération jusqu'à 174 km/h, volant à gauche et à droite, frein.
- **Retour de force** : la carte de retour de force d'ID4 (`cFeedbackSCI`) parle le protocole de la carte servo
  Sega sur un port série ; ses trames de 4 octets (`_sSCI::send`) vont maintenant au modèle FFB du loader, à l'agent
  et aux network outputs. En course : 3 641 trames de couple (84), 55 vibrations (85), l'amortisseur (86).
- **Menu de service** : programme de test (`-t`) : « GAME TEST MODE », navigation avec Service, Test pour entrer et
  sortir (test des sorties vu).
- **vJoy** : partie menée entièrement par vJoy (pièces, Start dans les menus, accélérateur, volant) dans une fenêtre
  visible ; vJoy reçoit 48 forces constantes (non nulles dès le départ de la course), 6 conditions, 2 vibrations.
- `id4.elf` est l'ELF anglais ; les textes français viennent des données ; `id4.elf originallllll` (traduit) non
  essayé.

### Maximum Heat 3D (Namco ES1, US DRIVE rév. 8807)
- Le moteur de Dead Heat (rév. 6273) : entrée de jeu (CRC partiel 0xF932E88E) et groupe commun avec Dead Heat ;
  mêmes patches à ses adresses (groupe du link, destructeurs) ; cartes N2 et Kickback émulées par une
  table d'adresses par jeu.
- **Démarrage** : ses shaders commencent par `#version 120` (la déclaration de `gl_TexCoord` va après) ; la caméra
  (`USE_CAMERA_DEVICE`) est coupée ; le contrôle du link au démarrage passe maintenant comme sur une borne seule :
  - le jeu règle lui-même son adresse (`sudo ifconfig eth0 192.168.32.115`) et la relit (`ifconfig.pl`, ioctl) :
    le loader garde l'adresse réglée ;
  - ce PC répond pour cette adresse : ses propres paquets multicast reviennent de 192.168.32.115, ses connexions
    TCP vers elle vont à 127.0.0.1, `getpeername`/`getsockname`/`accept` rendent son adresse ;
  - au passage : options de socket Linux traduites, `write`/`read` sur les sockets, `shutdown` en cdecl.
- **Contrôles** (mode agent) : pièces, accélérateur pour choisir (voiture, couleur, ville, cadre photo), course :
  départ, jusqu'à 211 km/h, boîte, volant, frein, changement de vue.
- **Retour de force** : 287 ressorts, 17 amortisseurs, 178 forces constantes en course (carte Kickback → trames
  servo).
- **Menu de service** : interrupteur Test (qui reste enclenché) : menu, curseur, test des E/S ; Test relâché, le jeu
  redémarre.
- **vJoy** : partie menée par vJoy ; vJoy reçoit 50 conditions et des forces constantes en course.
- Dead Heat à revérifier (code commun modifié) : à recopier quand le partage sera libre.

### Changements pour tous les jeux (22/10)
- **GPU dédié** : `NvOptimusEnablement` et `AmdPowerXpressRequestHighPerformance` exportés par le loader (les jeux
  visent NVIDIA ; Gaelco dessine avec les register combiners que l'Intel n'a pas).
- **Titre de fenêtre** : « titre [nom court] - FPS: n » (`[id4-exp-rev-d]`, `[deadheat]`...) pour l'autodétection de
  BackForceFeeder, comme windowsloader (demande de l'utilisateur relayée par windowsloader).
- **ASPECT_RATIO** aligné sur windowsloader : 4:3 et 16:9 sont la forme du cadre, l'image garde la sienne dedans avec
  des bandes noires ; seuls les modes étirés étirent ; 4:3_LETTERBOX = 4:3.
- Mode agent sans manette : le modèle de la carte servo démarre quand même (le journal `ffb` de l'agent marche avec
  vJoy caché).

### Dead Heat Riders (Namco, DH RIDERS rév. 1247)
- Le moteur de Dead Heat : entrée de jeu (CRC partiel 0x5D3DF1CD), groupe Dead Heat (shaders, entrées).
- libstdc++ plus récente (GLIBCXX_3.4.14) : le loader choisit maintenant `libstdc++.so.6.0.N` selon ce que le jeu
  demande ; `libstdc++.so.6.0.14` ajoutée aux dépendances (reprise de `ElfLdr2\libs` de TeknoParrot) ; `fgetpos`,
  `fsetpos`, `strtof`, `strtold`, `strtoul`, `frexpl`, libusb sans périphérique.
- **Démarrage** (environ 70 s) : contrôle du réseau d'une borne seule (60 s), I/O CHECK « CONNECTING OK », attente de
  la caméra (10 s, abandonnée seule), démo.
- **Cartes** : N2 et Kickback émulées par `deadHeatIo.c` (même bloc que Dead Heat, calibrage espacé de 16 octets,
  type de couple à 0, auto-test passé). Pas de levier de vitesses.
- **Contrôles** (mode agent, SWITCH TEST) : Nitro sur Boost (aussi « DECIDE » des menus), View Change, Service, Enter
  (Start), guidon -300/+300 (LEFT OK/RIGHT OK), accélérateur +1000, frein en contact.
- **Course** : San Francisco, de la 7e à la 3e place, 198 km/h avec la nitro ; retour de force : ressort, amortisseur,
  couple constant en virage. MinGW et MSVC.
- **vJoy** : partie menée par vJoy dans une fenêtre visible (pièces, Nitro = bouton 7 de vJoy, guidon, accélérateur,
  frein) jusqu'à 207 km/h ; vJoy reçoit 307 forces constantes et 469 conditions.
- Photos des joueurs absentes (pas de caméra).

### Nirin (Namco ES1, NRN100, 2009)
- Entrée de jeu (CRC partiel 0x71E72FF6), groupe Dead Heat (shaders, interrupteur Test qui reste enclenché) ; six
  fonctions de libc et une de SDL ajoutées (dont un vrai `longjmp`).
- **Démarrage** : contrôles BACKUP DATA, JV I/O et CLOCK passés ; démo et écran titre. Carte d'E/S N2 émulée par
  `deadHeatIo.c` (bloc d'état d'une `n2jvio` plus ancienne, sans carte Kickback). Sauvegardes : `save0`/`save1`
  créés, `mkdir` et `find ... | sort` du jeu faits par le loader, `rename` qui remplace comme sous Linux.
- **Contrôles** (mode agent) : pièces comptées (CREDIT 2/2) ; dans le SWITCH TEST : Test, Service, Select (haut),
  Enter (Start), shift up (GearUp), shift down (GearDown), guidon dans les deux sens.
- **Menu de service** : interrupteur Test enclenché : MENU, I/O TEST, SWITCH TEST.
- **Partie** : « Insert 2 Coin(s) », OK des menus = bouton View, choix du mode, de la moto, de la boîte, du circuit ;
  course : tour complet, jusqu'à 249 km/h, la moto penche avec le guidon. MinGW et MSVC ; menée aussi par vJoy dans
  une fenêtre visible (BACKUP DATA OK, JV I/O OK, CLOCK OK au démarrage).
- **Calibrage** : le défaut de `data/save/testmode/handle_setting.lua` chargé par la remise à zéro du jeu quand il
  manque (sinon le guidon tournait à gauche des deux côtés) ; test des commandes : CENTER/LEFT/RIGHT OK, ACCEL et
  BRAKE de MIN à MAX.
- Plus de « BACKUP ERROR » au démarrage (liste `find` et `st_ctime` corrigés).
- Retour de force : le guidon de Nirin n'a pas de moteur (pas de carte Kickback dans le jeu).

### Wangan Midnight Maximum Tune 3 (Namco N2, Export, 2007)
- Entrée de jeu (CRC partiel 0xEC016AFD), groupe Dead Heat. Boost 1.33.1, Cg et sa libstdc++ sont dans `libso`,
  ajouté aux dossiers de recherche ; les liens symboliques de Linux y sont devenus de petits fichiers qui contiennent
  le nom de leur cible, que le loader suit maintenant.
- **Affichage** : le pilote NVIDIA « arcade » est lié dans le programme (couche `adm*`, qui pilote `/dev/nvidia`
  lui-même, et 1 102 points d'entrée OpenGL que le pilote remplit à l'exécution). Ses fonctions `adm*` sont
  répondues par la fenêtre et le contexte du loader (`graphics/admBridge.c`) et chaque point d'entrée OpenGL saute à
  la fonction du loader du même nom (346 utilisées, une seule absente puis ajoutée :
  `glGetCompressedTexImageARB`, sans laquelle les voitures sortaient noires : le jeu relit leurs textures compressées
  pour les peindre). Shaders Cg compilés par le Cg.dll de Windows. 640×480.
- **Démarrage** : `prepend-n2.pl` (dossiers du disque de travail, son copié dans `/tmp`) fait par le loader ;
  `/proc/cpuinfo` lu jusqu'à la fin ; mémoire d'Alchemy par `sbrk` (région réservée, pages engagées au fur et à
  mesure) ; clavier d'Alchemy lu sur l'entrée standard non bloquante. Contrôles du démarrage : HANDLE CHECK passé,
  réseau absent (« CANT START NETWORK », sans gêne pour une borne seule).
- **Cartes** : N2 émulée par `deadHeatIo.c` (bloc d'état de la `n2jvio` de Nirin) ; calibrage en centre et course ;
  carte Kickback en type de couple 0 (auto-test passé), ses champs ailleurs que ceux de Dead Heat.
- **Lecteur-imprimante de cartes** (`hardware/namco/n2CardPrinter.c`) émulé sur son port série (`/dev/ttyM2`) : avec
  CARD R/W à ON dans GAME OPTIONS, CARD CHECK passe ; « Do you have a card? » attend la carte (touche CardInsert, F7) ;
  sans carte, « Purchase a new card? » en donne une neuve (distributeur), le jeu y écrit le joueur (constructeur,
  voiture, couleur, boîte, nom) et l'imprime ; elle est enregistrée dans `[Cards] CARDFILE_01` (`Card_01.crd`), au
  format texte .RAW des émulateurs de cartes. Relue à la partie suivante : « Card data confirmed », AAAAA 00-01,
  Skyline GT-R V-Spec II, 60 parties. La carte d'exemple du dossier `Card Data` est refusée (éjectée) : faite pour
  une autre borne ou version. Le premier démarrage avec CARD R/W à ON attend la liaison entre bornes (~5 min,
  « DATA MERGE »), les suivants non. Pas encore vu : l'écriture et l'éjection de fin de course (la voiture du mode
  agent reste au point mort contre le mur).
- **Contrôles** (mode agent, SWITCH TEST) : pièces (2 pour 1 crédit), Test et Service (menus), volant (RIGHT OK),
  accélérateur (OK), frein, View Change (bouton 9), Intrude Change (bouton 10, Boost) ; le levier en H à six
  vitesses (quatre contacts) est mené par Gear Up et Gear Down (neutre au départ, de N à 6).
- **Partie** : démo (logos, film), « Do you have a card? », marque, voiture, couleur, boîte, mode Story, rival,
  course sur la Daikoku Line jusqu'à 119 km/h ; voitures à leurs couleurs.
- **Retour de force** : ressort qui suit la vitesse, amortisseur, couple constant en virage (journal de l'agent).
- **Son** : l'OpenAL de NVIDIA (pour la puce nForce) est lié dans le programme ; ses fonctions sautent à OpenAL Soft
  (`hardware/namco/n2Sound.c`) ; tous les sons du jeu sont en PCM. Capturé en WAV : silence des logos, musique de la
  démo, moteur et musique en course. Les mixeurs SSE d'OpenAL Soft sont coupés pour ce jeu : ils supposent une pile
  alignée sur 16 octets que leur thread n'a pas toujours (plantage une fois sur deux vers la 27e seconde).
- Paramètres : `tmp/data/testmode.lua`, `bookkeeping.lua`, `cardbackup.lua` écrits dans le dossier du jeu.
- **Options du jeu** (Tab) : « Jouer sans le lecteur de cartes », coché par défaut : le jeu se joue sans carte
  dès la première fois, sans passer par CARD R/W dans son menu de test (`[Game.mt3] NO_CARD_READER`). Décoché, c'est
  CARD R/W qui décide (à ON : les cartes ci-dessus).
- **Build MSVC** : la course se bloquait au chargement (thread de course sans fin dans
  `CSectionManager::GetSectInfo_InterArea` sur un odomètre NaN). Corrigé le 24/10 : le mode `"rt"` d'`fopen` que la
  bibliothèque C de MSVC refusait une fois le `b` ajouté (la cinématique d'avant-course n'avait pas sa table), et le
  point de départ fait de locales non initialisées d'un rival sans personnage (`NOTES.md`, demande 96).

- **C++ des programmes à dérouleur propre** : un programme qui lie son propre dérouleur d'exceptions (le runtime C++
  d'Intel, Gaelco Tuning) enregistre son `.eh_frame` par son propre `__register_frame_info`, appelé par la PLT ; le
  loader branchait ces relocations sur son pont (libgcc de MinGW) : la liste du jeu restait vide et toute exception
  finissait en abort. Comme ld.so, la définition du programme gagne maintenant pour ces quatre fonctions.
- **Volant des trois jeux Gaelco** : la carte Gaelco lit la gauche en valeur haute ; Ring Riders et Tokyo Cop
  tournaient à l'envers (et leur moteur poussait dans le sens du braquage). Vérifié dans les trois écrans de test des
  commandes, dans les menus (sélection à droite avec le volant à droite) et par le signe du couple.
- **`findStaticFnAddr` sous Windows** ne rend plus que les fonctions définies par le jeu (comme la lecture de
  `.symtab` sous Linux) ; il passait par la résolution générale et rendait le pont du loader pour un nom que le jeu
  importe : le remplacement posait alors un saut du pont vers lui-même. Sur MSVC (qui fusionne les fonctions
  identiques), les trois jeux Gaelco tournaient en rond à la création de leur fenêtre.
- **Fichiers pris pour des sockets** : `read`, `write` et `close` passaient à Winsock tout descripteur dont la valeur
  était aussi celle d'une socket ouverte ; Ring Riders a ainsi attendu dans `recv` en lisant ses modèles (selon les
  valeurs des handles, d'où un défaut intermittent). Un descripteur ouvert de la libc est maintenant toujours un
  fichier.
- **Build MSVC** : Ring Riders, Tokyo Cop et Gaelco Tuning démarrent aussi sur MSVC (démos ; Tuning jusqu'en course).
- **vJoy des jeux Gaelco (22/10, après le bilan)** : la boîte du pare-feu n'y était pour rien. Deux causes : les jeux
  Gaelco chargent 45 à 60 s en lancement normal (la pièce envoyée avant est perdue), et vJoy n'expose plus que 4 axes
  (X, Y, Z, Rz) : SDL voit l'accélérateur sur l'axe 3, le `controls.ini` de test l'attendait sur l'axe 5 (réglage d'un
  vJoy à 6 axes) ; corrigé dans `build-mingw/controls.ini`. Service se tient environ une seconde.

### Son des jeux Gaelco (22/10, après le bilan)
- La carte son Gaelco est un échantillonneur de 16 voies à 48 kHz. Au démarrage, le jeu lui envoie toute sa banque
  (`roms.new` : 14 à 22 Mo, une table d'en-têtes de 32 octets puis les échantillons), décrite par le `sound.h` livré
  avec Tuning : début, longueur, pas de phase à 48 kHz (2730 = 32 kHz, 3763 = 44,1 kHz), gain (0x4000 = 1), type
  (16 bits, 8 bits, IMA ADPCM 4 bits, quartet haut d'abord : le seul ordre où les deux voies d'une musique stéréo se
  ressemblent, 0,82 contre 0,11).
- Commandes (`add_comm`) : préparer une voie (échantillon, boucle), la lancer, fréquence (0x0FFF = hauteur
  d'origine), panoramique sur 4 sorties (0 gauche, 1 droite ; 2 et 3 non entendues), volume et vitesse de rampe,
  keyoff (fondu), stop ; commandes d'interface : volume général (le réglage du jeu au carré, 0,25 par défaut), balance,
  voies en cours, `sound_start`, tampon prêt.
- `gaelcoSound.c` mixe en stéréo sur le périphérique audio du loader : mis en pause par Tab, muet en mode agent caché.
  `LL_SOUND_WAV=<fichier>` enregistre ce que joue la carte (vérification sans écouter).
- Vérifié par captures (MinGW et MSVC) : Tuning, la musique de la démo à la hauteur et au niveau exacts, puis menus et
  course ; Ring Riders et Tokyo Cop, démo et partie. Pas encore écouté sur les haut-parleurs.

### Ring Riders (Gaelco PC, 2004)
- Image disque Linux complète : chemins absolus pris dans l'image (`/home/joc2001/...`, `/home/save0/...`).
- La carte d'interface Gaelco émulée (`hardware/gaelco/gaelcoIo.c`) : le jeu l'appelle par des appels système du
  noyau Gaelco (`iogaelco_*`, int 0x80 n° 229 à 236), remplacés par leurs symboles :
  - commande 8 : les registres des commandes (0 et 1 les contacts, actifs bas sauf le bouton d'urgence ; 4 à 8 les
    voies analogiques) ; commandes 6 et 7 : les compteurs des monnayeurs ; port 0x41 bit 1 : le bouton Test ;
  - commande 9 : les sorties (4 et 5 le moteur du guidon, 6 et 7 les vibreurs) → trames de la carte servo ;
  - Ring Riders : registre 0 bit 5 Start, bit 1 Service ; voie 4 le guidon, 5 la poignée (inversée), 6 le frein
    (plage fixe) ; le jeu calibre lui-même ses voies : le loader les balaie aux deux bouts au démarrage.
- Rendu correct sur la GeForce (register combiners NVIDIA) ; blanc sur l'Intel.
- Course jouée en mode agent (jusqu'à 138 km/h, dépassements, frein) ; retour de force : 1 146 trames de couple,
  26 vibrations ; menu TEST (Service), navigation (Start descend, gauche monte), mire.
- Guidon corrigé le 22/10 : l'écran Input/Output affichait 230 (+0,85) guidon à gauche, et le moteur poussait du côté
  du braquage ; inversé, la moto penche et tourne du bon côté et le couple ramène au centre (gauche −0,29, droite
  +0,60 en course).
- Manque : la plateforme de mouvement.

### Tokyo Cop (Gaelco PC, 2003)
- GCC 2.95 : ses méthodes `iolinux` font les appels système elles-mêmes, remplacées aussi ; son ancienne interface lit
  les commandes sur des ports (0x40, 0x41, 0x44 à 0x47) et écrit le couple du moteur sur les ports 0x44 et 0x45.
- Voies : 4 le volant, 5 l'accélérateur, 6 le frein (non inversés).
- Démo, menus (mode, agent, quartier, mission, garage), poursuite jusqu'à 113 km/h ; retour de force : 217 trames
  de couple.
- Menu de test (22/10) : `readDirectTest` (le port 0x41) n'est jamais appelée ; la boucle principale entre en test sur
  le bit 0x800, le code de touche 0x14 du port 0x42 (0x10 le crédit de service), comme Tuning. Le loader y renvoyait
  0xFF : la touche 0x1F du pavé numérique enfoncée en permanence. Menu : vue monte, sirène descend, klaxon entre ;
  Controls Test : volant 25 à gauche, 230 à droite, accélérateur 255, frein, sirène, vue justes.
- Son câblage (`test_io`) : port 0x40 bit 4 le frein (un contact : il ne freinait pas jusqu'ici ; le loader le lève
  au-delà du quart de la voie de frein), bit 5 le klaxon (confirme les menus : Start) ; port 0x41 bit 0 la vue, bit 1
  la sirène. En poursuite, freinage de 111 à 48 km/h, puis de 106 à 0 km/h.
- Volant corrigé comme Ring Riders (la sélection du garage partait à droite volant à gauche).
- Son : il n'envoyait aucune commande tant que la carte ne disait pas son tampon prêt (commande d'interface 11 chez
  lui, 13 chez les deux autres).
- Manque : le siège mobile.

### Gaelco Championship Tuning Race (Gaelco PC, 2005)
- Ajouts : `scandir`/`alphasort`, `XQueryKeymap`, la bibliothèque XF86VidMode liée en statique remplacée (modes par
  le pont X11, gamma en identité), socket Ethernet brute laissée en échec.
- L'abandon après 40 s n'était pas dans les données : la démo cherche « Retrovisor01 » dans chaque vue, aussi dans
  m_exhvp02a qui n'a que RETROVISOR02 ; `findMaterial` lance une exception que `iniEfecObjectSpotViewPort` attrape. Sous
  le loader, le dérouleur d'Intel lié au jeu ne trouvait aucune FDE (`_Unwind_Find_Fde` → abort) : voir « C++ des
  programmes à dérouleur propre » plus haut.
- Résolution native 720×540 (codée en dur dans le jeu) : l'entrée disait 640×480, l'image était coupée.
- Commandes (`Mandos::readControls`) : registre 0 bit 2 Start, bit 3 la vue, bits 4 et 5 les palettes ; registre 2 le
  code d'une touche (0x10 crédit de service, 0x14 menu de test) ; voies 4, 5 et 6 le volant, l'accélérateur et le
  frein, que le jeu calibre lui-même (balayées au démarrage). Parties en mode agent : pièce, menus (mode, voiture,
  boîte, circuit), course jusqu'à 215 km/h, boîte manuelle N-1-2-3 puis 3-2-1 aux palettes, changement de vue (le
  rétroviseur s'affiche).
- Retour de force : sorties 4 et 5 comme Ring Riders ; volant tenu à gauche, le couple pousse à droite (−0,39 en
  moyenne), et l'inverse.
- Menu Test : Adjust Screen, Controls, Sound, Motion System, Settings... ; Controls Test : Start, accélérateur 1,00,
  frein 1,00, volant −0,82 à gauche et +0,82 à droite. « Control de produccion » (numéro de série, adresse MAC) fait
  quitter le jeu au deuxième Start : écran d'usine, non creusé.
- vJoy : course menée par vJoy (pièce, Start, volant, accélérateur, frein, vue), 1 909 forces constantes reçues.
