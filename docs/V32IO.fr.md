# v32io : entrée clavier et souris

[Version anglaise / in English](V32IO.md) | [Version espagnole / en español](V32IO.es.md) <!-- langlinks -->

Trois en-têtes de `v32/` donnent aux programmes v32c++ un vrai **clavier** et une
vraie **souris** sur la console Vircon32. Ils sont un portage C++ des pilotes C
[v32io](https://github.com/wedge1020/v32io) pour Vircon32, découpés de la même
façon : les mêmes protocoles et le même décodage, enveloppés dans trois classes.

| En-tête            | Classe          | Remplace (bibliothèque C)                | Ce que c'est                                   |
| ------------------ | --------------- | ---------------------------------------- | ---------------------------------------------- |
| `v32/v32io.hpp`    | `v32::IoDevice` | `v32io.h` : `v32io` / `v32io_*()`        | le noyau : les 11 contrôles d'une manette lus comme des données |
| `v32/keyboard.hpp` | `v32::Keyboard` | `keyboard.h` : `v32kbd` / `v32kbd_*()`   | événements de touches, caractères saisis, touches maintenues |
| `v32/mouse.hpp`    | `v32::Mouse`    | `mouse.h` : `v32mouse` / `v32mouse_*()`  | un pointeur, le mouvement et trois boutons     |

Un programme inclut le ou les pilotes dont il a besoin (`keyboard.hpp`,
`mouse.hpp`, ou les deux) ; chacun inclut le noyau, `v32io.hpp`, qui n'est inclus
directement que pour utiliser `v32::IoDevice` seul.

```cpp
#include <v32/keyboard.hpp>

v32::Keyboard keyboard( v32::SecondGamepadPort );

void main()
{
    while( true )
    {
        keyboard.probe();                 // once on EVERY frame
        int key = keyboard.read();
        while( key > 0 )                  // every key pressed this frame
        {
            if( key == v32::KeyEnter ) { /* ... */ }
            else if( key >= 32 && key < 127 ) { /* a typed character */ }
            key = keyboard.read();
        }
        end_frame();
    }
}
```

---

## ⚠ Prérequis : un périphérique v32io

**Cette fonctionnalité EXIGE soit l'adaptateur matériel v32io, soit un
DesktopEmulator Vircon32 modifié.** Elle ne fonctionne pas avec l'émulateur
standard seul, et rien dans une cartouche ne peut y changer quoi que ce soit.

La console Vircon32 n'a ni port clavier ni port souris. Un périphérique v32io
l'atteint par un **port de manette** : la console voit une manette ordinaire, mais
ses 11 contrôles (quatre directions et sept boutons) transportent des données de
clavier ou de souris au lieu d'appuis de jeu. Quelque chose d'extérieur à la
console doit produire ces données :

1. **L'adaptateur matériel v32io** — un firmware dédié (`v32io-pico`) sur une
   carte Waveshare RP2350-USB-A (dont la résistance R13 est retirée). Branchez-y
   un clavier ou une souris USB standard, et branchez-le à l'ordinateur : il se
   présente comme un joystick nommé `v32io:kbd` ou `v32io:mouse`. Comme il n'est
   « qu'une manette » pour l'ordinateur, il fonctionne avec **n'importe quel**
   émulateur Vircon32, standard ou modifié, dès lors qu'un profil de joystick le
   associe (voir [Profil de joystick pour l'adaptateur](#profil-de-joystick-pour-ladaptateur)).
   Notes sur le firmware et sa construction : le dépôt v32io,
   `firmware/RP2350-USB-A/`.
2. **Un DesktopEmulator Vircon32 modifié** — aucun matériel nécessaire ; le
   clavier et la souris de l'ordinateur hôte deviennent des périphériques v32io.
   Soit :
   - le fork à <https://github.com/wedge1020/ComputerSoftware>, **branche
     `v32io`**, qui intègre les périphériques `v32kbd` et `v32mouse` ; soit
   - les sources du DesktopEmulator standard avec les deux correctifs du dépôt
     v32io appliqués (`emulator/v32kbd.patch`, `emulator/v32mouse.patch`).

> Le périphérique de manette **Keyboard** propre à l'émulateur standard n'est
> *pas* un clavier v32io : il associe quelques touches à des boutons de manette.
> Un `v32::Keyboard` qui le lit voit n'importe quoi. De même, une manette
> ordinaire sur le port où un programme attend un périphérique v32io produit des
> événements de touches et des mouvements de souris aléatoires.

Sans périphérique, un programme utilisant ces en-têtes se construit et s'exécute
quand même : le port n'est simplement pas connecté (`connected()` est faux) ou ne
montre rien, et aucun événement n'arrive. Les exemples affichent dans ce cas un
message « no device ».

---

## Installation

### Numéros de manette

Les menus de l'émulateur comptent les manettes à partir de 1 ; la console (et cet
en-tête) compte les identifiants de port à partir de 0 :

| Menu de l'émulateur | Id de port | Constante                  |
| ------------------- | ---------- | -------------------------- |
| Gamepad 1           | 0          | `v32::FirstGamepadPort`    |
| Gamepad 2           | 1          | `v32::SecondGamepadPort`   |
| Gamepad 3           | 2          | `v32::ThirdGamepadPort`    |
| Gamepad 4           | 3          | `v32::FourthGamepadPort`   |

La convention utilisée par les exemples est la **Gamepad 2** pour le clavier (et
pour la souris quand elle est seule), la **Gamepad 3** pour la souris quand les
deux sont utilisés, ce qui laisse la Gamepad 1 pour une manette ordinaire. N'importe
quel port convient ; c'est le programme qui décide.

### DesktopEmulator modifié

Ouvrez le menu *Gamepads*, choisissez la manette qu'attend votre programme, et
sélectionnez **`v32kbd`** ou **`v32mouse`**.

- `v32kbd` ne peut pas être sélectionné tant qu'une manette utilise le
  périphérique *Keyboard* standard, et inversement : tous deux ont besoin du
  clavier hôte.
- Les raccourcis de l'émulateur (Échap, F2, F4, F5 et les raccourcis Ctrl)
  continuent de fonctionner, et sont aussi envoyés au programme.
- Avec `v32mouse`, un **clic gauche sur l'écran du jeu capture le pointeur** (le
  clic lui-même n'est pas envoyé au programme). Appuyez sur **Ctrl gauche + Alt
  gauche** pour le libérer (l'Alt *gauche*, pour éviter les dispositions avec
  AltGr).

### Profil de joystick pour l'adaptateur

L'adaptateur apparaît comme un joystick, `v32io:kbd` ou `v32io:mouse` selon ce qui
y est branché, et chacun a besoin d'un profil de joystick dans l'émulateur (créé
avec *EditControls* ; l'adaptateur a un mode de configuration pour cela, décrit
dans le README du firmware de v32io). Les deux profils associent de la même façon
— **le bouton *n* au contrôle *n*** :

```xml
<joystick nickname="v32io-kbd">
    <guid>...</guid>              <!-- from EditControls, per computer -->
    <name>v32io: kbd</name>
    <left         button="0" />
    <right        button="1" />
    <up           button="2" />
    <down         button="3" />
    <button-start button="4" />
    <button-a     button="5" />
    <button-b     button="6" />
    <button-x     button="7" />
    <button-y     button="8" />
    <button-l     button="9" />
    <button-r     button="10" />
</joystick>
```

Le GUID et le nom exact dépendent de l'ordinateur ; prenez-les donc dans
*EditControls* avec l'adaptateur branché. Attribuez ensuite le joystick à la
manette qu'attend le programme.

**Vérifiez une installation avec `demos/cxx/v32io/monitor`.** Il montre, pour les
quatre ports, les 11 contrôles exactement comme la console les voit et ce qu'ils
signifient pour chaque pilote. Un contrôle qui ne s'allume jamais, ou deux qui
bougent toujours ensemble, sont une erreur d'association du profil.

---

## Utiliser les en-têtes

Ajoutez avec `-I` la racine du dépôt (le répertoire contenant `v32/`), ou installez
les en-têtes pour tout le système (`sudo make sysinstall`, ou `sudo cmake --install
build` ; voir « Installation » dans le README), après quoi aucun `-I` n'est
nécessaire :

```sh
v32c++ -I path/to/v32c++ -o obj/game.c game.cpp
```

Comme tout en-tête de `v32/`, ce sont des sources C++ insérées dans votre
programme. Ils sont autonomes : la bibliothèque **C** de v32io n'est pas
nécessaire pour construire un programme v32c++ (et les deux ne sont pas faites pour
être mélangées dans un même programme). Le noyau inclut les `input.h`, `time.h` et
`misc.h` du SDK, qui sont transmis au C généré.

### La seule règle : sonder à chaque image

Appelez `probe()` **une fois à chaque image** pour **chaque** périphérique, avant
`end_frame()`. La console ne met à jour les manettes qu'entre les images, et un
clavier ne signale au plus qu'un événement de touche par image : une image sans
sondage peut perdre une touche, ou mal lire le mouvement de la souris. Les appels
`probe()` supplémentaires dans la même image ne font rien, il est donc sans risque
de sonder depuis plusieurs endroits.

Les programmes à plusieurs écrans (un titre, un menu, le jeu) devraient sonder dans
chacun d'eux — ou garder un seul sondage en tête d'une unique boucle principale.

### Objets

Un `Keyboard` ou une `Mouse` est un objet ordinaire : une globale, ou une variable
locale de `main`. Il n'y a rien à libérer. Créez-en un par périphérique, chacun avec
son propre port ; un clavier et une souris peuvent être utilisés en même temps. En
copier un est inoffensif mais inutile (la copie a sa propre file et son propre
état) ; passez plutôt un pointeur ou une référence.

### `v32::Keyboard`

```cpp
v32::Keyboard keyboard( v32::SecondGamepadPort );
```

Le constructeur prend l'état actuel du périphérique comme point de départ (l'adaptateur
conserve son dernier événement à travers les réinitialisations de la console, de
sorte qu'un ancien événement n'est pas lu comme un nouveau).

| Membre                         | Ce qu'il fait                                                                                    |
| ------------------------------ | ------------------------------------------------------------------------------------------------ |
| `bool probe()`                 | Une fois par image. Met en file le nouvel événement de touche ou caractère saisi, s'il y en a un ; vrai quand il en est arrivé un. |
| `int read()`                   | Le prochain **appui** de touche comme caractère saisi (majuscules et verrouillage majuscule appliqués, compatible avec la police du BIOS). Les touches sans caractère renvoient leur code de touche (inférieur à 32, ou 127). Les **caractères saisis** envoyés par le périphérique (é, ß, ┌ ...) reviennent sous la forme de leur code dans la police du BIOS, plus `v32::KeyboardGlyph` quand il est inférieur à 32. Les relâchements sont ignorés. 0 quand la file est vide. |
| `int read_event(bool* pressed)` | Le prochain événement, appui **ou** relâchement, sous la forme de son **code de touche** (sans majuscule). `*pressed` est renseigné s'il n'est pas `NULL`. Les caractères saisis sont ignorés (ce ne sont pas des touches). 0 quand la file est vide. |
| `bool next_event(KeyEvent* e)` | Le prochain événement avec tout : `e->key`, `e->symbol`, `e->pressed`, `e->text` (vrai pour un caractère saisi : `key` et `symbol` sont alors son code dans la police du BIOS). Faux quand la file est vide. |
| `int glyph(int value)`         | Le caractère du BIOS à **dessiner** pour une valeur renvoyée par `read()` : ASCII et caractères saisis tels quels, 0 pour les touches sans caractère. Voir [Caractères saisis](#caractères-saisis). |
| `int pending()`                | Événements en attente.                                                                             |
| `void flush()`                 | Jette tous les événements en attente (les touches maintenues restent maintenues).                  |
| `bool is_down(int key)`        | Cette touche est-elle maintenue en ce moment ? Prend des codes de touche : `'a'`, `' '`, `v32::KeyLeft`, ... |
| `bool shift_down()`, `ctrl_down()`, `alt_down()`, `gui_down()` | L'une ou l'autre touche de la paire maintenue.                       |
| `bool caps_lock_on()`          | État du verrouillage majuscule (basculé à chaque appui sur verrouillage majuscule).                |
| `int symbol(int key)`          | Le caractère qu'un code de touche produit dans l'état *actuel* de majuscule / verrouillage majuscule (disposition US). |
| `int last_event()`             | Compteur d'images au dernier événement ; -1 avant le premier.                                     |
| `int dropped()`                | Événements perdus parce que la file était pleine.                                                  |
| `bool connected()`, `int port()` | L'état et l'id de la manette.                                                                    |
| `IoDevice* device()`           | Le noyau en dessous, pour les contrôles empaquetés bruts.                                         |

`read()` et `read_event()`/`next_event()` puisent dans la même file (64
événements) : n'utilisez qu'un style par clavier. `is_down()` et les requêtes de
modificateurs fonctionnent à côté de l'un ou de l'autre, et ne dépendent pas du
tout de la lecture de la file — un jeu qui ne veut que WASD peut sonder à chaque
image et ne jamais lire.

La fonction libre `v32::key_glyph( value )` est `glyph()` sans clavier.

**Saisir ou jouer.** Utilisez `read()` pour du texte (noms, discussion, une
console) ; `is_down()` pour les contrôles de jeu (touches de déplacement
maintenues) ; `next_event()` quand l'appui comme le relâchement comptent (un
synthétiseur associé aux touches, par exemple). La bibliothèque ne génère pas de
répétition de touche : maintenir une touche, c'est un appui, puis un relâchement.

**Les codes de touche** identifient des touches, pas des caractères. Les touches
dotées d'un caractère ASCII signalent celui-ci tel qu'il est saisi **sans majuscule
sur une disposition US** : `'a'`–`'z'`, `'0'`–`'9'`, espace, et
`` ` - = [ ] \ ; ' , . / `` — comparez-les à des littéraux de caractère. Les
autres :

| Code | Constante         | Code    | Constante                          |
| ---- | ----------------- | ------- | ---------------------------------- |
| 1    | `v32::KeyUp`      | 11      | `v32::KeyRCtrl`                    |
| 2    | `v32::KeyDown`    | 12      | `v32::KeyLAlt` (option gauche)     |
| 3    | `v32::KeyLeft`    | 13      | `v32::KeyEnter`                    |
| 4    | `v32::KeyRight`   | 14–25   | `v32::KeyF1` … `v32::KeyF12`       |
| 5    | `v32::KeyCapsLock`| 26      | `v32::KeyRAlt` (option droite)     |
| 6    | `v32::KeyLShift`  | 27      | `v32::KeyEscape`                   |
| 7    | `v32::KeyRShift`  | 28      | `v32::KeyLGui` (commande gauche / Windows) |
| 8    | `v32::KeyBackspace` | 29    | `v32::KeyRGui` (commande droite)   |
| 9    | `v32::KeyTab`     | 127     | `v32::KeyDelete`                   |
| 10   | `v32::KeyLCtrl`   | 30, 31  | `v32::KeyPageHigh`, `v32::KeyPageLow` : préfixes de page de codes des caractères saisis, jamais des touches (voir ci-dessous) ; 0 n'est jamais signalé |

Le pavé numérique signale les mêmes codes que le clavier principal. Les touches
fléchées (1–4) sont distinctes de `'a'`..`'z'` et des directions propres à la
manette.

### Caractères saisis

Les codes de touche disent quelle touche a été enfoncée, pas ce qu'elle a saisi ;
ils ne peuvent donc pas porter une lettre accentuée issue d'une touche morte ou
d'AltGr, ni un caractère de tracé de cadres. Un périphérique v32io peut les envoyer
à la place comme des **caractères saisis**, dans le jeu de caractères de la police
du BIOS de Vircon32 : Windows-1252 (`é ñ ß © € « »` ...) avec des caractères de
tracé de cadres et des teintes à la place des codes 14 à 31
(`┌ ─ ┐ │ └ ┘ ░ ▒ ▓ █` ...). `demos/cxx/v32io/glyphs` les montre tous.

D'où ils viennent :

- **L'émulateur modifié** envoie ce que saisissent la disposition de clavier propre à
  l'ordinateur, les touches mortes et la touche de composition, tant que la police du
  BIOS possède le glyphe. Rien à apprendre : tapez é comme vous l'avez toujours fait.
- **L'adaptateur** ne sait que quelle touche est enfoncée ; il a donc sa propre
  couche pour un clavier US : **AltGr** (Alt droit ; Option droite sur un Mac) plus
  une touche (AltGr+E = é, AltGr+S = ß, AltGr+C = ©, AltGr+5 = €), des **touches
  mortes** (AltGr+`'` puis une lettre lui met un accent aigu ; de même avec
  `` ` `` `^` `¨` `~`), et des **codes Alt** (maintenez l'Alt *gauche*, saisissez le
  code du BIOS sur le pavé numérique : `Alt 0233` est é, `Alt 021` est ─). Les
  tables complètes se trouvent dans le dépôt v32io, `docs/v32io/COMPOSE.md`. Une
  combinaison de touches qui saisit quelque chose ici n'est *pas* en plus signalée
  comme une touche.

Comment un programme les voit :

```cpp
v32::Keyboard keyboard( v32::SecondGamepadPort );
int text[ 64 ];
int length = 0;

keyboard.probe();                           // once on EVERY frame
int key = keyboard.read();
while( key > 0 )
{
    int glyph = keyboard.glyph( key );      // what to draw, 0 for a non-character key
    if( glyph > 0 && length < 63 )
    {
        text[ length ] = glyph;             // ASCII, é, ß, ┌ ... all alike
        length++;
        text[ length ] = 0;
    }
    key = keyboard.read();
}
print_at( 0, 0, text );                     // the BIOS font draws them all
```

- `read()` renvoie un caractère saisi sous la forme de son **code dans la police du
  BIOS** (32–126 et 128–255 tels quels). Un code inférieur à 32 serait pris pour un
  code de touche (13 est Entrée, 27 est Échap, mais 27 est aussi `├`) ; pour ceux-là,
  `read()` renvoie donc le code **plus `v32::KeyboardGlyph`** (256). **Ne testez pas
  vous-même la valeur renvoyée : passez-la à `glyph()`**, qui donne le code à dessiner
  (un caractère) ou 0 (une touche sans caractère : une flèche, F5, Échap,
  Suppr...). `key >= 32 && key < 127`, le test de « ASCII imprimable », fonctionne
  toujours pour l'ASCII seul.
- `next_event()` renseigne `e->text` pour eux, et `e->key` et `e->symbol` sont tous
  deux le code du BIOS, jamais décalé. `read_event()` les ignore.
- Ils ne touchent jamais `is_down()` : les touches d'une combinaison ne sont pas
  signalées, si bien qu'un jeu qui lit les touches maintenues n'est pas affecté, et
  qu'un programme qui ne lit pas les caractères ne les voit jamais.
- Sur le fil, un caractère occupe **deux événements**, donc deux images : un préfixe de
  page de codes (`v32::KeyPageHigh`, caractères du BIOS 128–255, ou
  `v32::KeyPageLow`, 0–127) et une charge avec les 7 bits de poids faible. Les deux
  sont des *relâchements* de touche, c'est pourquoi un pilote antérieur à eux ne
  voit que deux relâchements et aucun appui. `probe()` fait tout cela ; un préfixe
  qui attend plus de `v32::KeyboardPageTimeout` (30) images est oublié. Voir
  PROTOCOLS.md dans le dépôt v32io pour la description complète.
- Une manette qui vient d'être connectée relance le décodage (le strobe et tout
  préfixe en attente), comme pour la souris.

**Caractères dans les littéraux de chaîne.** Le texte peut contenir des glyphes de tracé
de cadres (codes 14 à 31) sous la forme d'un littéral ordinaire, `"\025\026"`. Le
compilateur Vircon32 écrirait ces codes de contrôle bruts dans son assembleur, et
l'assembleur les rejette ; v32c++ déplace donc un tel littéral dans un tableau global
caché de `char` et utilise celui-ci à la place (`docs/VIRCON32_QUIRKS.fr.md` n° 28).
Rien à faire dans la source. Tout ce qui est à partir du code 32, lettres accentuées
comprises, convient dans un littéral, écrit avec un échappement : utilisez
l'**octal** (`"caf\351e"` est café ; au plus trois chiffres, donc ce qui suit est
sûr), parce que C++ fait avaler à un échappement `\x` tous les chiffres hexadécimaux
qui le suivent (`"caf\xE9e"` n'est pas ce qu'il paraît ; scindez le littéral,
`"caf\xE9" "e"`). Le transpileur transmet l'un ou l'autre au compilateur sous la
forme `\xE9`.

### `v32::Mouse`

```cpp
v32::Mouse mouse( v32::SecondGamepadPort );
```

La souris tient un pointeur : il démarre au centre de l'écran, se déplace de
`pas × échelle` pixels par sondage (2 pixels par pas par défaut) et reste dans ses
limites (tout l'écran de 640×360 par défaut).

| Membre                                   | Ce qu'il fait                                                              |
| ---------------------------------------- | -------------------------------------------------------------------------- |
| `bool probe()`                           | Une fois par image. Vrai si la souris a bougé ou si un bouton a changé.    |
| `int x()`, `int y()`                     | Le pointeur.                                                               |
| `void position(int* x, int* y)`          | Les deux à la fois (l'un ou l'autre peut être `NULL`).                     |
| `int dx()`, `int dy()`                   | Mouvement lors du dernier sondage, en pixels — signalé même lorsque le pointeur est arrêté à ses limites, ce qui convient donc aussi aux contrôles de « vue à la souris ». |
| `void delta(int* dx, int* dy)`           | Les deux à la fois (l'un ou l'autre peut être `NULL`).                     |
| `int steps_x()`, `int steps_y()`         | Le même mouvement en pas bruts du périphérique (−5…+5).                    |
| `void set_position(int x, int y)`        | Place le pointeur (maintenu dans les limites).                             |
| `void set_bounds(int x0, int y0, int x1, int y1)` | Zone dans laquelle le pointeur peut se déplacer, bornes incluses. |
| `void set_scale(int pixels)`, `int scale()` | Pixels par pas (ignoré s'il n'est pas positif).                         |
| `bool is_down(int buttons)`              | L'un de ces boutons est-il maintenu ? `v32::MouseLeft`, `MouseRight`, `MouseMiddle`, `MouseAny`. |
| `bool pressed(int buttons)`, `released(int buttons)` | Est-il passé à l'état enfoncé / relâché lors du dernier sondage ? |
| `int buttons()`                          | Tous les boutons maintenus, sous forme de bits `MouseButton`.              |
| `int last_event()`                       | Image du dernier mouvement ou changement de bouton ; -1 avant tout.        |
| `bool connected()`, `int port()`, `IoDevice* device()` | Comme pour le clavier.                                       |

L'adaptateur maintient chaque changement de bouton pendant au moins 25 ms, si bien
que même un clic très rapide dure plus d'une image et que `pressed()` le verra. Il
n'y a **pas de molette** : les 11 contrôles n'ont plus de place pour elle.

Le mouvement est limité à 5 pas par axe et par image (le protocole ne peut pas
distinguer 6 de −6), ce que l'adaptateur respecte en n'envoyant qu'un pas par axe
toutes les 5 ms au plus — environ 200 pas par seconde, soit 400 pixels par seconde à
l'échelle par défaut. Pour un pointeur plus rapide, augmentez l'échelle.

### `v32::IoDevice`

Le noyau, pour écrire un pilote de votre cru ou voir ce qu'envoie un périphérique
(voir `demos/cxx/v32io/monitor.cpp`) :

| Membre                   | Ce qu'il fait                                                        |
| ------------------------ | -------------------------------------------------------------------- |
| `IoDevice(int port)`     | Lit les contrôles une fois, de sorte que `read()` démarre avec l'état actuel. |
| `bool probe()`           | Une fois par image ; vrai si les contrôles empaquetés ont changé.    |
| `int read()`             | Contrôles empaquetés du dernier sondage (`v32::IoLeft` … `v32::IoR`). |
| `int read_previous()`    | … et du sondage précédent.                                           |
| `int changed()`          | Bits qui ont changé lors du dernier sondage.                         |
| `bool is_down(int mask)` | Un contrôle du masque est-il actif ?                                 |
| `bool connected()`, `bool plugged()` | Connecté ; est devenu connecté lors du dernier sondage.  |
| `int scan()`             | Lit les contrôles tout de suite, sans toucher à l'historique des sondages. |
| `int port()`             | L'id de la manette.                                                  |

Les fonctions libres `v32::mouse_counter()`, `v32::mouse_steps()` et
`v32::mouse_buttons()` sont les étapes de décodage de la souris prises seules.

### Coûts

Mesuré sur la console sans écran : sonder un clavier ou une souris coûte bien moins
de 1 % du CPU d'une image (un programme de dessin plein écran passe ~0,3 % dans le
pilote de la souris et le reste à dessiner). Mémoire : un `Keyboard` fait environ 470
mots (surtout sa file de 64 événements et 128 indicateurs de touches maintenues), une
`Mouse` environ 90, un `IoDevice` 75 (dont 68 pour sa routine en code machine).

---

## Comment fonctionnent les périphériques

Tout périphérique v32io se lit de la même façon. Ses 11 contrôles sont empaquetés
en une seule valeur, les **contrôles empaquetés**, un bit par contrôle dans l'ordre
des ports INP (le bit *n* est le port `0x402 + n`) :

| Bit | Port    | Contrôle  | Constante    | v32kbd           | v32mouse            |
| --- | ------- | --------- | ------------ | ---------------- | ------------------- |
| 0   | `0x402` | Left      | `v32::IoLeft`  | strobe         | compteur X : trit − |
| 1   | `0x403` | Right     | `v32::IoRight` | strobe         | compteur X : trit + |
| 2   | `0x404` | Up        | `v32::IoUp`    | touche enfoncée | compteur Y : trit − |
| 3   | `0x405` | Down      | `v32::IoDown`  | touche relâchée | compteur Y : trit + |
| 4   | `0x406` | Start     | `v32::IoStart` | bit 0 du code de touche | bouton du milieu |
| 5   | `0x407` | A         | `v32::IoA`     | bit 1 du code de touche | bouton gauche |
| 6   | `0x408` | B         | `v32::IoB`     | bit 2 du code de touche | bouton droit |
| 7   | `0x409` | X         | `v32::IoX`     | bit 3 du code de touche | compteur X : Gray haut |
| 8   | `0x40A` | Y         | `v32::IoY`     | bit 4 du code de touche | compteur X : Gray bas |
| 9   | `0x40B` | L         | `v32::IoL`     | bit 5 du code de touche | compteur Y : Gray haut |
| 10  | `0x40C` | R         | `v32::IoR`     | bit 6 du code de touche | compteur Y : Gray bas |

La console ne montre jamais deux directions opposées (Gauche + Droite, Haut + Bas)
en même temps — appuyer sur l'une relâche l'autre — et les deux protocoles sont
conçus autour de cela.

Lire 11 ports par l'API C voudrait dire 11 appels ; `IoDevice` écrit donc en lui-même
une petite routine en code machine (`IN` de chaque port, `IGT` en 0/1, décalage à sa
place, `OR` pour l'insérer ; 68 mots) et `scan()` l'appelle par `CALL` au moyen d'un
bloc `asm` de quatre lignes — la même technique que la bibliothèque C.

**Clavier.** Le périphérique ne signale au plus qu'un événement de touche par image,
et le maintient jusqu'au suivant. Le **strobe** distingue les événements les uns des
autres : le premier événement montre Left, le suivant Right, puis de nouveau Left,
de sorte qu'un nouvel événement est arrivé chaque fois que le côté diffère du dernier
vu (ce qui permet à la même touche d'arriver deux fois de suite). Up signifie appui,
Down signifie relâchement, et les sept boutons portent le code de touche sur 7 bits.
Majuscule et verrouillage majuscule sont des touches ordinaires ; `Keyboard` les suit
pour produire des caractères. Les caractères qui ne sont pas sur un clavier US (é, ß,
┌ ...) arrivent par **paire** d'événements, un préfixe de page de codes (code de
touche 30 ou 31) et une charge, tous deux comme des relâchements de touche : voir
[Caractères saisis](#caractères-saisis).

**Souris.** Le mouvement n'est pas envoyé sous forme de deltas — un delta maintenu
pendant une image ne se distingue pas du même delta envoyé deux fois — mais sous
forme de deux **compteurs**, un par axe, parcourant un cycle de 12 positions. Chaque
pas déplace un compteur d'une position et change **exactement un contrôle**, de sorte
que la console ne voit jamais un état intermédiaire entre deux positions. Un compteur
est un code de Gray sur 2 bits (4 groupes) plus un *trit* sur une paire de directions
opposées (3 états) :

| Position | 0  | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 10 | 11 |
| -------- | -- | -- | -- | -- | -- | -- | -- | -- | -- | -- | -- | -- |
| Gray     | 00 | 00 | 00 | 01 | 01 | 01 | 11 | 11 | 11 | 10 | 10 | 10 |
| Trit     | −  | 0  | +  | +  | 0  | −  | −  | 0  | +  | +  | 0  | −  |

X utilise Left/Right pour son trit et X/Y pour son code de Gray ; Y utilise Up/Down
et L/R. Au repos après la mise sous tension, les deux compteurs sont à la position 1
(rien d'enfoncé). Chaque sondage compare les positions aux précédentes : on peut
distinguer jusqu'à ±5 pas par image. Cette disposition utilise tous les états que les
11 contrôles peuvent montrer (8 combinaisons de boutons × 12 × 12 positions = 1152),
ce qui explique qu'il n'y ait pas de place pour une molette. Lorsque la manette vient
d'être connectée, les compteurs repartent de zéro (aucun mouvement n'est pris sur
cette image).

La description complète du protocole se trouve dans `lib/README.md` et
`docs/PROTOCOLS.md` du dépôt v32io.

---

## Portage depuis la bibliothèque C

| C (`keyboard.h`, `mouse.h`, `v32io.h`)                | C++ (`v32/keyboard.hpp`, `mouse.hpp`, `v32io.hpp`) |
| ----------------------------------------------------- | -------------------------------------------------- |
| `v32kbd *kb = v32kbd_init (SECOND_GAMEPAD_PORT);`     | `v32::Keyboard kb( v32::SecondGamepadPort );`      |
| `v32kbd_free (&kb);`                                  | — (rien à libérer)                                 |
| `v32kbd_probe (&kb);`                                 | `kb.probe();`                                      |
| `v32kbd_read (&kb)`                                   | `kb.read()`                                        |
| `v32kbd_readevent (&kb, &pressed)`                    | `kb.read_event( &pressed )`                        |
| `v32kbd_getkey (&kb)` (+ `free()`)                    | `kb.next_event( &event )` (un `v32::KeyEvent`)     |
| `v32kbd_isdown (&kb, V32KEY_LCTRL)`                   | `kb.is_down( v32::KeyLCtrl )` ou `kb.ctrl_down()`  |
| `v32kbd_glyph (key)`                                  | `kb.glyph( key )` ou `v32::key_glyph( key )`       |
| `V32KBD_GLYPH`, `V32KBD_PAGE_TIMEOUT`                 | `v32::KeyboardGlyph`, `v32::KeyboardPageTimeout`   |
| `V32KEY_PAGE_HIGH`, `V32KEY_PAGE_LOW`                 | `v32::KeyPageHigh`, `v32::KeyPageLow`              |
| `key -> text` (un `v32key`)                           | `event.text` (un `v32::KeyEvent`)                  |
| `v32kbd_symbol (kb, key)`                             | `kb.symbol( key )`                                 |
| `v32kbd_scan (kb)`                                    | `kb.device()->scan()`                              |
| `kb -> capslock`                                      | `kb.caps_lock_on()`                                |
| `v32mouse *m = v32mouse_init (port);`                 | `v32::Mouse m( port );`                            |
| `v32mouse_position (&m, &x, &y)`                      | `m.position( &x, &y )` ou `m.x()`, `m.y()`         |
| `v32mouse_delta (&m, &dx, &dy)`, `m -> dx`            | `m.delta( &dx, &dy )`, `m.dx()`                    |
| `m -> stepx`                                          | `m.steps_x()`                                      |
| `v32mouse_setposition / setbounds / setscale (&m, …)` | `m.set_position / set_bounds / set_scale( … )`     |
| `v32mouse_isdown / pressed / released (&m, V32MOUSE_LEFT)` | `m.is_down / pressed / released( v32::MouseLeft )` |
| `v32mouse_counter (…)`, `v32mouse_steps (…)`          | `v32::mouse_counter( … )`, `v32::mouse_steps( … )` |
| `v32io *io = v32io_init (port);` … `v32io_read (&io)` | `v32::IoDevice io( port );` … `io.read()`          |
| `V32IO_A`, `V32KEY_ESCAPE`, `V32MOUSE_ANY`            | `v32::IoA`, `v32::KeyEscape`, `v32::MouseAny`      |

Les noms de constantes suivent une seule règle : retirer le préfixe C, mettre le
reste en CamelCase, et le placer dans `v32::` (`V32KEY_LSHIFT` → `v32::KeyLShift`,
`FOURTH_GAMEPAD_PORT` → `v32::FourthGamepadPort`).

Le comportement est le même, avec ces différences délibérées :

- La file du clavier est un tampon circulaire fixe de 64 événements à l'intérieur de
  l'objet au lieu d'une liste allouée par `malloc` (même limite, pas de trafic de tas
  par touche).
- Un événement qui arrive alors que la file est pleine met quand même à jour
  `is_down()` et le verrouillage majuscule ; `dropped()` le compte.
- La routine du noyau n'est régénérée qu'une fois, mais l'adresse où elle stocke son
  résultat est rafraîchie à chaque scan, de sorte qu'un `IoDevice` qui a été copié
  fonctionne toujours.

---

## Exemples

`demos/cxx/v32io/` (voir son [README](../demos/cxx/v32io/README.md)) :

| Programme        | Périphériques                    | Ce qu'il montre                                          |
| ---------------- | -------------------------------- | -------------------------------------------------------- |
| `typewriter.cpp` | clavier, Gamepad 2               | `read()`, majuscule / verrouillage majuscule, état des modificateurs |
| `paint.cpp`      | souris, Gamepad 2                | pointeur, limites, fronts de boutons, tracés lisses      |
| `monitor.cpp`    | n'importe lesquels, les quatre ports | `IoDevice` et le protocole brut — un outil de réglage/débogage |
| `notepad.cpp`    | clavier Gamepad 2 + souris Gamepad 3 | deux périphériques v32io à la fois                   |
| `glyphs.cpp`     | clavier Gamepad 2 (deux de ses trois pages n'en ont pas besoin) | tous les glyphes de la police du BIOS, comment les saisir, caractères saisis contre touches |

`cd demos/cxx/v32io && make` les construit (`v32c++` et les DevTools dans le `PATH`).

---

## Tester sans périphérique

Les outils sans écran de `tools/vircon32/` (`v32peek`, `v32shot`, `v32prof`) jouent
un script d'entrée d'appuis et de relâchements de manette. Un périphérique v32io
*est* une manette, de sorte que les entrées clavier et souris peuvent elles aussi
être scriptées : `tools/vircon32/v32io-script.py` encode des actions avec les vrais
protocoles.

```text
connect 1 1                  # plug a "gamepad" into port id 1 (Gamepad 2)
connect 2 1                  # ... and one into port id 2 (Gamepad 3)
at 300                       # the standard BIOS hands over around frame 250
kbd 1 type Hello, World!     # shift added as needed; \n is Enter
kbd 1 tap f1
kbd 1 char 233 0x80 20        # typed characters, by BIOS code: é € █ (2 frames each)
kbd 1 press lctrl
kbd 1 tap c
kbd 1 release lctrl
mouse 2 move 50 -20          # device steps, spread over frames (5 max per frame)
mouse 2 click left
wait 10
```

```sh
tools/vircon32/v32io-script.py actions.txt > script.txt
tools/vircon32/bin/v32shot tools/vircon32/bin/StandardBios.v32 game.v32 script.txt shot 400
```

`tests/119sample.cpp` est un test auto-vérifiant de tout l'en-tête piloté de cette
façon (`tests/119sample.actions` → `tests/119sample.input`) ; `make realcheck` le
lance. Son script a aussi été recoupé avec les pilotes C d'origine, qui en décodent
les mêmes 110 événements de touches et positions de souris.

`tests/131sample.cpp` fait de même pour les caractères saisis : décodage du préfixe
et de la charge (y compris des charges égales aux codes de préfixe), le délai
d'expiration, un préfixe qui rencontre un débranchement / rebranchement, `read()` +
`glyph()`, et `read_event()` ignorant les caractères. `kbd <port> char <code>...`
envoie des caractères comme le font l'adaptateur et l'émulateur ; `connect <port> 0`
puis `1` simule un rebranchement (le nouveau périphérique repart de zéro). La démo
`glyphs` a son propre script, `demos/cxx/v32io/glyphs.actions`, qui joue une session
de saisie afin de pouvoir photographier ses trois pages avec `v32shot` sans clavier ;
les captures d'écran de `demos/cxx/v32io/images/` en proviennent.

---

## Limites

- **Vircon32 uniquement.** Le noyau lit les ports INP de la console au moyen d'une
  routine en code machine ; avec `--target=standard`, l'en-tête se transpile mais ne
  veut rien dire.
- **Disposition US.** Les codes de touche sont des touches physiques signalées à la
  mode US, et `symbol()`/`read()` appliquent les règles de majuscule US. Les
  caractères imprimés des autres dispositions (et AltGr) ne sont pas traduits.
- **Les caractères sont ceux que la police du BIOS peut dessiner** : les codes de
  touche font 7 bits et identifient des touches ; tout ce qui dépasse l'ASCII
  (accents, symboles, tracé de cadres) arrive comme un caractère saisi, dans le code
  propre à la police (Windows-1252 plus tracé de cadres). Les caractères que la police
  n'a pas (cyrillique, CJK, ...) ne peuvent pas être saisis. Sur l'adaptateur, ils
  demandent un clavier US et sa couche AltGr / touches mortes / codes Alt ; les
  caractères imprimés des autres dispositions ne sont pas traduits (l'émulateur
  utilise la disposition de l'ordinateur et les traduit, lui).
- Les caractères saisis prennent deux images chacun (deux événements), donc une saisie
  rapide avec accents s'empile : 30 caractères par seconde au plus. Les glyphes de
  tracé de cadres dans un littéral de chaîne sont réécrits dans des tableaux cachés
  (voir plus haut).
- **Un événement par image.** Le protocole du clavier transporte au plus 60 événements
  par seconde (30 frappes, puisque appui et relâchement sont des événements séparés),
  quoi que fasse le dactylo. Aucune répétition de touche n'est générée.
- **Pas de molette**, et le mouvement de la souris est plafonné à 5 pas par axe et par
  image (augmentez l'échelle pour un pointeur plus rapide).
- La file contient 64 événements ; au-delà, les événements sont perdus (comptés par
  `dropped()`) si le programme ne lit pas.
- Les noms de classe `IoDevice`, `Keyboard` et `Mouse` sont émis nus dans le C généré
  (une limitation générale de v32c++ pour les classes dans un espace de noms), si bien
  qu'un programme ne peut pas définir sa propre classe portant l'un de ces noms.
