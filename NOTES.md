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
| 52 | 21/10 | Lancer les jeux depuis le partage de racecab1 (question de la session « Image borne ») | ✅ possible en lecture : Dead Heat lancé depuis `\\racecab1\C\racecab\Games`, rien d'écrit sur la borne ; mais démarrage en 100 s au lieu de 11 s et démo saccadée (données lues par le réseau) |
| 53 | 21/10 | Dossier d'écriture pour UWF : tout en lecture seule sauf un dossier, réglages, NVRAM et fichiers des jeux en overlay ; même spécification pour sm2-emu, Supermodel et Flycast | ⚠️ linuxloader : `--write-dir` / `[Paths] WRITE_DIR`, essayé avec OutRun et Dead Heat (dossiers des jeux intacts) ; spécification envoyée aux trois sessions ; sm2-emu et Supermodel l'ont faite (non commitée) ; Flycast sans réponse |
| 54 | 21/10 | Initial D 5 ne freine pas (signalé par un joueur) : le copier et l'essayer | ✅ copié dans `games/initiald5` ; le frein marche dans le loader : le défaut vient sans doute de la version, de la manette ou du `controls.ini` du joueur (à lui demander) |
| 55 | 21/10 | Quels autres jeux non protégés peuvent passer par le linuxloader (question à la session « Image borne ») | ✅ six : Initial D 4 Export (traduction FR), Maximum Heat 3D, Dead Heat Riders, Ring Riders, Tokyo Cop, Gaelco Championship Tuning Race ; copiés de racecab1 dans `games/` |
| 56 | 21/10 | Les faire tous, dans l'ordre | ✅ cinq jouables (ID4 FR, Maximum Heat 3D, Ring Riders, Tokyo Cop, Gaelco Tuning) ; Dead Heat Riders s'arrête sur sa clé HASP USB (« DONGLE ERROR 1 ») : contournement à décider ; détail dans `BILAN.md` |
| 57 | 21/10 | Creuser Initial D 4 FR (écran « Card Stocker Cleaning », image décalée) | ✅ les deux corrigés, démo en français plein écran (voir journal) |
| 58 | 21/10 | Tester aussi les autres nouveaux jeux | ✅ voir 56 |
| 59 | 22/10 | Continuer avec les autres jeux | ✅ voir 56 |
| 60 | 22/10 | Mettre à jour les notes avec les derniers développements | ✅ |
| 61 | 22/10 | Supprimer la copie locale de Cruis'n Blast si plus rien n'est possible dessus | ✅ `games/cruisnblast` (12 Go) mis à la Corbeille |
| 62 | 22/10 | Un bilan par fonction (contrôles, vJoy, FFB, affichage, menu de service...) dans un markdown rempli au fur et à mesure | ✅ `BILAN.md` : tableau par jeu, bilan par fonction, détail par jeu |
| 63 | 22/10 | Ne plus s'arrêter avant d'avoir lancé tous les jeux et fait tous les essais | ✅ sauf Dead Heat Riders (clé HASP, à décider) ; jeux Gaelco joués par vJoy le 22/10 au soir |
| 64 | 22/10 | Effacer les copies locales des jeux déjà pris en charge ; retirer FFB Plugin et DLL TeknoParrot de chaque nouvelle copie | ✅ ID4 FR, Maximum Heat 3D, Dead Heat mis à la Corbeille ; copies nettoyées ; restent `deadheatriders`, `gaelcotuning`, `ringriders`, `tokyocop` |
| 65 | 22/10 | (relayée par windowsloader) Titre de fenêtre « titre [nom court] » pour l'autodétection de BackForceFeeder | ✅ toutes les fenêtres (GLUT, GLX, SDL 1.2, SDL, X11) |
| 66 | 22/10 | (relayée par windowsloader) ASPECT_RATIO comme windowsloader : 4:3 et 16:9 = forme du cadre, image à son rapport avec bandes noires, seuls les modes étirés étirent | ✅ |
| 67 | 22/10 | Émuler le son des jeux Gaelco | ✅ carte son émulée (`gaelcoSound.c`) : banque `roms.new`, 16 voies, IMA ADPCM ; vérifié par captures WAV (MinGW et MSVC), pas encore écouté |
| 68 | 22/10 | Essai vJoy des jeux Gaelco après la série de windowsloader | ✅ les trois joués par vJoy (pièce, Start, commandes, retour de force reçu) ; accélérateur de vJoy sur l'axe 3 (vJoy à 4 axes) |
| 69 | 22/10 | Remettre le 4:3 « plein » (16:9 comprimé) et le 16:9 étiré ; ajouter 4:3 et 16:9 recadrés, avec un curseur de recadrage | ✅ `ASPECT_RATIO` = `4:3_STRETCH`, `16:9_STRETCH`, `4:3_CROP`, `16:9_CROP`, `CROP_POSITION` (−100 à 100) ; dans F10 avec le curseur ; 4:3 et 16:9 gardent leurs bandes noires (accord windowsloader) ; vérifié sur Tuning (4:3) et Dead Heat Riders (16:9), MinGW et MSVC compilés |
| 70 | 22/10 | Recadré : montrer quand même les parties coupées, comprimées sur les bords comme sur un cylindre (taille de la zone centrale, rayon de courbure) | ✅ `CROP_BARREL`, `BARREL_CENTER` (%), `BARREL_RADIUS` (%) ; dans F10 (case et deux curseurs, compilé, pas encore vu à l'écran) ; vérifié sur Dead Heat Riders (4:3 recadré) et Tuning (16:9 recadré), MinGW et MSVC compilés |
| 71 | 22/10 | Réglages par jeu et en ligne de commande | ✅ `<nom court>.ini` à côté de `linuxloader.ini` lu à sa place pour ce jeu ; bouton F10 « Sauvegarder pour ce jeu » (copie du fichier commun la première fois, puis les sauvegardes y vont ; compilé, pas encore vu à l'écran) ; `--set SECTION.CLE=VALEUR` répétable, par-dessus le fichier lu ; vérifié en mode agent (fichier du jeu, `--set`, les deux ensemble) |
| 72 | 22/10 | Demander à windowsloader, Flycast, Supermodel et sm2-emu les mêmes réglages d'aspect, de sauvegarde par jeu et de ligne de commande | ✅ spécification envoyée aux quatre sessions (windowsloader en file d'attente) |
| 73 | 22/10 | Que reste-t-il à faire ? | ✅ réponse : Nirin, Maximum Tune 3 et 3DX+, la clé de Dead Heat Riders, des vérifications à l'écran |
| 74 | 22/10 | Poursuivre avec les jeux de la borne | 🔄 Nirin : démarre jusqu'à l'écran titre (contrôles du démarrage, sauvegardes, pièces, menu de test, test des commandes), puis « USB DONGLE ERROR » : clé HASP, contournement à décider ; Maximum Tune 3 et 3DX+ ensuite |
| 75 | 22/10 | Pause pour redémarrer le PC, puis commit et push | ✅ `0b377d3` poussé (avec les fichiers de Cruis'n Blast, au choix de l'utilisateur) |
| 76 | 22/10 | Reprendre | ✅ voir 74 |
| 77 | 22/10 | Retirer l'utilisation du dongle (Nirin, Dead Heat Riders) | ✅ les deux jouables : Nirin (clé HASP répondue comme celle de Dead Heat, course menée en mode agent et par vJoy) ; Dead Heat Riders (sa clé USB présentée par la libusb du loader, cartes N2 et Kickback émulées, course menée en mode agent, MinGW et MSVC) ; voir journal |
| 78 | 23/10 | Commit et push, puis continuer avec Maximum Tune 3 | 🔄 `ea5690e` poussé ; Maximum Tune 3 jouable (démarrage, menu de test, partie, FFB, son en capture WAV ; MinGW, et MSVC jusqu'à la course qui s'y bloque) ; lecteur-imprimante de cartes pas encore émulé (CARD R/W OFF dans GAME OPTIONS) ; 3DX+ ensuite |
| 79 | 23/10 | Inventaire des jeux Linux de racecab1 (refait) | ✅ 65 dossiers parcourus : 18 jeux Linux (7 Lindbergh, 3 Namco ES1, 3 Namco N2, 2 Cruis'n Blast, 3 Gaelco), 47 Windows ; 15 se jouent, restent MT3DX+ (à copier), les cartes de MT3, Cruis'n Blast (abandonné) ; tableau dans `BILAN.md` |
| 88 | 23/10 | Continuer les jeux et l'intégration, commit à la fin seulement | ✅ Maximum Tune 3 mené par vJoy (entrées corrigées hors mode agent), écriture de la carte vue ; Maximum Tune 3DX+ jouable : démarrage, menus, course, FFB, vJoy, menu de test ; les deux en `WORKING` (voir journal) |
| 89 | 23/10 | Pour Cruis'n Blast 1.25, s'appuyer sur l'émulation HASP de Nirin | ❌ refusé : son programme est protégé par l'enveloppe Sentinel HASP (une protection contre la copie, pas les appels à la bibliothèque HASP de Nirin) ; le lancer sans la clé reviendrait à neutraliser cette protection |
| 90 | 23/10 | Tableau des jeux faits et restant à faire | ✅ donné dans la conversation ; `BILAN.md` à jour |
| 91 | 23/10 | Cruis'n Blast 1.25 n'est pas chiffré ? | ✅ exact, ma réponse était fausse : son code est en clair (6,47 bits par octet) ; l'enveloppe enlève les sections et cache les imports |
| 92 | 23/10 | Neutraliser l'enveloppe par émulation du dongle | ❌ refusé : l'enveloppe Sentinel est une protection contre la copie ; la neutraliser, même en émulant la clé, revient à la contourner |
| 93 | 23/10 | Reste-t-il des jeux ? | ✅ deux jeux incomplets : R-Tuned (course entière, plantage MSVC) et Initial D 4 Japon (carte servo, FFB) |
| 94 | 23/10 | Les faire | ✅ R-Tuned : plus de plantage MSVC, course entière, FFB, menu de test, vJoy ; Initial D 4 Japon : FFB par défaut, course, vJoy (voir journal) |
| 129 | 28/10 | Commit et push, une release, prévenir Image borne | ✅ poussé jusqu'à `e961a3e` ; `emulateurs/releases/linuxloader-v3.0.13-ffbedition-20261028-njz3-win32.zip` (première en 3.0.13) ; Image borne prévenue (voir journal) |
| 128 | 28/10 | Une option en ligne de commande qui liste les jeux supportés : nom, nom court (nom de rom), version, exécutable attendu, CRC32 de l'exécutable (le CRC32 seulement indicatif, un autre exécutable n'est pas refusé) | ✅ `--list-games` (tableau) et `--list-games csv` ; les 91 jeux de `gameData.c` ; CRC32 du fichier entier de l'exécutable propre, quand il est connu ; MinGW et MSVC (voir journal) |
| 127 | 28/10 | (relayée par la session « Image borne ») Amorcer le dossier d'écriture d'un jeu depuis un dossier de modèles, pas seulement depuis le dossier du jeu : les NVRAM d'une cabine link (LINK ID, siège) ne peuvent pas vivre dans le dossier du jeu, commun au solo et au link | ✅ `--seed-dir PATH` ou `[Paths] SEED_DIR` (relatif au dossier du chargeur, ou chemin complet), avec un dossier d'écriture seulement : au lancement du jeu, `<seed>\<jeu>` est recopié dans `WRITE_DIR\<jeu>`, sans jamais remplacer un fichier déjà là, une ligne de journal par fichier ; sans l'option rien ne change ; MinGW et MSVC, OutRun 2 ; branche `dev/seed-dir`, non poussée (voir journal) |
| 126 | 28/10 | Merge avec upstream | ✅ `lindbergh-loader/linuxloader` `4ffa095` (3.0.13 : VF5, alignement de pile, GSEvo, pbuffers en FBO, libsegaapi complète, shaders Race TV et Initial D) fusionné dans `dev/ffbedition` (`0b3f5a9`) ; 15 fichiers en conflit résolus ; plantage d'OutRun au départ de la course causé par un correctif d'upstream sur les ELF déjà patchés, corrigé ; MinGW et MSVC, OutRun jusqu'en course ; non poussé (voir journal) |
| 125 | 28/10 | Commit, push, une release, prévenir Image borne | ✅ rien de plus à commiter, déjà poussé jusqu'à `8752305` ; `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261028-njz3-win32.zip` ; Image borne prévenue (voir journal) |
| 124 | 27/10 | D'autres utilisateurs disent que les pédales ne marchent pas : vérifier les axes analogiques dans le menu de test | ✅ le jeu lit bien ses axes (OutRun, INPUT TEST : `00H` à `FFH`, `7FH` à mi-course, volant `00H`/`7FH`/`FFH`) ; par vJoy et `controls.ini`, une pédale qui repose en haut de son axe donne `FFH` au repos, au milieu `7FH`, et sans `controls.ini` l'accélérateur attend l'axe 5 (absent sur vJoy et bien des volants) ; la capture de Tab > Contrôles n'ajoutait jamais `_INVERTED` ni `_SIDE`. Corrigé : la capture prend le mode d'après la position de repos, les barres montrent la pédale telle qu'elle est lue, les liaisons par défaut suivent le BFF (accélérateur Y−, frein Z−) ; vérifié avec vJoy dans INPUT TEST, MinGW (voir journal) |
| 123 | 27/10 | Copier OutRun 2 et essayer en jeu les nouvelles réponses de la servoboard | ✅ démarrage (« Initializing the drive board », pas de faute), démo, course atteinte, DRIVE BOARD TEST MENU (il attend les 4 s de la recherche d'index par `FD`, comme sur la borne) ; mode agent caché, MinGW (voir journal) |
| 122 | 27/10 | Initial D 5 : essayer son test du retour de force (menu de test) | ✅ passe tel quel : « CHECKING.... » environ 21 s, puis le menu ; CENTER, RIGHT et LEFT envoient leurs couples, sens justes ; MinGW et MSVC, mode agent caché ; aucune réponse de la carte n'est attendue, rien à brancher ; Initial D 4 pas essayé (voir journal) |
| 121 | 27/10 | (signalée par la session « Driveboard émulation et force feedback ») La servoboard Sega émulée répond toujours un octet, la vraie quatre après `FA 00 10` ; relire DRIVEBOARD.md (exact, vérifié sur le matériel) et comparer avec l'émulation | ✅ réponses de `driveBoard.c` refaites d'après DRIVEBOARD.md : modes de réponse `FA 00 10`, `14`, `1F`, file de réponses, recherche d'index de 4,05 s, `tcflush` ; rejouées contre les relevés d'une vraie borne, identiques à la limite de la recherche près ; MinGW et MSVC ; Initial D 4 et 5 ne passent pas par là (demande 122) ; jeux de `driveBoard.c` pas relancés, aucune copie locale (voir journal) |
| 120 | 27/10 | Commit et push, puis une release | ✅ poussé jusqu'à `2aab762` ; `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261027-njz3-win32.zip` (voir journal) |
| 119 | 27/10 | (relayée par la session « Driveboard émulation et force feedback ») Choisir le périphérique de retour de force dans Tab > Contrôles > Retour de force : « Automatique » ou un périphérique branché, par son nom ; lui seul reçoit les effets, où que soit lu le volant ; changement en direct ; `[ForceFeedback] FFB_DEVICE`, réglable par `--set` | ✅ `FFB_DEVICE = AUTO` ou le nom SDL du périphérique (nom produit DirectInput sous Windows), comparé sans la casse ni les espaces autour ; Automatique tant qu'il est absent, repris dès qu'il est branché (règle du propriétaire pour tous les émulateurs) ; liste dans Contrôles, changement en direct ; essayé sur vJoy (voir journal) |
| 118 | 26/10 | Nommer les lampes connues dans la liste des sorties, documenter les modes `_POSITIVE_SIDE` / `_NEGATIVE_SIDE` (retour d'un utilisateur d'OutRun 2 : pédales actives au repos, pas de retour de force, lampe Start) | ✅ lampes d'OutRun 2 nommées, guide des contrôles complété ; réponse rédigée pour l'utilisateur (voir journal) |
| 117 | 26/10 | Commencer le link à deux bornes demandé par Image borne : OutRun 2, Hummer, Sega Race TV, Initial D 4 et 5 | ✅ côté loader : sockets non bloquantes par `fcntl`, pas d'erreur `WSAECONNRESET` en UDP, eth0 sur l'adresse de la borne ; réglages par borne transmis à Image borne ; essai à deux bornes à faire par Image borne ; Initial D 4 japonais pas pris en charge (voir journal) |
| 116 | 26/10 | Demander aux sessions sm2-emu, Flycast et Supermodel d'afficher aussi les noms des variables à côté de l'option des sorties réseau, dans le même style, selon le jeu en cours | ✅ consigne envoyée aux trois sessions avec la capture et le code de référence (`drawOutputsList`) |
| 115 | 26/10 | Afficher dans « Sorties et réseau » la liste, avec défilement, des variables que le jeu courant peut émettre et leur type | ✅ tableau Nom / Type / Signification / Valeur sous les ports, en jeu (voir journal) |
| 114 | 26/10 | Ajouter les lampes manquantes : cartes Namco et Gaelco, écritures JVS bit par bit | ✅ les lampes de la carte Kickback des cinq jeux Namco qui en ont une, les sorties et les ports de bits Gaelco, `CMD_WRITE_GPO_BIT` (voir journal) |
| 113 | 26/10 | Quelles sorties lampes sont disponibles pour chaque jeu (port 8000) ? | ✅ réponse : lampes JVS pour les sept jeux Lindbergh, rien pour les Namco et Gaelco, `RawDrive` partout ; trois trous relevés, comblés par la demande 114 |
| 112 | 25/10 | Commit et push, release, prévenir Image borne pour mettre à jour les deux bornes | ✅ rien à commiter (poussé jusqu'à `e67aa54`) ; `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261025-4-njz3-win32.zip`, la première faite par `release.py` ; Image borne prévenue (voir journal) |
| 111 | 25/10 | L'avertissement quand la copie des réglages dans le dossier d'écriture est plus ancienne | ✅ une ligne WARN au démarrage par fichier, `linuxloader.ini` et `controls.ini` (voir journal) |
| 110 | 25/10 | Ajouter le suffixe des releases du jour (-2, -3…) | ✅ `RELEASE_SUFFIX` écrit dans `version.h` le temps du build de release, dans le titre et `--version` ; la procédure le calcule d'après les zips du jour (voir journal) |
| 109 | 25/10 | Ajouter le journal de diagnostic des axes et des lectures Gaelco, faire une release, prévenir Image borne | ✅ `a121e39` ; `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261025-3-njz3-win32.zip` ; Image borne prévenue avec la marche à suivre (voir journal) |
| 108 | 25/10 | Commit et push, release, prévenir Image borne pour mettre à jour les deux bornes | ✅ poussé jusqu'à `af2871e` ; `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261025-2-njz3-win32.zip` ; Image borne prévenue (voir journal) |
| 107 | 25/10 | Dans le titre de la fenêtre de l'émulateur, pour cette branche : la version du build comme le package, et « FFB Edition » | ✅ « Tokyo Cop [tokyocop] - linuxloader-v2.0.0-ffbedition-20261025-njz3 FFB Edition - FPS: 57.34 » (`buildEdition`, `config.c`) ; une release y montre 3.0.12, comme son zip ; le sélecteur aussi (voir journal) |
| 106 | 25/10 | Quand la fenêtre de jeu est trop petite pour ouvrir les paramètres, diminuer la taille de la fenêtre de paramètres et mettre des barres de défilement | ✅ fenêtre de paramètres bornée à celle du jeu et ramenée dedans, onglets défilant dans les deux sens, échelle jusqu'à 80 % de la police ; vu dans le sélecteur à 640×480 et 480×360 (voir journal) |
| 105 | 25/10 | Corriger la recherche de la carte d'Initial D avec le dossier d'écriture, nouvelle release, prévenir Image borne | ✅ `idCardFileExists` passe par `writeDirForRead` ; `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261025-njz3-win32.zip` (commit `510f4d3`) ; pas essayé ici sur Initial D 4 (disque sous les 30 Go convenus), essai demandé à Image borne sur la borne (voir journal) |
| 104 | 24/10 | Corriger les bugs, faire une nouvelle release pour Image borne | ✅ trois correctifs (dlopen d'un nom nu, racine de l'image Linux, adresse de SEGA Race TV), chacun reproduit avec la release -4 puis corrigé, MinGW et MSVC ; `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261024-5-njz3-win32.zip` (commit `1c0adde`) ; Image borne prévenue (voir journal) |
| 103 | 24/10 | Diagnostiquer avec Image borne les jeux qui ne marchent pas ou n'ont pas de retour de force sur la borne | ✅ ses journaux `LL_LOG=info` : WMMT3 et 3DX+, Gaelco Championship Tuning Race, Ring Riders et Tokyo Cop échouent seulement avec le dossier d'écriture ; SEGA Race TV meurt à cause de `SRTV_IPADDRESS = 192.168.1.2` du fichier par défaut ; retour de force : câblage bon (joystick 0 = vJoy, mode volant), mesures à zéro sans doute hors course, à refaire avec l'enregistreur `vjoy_driver.py` (voir journal) |
| 102 | 24/10 | Faire la release et prévenir Image borne | ✅ `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261024-4-njz3-win32.zip` (commit `5b2f467`) ; Image borne prévenue (voir journal) |
| 101 | 24/10 | Corriger le point A de la campagne d'Image borne (SetErrorMode, pas de LoadLibrary sur un fichier ELF) et ajouter la phrase sur le dossier d'écriture dans les fichiers par défaut | ✅ plus de boîte système Windows (le processus et ceux qu'il lance) ; une bibliothèque Linux au nom en `.dll` va directement au chargeur ELF ; les `linuxloader.ini` et `controls.ini` neufs disent que, avec un dossier d'écriture, ce sont les copies de ce dossier qui sont lues (voir journal) |
| 100 | 24/10 | Faire le ménage ; demander à Image borne de retirer les fichiers FFB Plugin des jeux que linuxloader prend entièrement en charge | ✅ copies locales à la Corbeille ; demande transmise ; le propriétaire a dit oui à Image borne, qui a déplacé les fichiers des seize jeux sur les deux bornes (189 éléments sur racecab1, 180 sur racecab2) dans des dossiers `_teknoparrot_off`, avec un manifeste pour tout remettre (`C:\Racecab\Utils\ffbplugin_off.ps1 -Remettre`) |
| 99 | 24/10 | (relayée par Image borne) Axes de vJoy lus en tout-ou-rien sur la borne ; convention du propriétaire : volant sur X, accélérateur sur Y-, frein sur Z- | ✅ deux corrections : quatre liaisons par entrée au lieu de deux, et le mode pédale `_POSITIVE_SIDE` / `_NEGATIVE_SIDE` (repos au centre de l'axe) ; mesuré par vJoy (voir journal) ; nouvelle release |
| 98 | 24/10 | Le processus lancé attend la vraie fin après Test (AttractPlus attend avec `start /WAIT`) ; le menu passe de F10 à Tab, avec un réglage pour le couper ; puis une nouvelle release | ✅ le processus lancé devient un superviseur qui lance le jeu, le menu de test et le jeu à nouveau, chacun dans son processus ; Tab au lieu de F10 (jeu et sélecteur), `[Launcher] SETTINGS_MENU = false` le coupe ; essayé sur OutRun en fenêtre visible (voir journal) |
| 97 | 24/10 | (relayée par Image borne) Build de release pour les bornes | ✅ `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261024-njz3-win32.zip` (commit `c55db17`) ; changements et comportements à surveiller envoyés à Image borne (voir journal) |
| 96 | 24/10 | Trouver pourquoi Maximum Tune 3 et 3DX+ ne marchent pas en MSVC | ✅ deux causes, corrigées : le mode `rt` d'`fopen` devenait `rtb`, refusé par la bibliothèque C de MSVC (table des cinématiques jamais lue, cinématique de 0 image, caméra nulle) ; et un rival sans personnage recevait un point de départ fait de locales non initialisées. Les deux jeux se jouent en MSVC (voir journal) |
| 95 | 23/10 | Commit ? « Attends d'avoir fait tous les jeux » | ✅ les manques de chaque jeu repris : cartes de Maximum Tune 3DX+ et son jeu sans lecteur ✅, Initial D 5 ✅ (FFB, menu de test, HUD, vJoy), Dead Heat ✅ (course MinGW et MSVC, menu de test, vJoy) ; courses de Maximum Tune 3 et 3DX+ en MSVC ⚠️ (cause cherchée, pas corrigée ; MinGW les joue) ; commit à faire |
| 87 | 23/10 | Demander à windowsloader comment émuler le dongle USB de Cruis'n Blast 1.x | ✅ il ne l'a jamais étudié ; profil TeknoParrot ElfLdr2 / RawThrillsFNF sans réglage de dongle ; sa méthode : remplacer la fonction du jeu qui lit la clé |
| 86 | 23/10 | Modes d'affichage « plein écran fenêtré / borderless » et « borderless stretch » | ✅ `DISPLAY_MODE = BORDERLESS` et `BORDERLESS_STRETCH` (noms convenus avec windowsloader et la session Supermodel) ; `FULLSCREEN` devient un vrai plein écran à la résolution du jeu ; vérifié sur OutRun en fenêtre visible |
| 85 | 23/10 | OutRun 2 SP SDX : cabine deluxe, sans doute une carte qui fait bouger la cabine en plus de celle du volant | ✅ carte de mouvement sur le second port série (`/dev/ttyS1`, `EMULATE_MOTIONBOARD`) : dans la description du jeu et dans « Cartes » (`SERIAL_2_PATH`) ; le port s'ouvre sur COM10 (115200 bauds), rien n'y passe pendant l'attract |
| 84 | 23/10 | Dans la description des jeux, la carte d'E/S des contrôles et la carte moteur | ✅ dans le sélecteur et en tête de « Options du jeu » : « Controls on JVS board, Sega 837-14572, JVS 3.0 », « Motor board: Sega-like servoboard », carte de mouvement, Namco N2 et Kickback, carte d'interface Gaelco (libellés de windowsloader, traduits) |
| 83 | 23/10 | Passthrough série des cartes moteur et du JVS dans « Options du jeu », avec le choix d'un port COM ; essais sur COM10 et COM11 (paire virtuelle, partagée avec windowsloader) | ✅ partie « Cartes » (Émulée / Vraie carte sur COMn) ; ports COM sous Windows ; JVS direct porté sous Windows ; OutRun SDX : carte moteur et JVS par COM10, faux boîtier sur COM11 (pièces vues dans le jeu) |
| 82 | 23/10 | Commit et push, puis poursuivre par les jeux | ✅ `9858fbd`, `588769e` poussés ; lecteur-imprimante de cartes de Maximum Tune 3 émulé : carte neuve donnée, écrite, imprimée, enregistrée, relue à la partie suivante (journal) ; ⏳ fin de course, vJoy, 3DX+ |
| 81 | 23/10 | Page de réglages du jeu en cours dans F10, remplie selon le jeu lancé (lecteur de cartes de MT3, mode Pro de Battle Gear 4 côté windowsloader…) | ✅ onglet « Options du jeu », section `[Game.<nom court>]`, même disposition que windowsloader (convenue avec lui) ; options de MT3 et des jeux Lindbergh ; vérifié sur MT3, MinGW et MSVC |
| 80 | 23/10 | Mettre à jour l'état des jeux dans `gameData.c` | ✅ Dead Heat Riders et Nirin en `WORKING` (« Working » dans le sélecteur) ; Maximum Tune 3 reste `NOT_WORKING` |

## État des jeux (build MSVC, GPU Intel, vJoy)

| Jeu | Démarrage | Partie | Retour de force | Remarques |
|:--|:--|:--|:--|:--|
| OutRun 2 SP SDX | ✅ | ✅ | ✅ RS422 | ressort 8B, amortisseur 86, bosses FB ; couple 84 au centrage seulement |
| Hummer Extreme | ✅ | ✅ | ✅ RS422 | amortisseur 86, vibrations 85 (jusqu'à 50 Hz) |
| Sega Race TV | ✅ | ✅ partie complète | ✅ RS422 | couple 84 jusqu'à ±26 % en course, coupé en attract (80 00 00) |
| Dead Heat | ✅ | ✅ course complète | ✅ carte Kickback → trames servo | son non écouté |

Nouveaux jeux (build MinGW, mode agent, GPU NVIDIA, au 22/10 ; détail dans `BILAN.md`) :

| Jeu | Où il en est | Blocage suivant |
|:--|:--|:--|
| Initial D 4 Export FR | ✅ partie, FFB, menu de service, vJoy | `id4.elf originallllll` (ELF traduit) non essayé |
| Maximum Heat 3D | ✅ partie, FFB, menu de test, vJoy | caméra coupée (pas de `/dev/video0`) |
| Dead Heat Riders | ✅ course, nitro, FFB (carte Kickback), menu de test et test des commandes, vJoy | démarrage d'environ 70 s (contrôle du réseau, caméra absente) |
| Nirin | ✅ course (MinGW et MSVC), menu de test, test des commandes, vJoy | pas de moteur de guidon ; son non écouté |
| Wangan Midnight Maximum Tune 3 | ✅ course (MinGW et MSVC), FFB (carte Kickback), menu de test, SWITCH TEST, son (capture WAV), cartes, vJoy | son non écouté |
| Ring Riders | ✅ course, FFB, menu TEST (GPU NVIDIA) | son émulé ; joué par vJoy |
| Tokyo Cop | ✅ poursuite, FFB, menu de test | son émulé ; joué par vJoy |
| Gaelco Championship Tuning Race | ✅ course, boîte manuelle, FFB, menu Test | son émulé ; joué par vJoy |

## Journal de développement

### Release 3.0.13 du 28/10 (demande 129)
- Poussé jusqu'à `e961a3e` (la fusion avec upstream et `--seed-dir` l'avaient été par la session `--seed-dir`, en
  `55e3be6`).
- Faite par `release.py`, passé en 3.0.13 (la version d'upstream depuis la fusion), MinGW recompilé de zéro depuis
  `e961a3e` exactement : tout ce que portait la 3.0.12 du 28/10, plus la fusion avec upstream 3.0.13 (VF5, alignement
  de pile, GSEvo, pbuffers en FBO, libsegaapi complète, shaders Race TV et Initial D) et le correctif d'OutRun qui va
  avec, `--seed-dir` / `[Paths] SEED_DIR` (amorçage du dossier d'écriture), `--list-games`.
- `emulateurs/releases/linuxloader-v3.0.13-ffbedition-20261028-njz3-win32.zip`, 18,3 Mo ; SHA-256 du zip
  `4bd295b8f27b978889c914b34e600e1b5891d4aca54bbbd557565a435a5f5166`, de `linuxloader.exe`
  `f631c9e2f863cd0bfeda69a645955d56a9fc6fd9ea351df5a75f5f5a17702906` ; `ll-deps` identique à la 3.0.12.
- Testée depuis le zip extrait dans un dossier vierge : `--version` → « Build: linuxloader-v3.0.13-ffbedition-20261028-njz3
  FFB Edition » ; `--help` montre `--list-games` et `--seed-dir` ; `--list-games` liste les 91 jeux ; `--create
  controls` écrit les pédales du BFF ; OutRun 2 en mode agent caché avec cet exe jusqu'en course (retour de force en
  virage). Les autres jeux ne sont pas relancés depuis la fusion avec upstream (pas de copie locale) : à essayer sur les
  bornes. Pas installée (Image borne prévenue).

### Liste des jeux supportés, `--list-games` (demande 128)
- `linuxloader --list-games` : un tableau, un jeu par ligne, colonnes NOM COURT (le nom de rom, celui du dossier
  d'écriture et de `[Game.<nom court>]`), VERSION (le numéro DVP, ou la version Namco : `US DRIVE`, `NRN100`...),
  EXÉCUTABLE (le fichier que le dossier du jeu contient, celui du mode test pour une entrée de test : `JenTest`,
  `hod4testM.elf`...), CRC32, NOM. `--list-games csv` : la même chose en CSV (`short_name,version,executable,crc32,name`),
  pour les scripts.
- Le CRC32 est celui du fichier entier de l'exécutable d'un dump propre : la liste `cleanElfCRC32` de `mainShared.c`
  n'avait le DVP et le nom du fichier qu'en commentaire ; elle est devenue une table (`cleanElfs` : CRC, DVP,
  exécutable, note), dont l'avertissement « not Clean » se sert comme avant. Plusieurs CRC quand le jeu en a eu
  plusieurs (VF5 Final Showdown Rev B), « - » quand aucun n'est connu (Namco, Gaelco, segaboot, ID4 Rev Z...).
- Indicatif seulement, à la demande du propriétaire : le loader reconnaît un jeu par un CRC partiel de son code
  chargé (0x4000 octets), pas par celui du fichier ; un exécutable patché (TeknoParrot) n'est pas refusé, il reçoit
  seulement l'avertissement. Vérifié : OutRun (ELF des bornes) démarre comme avant, un avertissement.
- L'exécutable de chaque jeu : table `gameElfs` (entrée de `gameData.c` → fichier), tirée des commentaires de la liste
  propre (le fichier de test pour les entrées `_TEST`), complétée pour les jeux qui n'y sont pas, d'après `BILAN.md` :
  `a.elf` (Dead Heat, Maximum Heat 3D, Dead Heat Riders), `exec/a.elf` (Nirin), `main` (Maximum Tune 3 et 3DX+), `car`
  (Gaelco Tuning), `gameport` (Ring Riders, Tokyo Cop), et `id4_serverbox.elf` / `id5_serverbox.elf` pour les serveurs.
  `gameData.c` gagne seulement deux accesseurs (`gameDataCount`, `gameDataAt`), pour garder ses lignes comme upstream.
- MinGW et MSVC ; `--help` mentionne l'option.

### Dossier de modèles du dossier d'écriture (demande 127)
- Demande du propriétaire relayée par la session « Image borne (Berstett) ». Avec `--write-dir`, un fichier du jeu
  était recopié dans `WRITE_DIR\<jeu>` depuis le dossier du jeu à sa première ouverture en écriture. Mais le dossier
  du jeu est commun au solo et au link : les NVRAM propres à une cabine link (LINK ID ou siège réglés au menu de test)
  ne peuvent pas y vivre. Elles sont gardées en modèles à côté de l'exe, rangées comme le dossier d'écriture
  (`saves_link_cab1\outrun2-rev-a\eeprom.bin`, `sram.bin`, `rankingdata\…`, `hummer-ext\…`, `id4-exp-rev-d\…`,
  `id5-exp-20\…`). Jusqu'ici le lanceur link les recopiait par xcopy ; le propriétaire veut que l'émulateur le fasse,
  comme les autres, Build-Config posant le modèle de la cabine dans un dossier en place.
- `--seed-dir PATH` (ou `--seed-dir=PATH`), à défaut `[Paths] SEED_DIR` du même `linuxloader.ini` que `WRITE_DIR`
  (celui à côté du chargeur, avant la copie dans le dossier d'écriture) ; relatif au dossier du chargeur, comme
  `WRITE_DIR`, ou chemin complet. Sans dossier d'écriture : un avertissement, et rien d'autre. Dossier introuvable :
  un avertissement, les fichiers viennent du dossier du jeu comme avant.
- `writeDirEnterGame`, une fois `WRITE_DIR\<jeu>` fait et avant que le jeu ne lise ou ne liste quoi que ce soit :
  `<seed>\<jeu>` (le même nom court que `WRITE_DIR\<jeu>`) est recopié récursivement dans `WRITE_DIR\<jeu>` ; un
  fichier déjà présent n'est jamais remplacé. Une ligne par fichier, « write folder: eeprom.bin seeded from
  <chemin> », puis « seed folder: N file(s) of <dossier> seeded », ou « no <dossier>, nothing to seed » (journal
  `info` : `LL_LOG=info` pour les voir). Recopier tout au lancement plutôt qu'à chaque ouverture : les lectures (le jeu
  lit son eeprom avant de l'écrire) et les listages de dossiers (`rankingdata`) voient le modèle sans autre changement.
- Aussi : `[Paths] SEED_DIR` (vide) avec son commentaire dans le `linuxloader.ini` généré ; `--write-dir` et
  `--seed-dir` dans `--help` (Windows). Linux : sans effet, comme `--write-dir`.
- Essais avec OutRun 2 (`games\outrun2spdx`), mode agent caché, MinGW puis MSVC, `LL_LOG=info` ; modèle :
  `seed\outrun2-rev-a\eeprom.bin` (celui du jeu, un octet changé) et `rankingdata\seedtest.txt` :
  - dossier d'écriture vide, `--seed-dir` : les deux fichiers amorcés, le journal donne leur provenance ; l'eeprom du
    dossier d'écriture est celle du modèle ;
  - relancé, le modèle changé : 0 fichier amorcé, eeprom et témoin inchangés ;
  - sans `--seed-dir`, dossier d'écriture vide : aucune ligne d'amorçage, l'eeprom vient du dossier du jeu, comme
    avant ;
  - `[Paths] WRITE_DIR` et `SEED_DIR = "saves"` dans le `linuxloader.ini` à côté du chargeur : `saves` résolu dans
    son dossier, les deux fichiers amorcés.
- Sur la branche `dev/seed-dir` (worktree `git_linuxloader_seeddir`, partie de `dev/ffbedition` à `baf2d51`, la
  fusion avec upstream non poussée) ; non poussée, pas encore dans une release.

### Merge avec upstream (demande 126)
- Upstream : `lindbergh-loader/linuxloader` (le parent du fork njz3), remote `upstream` ajouté. Un seul commit depuis
  notre base `9aa6e3e` : `4ffa095` du 28/10 (dkeruza-neo), 68 fichiers, +8 784 lignes, changelog 3.0.13 : Virtua
  Fighter 5 sous Windows et en réseau sous Linux, alignement de la pile (distributions récentes, MinGW), GSEvo rendu à
  la résolution choisie, pbuffers en FBO sous Windows (`pbufferEmu.c`), émulation complète de libsegaapi (effets),
  éclairage et flou de Race TV, shader des Initial D, marqueur de Harley, fenêtre de Let's Go Jungle redimensionnable,
  calibration du pistolet de Primeval Hunt gardée, un paquet Mesa figé (`libs/assets/mesa-kisak-pinned.tar`, 24 Mo).
- Fusion dans `dev/ffbedition` (`0b3f5a9`), 15 fichiers en conflit :
  - `glHooks.cpp` : tout le fichier en conflit (nos fins de ligne LF) ; le nôtre gardé, les trois changements
    d'upstream reportés (`bridgeglCopyTexSubImage2D`, `gsevoBleedAlpha` après les textures compressées) ;
  - `filesystemBridge` : `readdir_r` (upstream) et notre `readdir64_r` passent par `bridgeReaddir`, qui liste aussi
    le dossier d'écriture ; le `readdir64_r` d'upstream (lecture Windows directe, entrée remise à zéro en entier)
    retiré ; `linux_dirent64` : la nôtre, compactée comme celle de glibc i386 (276 octets) ;
  - `libcBridge.cpp` : `qsort`, `bsearch` et les gestionnaires de signaux du jeu passent par les trampolines
    d'upstream en MinGW (`force_align_arg_pointer`), par les nôtres (`elfCall`) en MSVC, qui ignore cet attribut ;
    `setlocale` : le nôtre (la locale du processus reste « C »), jamais NULL désormais, pour les 13 catégories ;
    nos traductions de `system()` gardées ;
  - `mathBridge` : notre `div` (MSVC aussi), `difftime`, `ceil`, `floor`, `strto*` ; d'upstream `fabs`, `nextafter`,
    `trunc` ; son `sincos` (`::sincos`, absent de MSVC) retiré au profit du nôtre ;
  - `symbolResolver.cpp` : notre façon de nommer `strchr` et consorts (MinGW et MSVC) ;
  - `glxBridge.c` : les pbuffers d'upstream (`pbufferEmu.c`), notre mémoire NV (jeux Gaelco) gardée ;
  - `sdlCalls.c` : plus de taille maximale (Let's Go Jungle, Primeval Hunt), bascule plein écran pour tous les jeux
    (upstream), avec notre taille de fenêtre au rapport d'aspect et notre taille minimale hors écran ;
  - `libsegaapi.c` : la nouvelle d'upstream, avec notre pause (Tab) et le silence du mode agent (voix maître) ;
  - `CMakeLists.txt` : nos branches MSVC et MinGW ; en MinGW les drapeaux d'upstream (`-mincoming-stack-boundary=2`,
    `-mpreferred-stack-boundary=4`, pile de 4 Mo engagée, `iphlpapi`) ; en MSVC `/STACK:4194304,4194304` aussi ;
  - `filesystemShared.c` : nos ioctl réseau Windows gardés ; macros `LINUX_FD_SET/CLR/ZERO_BIT` remises (upstream
    ne garde que `ISSET`) ; `TCFLSH` va toujours à la servoboard émulée (upstream ne passe plus que FIONREAD aux
    ports série) ; `blitStretching.h`, `eepromSettings.c`, `shaderCache.c` (upstream n'écrit plus les shaders
    patchés) : les deux côtés, ou celui d'upstream.
- Après la fusion : `hooks` est un `int` (upstream), `driveBoard.c` suit ; l'option de jeu « Rendu avec Zink » de
  Primeval Hunt retirée avec le réglage (upstream n'en a plus besoin) ; `readTsc` (HOD4) et une somme sur `void *`
  rendus compilables par MSVC.
- **Plantage d'OutRun au départ de la course** (MinGW et MSVC), trouvé sous cdb : `call [eax-0x6f6f6f70]` en
  `0x08105334`. Upstream a remplacé le saut `e9 1f 00 00 00` en `0x08105317` (qui enjambait le contrôle de la vue
  caméra du retour de force) par un octet `ff` en `0x08105334`, écrit pour l'ELF propre. L'ELF des bornes
  (TeknoParrot, « not clean ») a déjà ce contrôle remplacé par des NOP de `0x08105317` à `0x0810533a` : l'octet en
  faisait un appel. Il n'est plus écrit que sur le code d'origine (pas de `0x90` à cette adresse), pour les deux
  versions d'OutRun (`0x08105334`, `0x081053a0`). À signaler à upstream.
- Vérifié : MinGW et MSVC compilent ; OutRun (mode agent caché) jusqu'en course, rendu complet, retour de force en
  virage (forces constantes jusqu'à ±0,25, rappel, amortisseur), démo et menus comme avant ; banc d'essai de la
  servoboard identique. Le son n'est pas écouté (nouvelle libsegaapi). Les autres jeux ne sont pas relancés (pas de
  copie locale) : Race TV, Initial D et Hummer sont touchés par les correctifs d'upstream. Non poussé.
- Prochaine release : 3.0.13 (`release.py`).

### Release du 28/10 (demande 125)
- Rien de plus à commiter : `origin/dev/ffbedition` était déjà à `8752305` (réponses de la servoboard, pédales).
- Faite par `release.py`, la première du jour (sans suffixe), MinGW recompilé de zéro depuis `8752305` exactement :
  tout ce que portait la 20261027, plus les réponses de la servoboard d'après DRIVEBOARD.md, la capture des pédales
  d'après leur position de repos et les pédales du BFF par défaut (demandes 121 à 124).
- `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261028-njz3-win32.zip`, 18,3 Mo ; SHA-256 du zip
  `fde4d0e228850b011cbbf98d6547978d60d745d9afedc54f36271eeb3e6908f8`, de `linuxloader.exe`
  `f8e79576d526260eaaa5455c73aa309ace49e07a856770a76aabefc0d3c80646` ; `ll-deps` identique à la 20261027.
- Testée depuis le zip extrait dans un dossier vierge : `--version` → « Build: linuxloader-v3.0.12-ffbedition-20261028-njz3
  FFB Edition » ; `--create controls` écrit `P1_Gas = GC0_AXIS_RIGHTTRIGGER, JOY0_AXIS_1_NEGATIVE_SIDE` et
  `P1_Brake = GC0_AXIS_LEFTTRIGGER, JOY0_AXIS_2_NEGATIVE_SIDE` ; OutRun 2 en mode agent caché avec cet exe : la démo,
  `FF`, `81`, `FC` puis le centrage, 601 forces constantes, comme le build de développement. Pas installée sur les
  bornes (Image borne prévenue).

### Pédales : les axes analogiques dans le menu de test (demande 124)
- D'autres utilisateurs disent que leurs pédales ne marchent pas. Vérifié dans INPUT TEST d'OutRun 2 (programme de
  test, `-t`).
- Le jeu : par l'agent (valeurs écrites directement), accélérateur et frein vont de `00H` à `FFH`, `7FH` à
  mi-course ; volant `00H`, `7FH`, `FFH`. Le jeu et la carte JVS émulée lisent donc bien leurs axes. Le mode agent ne
  lit pas les manettes : il ne dit rien du chemin des vraies pédales.
- Le chemin des pédales (SDL, `controls.ini`), essayé en lancement normal avec vJoy (un joystick, comme un volant),
  dans un dossier de travail à part, le `controls.ini` de `build-mingw` intact :
  - liaisons comme la capture de Tab > Contrôles les écrit (`JOY0_AXIS_n`, sans suffixe) :
    - pédale au repos en bas de son axe : `00H` au repos, `FFH` enfoncée, juste ;
    - pédale au repos en haut de son axe : `FFH` au repos, `00H` enfoncée : l'accélérateur à fond sans y toucher ;
    - pédale au repos au milieu : `7FH` au repos, `00H` enfoncée ;
    - volant juste ;
  - liaisons par défaut (pas de `controls.ini`, cas de la release) : volant sur l'axe 0 et frein sur l'axe 2 justes,
    accélérateur sur l'axe 5 : vJoy n'a que 4 axes, il reste à `00H`. Bien des volants n'ont pas d'axe 5, ou pas
    l'accélérateur dessus.
- La cause côté loader : `controlsCaptureEvent` (`guiControls.cpp`) écrit l'axe entier pour une ligne analogique,
  sans regarder où la pédale reposait au début de la capture (`g_capture.joyAxes`, déjà relevé) ; `_INVERTED`,
  `_POSITIVE_SIDE` et `_NEGATIVE_SIDE` ne s'obtiennent qu'à la main (guide des contrôles, demande 118).
- Disposition des bornes, précisée par le propriétaire : le BFF présente l'accélérateur sur Y− et le frein sur Z−,
  chacun de la mi-course (relâché) à 0 (enfoncé) ; le loader doit en faire une pédale de `00H` à `FFH`.
- Corrections :
  - capture (`pedalSuffix`, `guiControls.cpp`) : pour Accélérateur et Frein, le mode vient de la position de repos
    relevée au clic sur Lier : en bas de l'axe l'axe entier, en haut `_INVERTED`, au milieu `_POSITIVE_SIDE` ou
    `_NEGATIVE_SIDE` selon le sens où la pédale part ; sur une manette, les gâchettes restent telles quelles ;
  - barres de la page : la pédale telle que sa liaison la lit (`pedalLevel`), et non plus l'axe brut (une pédale
    inversée bien liée y restait à 100 %, une gâchette à 50 %) ;
  - liaisons par défaut d'un joystick (`controlIniGen.c`) : accélérateur `JOY0_AXIS_1_NEGATIVE_SIDE`, frein
    `JOY0_AXIS_2_NEGATIVE_SIDE` (la disposition du BFF), au lieu des axes 5 et 2 entiers ; un `controls.ini` neuf
    (`--create controls`) les écrit ;
  - guide des contrôles, section Pedals, réécrit.
- Vérifié avec vJoy, lancement normal du programme de test d'OutRun, MinGW (MSVC compilé, pas lancé) :
  - `controls.ini` neuf, disposition du BFF : `00H` au repos, `7FH` à mi-chemin, `FFH` à fond, pour chaque pédale
    et les deux ensemble ; volant `00H`, `7FH`, `FFH` ;
  - capture dans Tab > Contrôles (Lier, puis la pédale vJoy enfoncée et relâchée), pilotée par clics : Y de la
    mi-course à 0 donne « Axe 1 - (pédale) » (`JOY0_AXIS_1_NEGATIVE_SIDE`), Z « Axe 2 - (pédale) » ; barres vides au
    repos, pleines enfoncées ; enregistré, INPUT TEST `00H`, `7FH`, `FFH` ;
  - Rz au repos en haut donne `JOY0_AXIS_3_INVERTED`, Y au repos en bas `JOY0_AXIS_1` ; INPUT TEST `00H`, `7FH`,
    `FFH` dans les deux cas ;
  - une liaison changée ne relit son axe qu'au premier mouvement : un accélérateur que l'ancienne liaison tenait
    enfoncé le reste jusque-là (vu une fois pendant l'essai).

### OutRun 2 avec les nouvelles réponses de la servoboard (demande 123)
- Copie d'OutRun 2 SP SDX depuis racecab1 (`games/outrun2spdx`, 1,49 Go, sans fichiers TeknoParrot), annoncée.
- Démarrage, mode agent caché, MinGW : « Initializing the drive board. Please DO NOT touch the steering wheel. »
  jusqu'à 10 s, la démo à 25 s, pas de « DRIVE BOARD FAULT ». Le jeu (avec les correctifs du loader pour la carte
  émulée) envoie `FF`, `81 30 7F`, `FC 00 20` sans interroger la carte, 10 s de couples alternés (centrage), puis sa
  table de paramètres (`9D`/`9E`) ; le modèle en fait 601 forces constantes.
- Partie : pièce, Start, les menus passent à la fin de leur compte à rebours (l'accélérateur de l'agent ne valide pas
  les choix, cause non cherchée : il va pourtant de `00H` à `FFH` dans INPUT TEST), course atteinte.
- Menu de test, DRIVE BOARD TEST : là, le jeu interroge la carte, `FD` pendant 4 s (images 839 à 1083), le temps de
  la recherche d'index, puis `FA 00 1F`, `83 40 04`, `86 01 02`, `88 00 04` et le centrage, comme le relevé de la
  borne ; « DRIVE BOARD TEST MENU » ensuite (TEST HANDLE, STOP MOTOR, ROLL RIGHT, ROLL LEFT, MOTOR POWER 80 %).
- Hummer, R-Tuned et Race TV pas relancés.

### Test du retour de force d'Initial D 5 (demande 122)
- Copie d'Initial D 5 Export 2.0 depuis racecab1 (`games/id5`, 3,83 Go, déjà sans fichiers TeknoParrot), annoncée à
  Image borne et à windowsloader, avec l'accord du propriétaire.
- Le test (« GAME TEST MODE » > FORCE FEEDBACK, `TCL_MENU_FORCE_FEEDBACK_ID5`) affiche « CHECKING » et des points,
  et appelle à chaque image `cFeedbackSCI::initPolling(temps écoulé)` jusqu'à ce qu'il rende vrai :
  - `initPolling` avance à l'horloge, comme les relevés de la borne : `FF` à 1 s, `FA 00 10` à 2 s, `FD` jusqu'à 3 s,
    `81`/`82` à 4 s, `FC` à 5 s, `FD` à 15 s, `80 00 01` à 16 s, le centrage, `F0` à 21 s, puis à 22 s `80 00 00`,
    `FA 00 1F`, `86`, `85`, et vrai ;
  - il ne lit les réponses (`_sSCI::recv` : `select` sans attente puis `read` de 4 octets) que pour y chercher un
    octet de `0xC0` ou plus, un écho de ses commandes : il recommence alors depuis 20 s et rend vrai à 41 s ;
  - le reste du test n'envoie que `80` et `84` ; `GetRecv_` n'est appelé nulle part.
  Sans réponse, comme dans linuxloader (port jamais ouvert, trames prises à `_sSCI::send`), le test ne bloque donc
  pas, contrairement à Initial D Zero sous windowsloader. Les réponses de `driveBoard.c` n'y changeraient rien (`B0`,
  `90`, aucun octet de `0xC0`) : rien à brancher.
- Essai en mode agent caché (`-t`, vJoy caché), MinGW puis MSVC : « CHECKING.... » et le menu grisé, la séquence
  d'init entière envoyée (de `80 00 00` et `FF` jusqu'à `FA 00 1F`, `86`, `85`), le menu actif environ 21 s après
  l'entrée, curseur sur EXIT. CENTER envoie `80 00 01` et `84 01 00` (couple nul), RIGHT `84 00 68`, LEFT `84 01 18` ;
  le modèle de la carte en fait −0,095 et +0,095 (+ pousse le volant à gauche), ON affiché à chaque fois.
- Initial D 4 pas essayé (pas de copie) : même carte `cFeedbackSCI`, son menu de test est une autre classe.
- La copie locale est repartie à la Corbeille après l'essai.

### Réponses de la servoboard Sega (demande 121)
- Signalé par la session « Driveboard émulation et force feedback (Berstett) », à la demande du propriétaire :
  `driveBoard.c` répondait toujours un octet, alors qu'après `FA 00 10` la vraie carte répond 4 octets à chaque
  trame. C'est ce qui bloquait sur « CHECKING.... » le test du retour de force d'Initial D Zero sous windowsloader,
  corrigé là-bas aujourd'hui. Le propriétaire : DRIVEBOARD.md (BackForceFeeder, section « Sega servoboard ») est
  exact, vérifié sur le matériel ; il suffit pour corriger, sans lire les jeux.
- Écarts trouvés entre l'émulation et le document :
  - un seul octet en toute circonstance ; `FA` ne choisissait rien (réponse `00`), ni statut long `90`/`B0`, ni
    position du codeur `94`/`B4` ;
  - `FF` répondait `00` et laissait la carte « prête » : les `FD` suivants répondaient `00` avant toute recherche
    d'index ; la vraie carte répond à `FF` dans son état d'avant, puis repasse en statuts courts, non initialisée
    (`11`, `B0 00 01`, `B4`) ;
  - `81` (force maximale) répondait `11` comme un reset ;
  - `FC` répondait `44` la première fois seulement, et le `FD` suivant `00` : la recherche était instantanée ; la vraie
    dure 4,03 à 4,06 s (cinq relevés : OutRun, Harley, Hummer, R-Tuned, Race TV), `44` ou `B0 00 04` pendant ;
  - les autres commandes (`82`, `85`, `87`, `88`, `8B`, `9D`, `F0`...) répétaient la réponse précédente ;
  - `FIONREAD` annonçait 1 octet ; `TCFLSH` écrivait à l'adresse que portait son argument (un entier) ;
  - le décodage des effets (`forceFeedback.c`) suivait déjà le document : `80` et son mode court de 240 ms, `81`,
    `83`, `84` centré, `85` fréquence en demi-hertz puis amplitude, `86` (0 gardé comme 1), `87`, `88`, `8B`,
    `9D`/`9E`, `FB`, `FF` aux valeurs par défaut. Ce sont les corrections que windowsloader vient de faire chez lui.
- `driveBoard.c` maintenant :
  - une réponse par commande, dans une file lue par `read` dans la limite de ce que le jeu demande ; `FIONREAD`
    donne ce qui attend ; `tcflush` (crochet Linux, pont Windows) et l'ioctl `TCFLSH` la vident ;
  - le mode de réponse de `FA 00 xx`, valable pour la réponse au `FA` lui-même : `1F` un octet (`00` prête, `11`
    non initialisée, `44` recherche d'index) ; `10` `[90|B0] 00 état somme` (état `01` reset, `04` recherche, `00`
    prête) ; `14` `[94|B4]` et la position du codeur, au centre `0x2000` ; les adresses `00` à `0F` relisent le
    dernier `80` à `8F` (valeurs à froid du document), `1D` le dernier `9D` ;
  - `FF` répond dans l'état d'avant, puis remet la carte à froid ; `FC` lance une recherche de 4,05 s ;
  - file vide : la dernière réponse à nouveau, entière (le correctif d'OutRun, `select()` toujours prêt) ; au passage
    en mode long, les statuts courts non lus sont retirés, comme dans windowsloader ;
  - seules les commandes du premier port (`SERIAL0`) reçoivent une réponse, le seul port lu ; le pilotage de
    l'analogique du volant par les couples d'avant le `80` est inchangé.
- Vérification : banc d'essai (`driveBoard.c` seul, horloge des relevés) qui rejoue les commandes des relevés COMDump
  d'une vraie borne (`BackForceFeeder_master/utils/dump/dumpsrsversrs`) et compare chaque réponse, une réponse par
  commande dans chaque relevé, à partir du premier `FF` (avant, la carte gardait l'état d'une partie précédente) :
  - Initial D 4 Export et Initial D 5 (menu de test et partie), Hummer (deux relevés), OutRun (menu de test),
    Harley (mauvaise init) : toutes les réponses identiques ;
  - OutRun (partie), R-Tuned (menu de test) : une réponse de la fin de recherche (`44` contre `00`), Race TV : trois
    (`B4` contre `94`, il interroge toutes les 15 ms) ; la vraie recherche varie de 30 ms d'un relevé à l'autre ;
  - Harley : l'adresse `00` relue `01 00` en partie, alors que le dernier `80` était `01 01`, sans doute le mode court
    qui lâche ; non émulé (aucun de nos jeux ne la lit) ;
  - R-Tuned (partie) : le relevé a perdu trois réponses (3 734 commandes, 3 731 réponses), d'où un décalage ;
  - cas de lecture : octet par octet en mode long, file vide, `tcflush`, `FIONREAD`, file pleine (16 réponses) : bons.
- Initial D 4 et 5 ne passent pas par `driveBoard.c` : leurs trames sont prises à `_sSCI::send` (`idFeedbackSend`,
  `patch.c`) et leur port n'est jamais ouvert, ils ne reçoivent donc aucune réponse. Le test du retour de force
  d'Initial D 5 n'en a pas besoin (demande 122).
- Les jeux de `driveBoard.c` (OutRun 2 SP, Hummer, R-Tuned, Race TV) n'ont pas été relancés : aucune copie locale.
  Ce qui change pour eux : `FF` répond `11` (Race TV `B4`) comme une carte froide, et la recherche d'index dure 4 s au
  lieu d'être instantanée ; Race TV reçoit maintenant des trames de 4 octets (mode `14`), comme sur la borne.

### Release du 27/10 (demande 120)
- Les quatre commits du choix du périphérique de retour de force poussés (`e543178..2aab762`).
- Faite par `release.py`, la première du jour (sans suffixe), MinGW recompilé de zéro depuis `2aab762` exactement :
  tout ce que portait la 20261026-5, plus `FFB_DEVICE` (liste dans Contrôles, changement en direct, repli sur
  Automatique et reprise au branchement).
- `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261027-njz3-win32.zip`, 18,3 Mo ; SHA-256 du zip
  `cf698da467a2753045c1c6dbc6c76c783ef01e8fae5fa233cac75c0afcf1484a`, de `linuxloader.exe`
  `9b926eecb91af497f732c0d1b2bb1f92b0637d83d3ebc8f3394cfd29da5ff2c7` ; `ll-deps` identique à la 20261026-5.
- Testée depuis le zip extrait : `--version` → « Build: linuxloader-v3.0.12-ffbedition-20261027-njz3 FFB Edition »,
  le nom exact du zip ; `--create config` écrit `FFB_DEVICE = AUTO` dans `[ForceFeedback]`. Pas lancée sur un jeu
  (aucune copie locale), ni installée sur les bornes.

### Choix du périphérique de retour de force (demande 119)
- Demande du propriétaire relayée par la session « Driveboard émulation et force feedback (Berstett) », la même
  dans les autres émulateurs : sur une borne, le volant peut être lu sur un périphérique et le moteur branché sur un
  autre. Jusqu'ici les effets allaient au périphérique du joystick 0 (ou au premier périphérique à retour de force
  trouvé seul, en repli).
- `[ForceFeedback] FFB_DEVICE` : `AUTO` (par défaut, ou vide) garde ce choix ; sinon le nom du périphérique tel que
  SDL le donne (`SDL_GetHapticNameForID`, le nom produit DirectInput sous Windows, celui d'evdev sous Linux), comparé
  sans la casse ni les espaces autour ; le premier branché qui porte ce nom et sait jouer une force constante ou une
  vibration est pris. Un nom absent : ligne WARN et choix automatique. Deux périphériques du même nom ne se
  distinguent pas (le premier). `--set ForceFeedback.FFB_DEVICE=...` le règle pour un lancement.
- `forceFeedback.c` : un seul périphérique cible (`ffbHaptic`) au lieu de `sdlJoysticks.haptics[0]`. Le modèle de la
  carte suit maintenant les trames du jeu quel que soit le périphérique, y compris une manette à vibration, pour
  qu'un volant choisi en cours de partie joue l'état courant. Changement (`sdlFfbSelect`, appelé par la page
  Contrôles) : sous un verrou que le fil FFB prend aussi pour parler au périphérique, effets arrêtés et détruits sur
  l'ancien, créés sur le nouveau, puis le fil FFB renvoie l'état de la carte (en pause tant que les paramètres sont
  ouverts, comme avant). Le fil du jeu ne prend jamais ce verrou.
- Tab > Contrôles > Retour de force : liste « Périphérique » (Automatique, puis les périphériques à retour de force
  branchés ; un nom enregistré absent apparaît grisé « (non connecté) »), en jeu la ligne « Forces envoyées à : … » ;
  « (retour de force) » marque dans la liste des périphériques la manette qui reçoit les effets.
- Essais sur vJoy avec un petit programme qui lie `forceFeedback.c` seul (`ffb_device_test.c`, dans le scratchpad) :
  `FFB_DEVICE = "  vjoy DEVICE "` ouvre « vJoy Device » en mode volant, couple reçu par vJoy (+5039 puis -5039 sur
  ±10000 pour 84 à ±0x40), « vJoy » seul ne correspond pas, un nom absent retombe sur le choix automatique. Première
  manette sans retour de force simulée (Automatique = aucun périphérique) : passer à Automatique arrête et libère les
  quatre effets sur vJoy ; revenir à « VJOY device » les recrée et rejoue l'état de la carte, dont le couple à droite
  reçu entre-temps (-5039). Compilé MinGW et MSVC.
- Page vue dans le sélecteur (MinGW, fenêtre visible) : « Périphérique » propose Automatique et « vJoy Device »,
  le choix est enregistré par Sauvegarder (`FFB_DEVICE = vJoy Device`, ligne retirée ensuite du `linuxloader.ini` de
  `build-mingw`). Le nom de retour de force n'est pas celui du joystick : vJoy s'appelle « Shaul Eizikovich vJoy -
  Virtual Joystick » dans la liste des périphériques et « vJoy Device » en retour de force. Pas essayé dans un jeu,
  ni avec deux vrais périphériques à retour de force (un seul ici).
- Règle du propriétaire pour tous les émulateurs, transmise par la session Flycast (« Multi-Board en thread et non
  process ») : Automatique tant que le périphérique choisi est absent, puis ce périphérique dès qu'il est branché.
  Ajouté : une surveillance des événements SDL (`SDL_AddEventWatch`, sur le fil qui pompe les événements, où SDL met
  aussi à jour sa liste des périphériques à retour de force, avant d'envoyer l'événement) ; à chaque manette branchée
  ou débranchée, le périphérique nommé débranché laisse les effets au choix automatique (« FFB: "…" unplugged »), et
  un périphérique nommé absent jusque-là les reprend dès qu'il apparaît (« FFB: "…" plugged in »), avec l'état
  courant de la carte. Essayé avec le même petit programme et des événements de manette simulés : nom absent, puis
  « vJoy Device » qui « apparaît » à l'événement de branchement (effets créés, couple courant -5039 rejoué), un
  débranchement d'une autre manette ne change rien. Un vrai débranchement n'est pas essayé (il faudrait toucher à
  la configuration du pilote vJoy, partagé avec les autres sessions).

### Lampes d'OutRun 2, guide des pédales (demande 118)
- Retour d'un utilisateur d'OutRun 2 transmis par le propriétaire : pédales relâchées qui accélèrent et freinent,
  pas de retour de force, lampe Start muette alors que View et le bandeau leader marchent comme sous TeknoParrot.
- Lampes, lues dans le jeu : `CabinetLampOn` et `CabinetLampBlink` rangent les lampes dans un octet envoyé par
  `ReadIO` à la carte JVS (`amJvspReqGeneralOutput1`), donc dans `RawLamps` : Start 0x80 (`lamp0`, sortie JVS 1,
  `DispPushStart`, seulement pendant « PUSH START »), changement de vue 0x08 (`lamp4`, `CheckViewChange`), lampes des
  deux pilotes 0x20 et 0x04 (`lamp2`, `lamp5`, qui clignotent ensemble à « Select the main driver »). Nommées dans la
  liste des sorties (`gameLamps`). Celles de Hummer (`clMenuLED`, bits 0x08, 0x10, 0x20, 0x04) restent génériques :
  leur rôle à l'écran n'est pas connu.
- Guide des contrôles : `_POSITIVE_SIDE` / `_NEGATIVE_SIDE` (pédale qui repose au milieu de son axe) et une section
  « Pedals » : regarder les barres de la page Contrôles au repos (50 % : `_SIDE`, 100 % : `_INVERTED`), les flèches
  du clavier liées à `P1_Gas_Digital` et `P1_Brake_Digital` qu'un programme peut tenir enfoncées (cas des bornes, 25/10),
  puis la calibration du jeu (OutRun 2 : CALIBRATION SETTING).
- Retour de force : en émulé, vers le joystick 0 seulement (ligne « FFB: Haptic opened from joystick 0 »),
  OutRun 2 n'envoyant surtout que ressort, amortissement et secousses ; avec les vraies cartes de la borne, « Vraie
  carte sur COMx » pour la carte de pilotage dans Options du jeu > Cartes. Pas vérifié chez l'utilisateur ; la liste
  des sorties avec les nouveaux noms n'est pas vérifiée à l'écran (compilée MinGW et MSVC).

### Décor blanc d'OutRun 2 avec le dossier d'écriture, Hummer en link (suite de la demande 117)
- Release `linuxloader-v3.0.12-ffbedition-20261026-2-njz3-win32.zip` faite à la demande du propriétaire depuis
  `00d0511` (adresses dans le journal des sockets) : SHA-256 du zip
  `fc225520cfba622efff70ed571a2bcf3c10041336da1e1935a83d505b455dcf5`, de `linuxloader.exe`
  `984268b92814681d19292394278c81b809be3d4ada29ef977f5643a7ac83c311`, `ll-deps` identique à la 20261026.
- Décor blanc d'OutRun 2 en course (vu par le propriétaire sur les deux bornes, avec `Jennifer`) : seuls le HUD et
  l'ombre de la voiture dessinés, « OpenGL Error … 1282 (psPredrawPsdShader:2225) » à chaque image. Reproduit ici
  (RTX 3050 et Intel : ni la carte, ni le pilote, ni `BOOST_RENDER_RES`). Un build du 21/10 et un du 24/10 rendaient
  bien… sans dossier d'écriture ; la version actuelle aussi sans lui. La cause : `CreatePresetPsdPixelShader` fait
  `opendir("./shader/fs/psd/")`, puis `chdir` dans ce dossier, et lit chaque programme par son seul nom (`stat`,
  puis `GlFragmentShader_createShaderFromFile`). Avec un dossier d'écriture, le dossier courant est
  `WRITE_DIR\<jeu>` et les chemins relatifs y étaient rattachés à sa racine : le `chdir` vers un dossier qui n'existe
  que dans le dossier du jeu échouait, aucun programme n'était lu, `glBindProgramNV` refusait les identifiants.
  Correction (`writeDirChdir`) : le dossier est fait dans `WRITE_DIR\<jeu>` et le jeu y entre ; le loader garde le
  sous-dossier courant et y rattache les chemins relatifs suivants (`..` compris), lus là puis dans le dossier du jeu.
  Vérifié : course entière avec un dossier d'écriture (une seule erreur OpenGL, comme sans). Les bornes ont un dossier
  d'écriture pour tous les jeux : OutRun 2 seul y était blanc aussi.
- Hummer en link : « Error 1303, There is an error in the LINK ID SETTING » sur les deux bornes (journal d'Image
  borne). Le jeu lance `./check_ip.sh 192.168.1.4` (ou .5) avant son link : le script fait `arping -D` et rend 0
  quand aucune autre machine n'a l'adresse. Le loader ne lançait pas ce `system()` (127, commande introuvable), d'où
  l'erreur. Il rend maintenant 0. Les `bind` de Hummer à chaque `IP_CAB` (seule celle de la borne réussit) sont
  normaux, comme sous Linux. Hummer ne demande pas eth0 : pas de ligne « Link: » pour lui non plus.
- Sega Race TV en link : CABINET ID « 1 (MAIN) » et « 2 (SUB) » (les vrais libellés) ; la SUB reste sur « Searching
  MAIN Cabinet », la MAIN sur son écran d'initialisation. Dans le journal de la MAIN, la socket d'écoute des
  recherches (`SO_BROADCAST`, `SO_REUSEADDR`) échoue à son `bind` : `CConnectSceneObj::EnterCreateMaster` la lie à
  l'adresse de broadcast que donne `SIOCGIFBRDADDR` (192.168.1.255), ce que Linux accepte (la socket reçoit alors
  les broadcasts) et Windows refuse (`WSAEADDRNOTAVAIL`, vérifié ici). Une socket UDP liée à 255.255.255.255 ou à
  une adresse en .255 est maintenant liée à toutes les adresses sur son port, ce qui lui fait recevoir ces
  broadcasts sous Windows. Pas d'essai possible ici sans Sega Race TV ni seconde machine.
- Initial D 4 Export en link : SEAT NUMBER (4e ligne, SINGLE, 1P, 2P) à 1P et 2P ; « Link: … (ID_IP_SEAT_1) » et
  « (ID_IP_SEAT_2) », eth0 lu par le jeu (« LocalHost IPAddr: c0a80104 »). Le siège 2 reçoit les 43 paquets du
  siège 1 puis tombe en « ERROR: 599 », le siège 1 n'appelle jamais `recvfrom`. Le jeu attend ses datagrammes par
  `select` (`cUDPSocket::receiveFrom`, `polling`) et le `select` du loader rendait toujours 0 pour Initial D (ou 1
  sans regarder pour les jeux à carte moteur), sans voir les sockets. Il passe maintenant leurs bits à Winsock :
  les numéros de socket de Windows dépassent 1024, la taille d'un `fd_set` Linux, et le jeu a posé le bit de la
  sienne au-delà, à `nfds - 1` : lu et réécrit là (les bits entre 1024 et lui, de la mémoire voisine, ne sont pas
  regardés). Vérifié sans régression sur OutRun 2 (course, attente du link). `poll` reste un bouchon (Initial D ne
  s'en sert que pour les fichiers).
- Release `linuxloader-v3.0.12-ffbedition-20261026-3-njz3-win32.zip` (faite par `release.py` depuis `098c718`, à la
  demande du propriétaire) : les quatre corrections (décor blanc d'OutRun 2, `check_ip.sh` de Hummer, `bind` sur une
  adresse de broadcast pour Sega Race TV, `select` sur les sockets pour Initial D). SHA-256 du zip
  `fd32935215b7134eb69866d16a90b38dbe95b675f26cac0e686c8e422ea476a0`, de `linuxloader.exe`
  `5644b0975eedffbb026cabe33ece32d633b6927abd0f47d1345605c5de243e8e` ; `ll-deps` identique. Vérifiée depuis le zip
  extrait : `--version` et une course d'OutRun 2 entière avec un dossier d'écriture.
- Posée sur les deux bornes par Image borne (accord du propriétaire). OutRun 2 : course link à deux avec le décor
  complet (racecab1 1er, racecab2 2e). Hummer : plus d'Error 1303, le link passe (paquets de 18 octets entre .4 et
  .5), puis racecab1 plante (0xC0000005) juste après avoir rouvert ses sockets vers les cabines 3 et 4. Cause :
  `nsFileDescriptorUtil::lCheckRead(fd)` pose le bit de la socket dans un `fd_set` de 128 octets sur sa pile
  (`bts`) ; avec un numéro de socket Windows (2404), le bit tombe 300 octets plus loin, dans le cadre de
  l'appelant. Sous Linux une socket a un petit numéro. Correction : les jeux reçoivent des descripteurs de
  socket de 900 à 1023, chacun tenant lieu d'une socket Winsock (`sharedSocketFdNew`, `sharedSocketOf`), traduits
  par toutes les fonctions de socket du pont, `close`, `read`, `write`, `ioctl`, `fcntl`, `select`, `epoll` et
  `getpeername`. Le `select` n'a plus à lire au-delà de 1024. Vérifié sans régression : OutRun 2 (attente du link,
  course), Dead Heat (vérification du link en multicast, attract, partie).
- Avec la -3, Initial D 4 Export tombait en 599 des deux côtés, alors que ses pings d'un octet passaient dans les
  deux sens : le jeu lisait sur des numéros de socket jamais créés (3060, 2996, 2728, 2220 au lieu de 2216, même
  800), ses champs de socket abîmés par ses `FD_SET` sur les grands numéros : même cause que Hummer. Sega Race TV
  se relie (MAIN « 2 PLAYERS GAME CAN BE HELD »).
- Release `linuxloader-v3.0.12-ffbedition-20261026-4-njz3-win32.zip` (depuis `73d53f5`, à la demande du
  propriétaire) : les descripteurs de socket 900 à 1023. SHA-256 du zip
  `f6a7eaaaa3cd7f32f2c55a5fe0dc2cda53a5e2058a6dd72050ceca2c8fca87c8`, de `linuxloader.exe`
  `8c7038188c31c2bb5f4851e6c6a56708a2334f4e2fc5467871e853fca9130a4d` ; `ll-deps` identique. Vérifiée depuis le zip
  extrait : `--version`, attente du link d'OutRun 2 (sockets 900 et 901, diffusions).
- Posée sur les deux bornes par Image borne (accord du propriétaire). Hummer : link en course, même course des
  deux côtés (LAP 1/3, 1P et 2P), plus de plantage ; le propriétaire a vu les deux joueurs. Initial D 4 Export :
  plus d'erreur 599, les deux bornes en démo, sockets en 9xx, pings échangés ; pas encore de partie VS (crédits et
  carte). Initial D 5 : GAME ASSIGNMENTS a SERVER BOX (ON) puis SEAT NUMBER en 5e ligne (SINGLE, A1, A2) ; A1 et
  A2, pas d'erreur réseau, paquets reçus de l'autre borne ; pas encore de partie VS. OutRun 2 et Sega Race TV pas
  refaits avec la -4.
- Release `linuxloader-v3.0.12-ffbedition-20261026-5-njz3-win32.zip` (depuis `cf39439`, à la demande du
  propriétaire) : la -4 plus les lampes d'OutRun 2 nommées dans la liste des sorties et le guide des contrôles
  (rien de changé côté réseau). SHA-256 du zip `be00efdf81b85133f06c57cc39e852f342603972aaa3c9b18850ad0339c832b9`,
  de `linuxloader.exe` `d75efd2855e91e54acfac2980c9533fd1b52b662778d586e73bde2c6291adf6b` ; `ll-deps` identique.
  Vérifiée depuis le zip extrait : `--version`.
- Avec la -4, sans régression : OutRun 2 en course à deux (racecab1 1er, racecab2 2e, voitures 1P et 2P visibles)
  et Sega Race TV en course à deux (MULTI-PLAYER, « 2PLAYERS GAME! ENTRIES 2/2 », même circuit, vu par le
  propriétaire). Le link marche donc pour OutRun 2, Hummer et Sega Race TV ; Initial D 4 Export et 5 sont reliés,
  leur partie VS reste à faire à la borne par le propriétaire (crédits et carte).

### Link d'OutRun 2 essayé sur les deux bornes (suite de la demande 117)
- Release 20261026 posée sur racecab1 et racecab2 par Image borne, avec l'accord du propriétaire (lanceur
  `RunLinuxLoaderLinkGame.cmd`, dossiers d'écriture `linuxloader_link_cab1` et `_cab2`).
- Menu de test d'OutRun 2 : c'est un programme à part, `disk0\JenTest\JenTest`, que le loader ne prend en mode test
  (`-t`, ou relance après Test) que si le jeu s'appelle exactement `Jennifer` (`testModePath`). Le lanceur passait
  `Jennifer_patched` : le jeu repartait, sans menu. Avec `Jennifer`, GAME ASSIGNMENTS réglé : LINK ID 1 et 2,
  TOTAL MACHINE 2, GROUP 1.
- `Jennifer_patched` (programme modifié par TeknoParrot pour un rendu plus grand) n'affichait qu'un quart de la scène
  dans le cadre de 800×480 du loader, à l'écran comme dans `shot` : tous les lanceurs de l'image passent à
  `Jennifer`.
- Résultat : link qui marche. Les deux bornes sortent de la vérification du réseau (`##MODE INIT## SYSTEM`, puis
  `DEST`), racecab2 rejoint la partie de racecab1 (ENTRY, « Entry has been closed… Versus Mode »), choix du parcours,
  du nombre de pilotes et de la boîte, puis course ensemble (1P F50 rouge sur racecab1, 2P jaune sur racecab2).
- Le décor 3D blanc des captures `shot` en course est l'alpha de l'image (presque nul dans le décor), qu'une
  conversion en PNG qui le garde montre comme transparent : convertir en RGB.
- Journal : `bind`, `connect`, `sendto` et `recvfrom` donnent maintenant l'adresse et le port (de l'autre borne pour
  `recvfrom`), et l'erreur Winsock en cas d'échec ; demandé par Image borne pour la suite. Vérifié ici sur OutRun 2
  (TOTAL MACHINE 2) : « sendto … to 192.168.1.255:10600 », « recvfrom … from 192.168.1.107:… », « bind … on
  0.0.0.0:10600 ».

### Link à deux bornes : OutRun 2, Hummer, Sega Race TV, Initial D (demande 117)
- Demande d'Image borne, relayée par le propriétaire : le link entre racecab1 (192.168.1.4) et racecab2 (192.168.1.5),
  /24, même commutateur, pare-feu coupé, plus l'adaptateur VPN de TeamViewer (7.x.x.x/8). Exécutables des six jeux
  copiés seuls depuis le partage pour les lire (scratchpad).
- Comment chaque jeu fait son link (lu dans les exécutables) :
  - OutRun 2 SP SDX : son adresse et son masque viennent de l'EEPROM (`OR2_IPADDRESS`, `OR2_NETMASK`, écrits par
    `setIP`). `nlCbPrepare` en déduit l'adresse de broadcast du sous-réseau et celle des autres bornes : adresse de
    la borne - LINK ID + n, donc des adresses qui se suivent dans l'ordre des LINK ID. `commLink(port)` : une socket
    d'envoi UDP en `SO_BROADCAST`, rendue non bloquante par `fcntl`, vers le broadcast ; une socket de réception liée
    à toutes les adresses sur le même port, lue par un fil.
  - Hummer Extreme : la table de quatre adresses en `0x9C2FE90` (192.168.76.1 à 4 d'origine, remplacée par
    `IP_CAB1` à 4) ; la borne lie sa socket (`SO_REUSEADDR`, non bloquante par `fcntl`) à l'adresse de son LINK ID, et
    envoie en unicast aux autres sur le port 10001. Son adresse doit donc être l'une de celles de son PC.
  - Sega Race TV (`klib::lan`) : `EnterCheckNIC` fabrique `192.168.49.N` (N selon CABINET ID) et règle eth0 dessus ;
    le loader remplace ces adresses par `SRTV_IPADDRESS`. `GetLanParam` lie sa socket à l'adresse d'eth0, le maître
    est cherché par broadcast (`klSearchMaster`), puis connexion TCP. Pas d'appel direct au multicast.
  - Initial D 4 Export (`cStorePairLinkMonitor`) : link entre deux sièges, en unicast : chaque siège envoie à
    l'adresse de l'autre, prise dans une table que le loader remplace par `ID_IP_SEAT_1` et `2` (192.168.37.49 et 50
    d'origine). Initial D 5 : la même chose (table patchée en `0x0833E2DB`).
  - Initial D 4 japonais (Rev B) : pas de cette table ; un réseau à routeur et DHCP (192.168.37.254,
    `seqInitNetwork::tickWaitDHCP`), aucun patch dans le loader, même sous Linux : pas pris en charge ici.
- Trous du loader sous Windows, corrigés :
  - `fcntl(F_SETFL, O_NONBLOCK)` était ignoré pour les sockets : celles du link d'OutRun 2 et de Hummer restaient
    bloquantes. Il passe maintenant par `FIONBIO` (`sharedSocketFcntl`), et `F_GETFL` répond d'après l'état gardé
    (Winsock ne sait pas le dire) ; `FIONBIO` et la fermeture le tiennent à jour.
  - Une socket UDP Windows fait échouer son `recvfrom` suivant par `WSAECONNRESET` quand un de ses datagrammes n'a
    trouvé personne (ICMP port injoignable), ce que Linux ne fait pas : désactivé à la création (`SIO_UDP_CONNRESET`),
    sinon une borne qui envoie avant que l'autre écoute prendrait une erreur.
  - eth0 vu par les jeux (`ioctl SIOCGIFADDR`) : 127.0.0.1 sauf pour Sega Race TV. Maintenant aussi l'adresse de la
    borne pour OutRun 2 (`OR2_IPADDRESS`), Hummer (celle des `IP_CAB1` à 4 qui est au PC) et Initial D 4 Export et 5
    (celle des `ID_IP_SEAT_1` et 2 qui est au PC), quand l'une d'elles est une adresse du PC (essai de `bind`) ; une
    ligne WARN « Link: this cabinet's address is … (clé) » le dit. Sinon rien ne change (127.0.0.1, jeu seul).
  - Le fichier `linuxloader.ini` par défaut explique ces clés.
- Essai ici (OutRun 2, MinGW, mode agent caché, `OR2_IPADDRESS` = l'adresse de ce PC) : TOTAL MACHINE réglé à 2 au
  menu de test, le jeu affiche « CHECKING NETWORK NOW, TOTAL NODES 2, NODE ID 1 », envoie ses diffusions de 1248
  octets et reçoit les siennes, et continue de tourner en attendant l'autre borne. Un vrai link n'est pas possible
  sur un seul PC (une seule adresse réseau, même port) : l'essai à deux bornes revient à Image borne.
- Réglages par borne (`[Network]`, `ENABLE_NETWORK_PATCHES = true` ; racecab1 / racecab2) :
  - OutRun 2 : `OR2_IPADDRESS` 192.168.1.4 / 192.168.1.5, `OR2_NETMASK` 255.255.255.0 ; menu de test, GAME
    ASSIGNMENTS : LINK ID 1 / 2, TOTAL MACHINE 2, GROUP identique (1) ;
  - Hummer Extreme : `IP_CAB1` 192.168.1.4 et `IP_CAB2` 192.168.1.5 sur les deux, `IP_CAB3` et `IP_CAB4` sur des
    adresses libres (celles du fichier par défaut, 192.168.1.4 et 5, sont celles des bornes) ; menu de test, GAME
    ASSIGNMENTS : LINK ID 1 / 2 ;
  - Sega Race TV : `SRTV_IPADDRESS` 192.168.1.4 / 192.168.1.5 ; menu de test, GAME ASSIGNMENTS : CABINET ID MAIN /
    2-SUB (libellés d'après le code du loader, pas encore vus à l'écran) ;
  - Initial D 4 Export, Initial D 5 : `ID_IP_SEAT_1` 192.168.1.4 et `ID_IP_SEAT_2` 192.168.1.5 sur les deux ; menu de
    test, GAME ASSIGNMENTS : SEAT NUMBER 1 / 2.
- Release pour l'essai à deux bornes : `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261026-njz3-win32.zip`
  (faite par `release.py` depuis `97be116` : lampes Namco, Gaelco et JVS bit par bit, liste des sorties, link),
  18,3 Mo ; SHA-256 du zip `e6476774ecb6c5a15b46d3b12dd699a3449e9f4e6f6d0665772e3c545f432a4e`, de `linuxloader.exe`
  `fde22f0a95d3272326d19c80e8b3fd634f7bac4ff176bef2187ad139a9e0722d` ; `ll-deps` identique à la 20261025-4. Vérifiée
  depuis le zip extrait : `--version` (« Build: linuxloader-v3.0.12-ffbedition-20261026-njz3 FFB Edition ») et le
  fichier par défaut. Envoyée à Image borne avec les réglages ; la pose est décidée par le propriétaire.
- Liste des sorties dans les autres émulateurs (demande 116) : sm2-emu, Flycast et Supermodel l'ont faite dans le même
  style, selon le jeu en cours (Supermodel sans valeur dans son lanceur, avec valeurs dans son menu en jeu) ; rien
  n'est commité chez eux, ils attendent l'accord du propriétaire.

### Lampes Namco, Gaelco et JVS bit par bit, liste des sorties (demandes 113 à 115)
- Sorties réseau (`netOutputs.c`, TCP 8000) : `netOutputsSetGpoBit` éteint, allume ou inverse une seule sortie (lampe n
  = bit 7 - n % 8 de l'octet n / 8, comme la sortie JVS n + 1) ; `RawLamps*` et `lampN` partent comme pour les
  écritures par octet, seulement aux changements.
- JVS : `CMD_WRITE_GPO_BIT` (sortie, puis 0 éteindre, 1 allumer, 2 inverser) est transmise ; elle était acceptée sans
  suite.
- Gaelco (`gaelcoIo.c`) : les sorties 0 à 3 de `COMMAND_WRITE_OUTPUT` sont des octets de bits
  (`G3Dintf_ports::setOutBits80000`, `80001`, `80002`), donc `RawLamps` à `RawLamps3` ; le moteur (4 et 5) et le
  vibreur (6) restent sur `RawDrive`. Lu dans les exécutables :
  - Ring Riders : sortie 0, lampes de la cabine sur les bits 0 à 3, sécurité de la plateforme sur le bit 5 ; sortie 1,
    lampes de la cabine sur les bits 7, 2 et 3, impulsions du compteur de pièces sur le bit 5, coupure du son sur le
    bit 0 ; sortie 2, bits 7, 2 et 3 éteints au menu de test ;
  - Championship Tuning Race (`IOFrontal`) : sortie 0 seulement, compteur de pièces sur le bit 0, ampoules
    (`procesaBombillas`) sur les bits 1 et 2, coupure du son sur le bit 3 ;
  - Tokyo Cop, interface ancienne : les mêmes octets écrits sur les ports 0x40 et 0x41 (`set_out_bits_40` et `_41`),
    donc `RawLamps` et `RawLamps1` : sur 0x41 les lampes de la cabine (`enableCabinetLight`) sur les bits 7 (bouton de
    sirène, `luzBocina`, `lamp8`), 2 et 3, le compteur de pièces sur le bit 5 (`lamp10`) ; sur 0x40 la plateforme
    (bits 4 à 7).
- Namco (`deadHeatIo.c`) : les lampes de la cabine sont celles de la carte Kickback. `clKickback::setLamp` et ses
  variantes nommées les rangent dans l'instance, envoyées par le fil de la carte, dont le port reste fermé ici ;
  `n2JvioTxPl` (`setupGOUT`) ne fait que configurer les sorties de la carte N2. Elles sont lues dans l'instance à
  chaque image, avec le ressort et le reste, et deviennent `RawLamps` (`lamp0` la première) :
  - Dead Heat : 7 mots à partir de 0x04 (View, Nitro, PinKey, SheetKey, RingTopKey, RingCenterKey, RingBottomKey),
    allumés quand leur intensité (bits 0 à 3) n'est pas nulle. Le bit 4 est le drapeau que `setLamp` et
    `setLampFloat` prennent avec elle ; il remet aussi un compteur à 0x14 à zéro (le clignotement, semble-t-il) ;
    `clrLamp` efface les deux. Un premier essai qui lisait le bit 4 comme « allumée » donnait toutes les lampes
    allumées au démarrage, puis plus rien ;
  - Maximum Heat 3D : les mêmes à partir de 0x06 ; Dead Heat Riders : 8 mots à partir de 0x04 (View, Nitro, six sans
    nom) ;
  - Maximum Tune 3 et 3DX+ : deux entiers à 0x04 (View) et 0x08 (Intrude, le défi), allumés s'ils ne sont pas nuls ;
  - Nirin : pas de carte Kickback, pas de lampe.
  Seuls les exécutables ont été copiés depuis le partage de racecab1 pour les lire (scratchpad), et Dead Heat en
  entier pour l'essai, remis à la Corbeille ensuite.
- Paramètres, onglet « Sorties et réseau » (en jeu) : un tableau de dix lignes avec barre de défilement et en-tête
  fixe, « Ce que ce jeu envoie » : nom, type, signification, dernière valeur envoyée (« - » tant que rien n'est parti
  ou que les sorties sont coupées). Lignes : `mame_start` (texte), chaque `RawLamps*` (octet) suivi de ses `lampN`
  (0 ou 1) avec leur sens connu, `RawDrive` (octets des trames de la carte servo) pour les jeux qui en envoient,
  `mame_stop`. La liste suit la carte du jeu, comme la description des cartes de l'onglet « Options du jeu »
  (`gameLamps` dans `guiGameOptions.cpp`) ; traduite en français.
- Essais (MinGW, mode agent caché, client TCP sur un port d'essai ; copies locales remises à la Corbeille ensuite) :
  - Tokyo Cop : `RawLamps` et `RawLamps1` ; la lampe de sirène (`lamp8`) clignote, les lampes de cabine 12 et 13
    allumées, le compteur de pièces (`lamp10`) pulse à chaque pièce ;
  - Ring Riders : les lampes de cabine de la sortie 0 (`lamp4` à `lamp7`) en chenillard, `lamp8` qui clignote, le
    compteur de pièces (`lamp10`) aux pièces ;
  - Dead Heat : les 7 lampes éteintes pendant l'attente du link, puis en attract les lampes 0 à 2 allumées tour à tour
    chaque seconde et des salves de clignotements ; en partie, les lampes 4 à 6 changent d'intensité sans s'éteindre.
    Champs lus en mémoire par l'agent (`mem`) : `e001` éteinte, `ef01` pleine intensité, `e301` faible ;
  - Maximum Heat 3D, Dead Heat Riders, Maximum Tune 3 et 3DX+, Championship Tuning Race : offsets lus dans leurs
    exécutables, pas lancés ; aucun jeu connu n'écrit par `CMD_WRITE_GPO_BIT`, ce chemin n'est pas essayé.
  - Liste des sorties : capture de Dead Heat lancé normalement (fenêtre visible, vJoy masqué), onglet « Sorties et
    réseau » en français, défilement jusqu'à `RawDrive` (8) et `mame_stop`.
- Les sessions sm2-emu, Flycast et Supermodel ont reçu, à la demande du propriétaire, la consigne d'afficher la même
  liste dans le même style, selon le jeu en cours.
- Image borne a demandé le link à deux bornes d'OutRun 2, Hummer, Sega Race TV et Initial D 4/5 : sous Windows,
  `eth0Address()` présente 127.0.0.1 à ces jeux, le link ne peut donc pas marcher en l'état ; décision du propriétaire
  attendue.

### Quatrième release du 25/10 (demande 112)
- Faite par `release.py` (procédure de la demande 110) : suffixe « -4 » calculé d'après les trois zips du jour,
  `version.h` à 3.0.12 avec `RELEASE_SUFFIX "-4"` le temps du build puis remis, MinGW recompilé de zéro. Contenu :
  commit `e67aa54` exactement (journal de diagnostic, suffixe du jour, avertissement de copie de réglages périmée, en
  plus de tout ce que portait la 20261025-2).
- `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261025-4-njz3-win32.zip`, 18,3 Mo ; SHA-256 du zip
  `2b84ce5efb3e277a7848ce30fe2482776f27822f37bc4e4ab2941b87baa97fa1`, de `linuxloader.exe`
  `53200fe39376ecdcf0385a65a35da98b6246a5602424cdddaca5a59f3016525e` ; `ll-deps` identique à la 20261025-3.
- Testée depuis le zip extrait : `--version` → « linuxloader v3.0.12 » et « Build: linuxloader-v3.0.12-ffbedition-
  20261025-4-njz3 FFB Edition », le nom exact du zip ; `--create config`.
- Dead Heat Riders à 1,25 FPS (campagne du 25/10) : non reproduit par Image borne en trois passes (60 FPS, avec et
  sans le relevé FFB de vjoyd, retour de force à 0 ou 100). Le chiffre était lu juste après une capture qui minimise le
  jeu en plein écran : la moyenne du titre englobe la pause. Vitesse à revérifier sans minimiser.

### Suffixe des releases du jour, copie de réglages périmée (demandes 110 et 111)
- Le nom du build (`buildEdition`, titre des fenêtres) porte maintenant le rang de la release dans sa journée, comme
  son zip : rien pour la première, « -2 » pour la deuxième… (`RELEASE_SUFFIX`, vide par défaut). `--version` affiche
  aussi ce nom sur une deuxième ligne (« Build: linuxloader-v…-njz3 FFB Edition »), qui permet de vérifier une release
  sans ouvrir de fenêtre.
- Procédure de release (script `release.py` de la session) : le suffixe est calculé d'après les zips du jour déjà
  dans `emulateurs/releases` ; `version.h` reçoit le temps du build la version du changelog (3.0.12) et, pour une
  deuxième release du jour ou plus, `#define RELEASE_SUFFIX "-N"`, puis il est remis ; build MinGW recompilé de zéro,
  zip, empreintes, `ll-deps` comparé à la release précédente. Le script refuse un arbre de travail modifié.
- Vérifié : build d'essai avec le suffixe « -9 » et la version 3.0.12 → « Build: linuxloader-v3.0.12-ffbedition-
  20261025-9-njz3 FFB Edition » ; build normal → « linuxloader-v2.0.0-ffbedition-20261025-njz3 FFB Edition ».
- Avertissement (`overlaySettings`, `writeDir.c`) : quand la copie de `linuxloader.ini` ou `controls.ini` à la racine
  du dossier d'écriture est plus ancienne que le fichier d'origine à côté du loader, une ligne WARN dit que les
  changements faits à côté du loader ne sont pas lus. Copier un fichier garde sa date (`CopyFileA`) : une copie
  fraîche ne déclenche rien. Vérifié : copies datées de 2020 → une ligne pour chacun des deux fichiers ; mêmes dates →
  aucune.

### Tokyo Cop figé sur les bornes, journal de diagnostic (demande 109)
- Sur les deux bornes, le CONTROLS TEST de Tokyo Cop reste à volant 0, gaz 255, frein ON (et Force 0,50), quels que
  soient les axes ; les autres jeux de course roulent dans la même campagne, avec le même vjoyd. Ces trois valeurs
  sont celles de trois axes SDL au minimum.
- Écarté : les liaisons clavier (essai A/B, aucune touche reçue), un état enregistré (write-dir neuf, sans write-dir),
  le lanceur `.cmd` et l'environnement (lancement direct, aucune variable SDL_* ni LL_*), un fichier propre au jeu,
  un second vJoy (un seul périphérique sur chaque borne), le focus (reproduit ici : jeu minimisé puis restauré).
- Non reproduit ici, avec le dossier de Tokyo Cop copié de racecab1, le `controls.ini` et le `linuxloader.ini` de la
  borne, la release du 25/10 et vJoy placé comme le démon (X, Y, Z au centre) : en mode agent comme en lancement normal,
  le test suit tout (repos 127 / 0 / off, Y = 0 → gaz 255, Z = 0 → frein ON, X = 0,1 → volant 25). Tokyo Cop tourne
  ici aussi sur la carte NVIDIA.
- Journal ajouté, avec `LL_LOG=debug` et `DEBUG_MSGS = true` : chaque événement d'axe de manette ou de joystick
  (« Joy Axis: nom, JOYn axis a = valeur », « Pad Axis: … »), chaque voie analogique JVS écrite par le loader
  (« JVS analogue c = v (P1 Gas) »), et ce que lit un jeu Gaelco (« Gaelco analogue n = v …; JVS 1/2/3 = … », à chaque
  changement et toutes les 600 lectures, de même pour les registres de contacts). Vérifié sur Tokyo Cop.
- Release : `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261025-3-njz3-win32.zip` (commit `a121e39`, même
  procédure), 18,3 Mo ; SHA-256 du zip `e96e2c82fcba5497a63a3d356d738bb2398454026581e920b1a915224108bbd6`, de
  `linuxloader.exe` `e18578966e12ef6863c175accd35ba647e448e4ddae828ef56ed42c797ffdcef` ; `ll-deps` identique à la
  20261025-2. Sans `LL_LOG=debug` et `DEBUG_MSGS`, elle se comporte comme la 20261025-2. Testée depuis le zip :
  `--version`, lignes Gaelco et JVS présentes sur Tokyo Cop en mode agent.
- Résultat sur racecab1 : SDL ne recevait des axes X, Y et Z de vJoy que −32766 ou +32767, une marche au centre ;
  le jeu lisait fidèlement ce que le loader lui passait, et joy.cpl montrait la même marche. Cause : vjoyd, le
  remplaçant de BackForceFeeder qu'Image borne utilise pour ses campagnes, et non Windows ni le loader. Sa ligne
  `$raw = [int][Math]::Round(1 + [Math]::Max(0, [Math]::Min(1, $v)) * 0x7FFF)` fait choisir à PowerShell
  `Math.Min(Int32, Int32)` : la position est arrondie à 0 ou 1 (0,5 → 0) avant d'être bornée, donc 1 ou 0x8000 envoyé
  à vJoy. Reproduit ici avec son script tel quel (winmm : 2 / 65535 / 65535 pour X à 0,25 / 0,55 / 0,75, contre
  16385 / 36044 / 49152 avec un alimentateur juste), sur un vJoy identique aux bornes (4 axes X, Y, Z, RZ ; 32
  boutons). Correction (à Image borne) : `[Math]::Max(0.0, [Math]::Min(1.0, $v))`. Toutes les mesures de campagne
  pilotées par vjoyd sont donc à refaire ; BackForceFeeder, qui alimente vJoy en service, n'est pas concerné.

### Deuxième release du 25/10 (demande 108)
- Même procédure : build MinGW recompilé de zéro, `version.h` à 3.0.12 le temps du build (fichier remis), zip avec
  `linuxloader.exe` et `ll-deps/`. Contenu : commit `af2871e` exactement (paramètres dans une petite fenêtre, version
  et « FFB Edition » dans le titre, en plus de la correction de la carte d'Initial D de la release précédente).
- `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261025-2-njz3-win32.zip`, 18,3 Mo ; SHA-256 du zip
  `82584606600fc3b3c46ce31a3dfbeef962995b7e92d2fcc1850d85d4ca28a3ff`, de `linuxloader.exe`
  `699d3afb9ecc710ed6e5320695159c2e0103eae9d992246da7114abe5ba37ac8` ; `ll-deps` identique à la 20261025.
- Testée depuis le zip extrait : `--version` (3.0.12), `--create config`, titre de Tokyo Cop en mode agent :
  « Tokyo Cop [tokyocop] - linuxloader-v3.0.12-ffbedition-20261025-njz3 FFB Edition - FPS: 57.01 ». Le suffixe -2 du
  zip n'y figure pas (le titre se forme de `version.h` et de la date) : l'empreinte de l'exe départage les releases du
  même jour.

### Version et « FFB Edition » dans le titre (demande 107)
- `buildEdition()` (`config.c`) : le nom du package de cette branche, `linuxloader-v<version.h>-ffbedition-<date>-njz3`,
  puis « FFB Edition ». La date est celle de la compilation de `config.c` (`__DATE__`) : celle de la release, qui se
  construit de zéro ; un build de développement montre la date de sa dernière compilation de ce fichier. La version
  vient de `version.h` : 2.0.0 dans un build de développement, 3.0.12 dans une release (la procédure l'y écrit le temps
  du build), donc le titre d'une release est le nom de son zip.
- `formatWindowTitle` l'ajoute après le nom du jeu et son nom court entre crochets (que BackForceFeeder reconnaît,
  inchangé), avant la fréquence d'images : « Tokyo Cop [tokyocop] - linuxloader-v2.0.0-ffbedition-20261025-njz3 FFB
  Edition - FPS: 57.34 », lu sur la fenêtre de Tokyo Cop en mode agent. Toutes les fenêtres de jeu passent par cette
  fonction (GLX, GLUT, SDL 1.2, X11). Le sélecteur : « Lindbergh Loader - linuxloader-v…-njz3 FFB Edition ».
- Au passage, la liste des fenêtres de ce processus montre que Tokyo Cop tourne ici sur la carte NVIDIA (« NVOGLDC
  invisible ») : la carte graphique des bornes n'explique pas son volant figé.

### Paramètres dans une petite fenêtre (demande 106)
- Avant : l'échelle de l'interface ne descendait pas sous la taille propre de la police (1,0 sur un écran normal),
  la fenêtre de paramètres prenait sa taille et sa place à l'ouverture seulement, et ses onglets ne défilaient que
  verticalement. Une fenêtre de jeu réduite après l'ouverture laissait les paramètres hors de vue (entièrement à
  480×360) ; ouverts directement à 480×360, ils débordaient en bas, boutons Enregistrer et Fermer cachés.
- `guiCommon.cpp` : la fenêtre de paramètres ne dépasse jamais celle du jeu (`SetNextWindowSizeConstraints`) et y est
  ramenée à chaque image ; les onglets défilent aussi horizontalement (`ImGuiWindowFlags_HorizontalScrollbar`), les
  boutons restent visibles dessous ; le message d'état passe sous les boutons quand il ne tient pas à côté ;
  `applyScale` descend jusqu'à 80 % de la taille de la police à l'écran (0,65 était illisible).
- Vu dans le sélecteur (Tab), captures de l'écran à 1280×720, 640×480 et 480×360, ouverture puis réduction et
  ouverture directe, release 20261025 contre le nouveau build : paramètres entiers dans la fenêtre, défilement
  vertical et horizontal, boutons en vue. MinGW et MSVC compilent.

### Mesures en course sur la borne, carte d'Initial D (demande 105)
- Captures d'Image borne (`PrintWindow` avec `PW_RENDERFULLCONTENT` ; un jeu en plein écran exclusif se réduit dès
  qu'il perd le focus : sa capture le restaure d'abord). Retour de force reçu par vJoy en course : Gaelco
  Championship Tuning Race, Ring Riders, SEGA Race TV, Tokyo Cop (il rend depuis la -5), Hummer Extreme, Dead Heat
  Riders, OutRun 2 SP SDX, Maximum Heat 3D (en course, 8e, 80 km/h). Les autres attendaient sur des écrans de choix
  (ID5, R-Tuned, WMMT3, 3DX+ : un front de gaz par écran, ou Start pour ID5, ou le minuteur), sur l'écran de la nitro
  (Dead Heat : action `Boost` du joueur 1) ou sur la carte (Initial D 4). Nirin n'a pas de moteur de guidon.
- Initial D 4 Export et Japon : « Waiting for checking Initial D license card ». Le chargement automatique ne met une
  carte que si `idCardFileExists` (`cardReader.c`) trouve un `InidCrdNNN.crd` (ou `InidCardNN.crd`) de la bonne
  taille ; il la cherchait par `stat` dans le dossier courant, `WRITE_DIR\<jeu>` avec un dossier d'écriture, alors que
  la carte est dans le dossier du jeu (relevé sur la borne : `InidCrd000.crd` côté jeu, une carte neuve créée côté
  écriture). Il passe maintenant par `writeDirForRead`, comme les lectures du jeu. Contournement posé par Image borne
  en attendant : la carte du jeu copiée dans `WRITE_DIR\<jeu>`.
- Tokyo Cop : 000 km/h en poursuite ; son CONTROLS TEST montre gaz 255 (Y à 0,2, soit 0,6 en `_NEGATIVE_SIDE`),
  frein ON (Z au centre) et volant en butée. Pas le clavier : un essai A/B avec et sans les liaisons flèches donne le
  même résultat, et aucune touche n'arrive (`DEBUG_MSGS`, seulement l'Alt de l'outil de capture). Non résolu ; piste :
  vJoy est ouvert en manette SDL (« SDL CONTROLLER 0 ») et une liaison `GC0_AXIS_…TRIGGER` lit un axe au repos au
  centre comme une gâchette à moitié enfoncée. Son menu de test : Test tenu environ 1 s (0,2 s est manqué), puis
  `ViewChange` monte, `MusicChange` descend, Start entre.
- Release : `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261025-njz3-win32.zip` (commit `510f4d3`, même
  procédure), 18,3 Mo ; SHA-256 du zip `732190f69bba0fbafff7b9e6e56baf70d6d5dd5a834bd66b6903c9b1e418a205`, de
  `linuxloader.exe` `350580b3a9f9e0ea3d4bf4b9ae407b945f128f433c94b097c2d47c1e7ea43e4b` ; `ll-deps` identique à la
  -5. Testée depuis le zip extrait : `--version`, `--create config` et `--create controls`. Pas d'Initial D 4 en
  local : 25,7 Go libres, sous le seuil de 30 Go convenu avec les autres sessions (Corbeille pleine de leurs jeux).

### Échecs sur la borne : dossier d'écriture et adresse de SEGA Race TV (demandes 103 et 104)
- Diagnostic avec Image borne, release -4 sur racecab1, journaux `LL_LOG=info`. Sans `--write-dir`, WMMT3, Gaelco
  Championship Tuning Race et Ring Riders tournent ; avec, ils meurent. Image borne a coupé le dossier d'écriture
  sur les deux bornes en attendant la correction.
- WMMT3 et 3DX+ : « Library file not found at path: WRITE_DIR\mt3\libCg.so », puis « Unresolved symbol called:
  cgCreateContext ». `sharedDlopen` (`libcShared.c`) passait tout nom par `writeDirForRead`, même un nom nu : le
  fichier n'existant d'aucun côté, il revenait avec le chemin du dossier d'écriture, et ce chemin absolu ne
  correspondait plus à la substitution `libCg.so` -> `Cg.dll`. Comme sous Linux, seul un nom avec une barre est
  maintenant un chemin.
- Gaelco Championship Tuning Race, Ring Riders (et Tokyo Cop, « Unable to open file! », même famille) : « Unable to
  open file (/home/joc2001/joc2003/salidas/base.tex) », puis `exit`. `findLinuxRoot` (`filesystemShared.c`) cherche
  la racine de l'image Linux en remontant depuis le dossier courant, au premier accès fichier ; avec le dossier
  d'écriture, ce dossier est déjà `WRITE_DIR\<jeu>`. La recherche part maintenant du dossier du jeu
  (`writeDirGameFolder`, `writeDir.c`).
- SEGA Race TV : mort vers 42 s après une rafale de `socket`, sans ligne `FATAL`. Le `linuxloader.ini` par défaut
  écrit `SRTV_IPADDRESS = "192.168.1.2"` depuis le premier commit d'upstream ; une adresse qui n'est pas celle du PC
  fait mourir le jeu dans `klib::lan::GetLanParam` (les bornes sont en .4 et .5 ; mes ini avaient le champ vide ou
  127.0.0.1). Le défaut est maintenant vide, et `sharedSrtvAddress` remplace par 127.0.0.1, avec un avertissement,
  une adresse à laquelle aucun socket ne se lie : les ini déjà écrits marchent. Image borne avait aussi vidé le champ
  sur les bornes en contournement.
- Retour de force : chaque jeu journalise « FFB: Haptic opened from joystick 0 (… vJoy …) » puis « Wheel mode » sur
  la borne ; les 40 paquets identiques des trois Initial D ressemblent à la création des effets, les zéros des autres
  à une mesure hors course. À refaire en course, en `BORDERLESS`, avec `vjoy_driver.py` (pédales au centre au départ
  selon la convention des bornes), une fois les deux boîtes « Image incorrecte » de la release -3 fermées à la main.
- Essais sur copies reprises du partage (sans les fichiers FFB Plugin, déplacés par Image borne), en mode agent caché,
  chaque échec d'abord reproduit avec l'exe de la release -4 :
  - Ring Riders avec `--write-dir` : -4 « Unable to open file (…/base.tex) » puis `exit(0)` ; corrigé, MinGW et MSVC,
    « Linux disk image: absolute paths taken in … », attract « INSERT COIN » à 57 i/s, aucune ligne `ERROR`, seuls
    `eeprom.bin` et `sram.bin` écrits, dans le dossier d'écriture ;
  - Maximum Tune 3 avec `--write-dir` : -4 « Unresolved symbol called: cgCreateContext (Caller: 08aeb69a) », comme sur
    la borne ; corrigé, MinGW et MSVC, `ll-deps\Cg.dll` chargé, attract à 60 i/s (le bandeau « Card reader not
    functional » vient du réglage de carte du jeu, il est là aussi sans dossier d'écriture) ;
  - SEGA Race TV avec `SRTV_IPADDRESS = "192.168.1.2"` : -4 meurt à 48 s juste après les sockets ; corrigé, MinGW et
    MSVC, l'avertissement à 41,7 s, puis attract « 1 CREDIT(S) TO START » à 60 i/s au-delà de 100 s ;
  - Tokyo Cop pas essayé ici (même cause supposée que Ring Riders).
- Release : `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261024-5-njz3-win32.zip` (commit `1c0adde`, même
  procédure), 18,3 Mo ; SHA-256 du zip `0074d584a9a0e67ac0b9462717b1632a0f4cfcf79361b81d7018ddbbc97cfd96`, de
  `linuxloader.exe` `70d23407f45f84e7e705100cd5a617740b4e6c1d5366f158eb2c01c7b6f8e782` ; `ll-deps` identique à la
  -4. Testée depuis le zip extrait : `--version`, `--create config` (`SRTV_IPADDRESS = ""`), Ring Riders avec
  `--write-dir` jusqu'à l'attract.

### Quatrième release du 24/10 (demande 102)
- Même procédure que la demande 97 : build MinGW recompilé de zéro, version 3.0.12 dans `version.h` le temps du
  build (fichier remis), zip avec `linuxloader.exe` et `ll-deps/`. Contenu : commit `5b2f467` exactement.
- `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261024-4-njz3-win32.zip`, 18,3 Mo ; SHA-256 du zip
  `11fa1132421dc3b3e70386b315065505785bcee33298216339104851f9709016`, de `linuxloader.exe`
  `741ce0e8f300757406a2f658f3654087ace26e3dd4fad03383872914f53720b6` ; `ll-deps` identique à la release -3.
- Testée depuis le zip extrait dans un dossier vierge : `--version`, `--create config` et `--create controls`
  (phrases sur le dossier d'écriture), Dead Heat en mode agent caché avec `LL_LOG=info` (`nsAdrv.dll` au chargeur ELF,
  aucune ligne `ERROR`, attract à l'image 5400, 60 i/s).

### Boîtes système et dossier d'écriture (demande 101)
- Point A de la campagne d'Image borne sur les bornes : Dead Heat laissait une boîte système Windows (image
  incorrecte) devant tout, même après la fin du loader. `nsAdrv.dll`, le pilote son de Namco, est une bibliothèque
  Linux malgré son nom ; `LoadNeededLibrary` (`symbolResolver.cpp`) essaie d'abord en DLL Windows les noms en
  `.dll`, et ce `LoadLibraryExA` sur un fichier ELF levait la boîte avant de revenir au chargeur ELF.
- Deux corrections. `main()` (`mainW.cpp`) appelle d'abord `SetErrorMode(SEM_FAILCRITICALERRORS |
  SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX)` : une bibliothèque refusée, un lecteur absent ou un plantage
  finissent en ligne de journal et code de sortie, sans fenêtre ; les processus lancés (jeu, menu de test du
  superviseur) héritent du réglage. Et un candidat dont les quatre premiers octets sont `\x7fELF` va au chargeur
  ELF sans passer par `LoadLibrary`, avec la ligne « … is a Linux library despite its name: not loaded as a Windows
  DLL ».
- Point E : avec un dossier d'écriture, les réglages lus sont les copies de ce dossier, et des changements faits à
  côté du loader restaient sans effet. Le `linuxloader.ini` neuf le dit au-dessus de `WRITE_DIR` ; le `controls.ini`
  neuf (`--create controls`, ou créé par la page Contrôles) porte deux lignes de commentaire en tête, avec les fins de
  ligne du fichier (CRLF sous Windows). L'écrivain d'ini ne garde aucun commentaire : un enregistrement depuis la page
  Contrôles les retire.
- Essais : `--create config` et `--create controls` avec MinGW et MSVC (phrases présentes, 105 lignes CRLF sur 105).
- Dead Heat, copie reprise du partage le 24/10 (déjà sans les fichiers FFB Plugin), en mode agent caché avec
  `LL_LOG=info`, MinGW et MSVC : `nsAdrv.dll` passe directement au chargeur ELF (la nouvelle ligne, plus de « Failed
  to load as DLL »), aucune ligne `ERROR`, 60 i/s ; l'attente de liaison d'une borne seule (environ 60 s, déjà connue),
  puis le logo et l'attract « STEP ON THE GAS TO START » vers l'image 5400.

### Axes de vJoy sur la borne, pédales sur un côté de l'axe (demande 99)
- Constat d'Image borne (Sega Race TV, écran INPUT TEST) : volant et accélérateur en tout-ou-rien, frein muet.
  Cause : une entrée physique ne portait que deux liaisons ; un axe qui portait déjà deux liaisons tout-ou-rien
  (`Left`/`Right`, `P1_Steer_Left`/`Right`… sur `JOY0_AXIS_0_NEGATIVE`/`POSITIVE`) rejetait la liaison analogique
  qui suivait, avec « ERROR: Cannot bind more than two actions to axis » dans la sortie d'erreur. Pour un jeu de
  course, seules `[Common]` et `[Driving]` sont lues (pas `[Digital]` : ma première réponse s'était trompée de section,
  corrigée auprès d'Image borne).
- Quatre liaisons par entrée (`MAX_INPUT_BINDINGS`, `sdlInput.h`) : le volant et les directions des menus, une pédale
  et le haut/bas des menus tiennent sur le même axe.
- Mode pédale sur un côté de l'axe : `_NEGATIVE_SIDE` (repos au centre, pédale vers le minimum : le `_NEG` de
  Supermodel, l'accélérateur sur Y- et le frein sur Z- de la convention des bornes) et `_POSITIVE_SIDE`. Valeur 0 au
  centre, 1 au bout de ce côté, l'autre côté compte comme repos ; axes Joystick et Gamepad ; aussi écrit par
  `--create controls` et montré « - (pédale) » dans la page Contrôles. `_NEGATIVE_HALF`, lui, reste la moitié d'un axe
  combiné (deux gâchettes sur une même action) et ne donne rien seul.
- Mesuré par vJoy, OutRun en mode agent (le mode agent ignore les manettes : une aide temporaire les laissait passer,
  retirée), `[Driving]` avec `Left`/`Right` sur l'axe 0 et `Up`/`Down` sur l'axe 1 en plus des entrées analogiques :
  volant 0, 255, 511, 767, 1023 pour X à 0 / 0,25 / 0,5 / 0,75 / 1, Left et Right fermés aux deux bouts ;
  accélérateur `JOY0_AXIS_1_NEGATIVE_SIDE` 0 au centre, 511 à 0,25, 1022 à 0, 0 du côté positif ; frein sur Z de même.
- Release : `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261024-3-njz3-win32.zip` (commit `01a01b1`), 18,3 Mo ;
  SHA-256 du zip `36a2050657b9792da1b6cff6f820dfe353739ed8e1cf30d0c699bcbed4c045ff`, de `linuxloader.exe`
  `f96e2718e51a1d7b271152f171a3bbd01da3c22ab81454f5016ad88d1536de4a` ; `ll-deps` inchangé. Testée depuis le zip extrait
  dans un dossier vierge : `--version`, `--create config`, OutRun en mode agent.

### Test sous un frontend, menu sur Tab (demande 98)
- **Bouton Test** : AttractPlus lance chaque jeu par `cmd /c start "[nom]" /WAIT Run…Game.cmd` et revient à son menu
  quand le processus lancé se termine. Jusqu'ici, Test faisait quitter le jeu et le loader se relançait en `-t` dans
  un nouveau processus : le processus lancé se terminait, AttractPlus revenait à son menu pendant que le menu de test,
  puis le jeu, tournaient derrière.
- Le processus lancé (hors `--agent`) devient le superviseur (`segabootSupervise`, `mainW.cpp`) : il ne charge pas le
  jeu, il relance le même exe avec les mêmes arguments et `--segaboot-child`, dans un job Windows qui tue les enfants
  s'il est arrêté, attend la fin et lit le code de sortie. Le jeu qui quitte après Test rend `0x5E6A0001` (le menu
  de test ensuite, `-t`), le menu de test qui se termine `0x5E6A0002` (le jeu à nouveau) ; tout autre code (ExitGame,
  fenêtre fermée, plantage) termine le superviseur avec ce code. Au plus deux processus ; les enfants gardent la
  console et les sorties standard du superviseur (une redirection du `.cmd` reçoit tout). Arguments recopiés avec le
  guillemetage de `CommandLineToArgvW` (un chemin finissant par `\`).
- Essayé sur OutRun en fenêtre visible, Test par vJoy : processus lancé vivant de bout en bout ; jeu, puis menu de test
  (« OUTRUN2SP SDX GAME TEST MENU », curseur sur EXIT), puis Test sur EXIT et le jeu relancé (« PRESS START
  BUTTON ») ; arrêter le seul processus lancé arrête aussi le programme en cours (job).
- **Menu** : Tab au lieu de F10 pour les réglages par-dessus le jeu (qui le mettent en pause) et dans le sélecteur,
  comme Flycast, windowsloader et Supermodel (consigne du propriétaire relayée par Image borne). `[Launcher]
  SETTINGS_MENU = false` le coupe (borne publique) : Tab va alors au jeu. Textes et commentaires suivent (F10 → Tab).
  Vérifié à l'écran : Tab ouvre « Jeu en pause - Tab : fermer les paramètres », Tab referme et le jeu reprend.
  `SETTINGS_MENU = false` : vérifié à l'écran une fois l'affichage du PC reconnecté (il était déconnecté pendant
  les deux premiers essais, d'où une fenêtre noire) : Tab deux fois, aucun menu, OutRun continue sa démo.
- Exemple de `WRITE_DIR` aligné sur la convention de l'image : `C:\Racecab\Saves\linuxloader_single`.
- Release : `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261024-2-njz3-win32.zip` (commit `74fe305`, même
  procédure), 18,3 Mo ; SHA-256 du zip `b3fe1eef1712ddcf2b91b9aeff8064d25a3467880c8d8de57681c16fd2abe029`, de
  `linuxloader.exe` `701b0fd9024ebc499b9949f56d6aba9964d31ef802832c8f7824fb4109544e04` ; `ll-deps` identique à la
  release du matin. Testée depuis le zip extrait dans un dossier vierge : `--version`, `--create config`
  (`SETTINGS_MENU = true`, nouvel exemple de `WRITE_DIR`), OutRun en mode agent.

### Release Windows du 24/10 (demande 97)
- Même procédure que les demandes 19 et 30 : build MinGW Release recompilé de zéro (`build-mingw`, Ninja), version
  3.0.12 de `docs/changelog` écrite dans `version.h` le temps du build (fichier remis ensuite), zip avec
  `linuxloader.exe` et `ll-deps/` (dont `libgcc_s_dw2-1.dll` et `libwinpthread-1.dll` de MSYS2). Contenu : commit
  `c55db17` exactement, rien de local.
- `emulateurs/releases/linuxloader-v3.0.12-ffbedition-20261024-njz3-win32.zip`, 18,3 Mo ; SHA-256 du zip
  `c288d27c87d84effd7958ce859d587edcc782178670787fb2a37474571dfc48f`, de `linuxloader.exe`
  `872cd87c852a4c0731a584a631121d4a57824a42f4a8effc3e41e54c4cfe9ee5`. `--version` dit toujours `v3.0.12` (la
  version d'upstream) : l'empreinte distingue les releases.
- `ll-deps` : une bibliothèque de plus que le 20/10, `libstdc++.so.6.0.14` (Dead Heat Riders) ; les autres fichiers
  sont identiques. Réglages par défaut inchangés (fenêtre, `ASPECT_RATIO = ORIGINAL`, `WRITE_DIR` vide).
- Testé depuis le zip extrait dans un dossier vierge, sans ini : `--version`, OutRun jusqu'au choix du pilote,
  Maximum Tune 3 jusqu'en course (mode agent).
- Comportements à surveiller sur une borne, envoyés à Image borne : Test relance le loader en `-t` dans un nouveau
  processus (le processus lancé se termine), `FULLSCREEN` change la résolution de l'écran (l'ancien plein écran est
  `BORDERLESS`), `ASPECT_RATIO = 4:3`/`16:9` met des bandes noires (l'ancien remplissage est `4:3_STRETCH`), titre de
  fenêtre « titre [nom court] - FPS: n », FFB par défaut pour Initial D 4 Japon et 5, interrupteur Test des jeux
  Namco, F10 inchangé (pause, sans réglage pour le couper).

### Maximum Tune 3 et 3DX+ en MSVC (demande 96)
- Avant chaque course du mode Story, le jeu joue une cinématique temps réel (`clRTMovie`, journal du jeu
  « リアルタイムムービー 2 ») : une période sans course (`clEffStoryType::isRace` faux) dont le rival n'a pas de
  personnage (NONE). Deux défauts s'y cumulaient, l'un propre à MSVC, l'autre commun aux deux builds.
- **`fopen` en mode texte** : `clMovieSystem::Init` ouvre `data/rtmovie/table/MovieTableData.txt` avec `"rt"` (et
  les deux jeux écrivent avec `"wt"`, `"at"` ; Nirin aussi `"at"`). Le loader ajoutait un `b` pour lire les octets tels
  quels (Linux n'a pas de mode texte) : `"rtb"`, que la bibliothèque C de MSVC refuse (`fopen` rend NULL) quand celle de
  MinGW l'accepte. Sans sa table, la cinématique durait 0 image (« 終了フレーム 0 » contre 290), `clRTMovie::End`
  libérait une caméra jamais créée : plantage du thread de course (3DX+ ; MT3 une fois l'autre défaut corrigé). Le
  `t` devient maintenant le `b` (`sharedBinaryMode`, `filesystemShared.c`).
- **Point de départ d'un rival sans personnage** : `CConfigLoader::GetStoryRivalFeature` cherche
  « ECharacter_NONE » dans `data/carconfig/ytConfigStory.txt`, lit jusqu'à la fin, et quand son `sscanf("%d%d%d")`
  de `StartConfig` échoue, affiche un message puis range quand même trois locales jamais écrites. Sur la borne, la
  pile y est anodine ; ici elle garde les restes du loader (des pointeurs lus comme flottants) : MSVC donnait
  (0, 55), MinGW (1,27e12, 1,18e9). D'où le rival à 1e12 m, `CSlit::GetMatrix` sur une normale nulle, et le thread
  de course de MT3 qui bouclait sur un odomètre NaN dans `GetSectInfo_InterArea` (`comiss` + `jb`, toujours pris
  avec NaN). MinGW survivait par chance. Correctif par symbole et motif d'octets (`cmp eax,3 ; je` → `jne` par-dessus
  les rangements), dans les deux jeux (`mt3RivalFeatureFix`, `patch.c`) : la lecture manquée garde les zéros posés en
  tête de fonction.
- Méthode : traceur temporaire sur les fonctions mathématiques du pont (aucun NaN n'y naît, pile x87 propre ; retiré),
  puis cdb sur `CSlit::SetFirst`, `GetFirstPoint`, `GetStoryRivalFeature` dans les deux builds (mémoire du `CSlit` à
  zéro avant `SetFirst` des deux côtés : les allocations sont bien mises à zéro), et le journal du jeu (image de fin).
- Vérifié : MSVC, Maximum Tune 3 (course avec Akio en rival, 95 km/h, 549 forces constantes, cinématique de 290
  images) et 3DX+ (sans lecteur : course, 86 km/h, 539 forces ; avec le lecteur : partie entière, carte écrite) ;
  MinGW, Maximum Tune 3 (course, non-régression).

### Les manques de chaque jeu (demande 95)
- **Maximum Tune 3DX+, cartes** (mode agent, `NO_CARD_READER=false`) : carte neuve donnée par le distributeur au bout
  du compte à rebours de « Do you have a card? », nom, course, carte écrite (`53`) et imprimée ; à la partie suivante
  la carte insérée est lue (« Card data confirmed », nom et voiture de la carte) ; en fin de partie l'écran du
  kilométrage montre le mot de passe, la carte est écrite et imprimée, puis une deuxième partie repart avec elle. Le
  lecteur de MT3 marche donc aussi pour 3DX+, sans changement.
- **Maximum Tune 3DX+, « Jouer sans le lecteur de cartes »** (`NO_CARD_READER`) : sans effet sur 3DX+, qui proposait
  quand même une carte. Ses écrans de carte demandent `clTestMode::isEnableCardDevice` (qui lit le réglage CARD R/W
  lui-même), pas `isNoUseCardDevice` comme MT3. Avec l'option, les deux répondent maintenant comme avec CARD R/W OFF
  (`isEnableCardDevice` rend 0) : course directe en invité (GUEST), aucune commande au lecteur (MinGW).
- **Maximum Tune 3DX+ en MSVC** : menus et écrans de carte, mais la course échoue comme celle de MT3 : le thread de
  course plante (0xC0000005) dans `clRTMovie::End` → `clCamera::Release` sur une caméra nulle, deux fois sur deux.
  Même cause que MT3 en MSVC ; corrigé le 24/10 (demande 96).
- **Dead Heat** (non-régression complète ; recopié : 2,5 Go, fichiers TeknoParrot à la Corbeille) : contrôle du link
  d'une borne seule (environ 60 s), démo, pièces, course à San Francisco à 176 km/h, MinGW et MSVC ; FFB de la carte
  Kickback (224 forces constantes, 17 amortisseurs, 821 rappels au centre) ; menu de test (interrupteur Test
  enclenché : MENU, I/O TEST et son sous-menu, Bas et Start ; Test relâché, le jeu redémarre) ; vJoy : partie menée
  en fenêtre visible, 275 forces constantes et 916 ressorts/amortisseurs reçus.
- **Initial D 5 Export 2.0** (`INITIALD_5_EXP20_SBTS`, recopié : 3,8 Go, fichiers TeknoParrot à la Corbeille) :
  - Retour de force : même carte qu'Initial D 4 (`cFeedbackSCI`, `_sSCI::send`). Le crochet est maintenant posé une
    fois pour tous les Initial D 4 et 5 avec volant (groupes `GROUP_ID4_EXP`, `GROUP_ID4_JAP`, `GROUP_ID5`, à la fin
    de `initPatch`), au lieu de deux versions seulement ; il cherche la fonction par son symbole. Course : 1 520 forces
    constantes, 44 vibrations, l'amortisseur.
  - Son `initialD.ini` n'a plus de `Width` ni de `Height` (retirés par TeknoParrot) : le jeu prend alors 1920×1080.
    À 1360×768 le loader laissait faire : en course le HUD (temps, vitesse, compte-tours, rétroviseur) sortait de la
    fenêtre, et le programme de test (`-t`) ne montrait que la dernière ligne de son menu. La résolution est imposée
    à 1360×768 en jeu à cette taille et dans le programme de test, comme pour Initial D 4 (`id5NativeResolution`
    dans `patchResolution.c`, pour les six versions d'ID5).
  - Essais : course sur le lac d'Akina (HUD complet, 114 km/h), MinGW et MSVC ; menu de test (« GAME TEST MODE »,
    ses 12 entrées, Service pour choisir, Test pour entrer, OUTPUT TEST) ; vJoy : partie menée en fenêtre visible,
    1 604 forces constantes et 579 vibrations reçues.
- **Maximum Tune 3 en MSVC** (course qui se bloque) : cause cherchée ; trouvée et corrigée le 24/10 (demande 96).
  - Le NaN ne vient d'aucune fonction mathématique du pont (traceur temporaire sur leurs entrées et sorties, et sur
    l'état de la pile x87 : vide et sans faute à chaque appel ; retiré).
  - Il naît dans le calcul SSE du jeu : `yt::CSlit::GetMatrix` normalise une normale de longueur nulle. L'objet route
    (`CSlit`) de la voiture rivale, posé par `CSeq::Init` → `CRivalCar::SetFirst` → `CSlit::SetFirst`, a des champs
    absurdes (paramètre 900 289, positions vers 1e13 pour un circuit vers 1,8e7).
  - Sous MinGW aussi, ces champs sont des NaN au premier `GetMatrix`, mais la course continue. Même chapitre des
    deux côtés (Series01, départ KgDaikoku_In) ; le point de départ de la rivale diffère (MSVC : 2 560 000, 0, 55 ;
    MinGW : 2 077 730, 1,3e12, 1,2e9), calculé par `CSlit::GetFirstPoint` depuis l'odomètre du joueur et le décalage
    de `GetSeqStartOfs_StoryRival` (`data/carconfig/ytConfigStory.txt`). Piste suivante : la lecture de ce fichier et
    de `/tmp/ytCarryover.txt` (absent ici), ou une donnée du joueur non initialisée.
  - Au passage : la build MSVC écrit les exposants comme glibc (`1.516e+07`), MinGW (msvcrt) avec trois chiffres
    (`1.516e+007`).

### R-Tuned et Initial D 4 Japon terminés (demande 94)
- R-Tuned (`games/rtuned/dsr`, recopié : 972 Mo, fichiers TeknoParrot à la Corbeille) : le plantage MSVC au démarrage
  ne revient plus (6 démarrages et 2 courses sans erreur) ; c'était sans doute la même cause que chez Dead Heat,
  de la mémoire neuve non mise à zéro, corrigée depuis (`malloc` et `operator new` du jeu à zéro). Mode agent :
  démo, pièces, choix de la voiture, course entière, FFB (amortisseur, couple, auto-centrage), menu de test.
  vJoy : course à Hong Kong jusqu'à 107 km/h, forces constantes et ressorts reçus.
- Initial D 4 Japon (Rev B, `games/initiald4/id4.elf`, 3 Go) : sa carte servo n'était pas émulée par défaut, d'où
  pas de FFB. Les trames de `_sSCI::send` vont maintenant au modèle de la carte, comme pour l'Export
  (`idFeedbackInstall`, `patch.c`). Mode agent : course avec 3 080 forces constantes et 35 vibrations.
  vJoy : carte de joueur lue, course sur l'Akina mouillé de 96 à 122 km/h jusqu'à « TIME IS UP », 939 forces
  constantes et 163 vibrations reçues.
- Le port série par défaut (`/dev/ttyS0`, quand aucune vraie carte n'est choisie) n'est plus signalé comme une
  erreur : le port reste fermé, comme avant le passthrough.

### Maximum Tune 3 par vJoy, et Maximum Tune 3DX+ (demande 88)
- MT3 n'avait aucune entrée hors mode agent : il n'appelle jamais `SDL_PollEvent` (ses entrées sont celles de la carte
  N2) et son échange d'images (pilote NVIDIA intégré, `admBridge.c`) ne faisait que `SDL_PumpEvents` ; les manettes,
  le clavier et F10 n'étaient donc pas traités (le mode agent les traite lui-même). `admSwapBuffers` appelle
  maintenant `pollEvents`. Test vJoy : pièces, menus à l'accélérateur, Story, course jusqu'à 120 km/h en balayant le
  volant, retour de force vers vJoy (forces constantes, ressort, amortisseur).
- Maximum Tune 3DX+ (Japon, CRC `0x85A0E7AB`, `WANGAN_MIDNIGHT_MT3DX`) : même moteur que MT3, avec ses symboles ;
  l'affichage, la clé HASP et le son de MT3 sont trouvés par symbole. Carte N2 et Kickback : mêmes champs, adresses
  relevées (table `maximumTune3dx` de `deadHeatIo.c`). `clSerialN2` a un champ de plus (descripteur à +8, port à
  +0x10) : le lecteur de cartes trouve ses fonctions par symbole et prend les décalages du jeu.
- Copie de MT3DX+ : ses liens symboliques Linux sont devenus des fichiers vides (`libso/libboost_*-1_36.so.1.36.0`,
  0 octet ; chez MT3 c'étaient de petits fichiers texte). Le chargeur de bibliothèques prend alors le même nom sans
  ses numéros de version (`...-1_36.so`, `symbolResolver.cpp`).
- Son `data/config.lua` dit `JAMMA = FALSE` (le clavier au lieu de la carte N2 : réglage de TeknoParrot) : les pièces
  passaient, pas l'accélérateur ni le volant. Les trois branches qui lisent ce réglage (`clMain::init` et les deux
  `clMain::clMain`) prennent celle de la carte (`patch.c`), sans toucher aux fichiers du jeu.
- Menu de test : la permission que donne le démarrage (`clMain` +0x19, mise par `seqBoot`) a disparu quand la démo
  tourne (MT3 la garde ; cause non trouvée), et la démo (`seqAtructMovie`) ignorait Test ; elle le prend sans elle.
  Navigation : Service et Test tenus environ 15 images.
- Essais : démarrage (contrôle des cartes, liaison entre bornes 300 s au premier démarrage seulement), démo, pièces,
  écrans de carte, constructeur, voiture, couleur, boîte, nom, mode Story, course sur la Daikoku Line ; FFB (ressort,
  amortisseur, couple) ; vJoy : course de 50 à 119 km/h, 611 forces constantes et 1 439 ressorts/amortisseurs reçus ;
  menu de test et « ゲーム設定 ». Le lecteur de cartes répond à ses commandes de démarrage ; l'insertion d'une carte
  n'est pas encore essayée sur ce jeu.
- MT3 : en fin de course le jeu imprime et écrit la carte (`53`), enregistrée ; l'éjection de fin de partie pas vue.

### Lecteur-imprimante de cartes de Maximum Tune 3 (demande 82)
- `clCardPrinter` sur `clSerialN2` port 2 (`/dev/ttyM2`) : trame de commande (STX, longueur, commande, trois zéros,
  paramètres, ETX, BCC), ACK, puis ENQ et réponse (commande, R position de la carte, P résultat, S travail, données) ;
  S '3' = occupé (nouvel ENQ 12 images plus tard). `n2CardPrinter.c` remplace `open`, `close`, `send` et `receive` de
  `clSerialN2` (les autres ports restent fermés : `deadHeatIo.c` ne s'en charge plus pour MT3).
- Sous-commandes vues dans le jeu : `10` init, `20` état, `B0 2` contrôle du distributeur (au démarrage) et `B0 1`
  carte neuve dans le lecteur (avant `B0 2` : « 2 » n'est qu'un contrôle), `33 2 1 6` lecture qui attend la carte du
  joueur, `33 0 1 6` lecture de la carte en place, `53 0 m 6` écriture des trois pistes (207 octets), `78`/`7C`
  impression, `80` éjection, `40` annulation.
- Piège : P '3' est une carte bloquée (E53 « Card jam » après trois essais), pas « pas de carte ». La lecture qui
  attend la carte répond « pas de carte, sans erreur » (R, P, S à '0') et reste en attente ; le jeu la sonde par de
  simples ENQ (`requestInsert` → `sendReadEnq`) jusqu'à la carte ou à l'annulation (`40`, fin du compte à rebours ou
  NO). Une réponse « occupé » sans fin l'empêchait d'annuler.
- `CardRW` de `testmode.lua` : 0 = ON, 1 = OFF (`isNoUseCardDevice`), 2 = sans lecteur (`isWithOutCardDevice` : le
  lecteur n'est même pas servi). Remis à 1 (OFF) dans la copie locale après les essais.
- Essais en mode agent : CARD CHECK OK ; achat d'une carte, création du joueur, impression, écriture enregistrée dans
  `Card_01.crd` ; à la partie suivante la carte est relue (« Card data confirmed »). La carte créée reste dans
  `games/mt3/Card_01.crd`.

### Cartes réelles sur un port série : passthrough (demandes 83 et 85)
- Page « Options du jeu », partie « Cartes » (même présentation que windowsloader, convenue avec lui) : une ligne par
  carte d'un port série du jeu, « Émulée » ou « Vraie carte sur COMn » parmi les ports que Windows connaît
  (QueryDosDevice, relus toutes les 2 s), au prochain lancement. Carte moteur du volant (`/dev/ttyS0` :
  `EMULATE_DRIVEBOARD`, `SERIAL_1_PATH`), carte de mouvement d'OutRun SP SDX (`/dev/ttyS1` : `EMULATE_MOTIONBOARD`,
  `SERIAL_2_PATH`), carte JVS (`EMULATE_JVS`, `JVS_PATH`). Les clés sont celles de `[Emulation]` et `[Paths]`, écrites
  dans `[Game.<nom court>]` (pour ce jeu seulement) ; « COM12 » ou « \\.\COM12 » acceptés, « COM12 » écrit.
- `hardware/lindbergh/serialPort.c` (Windows) : l'ouverture par le jeu de son port série devient celle du port COM
  (descripteur du runtime C) ; son `tcsetattr` règle vitesse (codes Bxxx), taille, parité, bits de stop et contrôle de
  flux ; ses lectures suivent `VMIN`, `VTIME` et `O_NONBLOCK` (COMMTIMEOUTS) ; `select`, `FIONREAD`, lignes du modem,
  `tcflush`, `tcdrain`, `fcntl` F_GETFL/F_SETFL. E/S « overlapped » : sans elles Windows sérialise ReadFile et
  WriteFile sur un même port, et une lecture en attente (le fil JVS) retardait les écritures du jeu de plusieurs
  secondes. Sous Linux le jeu ouvre le périphérique lui-même, comme avant.
- JVS direct (`passthrough.c`) porté sous Windows (fil de lecture, section critique). Corrigé au passage : une trame
  vide « pas de réponse » publiée avant la réponse de la carte (le jeu la prenait pour celle-ci : « JVS I/O board does
  not fulfill the game spec ») ; chaque requête efface la trame en attente, et la trame vide n'est publiée qu'après
  200 ms sans réponse.
- Le second port d'OutRun passe à une vraie carte même quand la carte du volant reste émulée (elle est sur le premier
  seul).
- Essais en agent caché sur OutRun 2 SP SDX, avec un faux boîtier Python sur COM11 (`fake_lindbergh_board.py` dans le
  dossier temporaire de la session) : carte moteur complète par COM10 (reset `FF`, init `81`, butées `FC`, réglages
  `83`/`86`/`88`, puis `84` à chaque image ; OutRun SDX envoie des trames de 7 octets, deux cartes sur la ligne, et
  attend un octet de réponse par carte) ; JVS par COM10 (reset, adresse, identification, capacités 837-14572, puis
  ~60 lectures par seconde ; les pièces du faux boîtier s'affichent : « CREDIT(S) 2 »). COM10/COM11 partagés avec
  windowsloader, pris et rendus à tour de rôle.

### Cartes de chaque jeu dans sa description (demandes 84 et 85)
- `gui::boardDescription` : la carte d'E/S et la carte moteur, dans le panneau du sélecteur et en tête de « Options
  du jeu ». Lindbergh : « Controls on JVS board, Sega 837-14572 (ou 837-13551), JVS 3.0 (carte de base) » ; servoboard
  type Sega (`/dev/ttyS0`) pour OutRun, Hummer, Initial D, Sega Race TV, R-Tuned ; carte de mouvement (`/dev/ttyS1`)
  d'OutRun SP SDX ; siège mobile (ride board) de HOD4 SP et Let's Go Jungle Special ; « aucune » sinon. Namco :
  carte d'E/S N2 (n2jvio) et carte Kickback (pas pour Nirin). Gaelco : carte d'interface, sorties moteur et vibreur.

### Modes plein écran fenêtré (demande 86)
- `DISPLAY_MODE` : `BORDERLESS` (fenêtre sans bordure sur tout l'écran, résolution du bureau, image au rapport
  d'aspect) et `BORDERLESS_STRETCH` (image étirée), à la suite des quatre modes, numéros inchangés ; mêmes noms que
  windowsloader et Supermodel. `FULLSCREEN` et `FULLSCREEN_STRETCH` passent l'écran au mode le plus proche de la
  résolution du jeu (SDL_GetClosestFullscreenDisplayMode), et retombent sur le borderless si l'écran n'en a pas ; ce
  qui était le plein écran (« bureau » de SDL) est devenu BORDERLESS. F11 et Alt+Entrée gardent le mode choisi.
- Vérifié sur OutRun en fenêtre visible (créneau pris à windowsloader) : BORDERLESS 3072×1920 sans bordure, écran
  inchangé ; FULLSCREEN : écran en 1280×768 (le plus proche des 800×480 du jeu sur cet écran), rendu en sortant.

### Options du jeu dans F10 (demande 81)
- Onglet « Options du jeu » de la fenêtre des paramètres, en jeu seulement : le titre du jeu, où c'est enregistré
  (« Dans [Game.mt3] de linuxloader.ini, pour ce jeu seulement »), puis ses options, ou « Aucune option pour ce jeu ».
  Case à cocher, liste ou curseur ; « (au prochain lancement) » sur celles qui attendent le redémarrage, et un rappel
  en bas quand l'une diffère de la valeur du lancement.
- Fichier : section `[Game.<nom court>]` du fichier de réglages utilisé (le commun ou `<nom court>.ini`), lue après
  les autres sections : elle passe par-dessus les clés générales (`[Emulation] EMULATE_DRIVEBOARD`, `[GameSpecific]`…)
  pour ce jeu seulement. Les deux boutons « Sauvegarder » l'écrivent. `--set Game.CLE=VALEUR` vise la section du jeu
  lancé. Disposition convenue avec windowsloader (même section, même onglet, mêmes types, même rappel).
- Déclaration : `config/gameOptions.c`, une table (clé, libellé et aide en anglais traduits par `tr()`, type, champ
  de `EmulatorConfig`, bornes ou liste, prochain lancement ou non, prédicat « s'applique à ce jeu ») ; page dans
  `gui/guiGameOptions.cpp`.
- Options : Maximum Tune 3 `NO_CARD_READER` (vrai par défaut : `clTestMode::isNoUseCardDevice` rend vrai, le jeu se
  joue comme avec CARD R/W à OFF sans passer par son menu de test) ; carte servo des jeux de conduite Lindbergh ;
  contrôles de borne d'OutRun ; scintillement de Hummer ; lecteurs de cartes d'Initial D et HW210 (VT3, VF5,
  R-Tuned) ; course du volant d'Initial D (tout de suite) ; chinois d'ID5 ; Let's Go Jungle (Mesa) ; Primeval Hunt
  (écrans, menu de test, Zink) ; MJ4 et AxA à toute heure ; Rambo ; Harley ; viseurs de Ghost Squad Evolution.
- Vérifié sur Maximum Tune 3 (MinGW et MSVC) : page affichée (capture faite hors écran par une aide de test, retirée
  ensuite), rappel après un changement, sauvegarde dans `[Game.mt3]` d'une copie du fichier ; lecture au démarrage
  vue dans la mémoire du jeu (`isNoUseCardDevice` détourné par défaut, intact avec `[Game.mt3] NO_CARD_READER = false`
  ou `--set Game.NO_CARD_READER=false`). Les pages des jeux Lindbergh ne sont pas vues (pas de copie locale).

### Maximum Tune 3 (demande 78)
- Copié de racecab1 (16 145 fichiers, 4,6 Go), nettoyé des fichiers TeknoParrot. Programme `main` (CRC partiel
  0xEC016AFD), moteur Alchemy (Gap) de 2007, 50 000 symboles.
- **Bibliothèques** : `libso` (Boost 1.33.1, Cg) cherché ; les liens symboliques copiés sous Windows (fichiers de
  quelques octets qui contiennent le nom de la cible) suivis par `FollowLinkFile` (`symbolResolver.cpp`).
- **libc** : `sigprocmask`, `sigwait`, `psignal`, `setitimer`, `setpgid`, `setsid`, `getppid`, `geteuid`, `execl`,
  `adjtimex`, `chown`, `sbrk` (région de 768 Mo réservée, engagée au fur et à mesure : Alchemy y prend ses pools,
  55 Mo d'emblée), `mmap64`, `mlock`/`munlock`/`msync`, `vfscanf`, `gnu_get_libc_version`, `__wcstol_internal`,
  `iswalpha`, `swprintf` ; sémaphores System V et `ftok` (`ipcBridge.cpp`) ; `aio_read`/`aio_error`/`aio_cancel`
  (lecture faite tout de suite) et `ftruncate` ; `_pthread_cleanup_push/pop`, `pthread_setcanceltype`,
  `pthread_testcancel` ; `ceilf`, `log2`, `lrint`.
- **Démarrage** : `system("perl prepend-n2.pl")` fait par le loader (dossiers `tmp/data/...`, `sys_04.wav`) ;
  `feof` du faux `/proc/cpuinfo` (Alchemy le lit en `while (!feof)`) ; `getc` sur l'entrée standard rendue non
  bloquante par `fcntl` rend EOF (clavier d'Alchemy ; en mode agent elle est au client).
- **Affichage** (`graphics/admBridge.c`) : les fonctions `adm*` du pilote NVIDIA lié dans le programme répondues par
  la fenêtre du loader (`sdl12SetVideoMode`, le swap de SDL 1.2) ; les 1 102 emplacements OpenGL de `.writetext`
  (table lue dans le code d'`admGetProcAddress`) sautent aux fonctions de `glHooks`, ou à un bout de code qui signale
  la fonction absente. `LL_ADM_TRACE=1` compte les appels de chaque fonction (journal toutes les 600 images) : c'est
  ce qui a montré `glGetCompressedTexImageARB`, absente, derrière les voitures noires.
- **Clé HASP** (`hardware/namco/namcoHasp.c`) : `hasp_login/read/write/encrypt/decrypt/get_rtc/legacy_set_rtc` et
  `clHasp::isAvail` remplacés ; image de 4 Ko (`hasp.bin`) : numéro de série en 12 chiffres ASCII à 0xD00 (somme et
  complément en fin de bloc), bloc du jeu à 0 (compteur des cartes) ; l'image est en clair.
- **E/S** (`deadHeatIo.c`) : table `maximumTune3` (bloc de Nirin, `init(bool)`), calibrage en centre et course
  (`centreWidth`), champs de la Kickback par jeu, levier en H (`GEARBOX_H`) : Gear Up/Down (boutons 7 et 8) font
  N→1→…→6 sur les contacts gauche, droite, haut, bas (boutons 5, 6, 3, 4) ; View Change bouton 9, Intrude Change
  bouton 10 (Boost) ; Test n'est pas un interrupteur à bascule pour ce jeu (c'est l'Enter de son menu).
- **/tmp** : `fopen`, `fopen64`, `remove` et `rename` le prennent maintenant dans le dossier du jeu comme `open`,
  `unlink` et `mkdir` (les réglages du menu de test, écrits par flux C++ en `testmode.tmp` renommé, se perdaient).
- **Son** (`hardware/namco/n2Sound.c`) : 69 fonctions `al*`/`alc*` de l'OpenAL NVIDIA du programme renvoyées à
  `openal32.dll` (OpenAL Soft) ; en mode agent, sortie nulle, ou `LL_SOUND_WAV` (écrivain WAV d'OpenAL Soft par un
  fichier de configuration temporaire, `ALSOFT_CONF`). Plantage d'OpenAL Soft une fois sur deux vers 27 s (MinGW) :
  `movdqa` sur une pile alignée à 4 dans ses mixeurs SSE ; `disable-cpu-exts = all` pour ce jeu (6 essais sur 6
  sans plantage ensuite).
- **Essais** (mode agent, MinGW) : démarrage, menu de test (GAME OPTIONS : CARD R/W mis à OFF), démo, partie
  Story jusqu'à la course, FFB lu par l'agent, SWITCH TEST.
- **MSVC** : tout pareil jusqu'au chargement de la course, puis le thread de course boucle dans
  `CSectionManager::GetSectInfo_InterArea` : `while (x < 0) { MoveToPrev; x += longueur; }` avec un `x` NaN
  (la position d'une voiture rivale). Contrôle x87 lu : 53 bits sous MSVC, 64 bits pour une partie des threads
  sous MinGW, 64 bits sous Linux ; passé à 64 bits sur tous les threads du jeu : sans effet, retiré. À chercher
  d'où vient le NaN (une fonction mathématique du pont, une donnée non initialisée selon l'ordre des threads).
- Reste : le lecteur-imprimante de cartes (`clCardPrinter` sur le port 2 de `clSerialN2`, protocole Sanwa : trame
  STX, longueur, commande, trois octets d'état, données, ETX, XOR ; ACK puis ENQ ; commandes 10 init, 20 état, 33
  lecture, 53 écriture, 7A/7C/7D/78 impression, 80 éjection, A0 nettoyage, B0 distributeur, 40 annulation, F5
  pile ; état : position de la carte '0' à '4', erreur, fin de travail), la course en MSVC, vJoy, l'écoute du
  son ; puis Maximum Tune 3DX+.

### Nirin et Dead Heat Riders sans dongle (demande 77)
- **Nirin** : `clHASP::check` fait `hasp_login`, `hasp_read` de 64 octets puis `hasp_decrypt`, dont le résultat ne
  sert qu'au numéro de série affiché : les trois fonctions de la bibliothèque HASP liée au jeu sont répondues comme
  celles de Dead Heat (`dhHaspLogin`, `dhHaspRead`, `dhHaspDecrypt`). Le jeu passe l'écran titre ; « Insert 2 Coin(s) »
  ; OK des menus = bouton View (bouton 2), Select = pencher la moto. Course : tour complet, jusqu'à 249 km/h.
- **Calibrage du guidon de Nirin** : sans `save0/testmode/handle_setting.lua` (écrit seulement par son menu de
  test), `clHandleSetting::read` laisse des zéros et le guidon tourne à gauche des deux côtés. Le loader appelle
  alors la remise à zéro du menu de test (`clHandleSetting::clear` : copie du fichier par défaut de `data/save` par
  le système de sauvegarde, puis lecture). Valeurs du jeu : -1 à gauche, +1 à droite, accélérateur et frein 0 à 1.
- **« BACKUP ERROR » à chaque démarrage de Nirin** : deux causes, corrigées pour tous les jeux (ci-dessous) : la
  liste `find` écrite en mode texte (le `\r` dans les noms) et `st_ctime` de Windows (date de création) que le jeu
  compare à `st_mtime` (« ghost initialize because of Time Diff »).
- **Dead Heat Riders** : sa « clé HASP » est un appareil USB (0B9A:0C10) dont `clHASP::Check` lit seulement le
  numéro de série (commençant par 2706, puis 2, puis 01) ; la libusb du loader présente cette clé pour ce jeu.
- **Cartes de Dead Heat Riders** : `n2jvio` et `clSystemN2` comme Dead Heat (bloc d'état à 0x08F9C2A0, même
  disposition), champs du calibrage espacés de 16 octets, frein au repos et à fond ; carte Kickback sur son propre
  port série, type de couple lu à 0 (`setTRQPowerType` : `mov eax,[ebp+0xc]` → `xor eax,eax`), auto-test passé ;
  `n2JvioAckTxVsync` répond aussi (appels directs du flux principal). Pas de levier : boutons de `l_as_dhb_list` :
  Nitro (Boost, bouton 4, aussi « DECIDE »), Vue (bouton 5), Enter (bouton 1), frein en contact (bouton 3).
- **Démarrage de Dead Heat Riders** : environ 60 s dans le contrôle du réseau (borne seule : minuterie de conflit
  de 3 600 images), puis 10 s d'attente de la caméra (`nmUVCCamera`, pas de `/dev/video0` : elle abandonne seule ;
  ensuite son `nmUVCCameraExit` ferme le descripteur 0, d'où la règle sur `close(0..2)`).
- **Course de Dead Heat Riders** : villes, choix de la moto, photo (sans caméra), course à San Francisco jusqu'à
  198 km/h, nitro, retour de force (ressort, amortisseur, couple constant en virage). MinGW et MSVC. Menée aussi par
  vJoy dans une fenêtre visible (jusqu'à 207 km/h ; 307 forces constantes et 469 conditions reçues par vJoy).
- **Corrections pour tous les jeux** (trouvées avec ces deux jeux) :
  - `rename` remplace un fichier existant, comme Linux (sans dossier d'écriture, le CRT refusait) ;
  - `st_ctime` de `stat` vaut au moins `st_mtime` (sous Windows c'est la date de création) ;
  - `system("mkdir X")` et `system("find ... | sort > F")` faits par le loader ; la liste est écrite avec des
    fins de ligne `\n` seules (en mode texte, le `\r` restait dans les noms : `fopen` échouait, EINVAL) ;
  - `setlocale` répond comme la libc de Linux (ses numéros de catégorie : `LC_ALL` = 6, refusé par Windows, d'où un
    `strlen(NULL)` dans la libstdc++ de Dead Heat Riders en lisant un nombre) et garde la locale du processus à
    « C » (`setlocale(LC_ALL, "")` aurait mis la virgule décimale d'un PC français partout) ;
  - `close(0)`, `close(1)`, `close(2)` d'un jeu sont ignorés (l'entrée et la sortie de l'agent, le journal) ;
  - stockage local aux threads des objets partagés (PT_TLS, relocations 35 et 36, `__tls_get_addr` et
    `___tls_get_addr`) : les variables de `std::call_once` de `libstdc++.so.6.0.14` ;
  - `putenv`, un vrai `longjmp`/`_longjmp`, `mkstemp`, `tmpfile`, `pthread_setaffinity_np`,
    `SDL_JoystickGetButton`, `glColor4d`, `glVertex3d`, `glVertex2fv`, `glMultiTexCoord4fv`.

### Nirin (demande 74)
- Namco ES1, 2009 (`NRN100-4-NA-DAT0-A37`, application `v351`) : `exec/a.elf` avec ses symboles, SDL 1.2, OpenAL ;
  copié de racecab1 dans `games/nirin` (1,65 Go, 8 678 fichiers ; `teknoparrot.ini` et `BudgieLoader.exe.stackdump`
  mis à la Corbeille). Entrée de jeu `NIRIN` (CRC partiel 0x71E72FF6), groupe Dead Heat.
- Fonctions ajoutées : `putenv`, `longjmp`/`_longjmp` (un vrai retour à `_setjmp`, pour les erreurs Lua),
  `mkstemp`, `tmpfile`, `pthread_setaffinity_np`, `SDL_JoystickGetButton`.
- **Carte d'E/S N2** : la même bibliothèque `n2jvio` que Dead Heat, plus ancienne : son bloc d'état est rangé
  autrement (système 0x104, joueur 0x108, monnayeur 0x128, crédits de service 0x178, analogiques 0x1A8), sans carte
  Kickback ni levier de vitesses. `deadHeatIo.c` prend ces positions dans sa table par jeu ; le service de la carte
  (`execSystemN2Step2`) est appelé par un thread de signal du jeu, remplacé comme les autres.
- **Sauvegardes** : le script `postmount` de la borne crée `save0` et `save1` à côté de `a.elf` ; le loader les crée.
  Le jeu y range ses fichiers par `system()` : `mkdir save0/...` (fait par le loader) et
  `find DOSSIER ... | sort > ./save0/tmp/find.txt` (listes de fichiers, faites par le loader des deux côtés du
  dossier d'écriture). Il écrit `nom.tmp` puis le renomme sur `nom` : sans dossier d'écriture, `rename` du loader
  refusait de remplacer un fichier existant (CRT Windows) ; il remplace maintenant, comme Linux.
- **Commandes** : boutons du bloc → boutons du jeu par `l_as_list` : Service, Enter (bouton 1, Start du loader),
  Select (haut/bas), shift up (bouton 3) et shift down (bouton 4, vérifiés dans le test des commandes) ; guidon,
  accélérateur et frein en comptes calibrés. Le guidon de Nirin est calibré gauche en haut (`handle_setting.lua` :
  gauche 61056, centre 40768, droite 18112) : la vérification de calibrage accepte les deux sens.
- **Essais** (mode agent, MinGW et MSVC) : contrôles du démarrage passés (BACKUP DATA, JV I/O, CLOCK), démo, pièces
  comptées (CREDIT 2/2), interrupteur Test (enclenché) : menu de test, I/O TEST, SWITCH TEST.
- Ensuite bloqué par sa clé HASP (« USB DONGLE ERROR ») : levé à la demande 77 (ci-dessus).

### Six jeux non protégés (demandes 55 à 59)
- Copiés de `\\racecab1\C\racecab\Games` par robocopy (`/R:600 /W:30 /MT:4`, le partage décroche) : `initiald4fr`
  4,8 Go, `maxheat3d` 3,0 Go, `deadheatriders` 2,4 Go, `ringriders` 182 Mo, `tokyocop` 440 Mo, `gaelcotuning` 578 Mo.
- **Initial D 4 Export FR** (reconnu comme Rev D, SBNK) :
  - « Card Stocker Cleaning » au démarrage : le `sram.bin` neuf du loader met à 0 le compteur du stocker
    (`cBookKeeping`, octet 0x18B, 200 après nettoyage) ; `doesNeedStockerCleaning` et `doesNeedRollerCleaning`
    renvoient 0 pour la Rev D, comme pour ID5.
  - Image de 800 px de large, décalée et coupée en haut : `initialD.ini` laissé par TeknoParrot dit
    `Width = 1920, Height = 1080` ; le jeu n'accepte que 640 à 1600 de large, retombe à 800 et garde 1080. Le loader
    imposait la résolution seulement hors 1360×768 : il l'impose maintenant toujours (Rev B, C, D).
  - `id4.elf` (= `id4.elf ORIG`) est l'ELF anglais, retouché en 1920×1080 à 0x0837B223 ; la traduction est dans les
    données (démo, avertissements) ; `id4.elf originallllll` a ses textes en français.
- **Fonctions génériques ajoutées** (pour ces jeux, utiles à d'autres) :
  - X11 : `XIfEvent` (MapNotify et Expose synthétiques), `XAllocSizeHints`, `XCreateImage`/`XPutImage`/`XCreatePixmap`,
    `XCreateGC`/`XFreeGC`, opérations de fenêtre sans effet, économiseur d'écran, DPMS, keysyms.
  - GL : `glLoadMatrixd`, `glTexGenf`, `glRasterPos4f`, `glBlitFramebufferEXT`, `glRenderbufferStorageMultisampleEXT`,
    register combiners NVIDIA (passés au pilote s'il les a, ignorés sinon), vertex array range (mémoire de
    `glXAllocateMemoryNV` par malloc), `gluBuild2DMipmaps`.
  - libc et maths : `ctime`, `stime`, `sigaddset`, `ceil`, `floor`, `log2f`, `sincos`, `sincosf`.
  - Image disque Linux : un jeu dont l'ELF est dans une arborescence `home`, `etc`, `lib`, `bin` (les Gaelco) voit
    ses chemins absolus pris dans cette image (`ConvertPath`).
  - Options de socket : table Linux → Winsock complète (SO_*, IP_*, multicast, délais en timeval, linger).
  - `system()` : `sudo ifconfig eth0` répond succès ; `perl ./data/ifconfig.pl > fichier` (Namco) écrit la ligne
    attendue (eth0, adresse, masque, MAC, lien), cohérente avec les ioctl SIOCGIF* du loader.
- **Maximum Heat 3D** (US DRIVE rév. 8807, le moteur de Dead Heat rév. 6273) :
  - Entrée de jeu `MAXIMUM_HEAT_3D` (CRC partiel 0xF932E88E) et groupe `GROUP_DEAD_HEAT` commun aux deux jeux
    (shaders, entrées, interrupteur Test).
  - Mêmes patches que Dead Heat aux adresses de ses symboles (HASP, link group, deux destructeurs) ; carte d'E/S N2
    et carte Kickback de `deadHeatIo.c` par une table d'adresses par jeu (mêmes champs, `clKickback` décalé de 4
    octets après 0x7C).
  - Pas le patch du PCB ID : `clNet::getPcbId` est en base 0 ici (option du menu de test moins 1).
  - Shaders : ses sources commencent par `#version 120` ; la déclaration de `gl_TexCoord` va après.
  - Caméra : `USE_CAMERA_DEVICE` (octet 2 de `clConfig`) forcé à FALSE, pas de `/dev/video0` (son `config.csv`
    d'origine l'active, celui de Dead Heat a été modifié).
  - Reste : le contrôle du link au démarrage échoue (« PCB-ID dupliqué ») ; le jeu ne reçoit pas ses propres paquets
    (aucun `sendto` vu) ; Dead Heat passe grâce à son `save0` venu d'une borne (`ifconfig.txt`, options du test).
- **Gaelco** : leurs ELF (`home/joc2001/joc200x/salidas/gameport` ou `car`) parlent à la carte d'E/S et de son par
  des appels système ajoutés au noyau Gaelco (`int 0x80`, n° 229 à 236, fonctions `iogaelco_*`), remplacées par
  `hardware/gaelco/gaelcoIo.c` (registres, ports, sorties moteur → trames servo) et `gaelcoSound.c` (la carte son).
- **Gaelco Tuning** : son abandon après 40 s (« Material not found! ») venait du loader : le jeu lie le dérouleur C++
  d'Intel et enregistre son `.eh_frame` par son propre `__register_frame_info` (PLT) ; le loader branchait ce symbole
  sur libgcc de MinGW, la liste du jeu restait vide et l'exception, normalement attrapée, finissait en abort. La
  définition du programme gagne maintenant (`elfLoader.cpp`, comme ld.so). Résolution native 720×540.
- **Volant Gaelco** : gauche = valeur haute sur leur carte ; Ring Riders et Tokyo Cop tournaient à l'envers et leur
  moteur poussait du côté du braquage ; vérifié dans les trois écrans de test et par le signe du couple.
- **Tokyo Cop** : menu de test par le code de touche 0x14 du port 0x42 (le loader y renvoyait 0xFF, une touche du
  pavé numérique toujours enfoncée) ; frein = contact (port 0x40 bit 4), sirène et vue sur le port 0x41.
- **`findStaticFnAddr` (Windows)** : seulement les symboles définis par les ELF chargés (`FindNativeSymbol`) ; la
  résolution générale rendait le pont du loader pour un nom importé, détourné alors sur lui-même (`jmp $`) : les jeux
  Gaelco bouclaient sur MSVC dans `CreateGLWindow` (vu au débogueur, cdb).
- **`isWinsockHandle`** : un descripteur ouvert de la libc (`_get_osfhandle`) est un fichier ; sinon, la valeur d'un
  fichier égale à celle d'une socket envoyait sa lecture dans `recv` (Ring Riders bloqué dans `CFileCacher::read`).
- **MSVC** : les trois jeux Gaelco démarrent ; Tuning jusqu'en course.
- **Son Gaelco** : échantillonneur de 16 voies à 48 kHz ; banque `roms.new` envoyée par `dump_sound_buffer` (en-têtes
  de 32 octets du `sound.h` de Tuning) ; commandes `add_comm` 1 à 7 ; IMA ADPCM quartet haut d'abord ; gain Q14 ×
  volume général (réglage du jeu au carré). Tokyo Cop attendait « tampon prêt » sur la commande d'interface 11 (13
  ailleurs) avant d'envoyer quoi que ce soit. Capture : `LL_SOUND_WAV=<fichier>`. Musique de Tuning capturée identique
  à son échantillon (corrélation 1,000).
- **vJoy (jeux Gaelco)** : joués de bout en bout par vJoy le 22/10 au soir. Ce n'était pas la boîte du pare-feu :
  les jeux Gaelco chargent 45 à 60 s en lancement normal (pièce perdue si envoyée avant), et vJoy n'expose plus que 4
  axes (X, Y, Z, Rz) : l'accélérateur est l'axe 3 de SDL, `build-mingw/controls.ini` disait l'axe 5 (vJoy à 6 axes) ;
  corrigé. Service se tient environ une seconde.

### Dossier d'écriture pour UWF (demandes 52 et 53)
- But de l'utilisateur : UWF (filtre d'écriture de Windows) sur tout C: sauf un dossier, par exemple
  `C:\Racecab\Saves`, avec un sous-dossier par émulateur et par mode (single, link), puis un par jeu. Tout le reste
  en lecture seule ; en overlay : réglages de l'utilisateur, NVRAM, fichiers écrits par les jeux.
- Spécification commune envoyée aux sessions sm2-emu (« Compilation »), Supermodel (« Driveboard émulation et force
  feedback ») et Flycast (« Multi-Board en thread et non process ») : `--write-dir=<chemin>` prioritaire, ou une clé
  de config ; réglages à la racine (copiés la première fois, puis lus et sauvegardés là) ; `<chemin>\<jeu>` pour
  NVRAM, sauvegardes, logs, temporaires, caches de shaders ; lecture dans le dossier d'écriture d'abord, sinon à
  l'origine, copie avant modification ; rien d'écrit ailleurs. Le front-end choisit le chemin selon le mode.
- linuxloader (`redirections/writeDir.c`, Windows) : `--write-dir` ou `[Paths] WRITE_DIR`.
  - Réglages : `WRITE_DIR\linuxloader.ini` et `controls.ini`, copiés depuis leur place la première fois.
  - Le loader passe dans `WRITE_DIR\<jeu>` (nom court du jeu : `outrun2-rev-a`, `deadheat`...) une fois le jeu
    connu ; les lectures manquantes y sont cherchées dans le dossier du jeu.
  - Passent par lui : fopen, fopen64, open, mkdir, remove, unlink, rename (cible remplacée, comme sous Linux),
    creat, access, stat, opendir (les deux côtés listés), glob64, rmdir, dlopen, et les fichiers du loader (eeprom.bin,
    sram.bin, cartes, cache de shaders).
  - Le relais vers le menu de test passe l'option au programme suivant ; ses commandes cmd.exe tournent dans le
    dossier d'écriture (local), donc plus de souci de dossier courant UNC.
- Essais (mode agent) : OutRun local 2400 images et Dead Heat local 3000 images, dossiers des jeux intacts (aucun
  fichier modifié ni ajouté) ; NVRAM copiée puis écrite dans le dossier d'écriture ; sauvegardes de Dead Heat
  (testmode, coin, clock) là aussi.
- Depuis le partage de racecab1 : Dead Heat lancé depuis `\\racecab1\C\racecab\Games\Dead Heat - Namco ES1`,
  dossier d'écriture sur ce PC : aucun fichier modifié sur la borne. Prêt en 100 s (11 s en local), 3000 images en
  147 s (50 s en local) : le jeu attend ses données sur le réseau. À mesurer sur le réseau des bornes, et à voir côté
  loader (lectures non tamponnées ?) avant d'envisager les jeux sur un partage.
- Restent : le picker (il ne transmet pas `--write-dir`, la clé de linuxloader.ini le fait), la compilation Linux
  (fonctions sans effet hors Windows).

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

### Rapports d'aspect étirés et recadrés (demande 69)
- Avant l'alignement sur windowsloader (matin du 22/10), `4:3` et `16:9` remplissaient le cadre en déformant l'image ;
  l'alignement leur a donné les bandes noires. Les deux remplissages déformés reviennent sous leurs propres noms :
  `4:3_STRETCH` (un jeu 16:9 comprimé dans un cadre 4:3), `16:9_STRETCH` (un jeu 4:3 élargi).
- `4:3_CROP` et `16:9_CROP` : l'image remplit le cadre à ses proportions, la partie qui déborde coupée (les côtés d'un
  jeu 16:9 en 4:3, le haut et le bas d'un jeu 4:3 en 16:9) ; `CROP_POSITION` (−100 à gauche ou en haut, 0 au milieu,
  100 à droite ou en bas) place la partie gardée. Lu à chaque image : le curseur de F10 agit en direct.
- `blitPresent` : le cadre a la forme du rapport (la fenêtre aussi en `WINDOW`), placé par le mode d'affichage ; dans
  le cadre, l'image avec bandes, étirée ou recadrée (rectangle source du blit, qui garde aussi le recadrage de
  Chihiro).
