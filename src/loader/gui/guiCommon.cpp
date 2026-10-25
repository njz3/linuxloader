// Shared pieces of the game picker and of the Tab settings overlay.
//
// The interface is modelled on sm2-emu's (https://github.com/dmanlfc/sm2-emu), a Sega Model 2 emulator by Daniel
// Martin (dmanlfc) and the sm2-emu contributors, GPL-3.0-or-later: its dark style, its scaling with the window, its
// picker with a description pane and its tabbed settings. This is a modified work based on sm2-emu's interface;
// the About tab says so, and credits Flycast (https://github.com/flyinghead/flycast), whose controls page the
// Controls tab follows (guiControls.cpp).

#include "guiCommon.h"

#include <glad/gl.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "../../imgui/imgui.h"
#include "../../imgui/backends/imgui_impl_opengl2.h"
#include "../../imgui/backends/imgui_impl_sdl3.h"

#include "gui.h"
#include "../config/config.h"
extern "C"
{
#include "../graphics/fpsLimiter.h"
}

#ifdef _WIN32
#include <windows.h>
#endif

namespace gui
{
namespace
{
bool g_initialised = false;
bool g_french = false;
float g_uiScale = 0.0f;
ImGuiStyle g_baseStyle;
std::string g_status; // result of the last save, shown under the buttons

// The list's tooltip: the games patchResolution.c resizes, and their limits
const char *internalResolutionGames =
    "Games whose render size is patched:\n"
    "  OutRun 2 SP SDX: any size above 800x480 (640x480 is the Chihiro mode)\n"
    "  Sega Race TV: any size, with black bars of its own beyond 4:3\n"
    "  R-Tuned: any size\n"
    "  Initial D 4: any size (the service menu stays at 1360x768)\n"
    "  Hummer, Hummer Extreme: any size, TeknoParrot's 2560x1440 in kernel.nut too\n"
    "Patched, not tried here:\n"
    "  After Burner Climax, Harley-Davidson, Rambo, Too Spicy, Virtua Fighter 5,\n"
    "  House of the Dead 4 and EX, Let's Go Jungle: any size\n"
    "  Initial D 5: 1360x768 and above\n"
    "  House of the Dead 4 Special: above 1024x768\n"
    "  Primeval Hunt: above 1280x480\n"
    "  Virtua Tennis 3: above 1360x768\n"
    "  Let's Go Jungle Special: the width chosen, the height follows at 16:9\n"
    "Fixed size: Ghost Squad Evolution, Mahjong Fight Club 4.\n"
    "Other games, Dead Heat included: keep Native.";

// English text -> French
const std::unordered_map<std::string, const char *> &frenchTexts()
{
    static const std::unordered_map<std::string, const char *> texts = {
        {"Settings", "Paramètres"},
        {"Display", "Affichage"},
        {"Force feedback", "Retour de force"},
        {"Outputs and network", "Sorties et réseau"},
        {"Picker", "Catalogue"},
        {"About", "À propos"},
        {"Aspect ratio", "Rapport d'aspect"},
        {"Original", "Original"},
        {"Display mode", "Mode d'affichage"},
        {"Window, square pixels", "Fenêtre, pixels carrés"},
        {"Window, stretched", "Fenêtre étirée"},
        {"Full screen", "Plein écran"},
        {"Full screen, stretched", "Plein écran étiré"},
        {"Borderless fullscreen window", "Plein écran fenêtré (sans bordure)"},
        {"Borderless fullscreen window, stretched", "Plein écran fenêtré étiré"},
        {"Original: the shape of the game's render resolution.", "Original : la forme de la résolution de rendu du jeu."},
        {"4:3, black bars", "4:3, bandes noires"},
        {"16:9, black bars", "16:9, bandes noires"},
        {"4:3, stretched", "4:3, étiré"},
        {"16:9, stretched", "16:9, étiré"},
        {"4:3, cropped", "4:3, recadré"},
        {"16:9, cropped", "16:9, recadré"},
        {"Crop position", "Position du recadrage"},
        {"Black bars: the frame's shape, with the whole picture inside at its own shape and black bars where the shapes differ. Stretched: the picture fills the frame, deformed. Cropped: the picture fills the frame at its own shape, cut on the sides (or above and below); the crop position moves the part kept. The display mode then fits the frame to the window or the screen, or stretches it.",
         "Bandes noires : la forme du cadre, avec l'image entière dedans à ses propres proportions et des bandes noires là où les formes diffèrent. Étiré : l'image remplit le cadre, déformée. Recadré : l'image remplit le cadre à ses proportions, coupée sur les côtés (ou en haut et en bas) ; la position du recadrage déplace la partie gardée. Le mode d'affichage ajuste ensuite le cadre à la fenêtre ou à l'écran, ou l'étire."},
        {"-100: left or top, 0: the middle, 100: right or bottom.", "-100 : à gauche ou en haut, 0 : au milieu, 100 : à droite ou en bas."},
        {"Bend the cut parts onto the edges", "Courber les parties coupées sur les bords"},
        {"Undistorted middle", "Milieu non déformé"},
        {"Curvature radius", "Rayon de courbure"},
        {"The whole picture stays visible: the middle (in % of the frame) as cropped, the parts the crop would cut squeezed onto the edges as on a cylinder seen from the front. A small radius bends hard at the edge, a large one squeezes evenly.",
         "L'image reste entière : le milieu (en % du cadre) comme recadré, les parties que le recadrage couperait comprimées sur les bords comme sur un cylindre vu de face. Un petit rayon courbe fort au bord, un grand comprime uniformément."},
        {"Internal resolution", "Résolution interne"},
        {"Native (AUTO)", "Native (AUTO)"},
        {"%d x %d (custom)", "%d x %d (personnalisée)"},
        {"Hover the list for the games it works with.", "Survolez la liste pour voir les jeux concernés."},
        {internalResolutionGames,
         "Jeux dont la taille de rendu se patche :\n"
         "  OutRun 2 SP SDX : toute taille au-delà de 800x480 (640x480 est le mode Chihiro)\n"
         "  Sega Race TV : toute taille, avec ses propres bandes noires au-delà du 4:3\n"
         "  R-Tuned : toute taille\n"
         "  Initial D 4 : toute taille (le menu de service reste en 1360x768)\n"
         "  Hummer, Hummer Extreme : toute taille, même avec le 2560x1440 du kernel.nut de TeknoParrot\n"
         "Patchés, pas essayés ici :\n"
         "  After Burner Climax, Harley-Davidson, Rambo, Too Spicy, Virtua Fighter 5,\n"
         "  House of the Dead 4 et EX, Let's Go Jungle : toute taille\n"
         "  Initial D 5 : 1360x768 et au-delà\n"
         "  House of the Dead 4 Special : au-delà de 1024x768\n"
         "  Primeval Hunt : au-delà de 1280x480\n"
         "  Virtua Tennis 3 : au-delà de 1360x768\n"
         "  Let's Go Jungle Special : la largeur choisie, la hauteur suit en 16:9\n"
         "Taille fixe : Ghost Squad Evolution, Mahjong Fight Club 4.\n"
         "Autres jeux, Dead Heat compris : laisser Native."},
        {"The size the game computes and draws its frame at, when it can be patched to; then fitted to the aspect ratio and shown by the display mode. Takes effect at the next start.",
         "La taille à laquelle le jeu calcule et dessine son image, quand on peut le patcher ; elle est ensuite recadrée selon le rapport d'aspect et affichée selon le mode. Prise en compte au prochain lancement."},
        {"Limit the frame rate", "Limiter la fréquence d'images"},
        {"Target frame rate", "Fréquence visée"},
        {"Show the frame rate (F12)", "Afficher la fréquence d'images (F12)"},
        {"Hide the mouse pointer", "Masquer le pointeur de la souris"},
        {"Strength", "Intensité"},
        {"Strength of the drive board's forces on the wheel, in percent of the board's full scale. The games ask for "
         "about a quarter to a half of it.",
         "Intensité des forces de la carte de commande sur le volant, en pourcentage de sa pleine échelle. Les jeux en "
         "demandent environ un quart à la moitié."},
        {"Invert the direction", "Inverser le sens"},
        {"Check it if the wheel pulls away from the centre or turns the wrong way in a collision.",
         "Cochez si le volant fuit le centre ou part du mauvais côté lors d'un choc."},
        {"Send lamps and drive board data over the network (MAME outputs)",
         "Envoyer les lampes et la carte de commande sur le réseau (sorties MAME)"},
        {"TCP port", "Port TCP"},
        {"UDP announcement port", "Port UDP d'annonce"},
        {"Network patches", "Correctifs réseau"},
        {"Sega Race TV IP address", "Adresse IP de Sega Race TV"},
        {"Games folder", "Dossier des jeux"},
        {"Empty: the games folder next to the program.", "Vide : le dossier games à côté du programme."},
        {"Language", "Langue"},
        {"Automatic", "Automatique"},
        {"Save", "Sauvegarder"},
        {"Close", "Fermer"},
        {"Saved to %s", "Sauvegardé dans %s"},
        {"Save for this game", "Sauvegarder pour ce jeu"},
        {"Saved to %s (this game only)", "Sauvegardé dans %s (ce jeu seulement)"},
        {"Cannot write %s", "Impossible d'écrire %s"},
        {"Takes effect at the next start.", "Prend effet au prochain démarrage."},
        {"Game options", "Options du jeu"},
        {"Unknown game", "Jeu inconnu"},
        {"In [%s] of %s, for this game only.", "Dans [%s] de %s, pour ce jeu seulement."},
        {"(at the next start)", "(au prochain lancement)"},
        {"No options for this game.", "Aucune option pour ce jeu."},
        {"Changed options take effect at the next start, once saved.", "Les options changées prennent effet au prochain lancement, une fois enregistrées."},
        {"Play without the card reader", "Jouer sans le lecteur de cartes"},
        {"As when its test menu's CARD R/W is OFF: no card check at start, games without a card. Unticked, the test menu decides; with CARD R/W ON the card reader-printer is emulated, its card in [Cards] CARDFILE_01 (the card key inserts it), and the start waits for the link check (about 5 minutes).", "Comme avec CARD R/W à OFF dans son menu de test : pas de contrôle des cartes au démarrage, parties sans carte. Décoché, c'est le menu de test qui décide ; avec CARD R/W à ON, le lecteur-imprimante est émulé, sa carte dans [Cards] CARDFILE_01 (la touche carte l'insère), et le démarrage attend la liaison entre bornes (environ 5 minutes)."},
        {"Boards", "Cartes"},
        {"Controls on JVS board", "Contrôles sur carte JVS"},
        {"base board", "carte de base"},
        {"Controls on Namco N2 I/O board (n2jvio, serial)", "Contrôles sur carte d'E/S Namco N2 (n2jvio, série)"},
        {"Controls on Gaelco interface board", "Contrôles sur carte d'interface Gaelco"},
        {"Motor board: none", "Carte moteur : aucune"},
        {"Motor board: Sega-like servoboard", "Carte moteur : servoboard type Sega"},
        {"Motor board: Namco Kickback board (serial)", "Carte moteur : carte Kickback de Namco (série)"},
        {"Motor board: Gaelco interface board's motor and vibrator outputs", "Carte moteur : sorties moteur et vibreur de la carte d'interface Gaelco"},
        {"Motion board: Sega motion board", "Carte de mouvement : carte de mouvement Sega"},
        {"Ride board: Sega ride board", "Carte de siège mobile : ride board Sega"},
        {"Drive board", "Carte moteur du volant"},
        {"Motion board", "Carte de mouvement"},
        {"JVS I/O board", "Carte JVS"},
        {"Emulated", "Émulée"},
        {"Real board on %s", "Vraie carte sur %s"},
        {"(not found)", "(absent)"},
        {"A real board: the game talks to the cabinet's board on that serial port of the PC (a USB adapter, RS-232 for the drive board, RS-485 for JVS) instead of the loader's emulation.", "Vraie carte : le jeu dialogue avec la carte de la borne sur ce port série du PC (un adaptateur USB, RS-232 pour la carte moteur, RS-485 pour le JVS) au lieu de l'émulation du loader."},
        {"Skip the cabinet checks", "Sauter les contrôles de la borne"},
        {"The drive board and tower checks of the start skipped.", "Les contrôles de la carte servo et de la tour au démarrage sont sautés."},
        {"Fix the flickering", "Corriger le scintillement"},
        {"A shader patch for the flickering some graphics cards show.", "Une correction des shaders pour le scintillement de certaines cartes graphiques."},
        {"Emulate the card reader", "Émuler le lecteur de cartes"},
        {"The players' cards kept in files (Cards section).", "Les cartes des joueurs gardées dans des fichiers (section Cards)."},
        {"Steering travel used", "Course du volant utilisée"},
        {"In % of the wheel's travel: a smaller turn of the wheel for a full turn in the game. 0: the whole travel.", "En % de la course du volant : un plus petit tour de volant pour un tour complet dans le jeu. 0 : toute la course."},
        {"Chinese language", "Langue chinoise"},
        {"The game in Chinese (DVP-0084 and DVP-0084A).", "Le jeu en chinois (DVP-0084 et DVP-0084A)."},
        {"The HW210 IC card reader, its cards kept in files.", "Le lecteur de cartes à puce HW210, ses cartes gardées dans des fichiers."},
        {"Render with the Mesa patches", "Rendu avec les corrections Mesa"},
        {"The shaders patched as for Mesa's drivers.", "Les shaders corrigés comme pour les pilotes Mesa."},
        {"Screens", "Écrans"},
        {"Where the touch screen goes beside the main one.", "Où va l'écran tactile à côté de l'écran principal."},
        {"Default (no scaling)", "Par défaut (sans mise à l'échelle)"},
        {"No touch screen", "Sans écran tactile"},
        {"Side by side", "Côte à côte"},
        {"Touch screen on the right", "Écran tactile à droite"},
        {"Touch screen below", "Écran tactile en dessous"},
        {"Test menu on one screen", "Menu de test sur un écran"},
        {"The test menu on the main screen alone.", "Le menu de test sur l'écran principal seul."},
        {"Render with Zink", "Rendu avec Zink"},
        {"A special OpenGL library that fixes the graphic glitches.", "Une bibliothèque OpenGL spéciale qui corrige les défauts graphiques."},
        {"Playable at any time", "Jouable à toute heure"},
        {"The game also runs at the hours its servers refused.", "Le jeu tourne aussi aux heures que ses serveurs refusaient."},
        {"Other gun display", "Autre affichage des armes"},
        {"The cabinet's switch that changes how the guns are shown.", "L'interrupteur de la borne qui change l'affichage des armes."},
        {"Swap handlebar and throttle", "Échanger guidon et poignée"},
        {"For JVS passthrough: the two analogue channels swapped.", "Pour le JVS direct : les deux voies analogiques échangées."},
        {"Crosshairs always shown", "Viseurs toujours affichés"},
        {"Crosshairs always hidden", "Viseurs toujours cachés"},
        {"Interface based on sm2-emu's, by Daniel Martin (dmanlfc) and the sm2-emu contributors, modified for the "
         "Lindbergh loader:",
         "Interface basée sur celle de sm2-emu, de Daniel Martin (dmanlfc) et des contributeurs de sm2-emu, modifiée "
         "pour le Lindbergh loader :"},
        {"Controls page organised as Flycast's, the Dreamcast, Naomi and Atomiswave emulator:",
         "Page des contrôles organisée comme celle de Flycast, l'émulateur Dreamcast, Naomi et Atomiswave :"},
        {"Select a game", "Choisissez un jeu"},
        {"Enter: launch   Tab: settings   F9: quit", "Entrée : lancer   Tab : paramètres   F9 : quitter"},
        {"No game was found in %s. Set the games folder in Settings (Tab) > Picker.",
         "Aucun jeu trouvé dans %s. Réglez le dossier des jeux dans Paramètres (Tab) > Catalogue."},
        {"Unknown version of the game", "Version du jeu inconnue"},
        {"Running...", "En cours..."},
        {"Folder", "Dossier"},
        {"Program", "Programme"},
        {"Configuration", "Configuration"},
        {"Native resolution", "Résolution native"},
        {"Status", "État"},
        {"Working", "Fonctionne"},
        {"Not working", "Ne fonctionne pas"},
        {"Partially working", "Fonctionne en partie"},
        {"Game paused - Tab: close the settings", "Jeu en pause - Tab : fermer les paramètres"},
        {"Controls", "Contrôles"},
        {"Saved to %s and controls.ini", "Sauvegardé dans %s et controls.ini"},
        {"No controller connected.", "Aucune manette branchée."},
        {"All games", "Tous les jeux"},
        {"Driving", "Conduite"},
        {"Buttons", "Boutons"},
        {"Flying", "Vol"},
        {"Shooting", "Tir"},
        {"Mahjong", "Mahjong"},
        {"Game type", "Type de jeu"},
        {"This section of controls.ini is empty.", "Cette section de controls.ini est vide."},
        {"Action", "Action"},
        {"Inputs", "Entrées"},
        {"(none)", "(aucune)"},
        {"Dead zone %d", "Zone morte %d"},
        {"Add an input", "Ajouter une entrée"},
        {"Set", "Définir"},
        {"Replace by an input", "Remplacer par une entrée"},
        {"Clear", "Effacer"},
        {"Press a key or a button, or move an axis, for: %s", "Appuyez sur une touche ou un bouton, ou bougez un axe, pour : %s"},
        {"Escape: cancel", "Échap : annuler"},
        {"Cancel", "Annuler"},
        {"Devices", "Périphériques"},
        {"Name", "Nom"},
        {"In controls.ini", "Dans controls.ini"},
        {"Keyboard", "Clavier"},
        {"Mouse", "Souris"},
        {" (force feedback)", " (retour de force)"},
        {"Map", "Lier"},
        {"Unmap", "Délier"},
        {"Bindings", "Liaisons"},
        {"Reset...", "Réinitialiser..."},
        {"Set this device's bindings back to the defaults?", "Remettre les liaisons de ce périphérique par défaut ?"},
        {"Yes", "Oui"},
        {"No", "Non"},
        {"Player %d", "Joueur %d"},
        {"Press an input of %s for: %s", "Appuyez sur une commande de %s pour : %s"},
        {"Button %d", "Bouton %d"},
        {"Axis %d", "Axe %d"},
        {"Hat %d", "Chapeau %d"},
        {"up", "haut"},
        {"down", "bas"},
        {"left", "gauche"},
        {"right", "droite"},
        {" (half)", " (moitié)"},
        {" (pedal)", " (pédale)"},
        {" (inverted)", " (inversé)"},
        {"Left button", "Bouton gauche"},
        {"Right button", "Bouton droit"},
        {"Middle button", "Bouton du milieu"},
        {"X axis", "Axe X"},
        {"Y axis", "Axe Y"},
        {"Dead zones and options", "Zones mortes et options"},
        {"The wheel's force feedback starts with the game, on the first controller.",
         "Le retour de force du volant démarre avec le jeu, sur la première manette."},
        {"Gun X centring", "Centrage du pistolet X"},
        {"Gun Y centring", "Centrage du pistolet Y"},
        {"Screen shake: increase", "Tremblement de l'écran : montée"},
        {"Screen shake: decay", "Tremblement de l'écran : retombée"},
        {"Screen shake: minimum", "Tremblement de l'écran : minimum"},
        {"Insert card: toggle", "Insérer la carte : bascule"},
        {"Insert card 1: toggle", "Insérer la carte 1 : bascule"},
        {"Insert card 2: toggle", "Insérer la carte 2 : bascule"},
        {"Coin", "Pièce"},
        {"Start", "Départ"},
        {"Exit the game", "Quitter le jeu"},
        {"Steering", "Volant"},
        {"Accelerator", "Accélérateur"},
        {"Brake", "Frein"},
        {"Steer left", "Volant à gauche"},
        {"Steer right", "Volant à droite"},
        {"Accelerator (digital)", "Accélérateur (tout ou rien)"},
        {"Brake (digital)", "Frein (tout ou rien)"},
        {"Gear up", "Vitesse +"},
        {"Gear down", "Vitesse −"},
        {"Change view", "Changer de vue"},
        {"Change music", "Changer de musique"},
        {"Boost right", "Boost droit"},
        {"Up", "Haut"},
        {"Down", "Bas"},
        {"Left", "Gauche"},
        {"Right", "Droite"},
        {"Button 1", "Bouton 1"},
        {"Button 2", "Bouton 2"},
        {"Button 3", "Bouton 3"},
        {"Insert card", "Insérer la carte"},
        {"Insert card 1", "Insérer la carte 1"},
        {"Insert card 2", "Insérer la carte 2"},
        {"Trigger", "Gâchette"},
        {"Reload", "Recharger"},
        {"Gun button", "Bouton du pistolet"},
        {"Action button", "Bouton d'action"},
        {"Left pedal", "Pédale gauche"},
        {"Right pedal", "Pédale droite"},
        {"Gun X", "Pistolet X"},
        {"Gun Y", "Pistolet Y"},
        {"Stick X", "Manche X"},
        {"Stick Y", "Manche Y"},
        {"Stick left", "Manche à gauche"},
        {"Stick right", "Manche à droite"},
        {"Stick up", "Manche en haut"},
        {"Stick down", "Manche en bas"},
        {"Throttle", "Manette des gaz"},
        {"Throttle up", "Gaz +"},
        {"Throttle down", "Gaz −"},
        {"Gun trigger", "Gâchette du canon"},
        {"Missile trigger", "Tir de missile"},
        {"Climax switch", "Bouton Climax"},
    };
    return texts;
}

// Sets key = value in section of the lines of an ini file, keeping every other line.
void iniSet(std::vector<std::string> &lines, const char *section, const char *key, const std::string &value)
{
    auto trim = [](const std::string &s) {
        size_t b = s.find_first_not_of(" \t\r");
        size_t e = s.find_last_not_of(" \t\r");
        return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
    };
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    };

