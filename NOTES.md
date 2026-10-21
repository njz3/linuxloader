# Suivi des demandes et du développement

Branche `dev/ffbedition` (ex-`dev/ffbedition8`, renommée le 21/10), poussée sur `origin`. Les 3 jeux (OutRun 2 SP SDX, Hummer Extreme, Sega Race TV)
marchent avec retour de force ; commits signés `njz3 <benjamin.maurin@gmail.com>`, après accord.

Légende : ✅ fait et vérifié · 🔄 en cours · ⏳ à faire · ⚠️ fait, non vérifié

## Demandes

| # | Date | Demande | État |
|--:|:--|:--|:--|
| 1 | 19/10 | Compiler linuxloader avec MSVC | ✅ preset `msvc-x86`, vcpkg x86-windows-static |
| 2 | 19/10 | Format de jeu attendu pour OutRun 2 SP SDX | ✅ dossier du jeu extrait, ELF `Jennifer` (pas de CHD) |
| 3 | 19/10 | Copier OutRun, Hummer, Sega Race TV depuis racecab1 | ✅ `games/outrun2spdx`, `games/hummer`, `games/segaracetv` |
| 4 | 19/10 | Lancer OutRun, vérifier l'affichage, essayer les autres ELF | ✅ |
| 5 | 19/10 | Déboguer en local jusqu'à une partie avec vJoy et retour de force | ✅ OutRun, Hummer |
| 6 | 19/10 | Sega Race TV + network outputs (protocole de sm2-emu) | ✅ SRTV jouable, outputs TCP 8000 / UDP 8001 |
| 7 | 19/10 | Ne plus dépendre d'outputblaster.dll / FFBPlugin | ✅ fichiers mis à la Corbeille, FFB SDL natif |
| 8 | 19/10 | Résoudre le problème des shaders (SRTV) | ✅ programmes ARB NVIDIA traduits, voir journal |
| 9 | 19/10 | Expliquer le blocage « INITIALIZE STEP [4] » | ✅ puis corrigé (réseau, voir journal) |
| 10 | 19/10 | Pas de commit tant que les 3 jeux ne marchent pas avec FFB | règle en vigueur |
| 11 | 19/10 | Retour de force : traduire les effets selon le protocole RS422 (`DRIVEBOARD.md` de BackForceFeeder) | ✅ vérifié sur les 3 jeux avec vJoy |
| 12 | 19/10 | Interface graphique : reprendre celle de sm2-emu (picker de catalogue, paramètres F10) | ✅ picker + F10 (testés par captures automatiques ; à essayer au clavier) |
| 13 | 19/10 | Aspect ratio (original, 4:3, 16:9) et modes d'affichage (fenêtre pixels carrés, fenêtre étirée, plein écran, plein écran étiré) | ✅ `ASPECT_RATIO`, `DISPLAY_MODE`, aussi dans F10 (appliqué en direct) |
| 14 | 19/10 | Tenir ce fichier de suivi | 🔄 |
| 15 | 19/10 | Renommer le fichier de suivi en notes.md | ✅ `NOTES.md` |
| 16 | 19/10 | Commit et push | ✅ `09bc3fd` sur `origin/dev/ffbedition8` (réécrit, voir 18) |
| 17 | 19/10 | Page de réglage des contrôles | ✅ onglet Contrôles de F10 (testé : capture d'un axe vJoy, sauvegarde, rechargement en jeu) |
| 18 | 19/10 | Signer les commits `njz3 <benjamin.maurin@gmail.com>`, pas l'email pro | ✅ config locale du dépôt ; `f118420` réécrit en `09bc3fd`, poussé en force |
| 19 | 19/10 | Build et zip comme une release officielle | ✅ `emulateurs/releases/linuxloader-v3.0.12-ffbedition8-20261019-njz3-win32.zip` |
| 20 | 19/10 | Comparer le FFB avec celui d'un autre fork | ✅ VooDooUdo2 (repris d'Arkhist) : pages 9D/9E/FB à reprendre, et leur sens est le bon ; hyp36rmax : recherche sur OutRun SDX, rien à fusionner |
| 21 | 19/10 | Reprendre la lecture des pages 9D/9E/FB, option d'inversion du sens dans F10 | ✅ testé sur OutRun avec vJoy (pages jouées à 16 ms par pas) |
| 22 | 19/10 | Thread séparé pour le FFB ? (un SideWinder ralentissait flycast) | ✅ thread FFB unique, le dernier état gagne ; OutRun et SRTV testés avec vJoy, Hummer à refaire avec `-c hummer.ini` |
| 23 | 19/10 | F10 : mettre le jeu en pause et afficher la fenêtre devant (invisible pendant le jeu), couper aussi le son | ✅ vérifié par capture en pleine course sur OutRun et SRTV ; arrêt du son à confirmer à l'oreille |
| 24 | 19/10 | Hummer : retour de force sans `hummer.ini` | ✅ carte servo émulée en `AUTO` pour les 4 Hummer ; Extreme vérifié avec vJoy |
| 25 | 19/10 | Hummer : changer la résolution ou remplir la fenêtre | ✅ patch de résolution appliqué à toutes les tailles (demande 29) : l'image remplit la fenêtre de 640×480 à 1920×1080 |
| 26 | 19/10 | Page de contrôles organisée comme celle de Flycast | ✅ testée dans OutRun avec vJoy (capture, jauges, réinitialisation) ; à essayer à la main |
| 27 | 20/10 | Copier R-Tuned et Initial D 4 depuis racecab1, les lancer en local | ✅ les deux affichent leur mode démo ; R-Tuned après 12 fonctions de la libc ajoutées ; Initial D 4 : les taches noires sont des feuilles mortes qui volent, un effet voulu (confirmé par l'utilisateur) |
| 28 | 20/10 | Essayer tous les jeux en différents rapports d'aspect (surtout étiré) et résolutions (640×480 à 1920×1080) | ✅ 5 jeux × 6 résolutions en fenêtre étirée, aspects, plein écran (étiré et 4:3), 3840×2160 ; 2 défauts trouvés et corrigés (OutRun 640×480, Hummer ≤ 1280×720) |
| 29 | 20/10 | Faire aussi les trois tâches proposées : shaders d'Initial D 4, rendu hors écran, alignement de pile | ✅ rendu hors écran et alignement de pile en place ; Initial D 4 : programmes rejetés corrigés, les taches noires sont des feuilles mortes voulues (effet d'automne) |
| 30 | 20/10 | Nouvelle compilation release officielle | ✅ `emulateurs/releases/linuxloader-v3.0.12-ffbedition8-20261020-njz3-win32.zip` (commit `9b84019`) ; 5 jeux lancés depuis le zip |
| 31 | 20/10 | F9 ne quitte pas le picker | ✅ après un jeu, la console du picker gardait le clavier, et le jeu lancé avec Entrée se relançait ; le picker reprend le clavier et relâche les touches. Testé : F9 le ferme après un jeu |
| 32 | 20/10 | Page Contrôles : impossible d'associer des valeurs | ✅ sans `controls.ini` (dossier neuf, picker), la page n'avait aucune ligne ; le fichier est créé avec les valeurs par défaut à l'ouverture. Testé dans un dossier vierge (touche B liée et enregistrée) |
| 33 | 20/10 | Fenêtre étirée / plein écran étiré : impossible de descendre à 512×384 ou 640×480 | ✅ taille minimale de la fenêtre abaissée à 320×240 sous Windows (rendu hors écran) ; OutRun 1024×768 réduit à 400×300, image entière |
| 34 | 20/10 | Taches noires d'Initial D 4 | ✅ normales : des feuilles mortes qui volent (effet d'automne) |
| 35 | 20/10 | OutRun : « DRIVE BOARD FAULT DETECTED (2) OVERCURRENT ALARM » en démo | ✅ la carte servo émulée renvoyait parfois un octet jamais écrit ; elle renvoie maintenant sa dernière réponse. Vu 43 valeurs parasites en 2 min avant la correction |
| 36 | 20/10 | Le bouton Test n'ouvre pas le menu de service ; les autres jeux en ont-ils un ? | ✅ comme segaboot sur une borne : le jeu qui quitte après Test lance son mode test (`JenTest` pour OutRun, `-t` pour les autres), et quitter le menu relance le jeu. Menus vus : OutRun, SRTV, Hummer, Initial D 4 ; R-Tuned : deux fonctions GL d'arrêt manquaient, corrigé |
| 37 | 20/10 | Charger Dead Heat | 🔄 jeu Namco ES1, pas Lindbergh : SDL 1.2, dongle HASP, ~20 fonctions système à ajouter ; étude ci-dessous ; en attente : les menus Lindbergh d'abord, puis l'inventaire des jeux Linux de racecab1 |
| 38 | 21/10 | Boutons vJoy pour tester sans clavier (Coin, Test, Service, Start, vue 1, vue 2...) | ✅ `controls.ini` de `build-mingw` et `build-msvc/Release`, clavier gardé |
| 39 | 21/10 | Mode agent : rendu capturable, télémétrie, image par image, comme sm2-emu et Supermodel | ✅ `--agent` / `--agent-show`, protocole de sm2-emu ; client `scripts/agent/llagent.py` |
| 40 | 21/10 | Finir les menus de service, essayer le mode agent dans tous les jeux avec captures | ✅ menus : sortie du test et job du picker vérifiés ; agent : les 5 jeux, du démarrage au choix de la voiture |
| 41 | 21/10 | Inventaire des jeux Linux de racecab1 | ✅ 18 jeux Linux : 7 Lindbergh, 3 Namco ES1 (Dead Heat, Maximum Heat 3D, Nirin), 3 Namco N2 (Dead Heat Riders, Maximum Tune 3 et 3DX+), Cruis'n Blast, 3 Gaelco (Championship Tuning Race, Ring Riders, Tokyo Cop) ; les autres sont des jeux Windows |
| 42 | 21/10 | Support de Dead Heat | ✅ jouable : course complète en mode agent, menus, menu de test (voir 47) ; son à écouter |
| 43 | 21/10 | Un « windowsloader » pour Sega Rally 3 ou Daytona Championship USA : dans ce projet ? | ✅ réponse : un dépôt à part, qui reprend le code commun (carte servo, JVS, FFB, interface) ; il ne tournerait que sous Windows, ou sous Linux par Wine |
| 44 | 21/10 | Aspect « 4:3 avec bandes noires » : une image 16:9 entière sur un écran 4:3 | ✅ `ASPECT_RATIO = 4:3_LETTERBOX`, aussi dans F10 ; vérifié en mode agent (ID4 en fenêtre et fenêtre étirée, OutRun 1920×1080) |
| 45 | 21/10 | Paramètre « Résolution interne » : taille de calcul et de rendu du jeu, avant l'aspect et le mode | ✅ liste dans F10 (Native, 640×480 à 3840×2160), écrite en `WIDTH`/`HEIGHT` ; OutRun calculé en 1920×1080 |
| 46 | 21/10 | Info-bulle de la résolution interne : les jeux pris en charge | ⚠️ liste et limites tirées de `patchResolution.c` et des essais de la demande 28 ; compilée, pas encore vue à l'écran |
| 47 | 21/10 | Dead Heat : commandes, son, une partie, appels su/sudo, plantage MSVC | ✅ carte d'E/S N2 et carte volant émulées (volant, pédales analogiques, monnayeur, Test, retour de force) ; course jouée, menu de test ; plantage MSVC trouvé et corrigé (mémoire non mise à zéro) ; commandes Linux plus lancées sous Windows ; son non écouté |
| 48 | 21/10 | Nom de cette session | ✅ « MSVC compilation » |
| 49 | 21/10 | Code commun aux deux chargeurs, chargeur Windows | ✅ décision : pas de sous-module commun (il compliquerait les fusions avec upstream) ; `windowsloader` (https://github.com/njz3/windowsloader.git) reste indépendant de linuxloader, projet CMake multiplateforme comme lui ; pas encore commencé |
| 50 | 21/10 | Renommer `dev/ffbedition8` en `dev/ffbedition`, en local et sur origin | ✅ aucune PR ouverte sur l'ancienne branche ; `dev/ffbedition` suit `origin/dev/ffbedition` |
| 51 | 21/10 | Réorganiser l'historique en commits par fonction, pour des PR vers upstream (FFB, interface...) | ✅ 9 commits par fonction + NOTES.md, poussés en force sur `dev/ffbedition` ; arbre final identique ; chaque étape compile (MinGW et MSVC) ; ancien historique dans la branche locale `backup/ffbedition-84d5d1e` ; Linux non essayé |

## État des jeux (build MSVC, GPU Intel, vJoy)

| Jeu | Démarrage | Partie | Retour de force | Remarques |
|:--|:--|:--|:--|:--|
| OutRun 2 SP SDX | ✅ | ✅ | ✅ RS422 | ressort 8B, amortisseur 86, bosses FB ; couple 84 au centrage seulement |
| Hummer Extreme | ✅ | ✅ | ✅ RS422 | amortisseur 86, vibrations 85 (jusqu'à 50 Hz) |
| Sega Race TV | ✅ | ✅ partie complète | ✅ RS422 | couple 84 jusqu'à ±26 % en course, coupé en attract (80 00 00) |

## Journal de développement

### Historique par fonction (demande 51)
- `dev/ffbedition` a été réécrite le 21/10 : les 10 commits de travail sont devenus une suite de commits par
  fonction, dans l'ordre où ils s'appuient les uns sur les autres (une PR possible par commit, empilées) :
  1. `d85bf98` feat(windows) : build MSVC et correctifs des ponts Windows ;
  2. `bd89ff5` feat(display) : rapport d'aspect, modes d'affichage, rendu hors écran, résolution interne ;
  3. `ad6c9e5` fix(games) : Sega Race TV, Initial D 4, R-Tuned, Hummer, OutRun ;
  4. `9a1e653` feat(ffb) : retour de force de la carte servo Sega ;
  5. `cd06bbd` feat(outputs) : sorties réseau façon MAME ;
  6. `6bd4346` feat(gui) : picker, paramètres F10, page de contrôles façon Flycast ;
  7. `3540d19` feat(service) : menus de test par le relais de segaboot ;
  8. `509d65b` feat(agent) : mode agent ;
  9. `36d0fe7` feat(deadheat) : Dead Heat ;
  puis `b9cdff5` docs : ce fichier, à ne pas proposer à upstream.
- Seuls 1, 2 et 4 s'appliquent seuls sur le `main` d'upstream (sans compilation vérifiée dans cet état) ; les autres
  demandent les précédents.
- Les hashes cités plus haut et plus bas (`09bc3fd`, `77d8a79`, `e0bfd80`, `ae3fe66`, `9b84019`, `29259be`,
  `5414af7`, `9dc766d`, `06b9a1b`, `84d5d1e`) sont ceux de l'ancien historique, gardé dans la branche locale
  `backup/ffbedition-84d5d1e` (non poussée). La release du 20/10 correspond à `9b84019` de cet historique.
- Méthode : chaque bloc du diff `main..84d5d1e` étiqueté par fonction (découpe à la ligne quand un bloc en mêle
  plusieurs, substitution pour trois lignes mixtes), l'arbre de chaque étape reconstruit et compilé avec MinGW et
  MSVC avant son commit ; l'étape finale comparée à l'ancien arbre (identique) et l'étape 0 à `main`.


### Build MSVC (demande 1)
- `CMakePresets.json`, `vcpkg.json`, branche `if(MSVC)` dans `CMakeLists.txt`, en-têtes de compatibilité
  POSIX dans `src/msvc/`.
- ABI Linux i386 : `time_t` 32 bits (`_USE_32BIT_TIME_T`), pile alignée sur 16 octets à l'entrée des threads,
  gestionnaire de paramètre invalide de l'UCRT neutralisé.

### Retour de force et outputs (demandes 5 à 7)
- `forceFeedback.c` : mode volant SDL3 haptic (constante, ressort, friction, sinus) piloté par les commandes
  de la carte de commande du volant.
- `netOutputs.c` : outputs réseau au format MAME (TCP 8000, annonce UDP 8001), lampes et RawDrive.

### Sega Race TV
- **Shaders** : les 52 programmes ARB précompilés (cgc fp40/vp40) exigent `NV_fragment_program2` /
  `NV_vertex_program3`, refusés par le pilote Intel. Nouveau traducteur `src/loader/graphics/nvProgramToArb.c` :
  suffixes R/H/X, codes condition (CMP en fragment, masque arithmétique en vertex), `_SAT` en vertex,
  `|x|`, registre d'adresse vectoriel et grands offsets. Branché dans `gl_ProgramStringARB` pour SRTV hors
  NVIDIA. GLSL : octets NUL des shaders patchés retirés.
- **Faux « écran blanc »** : les captures PNG gardaient l'alpha du framebuffer (≈ 0). Captures désormais en RGB.
- **4 wrappers GL** déclarés `void` alors que la fonction renvoie une valeur (`glCheckFramebufferStatusEXT`,
  `glGetUniformLocationARB`, `glCreateProgramObjectARB`, `glIsProgramNV`) : corrigés dans `glHooks.cpp`.
- **Blocage étape 4** : la scène suivante (`CConnectSceneObj`, réseau) restait bloquée dans `recvfrom`.
  - `ioctl(FIONBIO)` (0x5421) n'était pas traduit sous Windows : socket restée bloquante. Corrigé dans
    `filesystemShared.c` (`windowsSocketIoctl`, aussi `FIONREAD`).
  - `connect` non bloquant : Winsock renvoie `WSAEWOULDBLOCK`, le jeu attend `EINPROGRESS` (115). Et la table
    errno utilisait les valeurs de la CRT Windows (EINPROGRESS = 112). Corrigé dans `networkBridge.cpp` :
    numéros Linux i386.
  - Résultat : `Network Initialize / Stand Alone / Initialized [ Success ]`, mode attract, partie complète.

### Retour de force RS422 (demande 11)
- Référence : `dev_soft/bff-vjoy/BackForceFeeder_master/DRIVEBOARD.md` (section *Sega servoboard*).
- Trames de 4 octets : opcode, 2 valeurs sur 7 bits, checksum `(b1^b2^b3) & 0x7F` (OutRun envoie `7D` partout).
- `forceFeedback.c`, mode volant : la carte est émulée comme un état (`ServoBoard`) que chaque trame met à jour :
  - `80` mode court/permanent et activation (SRTV coupe les effets hors partie) ; en mode court, le couple est
    coupé 240 ms après la dernière trame (d'abord `sdlFfbTick` dans `pollEvents`, puis le thread FFB, demande 22) ;
  - `81` plafond (1er octet, les deux sens), `83` gain : sortie = valeur × `83`/127, plafonnée par `81`,
    rapportée à l'échelle 0..127 de la carte, puis × `FFB_STRENGTH` (nouveau réglage `[ForceFeedback]`, 100 %) ;
  - `84` couple centré `((b1<<7)|b2) − 0x80`, + = gauche → effet constant (sens corrigé, demande 21) ;
  - `85` sinus, fréquence en demi-hertz, amplitude → effet sinus continu ; `FB 00 pp` (pages, OutRun) → bosses,
    remplacées par la lecture des pages (demande 21) ;
  - `86` amortisseur (rapport des deux paramètres) → effet `DAMPER` SDL (friction si absent) ;
  - `87` centre, `88` activation, `8B` force → effet ressort SDL, saturé par le plafond ;
  - `FF` reset aux valeurs mesurées à froid (`81 40`, `83 40`, effets coupés jusqu'à `80`).
- Trames capturées (`drive.txt`) : OutRun `81 30 7F`, `83 40`, `86 2F 02`, `8B 20..50`, `FB` ; Hummer `86 2D 1C`,
  `85`, `87 01 00`, `F1 0B 11` (inconnu) ; SRTV `83 53`, `84` en continu, `80 00 00/01`.
- Vérifié avec vJoy (journal des paquets FFB) sur une partie de chaque jeu.

### Aspect ratio et modes d'affichage (demande 13)
- `[Display]` : `ASPECT_RATIO = ORIGINAL | 4:3 | 16:9` (ORIGINAL = rapport de la résolution de rendu du jeu) et
  `DISPLAY_MODE = WINDOW | WINDOW_STRETCH | FULLSCREEN | FULLSCREEN_STRETCH`. Sans `DISPLAY_MODE`, `FULLSCREEN`
  garde son sens.
- `blitStretching.c` : l'image est cadrée à l'aspect choisi (bandes noires), ou étirée dans les modes STRETCH.
  En WINDOW, la fenêtre prend la forme de l'aspect (élargie ou rehaussée, jamais plus petite que le rendu du
  jeu, qui dessine dans le framebuffer de la fenêtre) ; idem en sortant du plein écran (F11, Alt+Entrée).
- Vérifié sur SRTV : 16:9 fenêtre → 853×480 rempli ; plein écran étiré → 3072×1920 rempli.

### Interface : picker et paramètres F10 (demande 12)
- Dear ImGui 1.92.0 (MIT) copié de sm2-emu dans `src/imgui` : cœur, backend SDL3, backend OpenGL2 compilé sur
  glad (`src/loader/gui/imguiOpenGL2Glad.cpp`). Build Windows seulement (MSVC et MinGW).
- `src/loader/gui/` : `guiCommon.cpp` (style, échelle avec la fenêtre, police Segoe UI, traductions FR/EN,
  fenêtre de paramètres, sauvegarde ini qui garde les commentaires), `picker.cpp`, `guiOverlay.cpp`.
- **Picker** : `linuxloader.exe` sans argument. Il scanne `GAMES_DIR` (sinon `games` à côté du programme, ou un
  ou deux niveaux au-dessus), reconnaît les ELF connus et les identifie par CRC (`getGameData`). Tuiles,
  panneau de description (titre, année, DVP, état, résolution, dossier, programme, config). Entrée, double-clic
  ou A pour lancer, F10 paramètres, F9 quitter. Le jeu tourne dans un processus fils (`-c` ini du jeu s'il existe :
  `linuxloader.ini` du dossier du jeu, `<titre court>.ini` ou `<dossier>.ini` à côté du programme), le picker
  revient à la fin. Jaquette : `snap|title|art|cover|<dossier>.png/.jpg` dans le dossier du jeu, si présente.
- **F10 en jeu** : surimpression dessinée après le blit final ; l'état GL du jeu (programmes, buffers, unité de
  texture) est sauvegardé et restauré. Onglets Affichage (aspect et mode en direct), Retour de force
  (`FFB_STRENGTH` en direct), Sorties et réseau, Catalogue (`GAMES_DIR`, `LANGUAGE`), À propos. Les réglages qui
  demandent un redémarrage le disent. Sauvegarder écrit dans le `.ini` du jeu en cours.
- Licence : sm2-emu est GPL-3 avec des conditions section 7 (attribution, « basé sur sm2-emu » + lien dans le
  source et l'écran À propos) : en-têtes des fichiers `gui/` et onglet À propos.
- Vérifié par captures : picker (3 jeux, panneau, paramètres), lancement de SRTV depuis le picker, F10 en jeu,
  changement d'aspect en direct (640×480 → 853×480), OutRun et Hummer en course avec l'interface compilée.
  Non essayé : navigation au clavier, à la souris et à la manette (session verrouillée).

### Page de contrôles (demande 17)
- Onglet « Contrôles » de la fenêtre de paramètres (F10 en jeu et dans le picker), `src/loader/gui/guiControls.cpp`.
- Liste les manettes (JOYn / GCn comme les nomment les liaisons ; en jeu, n = le joueur), puis les actions d'une
  section de `controls.ini` (celle du type du jeu en cours par défaut : Conduite, Boutons, Vol, Tir, Mahjong, ou
  « Tous les jeux » = [Common]), avec leurs entrées.
- Par action : « + » ajoute une entrée, « Définir » la remplace, « x » efface. La capture prend la prochaine touche,
  bouton de manette ou de joystick, chapeau, ou axe déplacé de plus de la moitié de sa course depuis sa position de
  départ (une pédale au repos en bout de course compte) ; un axe sur une action tout ou rien reçoit `_POSITIVE` ou
  `_NEGATIVE`. La souris n'est capturée que pour les jeux de tir. Échap annule.
- Actions analogiques : barre en direct (centrée pour le volant et les manches, jauge pour les pédales, reprise de
  la `centre_bar` de sm2-emu) et zone morte `[Config] <Action>_DeadZone`.
- Sauvegarder écrit les liaisons modifiées dans `controls.ini` (commentaires gardés) ; en jeu, `sdlReloadBindings`
  vide les tables de liaisons et relit le fichier : les changements s'appliquent sans relancer.
- Testé dans SRTV avec vJoy : capture de `P1_Steer` → `JOY0_AXIS_0`, sauvegarde et rechargement ; rendu des barres
  et des zones mortes. Non essayé à la main (clavier, souris).

### Release Windows (demande 19)
- Comme `scripts/windows/windows-build.sh` : build MinGW Release (MSYS2 mingw32, statique), version lue dans
  `docs/changelog` (3.0.12) et écrite dans `version.h` le temps du build (fichier remis ensuite), zip avec
  `linuxloader.exe` et `ll-deps/` à la racine.
- Écart : `ll-deps/libwinpthread-1.dll` en plus, la `libgcc_s_dw2-1.dll` de MSYS2 en dépend (celle de la CI, Ubuntu
  « win32 threads », non) ; le loader ajoute `ll-deps` aux dossiers de recherche des DLL.
- Contient l'état de travail du 19/10 au soir : commit `09bc3fd` + page de contrôles + correction ci-dessous (non
  commités).
- **Correction trouvée en testant la release** : sans `controls.ini`, les combinaisons par défaut (ExitGame
  `JOY0_BUTTON_11 + JOY0_BUTTON_10`, `Start + Back` sur manette) n'avaient pas de groupe : relâcher un de ces boutons
  appuyait sur Test, et le jeu sortait vers le menu système (`exit(0)`). `addDefaultBindings` (sdlInput.c) leur crée
  leur groupe. Défaut présent aussi dans la release officielle.
- Testé depuis le zip extrait dans un dossier vierge (sans ini) : `--version` 3.0.12, picker, OutRun, Hummer, SRTV ;
  partie complète de SRTV avec vJoy et retour de force.

### Release Windows du 20/10 (demande 30)
- Même procédure que la demande 19 : build MinGW Release recompilé de zéro (`build-mingw`, Ninja), version 3.0.12
  lue dans `docs/changelog` et écrite dans `version.h` le temps du build, zip avec `linuxloader.exe` et `ll-deps/`
  (dont `libgcc_s_dw2-1.dll` et `libwinpthread-1.dll` de MSYS2). Contenu : commit `9b84019` exactement, rien de local.
- `emulateurs/releases/linuxloader-v3.0.12-ffbedition8-20261020-njz3-win32.zip`, 17,8 Mo, SHA-256
  `69cdaa415acd7b63de44db932fa0543191e6c66028a090a52aa6f70570cc40fe`.
- Les changements du jour compilent sous MinGW sans nouvel avertissement (les thunks `elfCall` sont réservés à
  MSVC, GCC garde `force_align_arg_pointer`).
- Testé depuis le zip extrait dans un dossier vierge, sans ini : `--version` 3.0.12, picker, puis OutRun, Hummer,
  SRTV, R-Tuned et Initial D 4 en démo, entre 54 et 60 i/s ; partie de SRTV avec vJoy : 2 383 commandes de couple,
  jusqu'à ±24 % en course, ressort et amortisseur créés.

### Picker, contrôles et taille de fenêtre (demandes 31 à 33)
- **F9 (31)** : reproduit sur ce PC avec la release du 20/10. Le picker cache sa fenêtre pendant le jeu ; à la
  fermeture du jeu, Windows donne le clavier à la fenêtre suivante, la console du picker, et le refuse ensuite au
  picker réaffiché : F9 partait dans la console. De plus, Entrée (ou A sur manette) relâchée dans la fenêtre du jeu
  restait enfoncée pour ImGui, et sa répétition relançait le jeu dès le retour (ce qui recachait le picker).
  `launchGame` reprend le clavier (attaché à l'entrée de la fenêtre au premier plan, jusqu'à le garder 300 ms) et
  vide l'état des touches d'ImGui (`ClearInputKeys`). Le réglage SDL `SDL_HINT_FORCE_RAISEWINDOW` seul ne suffisait
  pas : la console reprend le clavier après coup. Testé : jeu lancé avec Entrée, fermé, le picker a le clavier, pas
  de relance, F9 le ferme.
- **Liaisons (32)** : le jeu prend ses liaisons intégrées quand `controls.ini` manque et n'écrit pas le fichier ; la
  page Contrôles lit ses lignes dans ce fichier, donc sans lui elle était vide (« Cette section de controls.ini est
  vide »). C'était le cas dans un dossier neuf (zip extrait, `build-mingw`). Le fichier est maintenant créé avec les
  valeurs par défaut à l'ouverture de l'onglet. Picker : `controls.ini` à côté de l'exe, celui que lisent ensuite les
  jeux lancés depuis le picker ; en jeu : celui du dossier du jeu, comme avant.
- **Taille de fenêtre (33)** : `SDL_SetWindowMinimumSize` empêchait de réduire la fenêtre sous la résolution du jeu.
  Sous Windows, le minimum est 320×240 : une fenêtre plus petite que le rendu passe en rendu hors écran et l'image
  est réduite. Linux garde l'ancien minimum (pas de rendu hors écran). Testé de l'extérieur (`SetWindowPos`) sur
  OutRun 1024×768 étiré : l'ancienne version reste à 1024×768, la nouvelle descend à 400×300 puis s'arrête à 320×240,
  avec l'image entière.
- **Picker MSVC** (trouvé en testant) : il plantait au démarrage (0xC0000409) depuis l'ajout d'Initial D 4 dans
  `games` : trois fichiers de `data/` ont des noms abîmés (U+FFFD), et `path::string()` de MSVC lève une exception sur
  un nom que la page de code ANSI ne sait pas écrire. Les noms sont comparés en chemins, et chaque dossier de jeu est
  protégé (`scanGame` dans un `try`). La version MinGW n'était pas touchée.
- Pendant les tests, Windows a demandé l'accès réseau pour `build-msvc\Release\linuxloader.exe` (pare-feu) :
  laissé à l'utilisateur.

### Carte servo : alarme parasite d'OutRun (demande 35)
- Le jeu (`CabinetCtrlMIDI_Input`, `PrintAlarmMessage`) lit un octet d'état par carte servo (deux sur ce modèle) ;
  un octet n'est valable que si ses deux quartets sont égaux, et 0x11, 0x22, 0x33 sont les alarmes codeur,
  surintensité, surchauffe de la carte n (« (2) » = deuxième carte).
- `sharedSelect` répond « prêt » pour tous les descripteurs quand la carte est émulée, et `hardcomRecChk` lit alors
  `/dev/tts/0` dans un tampon de pile. `driveboardRead` renvoyait 1 octet sans l'écrire quand aucune réponse
  n'attendait : le jeu lisait ce qui traînait sur sa pile. Trace temporaire : 43 valeurs différentes en 2 minutes
  de démo (0xFF, 0xC0, 0x44, 0x20...). Correction : chaque lecture renvoie la dernière réponse.

### Bouton Test et menus de service (demande 36)
- Sur une borne, le menu de test n'est pas dans le jeu : quand Test est pressé, le jeu crée `/tmp/segaboot/test`
  (`touch`) et quitte ; le programme de démarrage (segaboot) voit le fichier et lance le programme de test.
  Vérifié avec un bouton vJoy en démo : OutRun, SRTV et Hummer quittent 1 à 4 s après Test, Initial D 4 affiche
  « TEST LOADING » et quitte après 7 s (code 0 pour tous) ; le fichier `tmp/segaboot/test` apparaît dans leur
  dossier (le loader y redirige `/tmp/segaboot`).
- Le loader le fait maintenant (Windows) : quand le programme du jeu se termine de lui-même (`exit` ou retour de
  `main`, pas ExitGame ni la fermeture de la fenêtre), `segabootOnExit` (mainW.cpp) regarde si le jeu a créé
  `tmp/segaboot/test` (ou `testmode`) pendant la partie, ou si Test a été pressé dans les 15 dernières secondes
  (`jvs.c`). Si oui, il efface le fichier et lance le loader avec `-t` dans un nouveau processus, depuis le dossier
  de départ. Quand le programme de test se termine (EXIT du menu), le jeu est relancé. Le picker attend tous ces
  processus (job Windows) et ne réapparaît qu'à la fin.
- Testé : OutRun → « OUTRUN2SP SDX GAME TEST MENU » (`JenTest`), EXIT → le jeu redémarre ; SRTV → « GAME TEST
  MODE » ; Hummer → « GAME TEST MENU » ; Initial D 4 → programme de test « InitialD Next » (avec la fenêtre de
  15 s ; 3 s ne suffisaient pas).
- Sortie du menu de test vérifiée aussi pour SRTV, Hummer et Initial D 4 (Test sur EXIT, curseur par défaut) : le
  programme de test se ferme et le jeu redémarre.
- Le picker : son attente reproduite à l'identique (OutRun lancé suspendu dans un job, Test par vJoy). À la fermeture
  du jeu (41 s), le job compte encore 1 processus, le menu de test : le picker reste caché jusqu'à la fin de la
  chaîne. Le lancement par le picker lui-même n'est pas essayé (session verrouillée, pas de clavier).
- `JenTest` et `Jennifer` de ce dump ne sont pas de la même version : le jeu enregistre sa comptabilité sous `SBJJ`,
  `JenTest` la lit sous `SBMB`, la trouve invalide (« BACKUP_DATA_ERROR_FAIL ») et remet la comptabilité et les
  classements à zéro. Les réglages (EEPROM, « Game Assignment ») sont gardés. Défaut du dump, pas du loader.
- R-Tuned (build MinGW) : sur Test, il se figeait sans créer le fichier. Une boîte « Lindbergh Loader Error » était
  ouverte derrière : en quittant, il libère ses buffers OpenGL avec `glIsBuffer` et `glGetBufferParameteriv`, que
  `glHooks` ne fournissait pas (ils ne servent qu'à l'arrêt, d'où le défaut seulement avec Test). Les deux sont
  ajoutés ; R-Tuned quitte, `dsr -t` démarre et tourne à 60 i/s. Son menu n'est pas vérifié à l'œil : la capture
  d'écran Windows voit sa fenêtre noire, même en démo. Seuls ces deux imports `gl*` manquaient, sur les cinq jeux
  et `JenTest` (les `glX*` passent par graphicsBridge).
- R-Tuned (build MSVC) : plante au démarrage (0xC0000005 après 6 s) ou dans la démo, de façon irrégulière (pas sous
  cdb) ; la version MinGW tourne 3 minutes sans souci. À étudier à part.
- Tests sans clavier : boutons vJoy dans l'ordre de l'utilisateur (bouton n = `JOY0_BUTTON_(n-1)`) : 1 Coin, 2 Test,
  3 Service, 4 Start, 5 vue 1 (ViewChange), 6 vue 2 (MusicChange), 7 Boost, 8 BoostRight, 9 GearUp, 10 GearDown,
  11 carte. Appliqué aux `controls.ini` de `build-mingw` et `build-msvc/Release`, touches clavier gardées (Échap
  pour quitter) ; la combinaison vJoy de sortie (boutons 11+12) est retirée.

### Mode agent (demandes 39 et 40)
- `linuxloader --agent JEU` (fenêtre cachée, pas de cadence imposée, aucun périphérique, son muet) ou
  `--agent-show JEU` (fenêtre, son, cadence du jeu) : le jeu est piloté par un autre programme sur stdin/stdout, une
  commande par ligne, avec le protocole des modes agent de sm2-emu et de Supermodel (`src/loader/agent/agent.c` le
  décrit en tête). La sortie du jeu et du loader passe sur stderr.
- Image par image : le jeu est retenu dans `blitStretch`, après la copie de son image dans le FBO du loader, jusqu'à
  la commande suivante (`step N`) ; `ready GAME W H` à la première image.
- Commandes : `step`, `set`/`setraw`/`release`/`get`/`inputs` (les noms de controls.ini, `Steering` et `Accelerator`
  comme les autres modes agent, appliqués par le même chemin que les manettes, donc avec les adaptations par jeu),
  `obs W H` et `shot PATH` (l'image lue dans le FBO : marche fenêtre cachée ou session verrouillée, et R-Tuned que la
  capture Windows voit noir), `ffb` (les effets du modèle de la carte servo, à chaque changement), `servo` (les
  trames écrites à la carte), `tel` (JSON : image, i/s, jeu, interrupteurs, voies analogiques, pièces, retour de
  force), `mem ADRESSE LONGUEUR` (la mémoire du jeu, son programme est à ses propres adresses), `perf`, `frame`,
  `quit`. `save`/`load` répondent `err` : on ne fige pas un programme natif.
- En mode caché : le rendu hors écran est forcé, la fenêtre n'est jamais montrée, l'intervalle de swap est mis à 0,
  le limiteur coupé, FAudio et l'audio SDL à volume 0 (le son tourne, muet), le volant n'est pas ouvert (le modèle
  de la carte servo est tenu pour le journal). Le relais segaboot est coupé (l'agent relance lui-même).
- Limites : les jeux se cadencent eux-mêmes (60 i/s même sans limiteur, Initial D 4 37 i/s) ; leurs threads (son,
  réseau) tournent pendant que l'agent réfléchit, donc pas de déterminisme strict ; pas d'horloge virtuelle pour
  l'instant ; télémétrie générique (pas encore de vitesse ni de position par jeu).
- Client : `scripts/agent/llagent.py` (`LLAgent`, une méthode par commande) ; `scripts/agent/tour.py` fait le tour
  d'un jeu : images au démarrage et en démo, une pièce et Start, images après, télémétrie et retour de force.
- Essai sur les 5 jeux (MinGW, caché) : prêts en 5 à 11 s ; OutRun jusqu'au choix du nombre de pilotes, SRTV jusqu'à
  « Select car », Hummer jusqu'à « Choose a car », R-Tuned jusqu'à « Select your car », Initial D 4 en démo (il faut
  2 crédits). Pièce comptée partout ; journal de retour de force : OutRun 603 changements, SRTV 4, R-Tuned 8,
  Hummer et Initial D 4 aucun sur ces séquences. Version MSVC : même essai court sur OutRun, identique.
- `--agent-show` essayé sur SRTV : la fenêtre continue pendant que l'agent attend.

### controls.ini du dossier du jeu (trouvé avec le mode agent)
- `saveGuidsToIni` écrivait les GUID des manettes dans `controls.ini` du dossier courant (celui du jeu), même quand
  les liaisons venaient d'un autre fichier ou des valeurs intégrées : chaque jeu avait un `controls.ini` avec la seule
  section `[ControllerGUIDs]`. La page Contrôles de F10 lit ce fichier en jeu : aucune liaison à associer. C'est sans
  doute la vraie cause de la demande 32 en jeu. Corrigé : les GUID vont dans le fichier chargé (rien sans fichier),
  le test de réussite était inversé (`iniSave` renvoie 1), et la page complète un fichier sans `[Common]` avec les
  liaisons par défaut en gardant ses autres valeurs. Non essayé dans l'interface (session verrouillée).

### Dead Heat (demandes 37 et 42)
- Ce qu'il a fallu, dans l'ordre où le jeu s'arrêtait :
  - `sysBridge.cpp` (nouveau) : epoll et eventfd (descripteurs « virtuels » du loader, interceptés dans read, write et
    close), pipe, statvfs64, readdir64, clock (microsecondes), gmtime, timegm, sysconf (dont
    `_SC_THREAD_SAFE_FUNCTIONS`, que boost::filesystem lit pour prendre readdir_r), syscall (gettid), getpwuid_r,
    catopen/catgets, strerror_r (GNU), getpeername, if_nametoindex, inet_ntop, pthread_kill, pthread_sigmask...
  - SDL 1.2 (`graphics/sdl12Bridge.c`, nouveau) : les 18 fonctions du jeu au-dessus de SDL 3 et de la fenêtre du
    loader (SDL_SetVideoMode, SDL_GL_SwapBuffers avec la mise à l'échelle, l'overlay et l'agent, les événements
    traduits au format SDL 1.2, clavier, souris, manettes) ; `libSDL-1.2.so.0` et `librt.so.1` enregistrées.
  - ALSA : le pilote son de Namco (`nsAdrv.dll`, une bibliothèque Linux) demande 37 fonctions de plus (mixeur vide,
    infos de carte, taille de tampon et de période, `snd_pcm_wait`) et le format S32 ; le tampon arrondi au-dessus
    (255 trames au lieu de 256 faisait quitter le jeu).
  - Une entrée de jeu (`DEAD_HEAT_US`, CRC 0x7A3A5554) et les patchs de `dh_emu.c` : dongle HASP (login, read,
    decrypt), PCB ID (1, ou `DH_PCB_ID`), groupe réseau, deux arrêts d'objets.
  - Shaders : le jeu quitte si un message de compilation n'est pas vide ; le pilote Intel avertit du `#version`
    absent et refuse `gl_TexCoord[i]` sans taille déclarée (écrit pour NVIDIA). Pour Dead Heat, `#version 110` et
    `varying vec4 gl_TexCoord[8];` sont ajoutés en tête, et le message d'un shader compilé est vidé.
  - `glHooks` : 9 fonctions de plus (glUniform2f/2i/3i/4f/4i, glGetQueryiv, glMultiDrawArrays,
    glStencilFuncSeparate, glStencilOpSeparate).
  - Fichiers ouverts en binaire sous Windows (fopen, fopen64, fdopen) : en mode texte, un fichier CRLF lu dans un
    tampon de sa taille laissait la fin non écrite (un shader de Dead Heat finissait par des octets au hasard).
  - `config.csv` de la copie locale : `USE_CAMERA_DEVICE` à FALSE (pas de caméra) ; `USE_JAMMA_DEVICE` est déjà à
    FALSE, donc le jeu lit manette et clavier par SDL 1.2, sans carte d'entrées-sorties.
- Le mode agent a eu besoin d'un correctif : son thread lisait stdin avec `fgets`, et le `fflush(NULL)` du jeu
  attendait le verrou de stdin indéfiniment ; stdin est maintenant lu par `ReadFile`.
- `bridgeExit` note l'adresse de l'appelant dans le jeu : c'est ce qui a mené aux shaders.
- État : démarrage (« STEERING CHECK », PCB ID), avertissement, démo (caméra, photo, course en 3D) à 60 i/s, version
  MinGW ; la version MSVC a planté une fois au démarrage (comme R-Tuned, irrégulier). Suite (commandes, partie,
  plantage, `su`/`sudo`) : demande 47 ci-dessous.

### Dead Heat : bornes émulées, partie, plantage MSVC (demande 47)
- Deux chemins d'entrée dans le jeu : une manette SDL 1.2 (axe 0 pour le volant, accélérateur et frein en boutons
  tout-ou-rien, pas de monnayeur) ou la borne (USE_JAMMA_DEVICE). Choisi : la borne, émulée dans
  `hardware/namco/deadHeatIo.c`.
  - Carte d'E/S N2 : le jeu lui parle par port série (bibliothèque `n2Jvio` de Namco) et garde son état dans le bloc
    de `clSystemN2` (0x09033470). Les fonctions de démarrage et de service par image de `clSystemN2` sont remplacées :
    le bloc est écrit depuis l'état JVS du loader (controls.ini, l'agent). Les contacts ont la disposition JVS du
    loader : Service 0x4000, Haut/Bas (menu de test), bouton 1 Entrée, bouton 3 frein tout-ou-rien, 4 nitro, 5 vue,
    6 à 9 les quatre rapports du levier (Montée/Descente de rapport y sont converties), Test dans l'octet système.
  - Volant, accélérateur, frein : comptes 16 bits selon la calibration du jeu ; une sauvegarde jamais calibrée (des
    zéros, venue d'une machine sans la carte) reprend les valeurs par défaut du jeu (28600/4000/55000…).
  - Monnayeur : compteurs cumulés (le jeu ajoute la différence).
  - Carte volant Kickback : sur son propre port série, laissé fermé ; son type de couple lu comme 0 (pas de moteur),
    ce qui fait passer le « STEERING CHECK » du démarrage. Ses effets (ressort, viscosité, reflect, centre), fixés à
    chaque image de course par `clGameInput::update`, deviennent des trames de la carte servo Sega du modèle FFB :
    volant, journal `ffb`/`servo` de l'agent et sorties réseau suivent comme pour les jeux Lindbergh.
  - Test : interrupteur à bascule sur les bornes Namco (le menu dure tant qu'il est enclenché) : l'action Test est
    une bascule pour Dead Heat, avec un message à l'écran.
  - SDL 1.2 : plus aucune manette présentée au jeu (une manette n'est pas lue deux fois), les événements vont au
    loader (F10, F11, Échap, liaisons).
- Essais en mode agent (MinGW) : démarrage sans erreur, démo, « STEP ON THE GAS TO START » ; choix de voiture, de
  couleur et de ville au volant et à l'accélérateur ; photo (pas de caméra) ; course à San Francisco, 2e à
  152 km/h en boîte auto ; ressort et amortisseur envoyés au volant pendant la course ; menu de test (MENU, I/O TEST
  avec « I/F INITIALIZE (STEERING & GAS) ») entré par Test, parcouru avec Bas et Start, quitté en relâchant Test.
  Le reflect (couple) est resté à 0 pendant mes courses : son sens n'est pas vérifié.
- Plantage MSVC (6 lancements sur 10) : une exception Boost « Year is out of valid range: 1400..10000 ». Trouvé par
  un crochet temporaire sur l'affectation de l'année : `clTestMode` lit ses décalages d'horloge (+0x4dc..0x4e8)
  pendant sa construction, avant le chargement de la sauvegarde ; Linux donne de la mémoire neuve à zéro, Windows non
  (année ~50000). `malloc` et `operator new` du jeu rendent maintenant de la mémoire à zéro : 12 lancements sur 12.
  Cela peut expliquer d'autres plantages irréguliers (R-Tuned sous MSVC).
- `system()` sous Windows : seules les commandes traduites par le loader passent à cmd.exe. Les autres sont des
  commandes Linux (Dead Heat : `su -c 'date …'`, `sudo route del default`, `sudo ifconfig`, `sudo rm`,
  `sudo mount`…) : elles ne sont plus lancées et reçoivent le code de `/bin/sh` pour une commande introuvable (127).
  Avec le sudo de Windows activé, certaines auraient tourné en administrateur.
- Reste : le son à écouter (en mode agent caché il est coupé), le sens du reflect, une partie au volant réel.

### Dead Heat : étude (demande 37)
- `\\racecab1\C\racecab\Games\Dead Heat - Namco ES1` : `a.elf` (US DRIVE rév. 6273), `dh_emu.c` (patchs HASP, PCB
  ID, caméra, son), lancé par TeknoParrot (BudgieLoader). Copié : `games/deadheat/a.elf` seul.
- Bibliothèques : libSDL-1.2, libGL, libGLU, libstdc++.so.6, libpthread, librt, libz, libm, libgcc_s, libc, libdl.
- Manque au loader : les 18 fonctions SDL 1.2 utilisées (fenêtre OpenGL, événements, clavier, souris, manettes),
  une vingtaine de fonctions système (epoll, eventfd, pipe, `pthread_kill`, `pthread_sigmask`, `statvfs64`,
  `getpwuid_r`, `catopen`/`catgets`, `if_nametoindex`, `timegm`, `syscall`, `mlockall`...), les patchs HASP de
  `dh_emu.c` aux adresses de cette version, une entrée de jeu, et sans doute le son 5.1 et la caméra (à couper dans
  `data/csv/config.csv`). Les commandes passent par une manette SDL d'après le README.

### 4:3 avec bandes noires et résolution interne (demandes 44 à 46)
- `ASPECT_4_3_LETTERBOX` (3) : la fenêtre ou la zone d'écran prend la forme 4:3 (`blitDisplayAspect`), puis
  `blitPresent` place l'image à ses propres proportions dans ce cadre (bandes en haut et en bas pour une image plus
  large que 4:3, sur les côtés pour une plus étroite). Les bandes sont calculées en parts du cadre : en mode étiré,
  le cadre 4:3 est étiré à la fenêtre et les bandes le suivent (cas d'un écran 4:3 qui reçoit un signal 16:9).
- Résolution interne : `WIDTH`/`HEIGHT` de `[Display]` (`AUTO` = la taille native du jeu), appliqués par
  `patchResolution.c` quand le jeu a un patch ; la liste de F10 écrit ces deux clés. Prise en compte au lancement.
- Info-bulle de la liste (fermée et sur chaque entrée), d'après `patchResolution.c` :
  - essayés ici : OutRun au-delà de 800×480 (640×480 = mode Chihiro), SRTV, R-Tuned, Initial D 4 (le menu de
    service reste en 1360×768), Hummer et Hummer Extreme à toutes les tailles. Hummer ne se limite pas au
    2560×1440 du `kernel.nut` de TeknoParrot : `hummerRespatch` impose la taille choisie (demandes 25 et 28) ;
  - patchés, pas essayés : After Burner Climax, Harley-Davidson, Rambo, Too Spicy, Virtua Fighter 5,
    House of the Dead 4 et EX, Let's Go Jungle (toute taille), Initial D 5 (1360×768 et plus), HOTD4 Special
    (au-delà de 1024×768), Primeval Hunt (au-delà de 1280×480), Virtua Tennis 3 (au-delà de 1360×768),
    Let's Go Jungle Special (la hauteur suit la largeur en 16:9) ;
  - taille fixe : Ghost Squad Evolution, Mahjong Fight Club 4 ; les autres, Dead Heat compris : laisser Native.
- Commande d'agent `screen PATH` : l'image telle que la fenêtre la montre (aspect, mode, bandes), en BMP à la taille
  de la fenêtre ; `shot` reste l'image du jeu. Essais (`scratchpad/aspect_test.py`) :
  - Initial D 4, natif, 4:3 avec bandes, fenêtre : 1360×1020, image 1360×768 au centre, bandes de 126 pixels ;
  - même chose en fenêtre étirée : fenêtre 1360×768, le cadre 4:3 étiré, l'image sur 578 lignes au centre ;
  - OutRun, 1920×1080, 4:3 avec bandes : image du jeu 1920×1080, fenêtre 1920×1440, bandes de 180 pixels.
  Le premier essai d'OutRun restait en 800×480 : l'ini de test n'avait pas de ligne `WIDTH`, le script la
  remplaçait sans l'ajouter.

### Comparaison avec les autres forks (demande 20)
- **VooDooUdo2** (fusion d'Arkhist/main) : n'ouvre que les périphériques à rumble (`LEFTRIGHT`), donc ni vJoy ni la
  plupart des volants DirectInput ; effets recréés à chaque trame ; `81` ignoré, `83` → gain du périphérique, `85` →
  rumble, `87`/`88` journalisés, `8B` → autocentrage. Point fort : les pages `9D`/`9E` mémorisées et rejouées par `FB`.
  Leur `84` donne gauche = niveau positif, et c'est juste (voir plus bas) : le mien était inversé.
- **hyp36rmax** (`research/aer-01c-driveboard-recorder`) : enregistreur de trames et analyse du code d'OutRun 2 SP SDX
  (`DrCtrlDataSet`, `SetOutFactor`, tables de motifs, bit de sens `0x10` de `FB`). Pas de sortie SDL. Polarité moteur
  notée « OPEN — HARDWARE REQUIRED ». Sa capture a servi à reconstituer les pages d'OutRun.

### Pages 9D/9E/FB et sens du couple (demande 21)
- `DRIVEBOARD.md` : `9D index page` sélectionne un paramètre, `9E` l'écrit sur 14 bits ; 16 pages de 16 valeurs.
- Pages d'OutRun (capture hyp36rmax) : index 0 = `0x0010` partout, puis 15 échantillons autour de `0x80`. Exemples :
  page 0 `50 B0 B0 B0 B0` puis zéros (choc), pages C à F alternances `8C/72` ou `98/68` (vibrations). En partie,
  `FB 00 00` domine, puis `02`, `14`, `10`, `0B`, `04` ; le bit `0x10` joue la page dans l'autre sens.
- `forceFeedback.c` : table `pages[16][16]` dans `ServoBoard` (pages muettes au reset), `FB` lance la page, un
  échantillon toutes les `index 0` ms (16 ms, durée supposée, non mesurée), d'après le temps écoulé depuis le `FB`.
  L'échantillon s'ajoute au couple de `84`, puis gain, plafond et `FFB_STRENGTH` comme le reste. VooDooUdo2 jouait
  aussi l'index 0 comme un couple.
- Une partie d'OutRun (3 min) : 416 `FB`, premier pas 16-17 ms après, puis un pas toutes les 15-17 ms. Dans cette
  partie, la page 0 n'a pas reçu son index 1 (`9D 01 00` sans `9E`) ; la capture de hyp36rmax perd aussi des index 1.
- Sens : pour SDL, une direction cartésienne (1, 0) est une force venant de la droite, et `STEERING_AXIS` vaut la même
  chose (0x4000 sous Linux, 90° côté DirectInput, vu dans le journal vJoy). Un niveau positif pousse donc à gauche,
  comme le + de la carte : le signe est maintenant direct. BackForceFeeder lit aussi 90° comme « tourner à gauche ».
- `FFB_INVERT` (`[ForceFeedback]`, faux par défaut) et la case « Inverser le sens » de F10 retournent le couple
  (`84` et pages), pas le centre du ressort, qui est une position sur l'axe.

### Thread FFB (demande 22)
- Constat de flycast (session « Multi-Board », correctif `e815f019f`) : avec un SideWinder, Initial D tombait de 60 à
  30 images/s sur les vibreurs, CPU et GPU au repos. Chaque appel haptique SDL est un ou plusieurs rapports HID
  synchrones (DirectInput `SetParameters`, `Run`), envoyés presque à chaque image depuis le thread d'émulation.
- Ici, avant : trames appliquées dans le thread du jeu qui écrit sur la carte, timer des pages qui gardait le mutex
  pendant ses appels SDL, mode court vérifié dans la boucle d'événements.
- Maintenant : le thread du jeu met seulement à jour `ServoBoard` (drapeau `dirty`) et réveille le thread « FFB ».
  Ce thread attend un changement ou la prochaine échéance (pas de page, coupure du mode court), copie l'état sous
  `boardLock`, puis fait les appels SDL sans le verrou. Le dernier état gagne : un volant lent fait sauter des états
  (et des échantillons de page) au lieu de retarder le jeu. `sdlFfbTick` supprimé.
- Envois : un effet n'est renvoyé que s'il change (`memcmp`), et lancé (`Run`) seulement s'il ne tourne pas. Après
  une mise à jour en échec, il est arrêté puis renvoyé et relancé au changement suivant. Effets créés arrêtés.
- Partie d'OutRun identique, paquets reçus par vJoy : `Constant` 4 076 → 1 807, `EffectReport` 3 839 → 5,
  `Condition` 1 798 → 349.
- Pas de `SDL_LockJoysticks` autour des appels (piste de la session flycast contre la réacquisition DirectInput
  concurrente) : la boucle d'événements tourne dans le thread de rendu du jeu, qui attendrait de nouveau l'USB.
- Ne pas faire tourner deux programmes sur le FFB de vJoy en même temps : flycast a planté dans `pid.dll` pendant
  que linuxloader l'utilisait.

### Résolutions et rapports d'aspect (demande 28)
- Méthode : `scratchpad/matrix.ps1` lance chaque jeu avec un `-c test.ini` par combinaison (WIDTH, HEIGHT,
  DISPLAY_MODE, ASPECT_RATIO) et une capture interne temporaire (`LL_SHOT_EVERY`, lecture du framebuffer 0), puis
  `sheet.ps1` fait une planche par jeu. Écran physique 3072×1920 (affichage à 150 %).
- Fenêtre étirée, 640×480 à 1920×1080 :
  - OutRun : bon de 800×600 à 1920×1080 ; **noir en 640×480** (mode Chihiro : le jeu rend en 800×480 et le loader
    en garde le centre, mais la redirection du framebuffer 0 vers le FBO n'existait pas sous Windows).
  - SRTV : bon partout ; en 16:9 le jeu met lui-même des bandes noires (image 4:3 au centre de son framebuffer).
  - R-Tuned, Initial D 4 : bons partout (Initial D 4 a une démo en format cinéma).
  - Hummer : **bon en 1360×768 et 1920×1080**, rogné de 640×480 à 1280×720. Le patch de résolution
    (`hummerRespatch`) n'était appliqué qu'au-dessus de 1280×768 ; il marche donc avec le dump TeknoParrot malgré son
    `kernel.nut` en 2560×1440. Mes écrans « noirs » de la veille avec le patch forcé venaient des captures GDI
    (noires sur ces fenêtres OpenGL), pas du jeu : patch rétabli à toutes les tailles.

- Après correctifs (rendu hors écran, patch de Hummer à toutes les tailles) :
  - OutRun 640×480 (Chihiro) s'affiche ; OutRun, R-Tuned et SRTV en 3840×2160 : fenêtre réduite à 3050×1715,
    image complète mise à l'échelle ;
  - Hummer 640×480 à 1280×720 : image complète ;
  - fenêtre 1280×720 en ORIGINAL / 4:3 / 16:9 : en 4:3 la fenêtre passe à 1280×960 et l'image prend cette forme ;
  - plein écran étiré 1024×768 : l'image couvre les 3072×1920 ; plein écran 4:3 en 1280×720 : image de
    2560×1920 centrée, bandes sur les côtés. SRTV 1024×768 en plein écran étiré remplit enfin l'écran.
  - Tous les essais à 48–60 i/s.

### Rendu hors écran (demande 29)
- `blitStretching.c` : si l'image du jeu ne tient pas dans la fenêtre (mode Chihiro d'OutRun, fenêtre réduite,
  résolution plus grande que l'écran), le jeu dessine dans `fboId` (couleur + profondeur/stencil
  `GL_DEPTH24_STENCIL8`) à sa taille, et la seconde copie le met à l'échelle de la fenêtre. Activation à la volée
  (`blitOffscreen`), Windows seulement.
- `glHooks.cpp` : `glBindFramebuffer(EXT)(…, 0)` → `fboId` ; `glDrawBuffer` / `glReadBuffer` de `GL_BACK`,
  `GL_FRONT`… → `GL_COLOR_ATTACHMENT0` quand `fboId` est lié. Après le swap, `blitAfterSwap` rend au jeu sa liaison ;
  pendant le swap la fenêtre reste liée pour les surimpressions (FPS, F10).
- `blitWindowedSize` : une fenêtre plus grande que la zone utile de l'écran est réduite à celle-ci, proportions
  gardées (le jeu passe alors hors écran).

### Initial D 4 : shaders (demande 29)
- Initial D 4 importe `glProgramStringARB` : sous Windows il aboutissait au hook générique, sans passer par
  `gl_ProgramStringARB` et ses correctifs d'Initial D. Il y passe maintenant (`wrap_glProgramStringARB`).
- Les deux « invalid program header » : le jeu demande à Cg la chaîne d'un programme NULL (des centaines de fois) et
  `bridgeCgGetProgramString` renvoie le dernier programme compilé. Deux fois, c'était un programme de fragment
  (`!!ARBfp1.0`) pour une cible de sommets. `gl_ProgramStringARB` garde le dernier programme valide de chaque type et
  le réutilise quand l'en-tête ne correspond pas à la cible : plus aucun programme rejeté.
- Essayé sans effet sur les taches noires : les ombres en 2D (`idFsAti` : `ARB_fragment_program_shadow` retiré,
  `SHADOW2D` → `2D`), retiré. Cg compile tous les programmes (avertissements seulement).
- Les taches apparaissent aussi en l'air près des roues : ce sont des feuilles mortes qui volent, un effet visuel
  voulu (automne), confirmé par l'utilisateur. Rien à corriger.

### Alignement de pile hôte → jeu (demande 29)
- MSVC ne garde qu'un alignement de pile de 4 octets, alors que le code Linux i386 suppose `esp % 16 == 0` à chaque
  appel (accès SSE alignés à la pile). `elfLoader/elfCall.h` : `elfCall0` à `elfCall4` (MSVC, `__asm`) réalignent la
  pile avant d'appeler le jeu ; avec GCC/MinGW les appels restent directs (`#ifdef _MSC_VER`).
- Points d'appel hôte → jeu trouvés et alignés : callbacks GLUT (affichage, redimensionnement, visibilité, idle —
  la boucle principale d'OutRun), `__libc_start_main` (init, main, fini), `DT_INIT` / `DT_INIT_ARRAY`,
  entrée de thread (ancien `CallLinuxAligned`, remplacé), `pthread_once` (les deux versions enregistrées ; la seconde
  remplace la première), destructeurs de clés TLS, comparateurs de `qsort` / `bsearch` (`qsort_s` / `bsearch_s` avec
  le comparateur du jeu en contexte), gestionnaires de `signal` / `sigaction` (appelés par la CRT via
  `hostSignalHandler`, qui passe aussi le numéro Linux : SIGABRT vaut 6, pas 22).
- Non concernés : `__cxa_atexit` (gestionnaires jamais appelés), `dl_iterate_phdr` et `XSetErrorHandler` (jamais
  rappelés), l'entrée `_start` (pile Linux préparée et alignée par `LinuxStack::Setup`).

### R-Tuned et Initial D 4 (demande 27)
- Copiés avec robocopy (`/S /E /MT:16`) dans `games/rtuned` et `games/initiald4` (ignorés par git).
- Initial D 4 (`games/initiald4/id4.elf`, 3,05 Go, 9 147 fichiers) : détecté « Initial D Arcade Stage 4 Rev B »,
  60 i/s, mode démo affiché (logo CRIWARE, avertissement, voitures, titre). Taches noires sur la carrosserie
  et la route (des feuilles mortes, voulues, voir demande 29) ; et deux programmes ARB rejetés au chargement (« Error, line 1: invalid program header », position 0 :
  en-tête non reconnu, peut-être d'anciens programmes NVIDIA). La correction de shaders d'Initial D (`idFsAti`, qui
  retire `ARB_fragment_program_shadow`) n'est appliquée que sur ATI. Carte servo non émulée en `AUTO`
  (`emulateDriveboard = 0`), donc pas de retour de force par défaut.
- R-Tuned (`games/rtuned/dsr`, variantes TeknoParrot `dsr_HD`, `dsr_VGA`, `*_FFB`) : 1,37 Go ; 4 756 fichiers non
  copiés, tous dans `drv/` et `drv.old/` (pilote NVIDIA de la borne, liens Linux), inutiles ici.
  - Premier lancement : « Unresolved symbol called: ldiv », puis `rmdir`. Liste des imports de `dsr` comparée aux
    ponts (`MAP`, `m_VTables`) : 12 fonctions de la libc manquaient. Ajoutées : `ldiv`, `difftime`, `ldexp`,
    `roundf` (mathBridge), `rmdir`, `rewinddir`, `readdir64_r`, `glob64`, `globfree64` (filesystemBridge, `glob64`
    gérant les jokers du dernier composant du chemin), `mount`, `umount`, `mtrace`, `muntrace` (sans effet).
  - `div` / `ldiv` renvoient une structure : l'ABI i386 Linux la renvoie par un pointeur caché passé en premier, que
    l'appelé dépile (`ret $4`, vu au point d'appel de `dsr`), alors que MSVC la renverrait dans edx:eax. L'ancien
    `bridge_div` était donc faux (aucun jeu testé ne l'appelait) : remplacé par un thunk en assembleur, MSVC et GCC.
  - Résultat : 50 i/s, démarrage système, logo SEGA et scènes de démo sans défaut visible.
- Hummer importe aussi `readdir64_r` (maintenant fourni) : toujours 60 i/s, après ~70 s de chargement lent comme
  avant.
- Capture : la capture GDI des fenêtres OpenGL est noire ici ; captures faites depuis le loader (aide temporaire,
  retirée).

### Page de contrôles façon Flycast (demande 26)
- Référence : `core/ui/settings_controls.cpp` de Flycast (« Périphériques Physiques » avec Lier / Préférences, page
  plein écran « Liaisons Contrôleur » avec Fermer, Réinitialiser…, le jeu de contrôles, puis Lier / Délier par action).
- Onglet Contrôles (`guiControls.cpp`, réécrit) :
  - **Périphériques** : clavier, souris, puis chaque manette (GCn / JOYn) ou joystick (JOYn), vJoy compris, avec
    leur préfixe dans `controls.ini` et un bouton « Lier » ; « (retour de force) » sur le premier quand son haptique
    est ouvert ;
  - **Retour de force** : intensité et inversion du sens (l'onglet Retour de force est supprimé) ;
  - **Zones mortes et options** : les clés de `[Config]` dans l'ordre du fichier (curseurs pour `*_DeadZone`, cases
    pour `*_Toggle`, nombres sinon).
- Page d'un périphérique (popup plein écran opaque) : Fermer (enregistre `controls.ini` et recharge les liaisons en
  jeu), Réinitialiser… (liaisons de ce périphérique reprises d'un `controls.ini` par défaut généré par
  `createDefaultControlsIni`), type de jeu. Puis « Tous les jeux » et le type choisi, une barre de titre par joueur ;
  par action : la liaison de ce périphérique seul en clair (« Axe 1 - », « Bouton 4 », « Chapeau 0 haut »…), une jauge
  en direct (barre centrée avec zone morte, barre de pédale, ou voyant), Lier / Délier. Lier et Délier ne touchent
  qu'aux alternatives de ce périphérique : celles des autres restent.
- Capture limitée au périphérique de la page ; la souris annule par Échap seulement (un clic serait sa commande) et
  lie `MOUSE_AXIS_X/Y` pour les actions analogiques.
- L'indication « Jeu en pause » passe derrière les fenêtres, pour ne pas chevaucher la page.
- Piège : `iniSave` renvoie 1 en cas de succès (son commentaire dit 0).
- Test : captures faites par le loader (la capture GDI des fenêtres OpenGL est noire ici, même hors premier plan) et
  clics simulés ; l'affichage est à 150 %, un PowerShell sans prise en charge du DPI voit des coordonnées / 1,5.

### Hummer : carte servo en AUTO et résolution (demandes 24 et 25)
- `gameData.c` : les 4 entrées Hummer avaient `emulateDriveboard = 0`, donc en `EMULATE_DRIVEBOARD = AUTO` le port série
  de la carte n'était jamais ouvert et le volant ne recevait rien (il fallait `-c hummer.ini`). Passées à 1 :
  `patch.c` prévoyait déjà, pour chacune, un patch de carte servo appliqué seulement si elle est émulée. Vérifié sur
  Extreme avec la config par défaut (ressort, amortisseur, 77 salves de sinus). Harley et Initial D 4/5 ont le même
  `0`, laissé tel quel : pas de dump de Harley, et Initial D partage ses ports série avec le lecteur de cartes.
- Résolution : le dump TeknoParrot de Hummer Extreme a son `data-hummer/config/kernel.nut` modifié en 2560×1440
  (`kernel.nut.org` = 1280×768) et un ELF « pas propre ». En `AUTO`, le loader ouvre une fenêtre de 1280×768 et le jeu
  y dessine du 2560×1440 : seul le quart inférieur gauche est visible. Le patch de résolution (`hummerRespatch`,
  appels `clSQObject::get<int>("SCREEN_XSIZE"...)`) est sauté en ≤ 1280×768.
- Essayé puis annulé : appliquer toujours `hummerRespatch`, et servir un `kernel.nut` réécrit en 1280×768 (copie
  identique hors tailles). Dans les deux cas le jeu reste bloqué au démarrage (écran noir, aucune initialisation de
  la carte servo vers 40 s). Ce dump ne tourne qu'en 2560×1440.
- Piège de test : Hummer reste noir jusqu'à ~190 s sans pièce, même avec l'ancienne release ; comparer aux mêmes
  instants et avec les entrées vJoy.
- Correctif proposé en tâche séparée : faire rendre le jeu dans un FBO à sa taille puis le mettre à l'échelle de la
  fenêtre (comme la redirection du FB0 du mode Chihiro d'OutRun).

### Pause du jeu avec F10 (demande 23)
- Avant : la fenêtre était dessinée par-dessus l'image à chaque image, le jeu continuait, et elle restait invisible
  en partie.
- `blitStretching.c` : la copie de l'image du jeu vers la fenêtre devient `blitPresent`. Si F10 est ouvert,
  `blitSettingsPause` garde le thread du jeu dans son swap : boucle à ~60 Hz qui réaffiche la dernière image du jeu
  (gardée dans le FBO, toujours à la bonne taille même si la fenêtre change), dessine les paramètres, échange et
  traite les événements. À la fermeture, l'image du jeu est réaffichée sans les paramètres, puis le jeu repart.
- Pendant la pause : forces du volant nulles (`sdlFfbPause`), moteur FAudio de libsegaapi arrêté (`segaapiPause`),
  périphérique SDL d'alsa2sdl en pause (`alsaPause`, et `snd_pcm_start` ne le relance pas).
- `guiOverlay.cpp` : tout l'état fixe est empilé (`glPushAttrib`/`glPushClientAttrib`) puis neutralisé autour
  d'ImGui : brouillard, test alpha, éclairage, plans de coupe, alpha-to-coverage, sRGB, toutes les unités de
  texture, matrice de texture, équation de mélange, tableaux d'attributs génériques.
- Indication en haut à gauche : « Jeu en pause - F10 : fermer les paramètres ».
- Vérifié avec F10 simulé en pleine course (aide de test retirée ensuite), capture lue depuis le framebuffer 0 :
  fenêtre et indication visibles devant l'image figée sur OutRun et SRTV ; reprise normale (OutRun : chrono 0'15"07
  à l'ouverture, 0'18"02 trois secondes après la fermeture, 5 s de pause) ; ressort et amortisseur à zéro pendant
  la pause puis rétablis (journal vJoy). Hummer (`-c hummer.ini`) : pause et reprise, sinus `85` à 203 s.
- Une première capture lisait le FBO (l'image du jeu seule) et faisait croire la fenêtre absente.

## Diagnostic temporaire
Tout retiré le 19/10 (hooks SRTV, captures BMP, journal des trames, aides de test du picker).