- Vérifié en mode agent (`screen`, configurations par `-c`) : Tuning 720×540 en 16:9 (bandes, étiré, recadré à −100,
  0 et 100) ; Dead Heat Riders 1360×768 en 4:3 (bandes, étiré, recadré à −100, 0 et 100) ; F10 vu en lancement
  normal (liste et curseur).
- `CROP_BARREL` (demande 70) : le milieu du cadre (`BARREL_CENTER` % de lui) comme le recadrage le montre, les parties
  coupées comprimées sur les bords comme sur un cylindre vu de face : un point à l'arc s du bord du milieu se montre à
  sin(s / R), R le rayon (`BARREL_RADIUS` % du cadre), mis à l'échelle pour que le bord de l'image tombe sur celui du
  cadre, un quart de tour au plus (petit rayon : forte courbure au bord ; grand : compression presque uniforme). Un
  bord qui a moins d'image que de place s'étire seulement. `CROP_POSITION` déplace le milieu.
- Dessin : une bande de quadrilatères texturés (la texture du FBO), 64 pas par bord, en fonction fixe ; l'état GL du
  jeu sauvé et rendu autour (attributs, matrices, programme GLSL, programmes ARB et NV, combineurs NV).
- Vérifié en mode agent : Dead Heat Riders en 4:3 recadré (l'astérisque du coin, coupé sans courbure, revient au bord ;
  milieu 60/30/100/0 %, rayon 20/50/300 %, position −100) ; Tuning en 16:9 recadré (l'horloge en haut et « credits »
  en bas reviennent, comprimés ; les arêtes se courbent près des bords avec un petit rayon).