    const std::string wantedSection = lower(section);
    const std::string wantedKey = lower(key);
    const std::string newLine = std::string(key) + " = " + value;
    bool inSection = false;
    long sectionStart = -1, sectionEnd = -1;
    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string line = trim(lines[i]);
        if (!line.empty() && line[0] == '[')
        {
            if (inSection)
            {
                sectionEnd = (long)i;
                break;
            }
            size_t close = line.find(']');
            inSection = lower(trim(line.substr(1, close == std::string::npos ? std::string::npos : close - 1))) == wantedSection;
            if (inSection)
                sectionStart = (long)i;
            continue;
        }
        if (!inSection || line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        size_t eq = line.find('=');
        if (eq != std::string::npos && lower(trim(line.substr(0, eq))) == wantedKey)
        {
            lines[i] = newLine;
            return;
        }
    }
    if (sectionStart < 0)
    {
        if (!lines.empty() && !trim(lines.back()).empty())
            lines.push_back("");
        lines.push_back(std::string("[") + section + "]");
        lines.push_back(newLine);
        return;
    }
    if (sectionEnd < 0)
        sectionEnd = (long)lines.size();
    // after the last non-empty line of the section
    long at = sectionEnd;
    while (at - 1 > sectionStart && trim(lines[at - 1]).empty())
        at--;
    lines.insert(lines.begin() + at, newLine);
}

// ASPECT_RATIO's names, in ASPECT_* order, and the list Tab offers (4:3_LETTERBOX shown as 4:3)
const char *aspectNames[] = {"ORIGINAL", "4:3", "16:9", "4:3_LETTERBOX", "4:3_STRETCH", "16:9_STRETCH", "4:3_CROP", "16:9_CROP"};
const int aspectChoices[] = {ASPECT_ORIGINAL,     ASPECT_4_3,      ASPECT_16_9,     ASPECT_4_3_STRETCH,
                             ASPECT_16_9_STRETCH, ASPECT_4_3_CROP, ASPECT_16_9_CROP};

// The internal resolutions offered: the game computes and draws its frame at this size when it can be patched to
struct Resolution
{
    int width, height;
};
const Resolution internalResolutions[] = {{640, 480},  {800, 600},   {1024, 768},  {1280, 720},  {1280, 768},
                                          {1360, 768}, {1600, 900},  {1920, 1080}, {2560, 1440}, {3840, 2160}};
const char *displayModeNames[] = {"WINDOW",     "WINDOW_STRETCH", "FULLSCREEN", "FULLSCREEN_STRETCH",
                                  "BORDERLESS", "BORDERLESS_STRETCH"};

int currentDisplayMode()
{
    EmulatorConfig *cfg = getConfig();
    if (cfg->displayMode >= 0)
        return cfg->displayMode;
    return cfg->fullscreen ? DISPLAY_FULLSCREEN : DISPLAY_WINDOW;
}

void restartNote(bool inGame)
{
    if (inGame)
        ImGui::TextDisabled("%s", tr("Takes effect at the next start."));
}

void applyScale()
{
    // Scale with the window as sm2-emu does: against a 992x768 base, the smaller ratio winning. A game window smaller
    // than that (640x480) shrinks the windows down to 80 % of the font's own size on the screen, still readable; the
    // settings then scroll when they do not fit.
    const ImGuiIO &io = ImGui::GetIO();
    const float sx = io.DisplaySize.x > 0.0f ? io.DisplaySize.x / 992.0f : 1.0f;
    const float sy = io.DisplaySize.y > 0.0f ? io.DisplaySize.y / 768.0f : 1.0f;
    const float fb = io.DisplayFramebufferScale.y > 0.0f ? io.DisplayFramebufferScale.y : 1.0f;
    float scale = std::min(sx, sy);
    scale = std::clamp(scale, std::clamp(0.8f / fb, 0.5f, 0.8f), 4.0f);
    if (scale != g_uiScale)
    {
        g_uiScale = scale;
        ImGuiStyle &style = ImGui::GetStyle();
        style = g_baseStyle;
        style.ScaleAllSizes(scale);
        style.FontScaleMain = scale;
    }
}

bool frenchByDefault()
{
#ifdef _WIN32
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_FRENCH;
#else
    const char *lang = getenv("LANG");
    return lang && strncmp(lang, "fr", 2) == 0;
#endif
}
} // namespace

bool iniUpdate(const std::string &path, const std::vector<IniValue> &values)
{
    std::vector<std::string> lines;
    {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }
    }
    for (const IniValue &v : values)
        iniSet(lines, v.section.c_str(), v.key.c_str(), v.value);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    for (const std::string &line : lines)
        out << line << "\r\n";
    return (bool)out;
}

const char *tr(const char *text)
{
    if (!g_french)
        return text;
    const auto &texts = frenchTexts();
    auto it = texts.find(text);
    return it != texts.end() ? it->second : text;
}

bool initContext(SDL_Window *window, const char *language)
{
    if (g_initialised)
        return true;

    if (language && strcmp(language, "fr") == 0)
        g_french = true;
    else if (language && strcmp(language, "en") == 0)
        g_french = false;
    else
        g_french = frenchByDefault();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr; // the settings live in the loader's ini

    // sm2-emu's dark style
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowBorderSize = 0.0f;
    g_baseStyle = style;
    g_uiScale = 0.0f;

    // A font with the accented letters of the French texts
#ifdef _WIN32
    char fontPath[MAX_PATH];
    const char *windir = getenv("WINDIR");
    snprintf(fontPath, sizeof(fontPath), "%s\\Fonts\\segoeui.ttf", windir ? windir : "C:\\Windows");
    if (SDL_GetPathInfo(fontPath, NULL))
        io.Fonts->AddFontFromFileTTF(fontPath, 17.0f);
#endif

    if (!ImGui_ImplSDL3_InitForOpenGL(window, SDL_GL_GetCurrentContext()) || !ImGui_ImplOpenGL2_Init())
    {
        ImGui::DestroyContext();
        return false;
    }
    g_initialised = true;
    return true;
}