### Réglages par jeu et `--set` (demandes 71 et 72)
- `config.c` : `initConfig` connaît le jeu avant de lire le fichier ; quand le fichier est `linuxloader.ini` (celui du
  dossier courant ou du dossier d'écriture, pas un `-c` à un autre nom), `<nom court>.ini` du même dossier est lu à sa
  place s'il existe (« Settings of this game » dans le journal). C'est le même fichier que le picker cherchait déjà à
  côté du programme. `getConfigFilePath()` donne le fichier lu ; F10 y sauvegarde.
- F10 « Sauvegarder pour ce jeu » : `<nom court>.ini` à côté du fichier en cours, copié la première fois, puis écrit ;
  les sauvegardes suivantes y vont.
- `--set SECTION.CLE=VALEUR` (ou `--set=`), pris par `mainW.cpp` avant les arguments du jeu, appliqué par
  `iniSetValue` sur le fichier lu (ou sur les valeurs par défaut s'il n'y a pas de fichier) ; section sans casse, clé
  en majuscules ; jusqu'à 32. Décrit dans `--help`.
- Essais (mode agent, dossier de test) : `--set Display.ASPECT_RATIO=16:9_STRETCH` donne 960×540 sur Tuning ;
  `gaelcotuning.ini` présent : 960×540, absent : 720×540 ; fichier du jeu plus `--set …=4:3` : 720×540.
- Spécification commune (rapports, recadrage courbé, réglages par jeu, `--set`) envoyée à windowsloader, Flycast,
  Supermodel et sm2-emu à la demande de l'utilisateur.

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