void shutdownContext()
{
    if (!g_initialised)
        return;
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    g_initialised = false;
}

bool processEvent(const SDL_Event *event)
{
    if (!g_initialised)
        return false;
    return ImGui_ImplSDL3_ProcessEvent(event);
}

void newFrame()
{
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    applyScale();
    ImGui::NewFrame();
}

void renderFrame(int width, int height)
{
    ImGui::Render();
    glad_glViewport(0, 0, width, height);
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

void drawSettings(bool *open, std::string &configFile, bool inGame)
{
    EmulatorConfig *cfg = getConfig();
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImVec2 size(std::min(viewport->WorkSize.x * 0.9f, 660.0f * g_uiScale), std::min(viewport->WorkSize.y * 0.9f, 500.0f * g_uiScale));
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    // Never larger than the game's window, even one made smaller while the settings are open
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), viewport->WorkSize);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    char title[64];
    snprintf(title, sizeof(title), "%s###settings", tr("Settings"));
    if (!ImGui::Begin(title, open, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }
    // Kept inside the game's window: one made smaller after the settings opened left them out of sight
    {
        const ImVec2 pos = ImGui::GetWindowPos(), extent = ImGui::GetWindowSize();
        const ImVec2 inside(std::clamp(pos.x, viewport->WorkPos.x, std::max(viewport->WorkPos.x, viewport->WorkPos.x + viewport->WorkSize.x - extent.x)),
                            std::clamp(pos.y, viewport->WorkPos.y, std::max(viewport->WorkPos.y, viewport->WorkPos.y + viewport->WorkSize.y - extent.y)));
        if (inside.x != pos.x || inside.y != pos.y)
            ImGui::SetWindowPos(inside);
    }

    const float footer = ImGui::GetFrameHeightWithSpacing() * 2.2f;
    // The tabs scroll both ways in a small window; the buttons below stay in sight
    ImGui::BeginChild("##tabs", ImVec2(0, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    if (ImGui::BeginTabBar("##settingsTabs"))
    {
        if (ImGui::BeginTabItem(tr("Display")))
        {
            // The internal resolution: the frame the game computes and draws, before the aspect ratio and the mode
            {
                const int count = (int)(sizeof(internalResolutions) / sizeof(internalResolutions[0]));
                int current = 0; // AUTO
                for (int i = 0; i < count; i++)
                    if (internalResolutions[i].width == cfg->internalWidth && internalResolutions[i].height == cfg->internalHeight)
                        current = i + 1;
                auto size = [](const char *pattern, int width, int height) {
                    char text[64];
                    snprintf(text, sizeof(text), pattern, width, height);
                    return std::string(text);
                };
                std::string preview = current ? size("%d x %d", cfg->internalWidth, cfg->internalHeight)
                                      : cfg->internalWidth > 0 ? size(tr("%d x %d (custom)"), cfg->internalWidth, cfg->internalHeight)
                                                               : std::string(tr("Native (AUTO)"));
                // The supported games show on the closed list and on each of its entries
                if (ImGui::BeginCombo(tr("Internal resolution"), preview.c_str()))
                {
                    if (ImGui::Selectable(tr("Native (AUTO)"), cfg->internalWidth <= 0))
                        cfg->internalWidth = cfg->internalHeight = 0;
                    for (int i = 0; i < count; i++)
                    {
                        std::string label = size("%d x %d", internalResolutions[i].width, internalResolutions[i].height);
                        if (ImGui::Selectable(label.c_str(), current == i + 1))
                        {
                            cfg->internalWidth = internalResolutions[i].width;
                            cfg->internalHeight = internalResolutions[i].height;
                        }
                        ImGui::SetItemTooltip("%s", tr(internalResolutionGames));
                    }
                    ImGui::EndCombo();
                }
                else
                    ImGui::SetItemTooltip("%s", tr(internalResolutionGames));
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("%s", tr("The size the game computes and draws its frame at, when it can be patched to; then fitted to the aspect ratio and shown by the display mode. Takes effect at the next start."));
                ImGui::TextDisabled("%s", tr("Hover the list for the games it works with."));
                ImGui::PopTextWrapPos();
            }

            const char *aspectLabels[] = {tr("Original"),        tr("4:3, black bars"), tr("16:9, black bars"), tr("4:3, stretched"),
                                          tr("16:9, stretched"), tr("4:3, cropped"),    tr("16:9, cropped")};
            int aspect = 0;
            for (int i = 0; i < IM_ARRAYSIZE(aspectChoices); i++)
                if (aspectChoices[i] == (cfg->aspectRatio == ASPECT_4_3_LETTERBOX ? ASPECT_4_3 : cfg->aspectRatio))
                    aspect = i;
            if (ImGui::Combo(tr("Aspect ratio"), &aspect, aspectLabels, IM_ARRAYSIZE(aspectLabels)))
            {
                cfg->aspectRatio = aspectChoices[aspect];
                if (inGame)
                    sdlApplyDisplaySettings();
            }
            if (cfg->aspectRatio == ASPECT_4_3_CROP || cfg->aspectRatio == ASPECT_16_9_CROP)
            {
                // Read at each frame (blitPresent): the picture follows the sliders
                ImGui::SliderInt(tr("Crop position"), &cfg->cropPosition, -100, 100);
                ImGui::TextDisabled("%s", tr("-100: left or top, 0: the middle, 100: right or bottom."));
                bool barrel = cfg->cropBarrel != 0;
                if (ImGui::Checkbox(tr("Bend the cut parts onto the edges"), &barrel))
                    cfg->cropBarrel = barrel;
                if (barrel)
                {
                    ImGui::SliderInt(tr("Undistorted middle"), &cfg->barrelCenter, 0, 100, "%d %%");
                    ImGui::SliderInt(tr("Curvature radius"), &cfg->barrelRadius, 10, 300, "%d %%");
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("%s", tr("The whole picture stays visible: the middle (in % of the frame) as cropped, the parts the crop would cut squeezed onto the edges as on a cylinder seen from the front. A small radius bends hard at the edge, a large one squeezes evenly."));
                    ImGui::PopTextWrapPos();
                }
            }
            ImGui::TextDisabled("%s", tr("Original: the shape of the game's render resolution."));
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", tr("Black bars: the frame's shape, with the whole picture inside at its own shape and black bars where the shapes differ. Stretched: the picture fills the frame, deformed. Cropped: the picture fills the frame at its own shape, cut on the sides (or above and below); the crop position moves the part kept. The display mode then fits the frame to the window or the screen, or stretches it."));
            ImGui::PopTextWrapPos();

            const char *modeLabels[] = {tr("Window, square pixels"), tr("Window, stretched"), tr("Full screen"),
                                        tr("Full screen, stretched"), tr("Borderless fullscreen window"),
                                        tr("Borderless fullscreen window, stretched")};
            int mode = currentDisplayMode();
            if (ImGui::Combo(tr("Display mode"), &mode, modeLabels, IM_ARRAYSIZE(modeLabels)))
            {
                cfg->displayMode = mode;
                cfg->fullscreen = mode >= DISPLAY_FULLSCREEN;
                if (inGame)
                    sdlApplyDisplaySettings();
            }

            ImGui::Separator();
            bool limiter = cfg->fpsLimiter != 0;
            if (ImGui::Checkbox(tr("Limit the frame rate"), &limiter))
                cfg->fpsLimiter = limiter;
            ImGui::SliderFloat(tr("Target frame rate"), &cfg->fpsTarget, 30.0f, 120.0f, "%.1f");
            restartNote(inGame);
            bool overlay = cfg->fpsOverlayEnabled != 0;
            if (ImGui::Checkbox(tr("Show the frame rate (F12)"), &overlay))
            {
                cfg->fpsOverlayEnabled = overlay;
                if (inGame)
                    fpsOverlayToggleVisibility();
            }
            bool hideCursor = cfg->hideCursor != 0;
            if (ImGui::Checkbox(tr("Hide the mouse pointer"), &hideCursor))
                cfg->hideCursor = hideCursor;
            ImGui::EndTabItem();
        }

        // Force feedback is set in Controls, with the devices
        if (ImGui::BeginTabItem(tr("Controls")))
        {
            drawControlsTab(inGame);
            ImGui::EndTabItem();
        }

        // The options of the game being run, from its module's table (config/gameOptions.c)
        if (inGame && ImGui::BeginTabItem(tr("Game options")))
        {
            drawGameOptionsTab(configFile);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(tr("Outputs and network")))
        {
            bool outputs = cfg->outputsNetwork != 0;
            if (ImGui::Checkbox(tr("Send lamps and drive board data over the network (MAME outputs)"), &outputs))
                cfg->outputsNetwork = outputs;
            ImGui::InputInt(tr("TCP port"), &cfg->outputsNetworkPort);
            ImGui::InputInt(tr("UDP announcement port"), &cfg->outputsNetworkUdpPort);
            ImGui::Separator();
            bool patches = cfg->enableNetworkPatches != 0;
            if (ImGui::Checkbox(tr("Network patches"), &patches))
                cfg->enableNetworkPatches = patches;
            ImGui::InputText(tr("Sega Race TV IP address"), cfg->srtvIP, sizeof(cfg->srtvIP));
            restartNote(inGame);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(tr("Picker")))
        {
            ImGui::InputText(tr("Games folder"), cfg->gamesDir, sizeof(cfg->gamesDir));
            ImGui::TextDisabled("%s", tr("Empty: the games folder next to the program."));
            const char *languageCodes[] = {"auto", "fr", "en"};
            const char *languageLabels[] = {tr("Automatic"), "Français", "English"};
            int language = 0;
            for (int i = 0; i < 3; i++)
                if (strcmp(cfg->language, languageCodes[i]) == 0)
                    language = i;
            if (ImGui::Combo(tr("Language"), &language, languageLabels, 3))
            {
                snprintf(cfg->language, sizeof(cfg->language), "%s", languageCodes[language]);
                g_french = language == 1 || (language == 0 && frenchByDefault());
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(tr("About")))
        {
            ImGui::TextUnformatted("Lindbergh Loader");
            ImGui::TextDisabled("Dear ImGui %s", IMGUI_VERSION);
            ImGui::Separator();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(tr("Interface based on sm2-emu's, by Daniel Martin (dmanlfc) and the sm2-emu contributors, modified "
                                      "for the Lindbergh loader:"));
            ImGui::PopTextWrapPos();
            ImGui::TextUnformatted("https://github.com/dmanlfc/sm2-emu");
            ImGui::Spacing();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(tr("Controls page organised as Flycast's, the Dreamcast, Naomi and Atomiswave emulator:"));
            ImGui::PopTextWrapPos();
            ImGui::TextUnformatted("https://github.com/flyinghead/flycast");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button(tr("Save")))
    {
        char message[1200];
        bool controlsWritten = false;
        if (!saveSettings(configFile))
            snprintf(message, sizeof(message), tr("Cannot write %s"), configFile.c_str());
        else if (!controlsSave(inGame, &controlsWritten))
            snprintf(message, sizeof(message), tr("Cannot write %s"), "controls.ini");
        else
            snprintf(message, sizeof(message), controlsWritten ? tr("Saved to %s and controls.ini") : tr("Saved to %s"), configFile.c_str());
        g_status = message;
    }
    // The game's own file, <short name>.ini beside the settings file: a copy of it the first time (its other settings
    // kept), then read in its place at the next start (config.c) and by the picker
    const char *shortTitle = getConfig()->gameShortTitle;
    if (inGame && shortTitle && shortTitle[0])
    {
        ImGui::SameLine();
        if (ImGui::Button(tr("Save for this game")))
        {
            namespace fs = std::filesystem;
            fs::path own = fs::path(configFile).parent_path() / (std::string(shortTitle) + ".ini");
            std::error_code error;
            if (!fs::exists(own, error) && fs::exists(configFile, error))
                fs::copy_file(configFile, own, error);
            char message[1200];
            bool controlsWritten = false;
            if (error || !saveSettings(own.string()))
                snprintf(message, sizeof(message), tr("Cannot write %s"), own.string().c_str());
            else
            {
                configFile = own.string(); // the next saves go there too
                controlsSave(inGame, &controlsWritten);
                snprintf(message, sizeof(message), tr("Saved to %s (this game only)"), configFile.c_str());
            }
            g_status = message;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("Close")))
        *open = false;
    if (!g_status.empty())
    {
        // Beside the buttons when it fits, else on the line under them
        const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(g_status.c_str()).x <= right)
            ImGui::SameLine();
        ImGui::TextDisabled("%s", g_status.c_str());
    }
    ImGui::End();
}

bool saveSettings(const std::string &configFile)
{
    EmulatorConfig *cfg = getConfig();
    char number[32];
    int mode = currentDisplayMode();
    snprintf(number, sizeof(number), "%.1f", cfg->fpsTarget);
    std::vector<IniValue> values = {
                                     {"Display", "ASPECT_RATIO", aspectNames[std::clamp(cfg->aspectRatio, 0, IM_ARRAYSIZE(aspectNames) - 1)]},
                                     {"Display", "CROP_POSITION", std::to_string(cfg->cropPosition)},
                                     {"Display", "CROP_BARREL", cfg->cropBarrel ? "true" : "false"},
                                     {"Display", "BARREL_CENTER", std::to_string(cfg->barrelCenter)},
                                     {"Display", "BARREL_RADIUS", std::to_string(cfg->barrelRadius)},
                                     {"Display", "WIDTH", cfg->internalWidth > 0 ? std::to_string(cfg->internalWidth) : "AUTO"},
                                     {"Display", "HEIGHT", cfg->internalHeight > 0 ? std::to_string(cfg->internalHeight) : "AUTO"},
                                     {"Display", "DISPLAY_MODE", displayModeNames[std::clamp(mode, 0, IM_ARRAYSIZE(displayModeNames) - 1)]},
                                     {"Display", "FULLSCREEN", mode >= DISPLAY_FULLSCREEN ? "true" : "false"},
                                     {"Display", "FPS_LIMITER_ENABLED", cfg->fpsLimiter ? "true" : "false"},
                                     {"Display", "FPS_TARGET", number},
                                     {"Display", "FPS_OVERLAY_ENABLED", cfg->fpsOverlayEnabled ? "true" : "false"},
                                     {"Display", "HIDE_CURSOR", cfg->hideCursor ? "true" : "false"},
                                     {"ForceFeedback", "FFB_STRENGTH", std::to_string(cfg->ffbStrength)},
                                     {"ForceFeedback", "FFB_INVERT", cfg->ffbInvert ? "true" : "false"},
                                     {"Outputs", "OUTPUTS_NETWORK", cfg->outputsNetwork ? "true" : "false"},
                                     {"Outputs", "OUTPUTS_NETWORK_PORT", std::to_string(cfg->outputsNetworkPort)},
                                     {"Outputs", "OUTPUTS_NETWORK_UDP_PORT", std::to_string(cfg->outputsNetworkUdpPort)},
                                     {"Network", "ENABLE_NETWORK_PATCHES", cfg->enableNetworkPatches ? "true" : "false"},
                                     {"Network", "SRTV_IPADDRESS", cfg->srtvIP},
                                     {"Launcher", "GAMES_DIR", cfg->gamesDir},
                                     {"Launcher", "LANGUAGE", cfg->language},
                                 };
    gameOptionsIniValues(values); // [Game.<short name>] of the game being run, none in the picker
    return iniUpdate(configFile, values);
}
} // namespace gui
