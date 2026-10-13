# Régie5 — Architecture

Document de la phase 0 (reconnaissance). Il décrit ce que Régie5 reprend de Jelly5,
ce qu'il réécrit, l'architecture des backends Linux et PS5, et les risques identifiés.

Référence étudiée : Jelly5 (`02dnot/Jelly5`, commit `552b866`, 9 octobre 2026), GPL-3.0-or-later.

---

## 1. Ce qu'est Jelly5, techniquement

Jelly5 est une application native PS5 (« app module » signée en fake-SELF, Title ID
`PPSA99505`) écrite en C++17 et en C. Elle ne passe ni par un navigateur, ni par SDL,
ni par OpenGL : elle dessine directement sur le GPU de la PS5.

| Couche | Fichiers | Rôle | Lié à la PS5 ? |
|---|---|---|---|
| API de rendu 2D | `app/src/gfx/gfx.h` | Rendu 2D en mode immédiat : rectangles arrondis, dégradés, images, texte, ombre, liseré lumineux, flou d'arrière-plan / verre, masque de fondu, opacité et scissor empilables. Espace logique **1920×1080** mis à l'échelle du panneau (×2 en 4K). | **Non** (l'en-tête ne dépend que de la STL) |
| Backend de rendu | `app/src/gfx/gfx_agc.cpp`, `gfx_pool.cpp`, `app/engine/media/src/evo_agc_*.c` | Implémentation de `gfx.h` sur `sceAgc` (pilote GPU bas niveau de Sony), repris du moteur d'EVO Player. Chaque primitive = un quad indexé ; le shader `ui_screen_2d` découpe chaque quad en rectangle arrondi anti-aliasé. | **Oui, entièrement** |
| Shaders | `app/shaders/liquid_glass.pipe`, `app/engine/shaders/agc/*_pipe.h` | Le shader du verre est écrit en **GLSL 450**, puis compilé hors ligne par `amdllpc` (gfx1013) dans un conteneur Docker. Le résultat est un en-tête C binaire. | Le source GLSL est portable, mais le binaire est propre à la PS5 |
| Widgets / style | `app/src/ui/screen.{h,cpp}`, `anim.h`, `nav.cpp` | La goutte de focus (`Drop`), `glass_panel`, le fond ambiant (`Ambient`), la levée des cartes (`Lifts`), le défilement des rangées (`row_scroll`), `pill_bar`, le texte défilant, les indications de boutons de manette. | Non, mais couplé à `jf::Client` / `jf::Item` |
| Écrans | `app/src/ui/*.cpp` | Accueil, bibliothèque, détail, Seerr, réglages… | Non, mais métier Jellyfin |
| Texte | `app/src/ui_text.c` | FreeType + HarfBuzz + FriBidi (Inter, repli Noto). Rastérisé sur CPU, puis mis en cache comme texture par couple (texte, style). | Non (polices système PS5 en option) |
| Images | `app/src/ui_image.c`, `gfx/art.cpp` | Téléchargement et décodage (libjpeg, libpng, libwebp, nanosvg) sur des threads de travail ; cache disque ; envoi vers le GPU de quelques textures par image affichée ; cache LRU avec budget mémoire ; BlurHash en attendant ; fondu à l'apparition. | Le HTTP des images passe par **FFmpeg avio** |
| HTTP | `app/src/jf/jf_http.h` + `jf_http_avio.cpp` (PS5) / `tests/host/jf_http_curl.cpp` (Linux) | Une seule fonction bloquante `http_request(method, url, headers, body, timeout)`. Sur PS5, c'est le client HTTP de FFmpeg ; sur Linux, libcurl (pour les tests). | Abstraction **déjà en place** |
| Input | `app/src/nuvio_input.{h,c}` | `scePad` vers un masque de bits (`pressed` / `released` / `held` / `repeats`), répétition automatique du D-pad, gâchettes analogiques, barre lumineuse, vibration, télécommande HDMI-CEC. | Oui (mais l'interface est un simple masque de bits) |
| JSON | `app/engine/addons/src/cJSON.c` | cJSON 1.7.18 (MIT) | Non |
| i18n | `app/src/app/i18n*.cpp` | Le texte norvégien sert de clé (`T("Spill av")`) ; 27 tables. | Non |
| Boucle principale | `app/src/main.cpp` | Lit l'input à chaque tour ; ne redessine que si quelque chose a changé ou si une animation est en cours, sinon `usleep(8 ms)`. `dt` est plafonné à 0,1 s. Pas de `exit()` (planterait sur PS5). | Partiellement |
| Packaging | `app/scripts/build.sh`, `toolkit/tools/native-app/`, `app/sce_sys/` | SDK ps5-payload-dev v0.43 + sysroot pacbrew v0.39 → lien PIE → module PS5 → fake-SELF → `PPSA99505/{eboot.bin, sce_sys/, sce_module/libc.prx}` → zip (+ image `.ffpfsc`). Polices et icônes intégrées dans le binaire via `.incbin`. | Oui |

### 1.1 Le verre « liquid glass » en détail

1. `gfx::backdrop_blur(r, radius, sigma)` floute **ce qui est déjà dessiné** sous `r`.
   Le flou gaussien est séparable (passe horizontale puis verticale) et s'applique à une
   zone agrandie de 30 px pour que la réfraction puisse aller chercher l'image juste à
   l'extérieur du bord.
2. Le shader `liquid_glass` s'exécute ensuite sur le panneau. Pour chaque pixel :
   - distance signée au rectangle arrondi, ce qui donne la couverture anti-aliasée et la normale ;
   - **réfraction d'après OverShifted/LiquidGlass** : `f(t) = 1 - 2.3·(5.2e)^(-6.9t - 0.7)`,
     déplacement `h·(1 - f³)` le long de la normale. L'effet est celui d'une lentille épaisse,
     très déformée au bord et plate au centre, avec une légère dispersion RVB ;
   - vibrance (saturation et luminosité), teinte, assombrissement automatique sur une image
     claire (lisibilité du texte blanc) ;
   - halo du bord selon l'angle avec la lumière, ombre interne, liseré, reflet venant du haut
     et léger grain. La direction de la lumière suit l'inclinaison de la DualSense.
3. **Deux replis** quand le GPU manque de place : flou sans shader (zoom de 3,5 % + bords
   « tirés »), puis panneau sombre opaque.

### 1.2 La goutte de focus (`ui::Drop`)

- Un **ressort sous-amorti** sur x, y, w, h : `v += (260·(cible - x) - 19·v)·h`, intégré
  par pas de 1/240 s (stable quel que soit le framerate), `dt` plafonné à 50 ms.
- Un second ressort `pop` (raideur 300, amortissement 14) : la goutte « gonfle » quand elle
  part (`vpop += 2.2` à chaque changement de clé) et « éclot » à l'apparition (`pop = -0.35`).
- Elle s'étire dans le sens du mouvement (+40 px à 1600 px/s) et s'amincit en travers.
- Elle est dessinée comme un `glass_panel` avec `lift = 1` : le même verre, plus clair,
  **sans passe de flou à elle** (sa propre lentille sur une image en mouvement scintillait).
- Une goutte par groupe de contrôles. Les coordonnées sont relatives au groupe, pour suivre
  un groupe qui défile.

### 1.3 Autres animations

- `ui::Anim` : approche exponentielle `value += d·(1 - e^(-rate·dt))`, indépendante du framerate.
- `Lifts` : levée des cartes au focus (échelle ×1,1, ombre).
- `Ambient` : le fond change seulement après 0,35 s de focus stable (pas de clignotement en
  défilement rapide), puis fond enchaîné au rythme 3,2/s avec `smoothstep`, assombri par
  `fill(kBg, dim)`. Le fond est le **BlurHash** agrandi, donc gratuit à flouter.
- `push_fade_mask` : fondu aux bords des listes qui défilent, calculé dans la géométrie
  (aucune passe hors écran).

---

## 2. Ce que Régie5 réutilise, adapte ou réécrit

| Élément | Décision | Pourquoi |
|---|---|---|
| **API `gfx.h`** | **Reprise quasi telle quelle** comme interface de rendu de Régie5 (`src/ui/gfx.h`), réduite à ce dont on a besoin. | C'est exactement l'abstraction demandée par le cahier des charges. Elle est déjà portée sur PS5 et en production. Les écrans n'ont pas à savoir s'il y a OpenGL ou AGC derrière. |
| Backend `gfx_agc.cpp` + runtime AGC d'EVO | **Pas en phase 1–5.** Réutilisé en phase 6 si on choisit la voie A (voir §4). | Il ne fonctionne que sur PS5. |
| Shader `liquid_glass` | **Porté en GLSL 330 / GLSL ES 3.00** pour le backend OpenGL. Mêmes uniforms, même profil de réfraction. | Le source GLSL 450 se traduit presque ligne à ligne. |
| `ui::Drop`, `glass_panel`, `Anim`, `Lifts`, `row_scroll`, `Ambient`, `pill_bar`, texte défilant, indications de boutons | **Copiés puis adaptés**, en conservant les constantes (raideurs, durées, couleurs). | C'est ce qui fait le rendu « Jelly5 ». Ils doivent être découplés de `jf::Client` (par exemple, `draw_poster` prend une URL et une couleur de repli au lieu d'un `jf::Item`). |
| Ombre en 9 tranches, texture du liseré, rectangles arrondis via SDF dans le shader 2D | **Réécrits pour OpenGL**, avec le même principe. | Ce sont des détails du backend. |
| `jf_http.h` (`http_request` bloquant) | **Interface reprise.** Une file de travail asynchrone sera ajoutée par-dessus (§3.3). | Elle est déjà double (avio / curl). |
| `ui_text.c` (FreeType + HarfBuzz + FriBidi) | **Réécrit plus simplement** : FreeType seul (crénage de base) pour FR/EN, avec le même cache texte→texture. | Pas de texte bidirectionnel ni de script complexe en V1. On ajoutera HarfBuzz si une langue l'exige. |
| `ui_image.c` + `art.cpp` | **Réécrits en reprenant leur conception** : threads de travail, budget LRU, quelques envois GPU par image, fondu. Décodage par **stb_image** (JPEG/PNG). | Sonarr/Radarr `mediacover`, Jellyfin (`format=Jpg`) et le proxy Seerr servent du JPEG ou du PNG. WebP et SVG ne sont pas nécessaires. |
| BlurHash | **Repris** (algorithme public), pour Jellyfin seulement. | Sonarr, Radarr et Seerr n'ont pas de BlurHash. Pour eux, le fond ambiant vient d'une **petite version de la jaquette** (≈ 64 px), floutée sur CPU puis agrandie en filtrage linéaire. C'est le même coût qu'un BlurHash. |
| `nuvio_input` | **Interface reprise** (masque de bits `pressed/released/held/repeats` + répétition automatique). Le backend Linux sera écrit avec SDL GameController et le clavier. | La boucle et les écrans ne verront jamais SDL. |
| cJSON | **Repris** (MIT). | Il est utilisé des deux côtés. |
| i18n | **Réécrit** : clés symboliques (`tr(S_TAB_HOME)`), tables FR et EN. | Le système à clé norvégienne de Jelly5 n'a pas de sens ici. |
| Boucle « ne redessine que si nécessaire » | **Reprise.** | Elle économise la PS5 et le laptop, et rend le 60 fps facile quand rien ne bouge. |
| Client Seerr de Jelly5 | **Lu comme référence seulement** (endpoints, `imageproxy`). | Régie5 utilise la clé d'API admin (`X-Api-Key`) et non la session utilisateur de Jelly5. Son périmètre est différent (approbation des demandes). |
| Packaging PS5 (`build.sh`, `toolkit/`, `sce_sys/`) | **Phase 6** : on reprend la chaîne et on change le Title ID, le `contentId` et les icônes. | C'est une chaîne déjà validée sur la console. |
| Lecteur vidéo, moteur EVO, sous-titres, HDR, upscaling | **Rien n'est repris.** | Régie5 ne lit pas de vidéo. |

**Licence.** Le code de Jelly5 (Drop, glass_panel, shader, `gfx.h`…) est sous GPL-3.0-or-later,
donc **Régie5 sera sous GPL-3.0-or-later dès le premier commit**. `THIRD_PARTY_NOTICES.md`
listera au minimum : Jelly5, Nuvio PS5 et EVO Player (pour les parties dont `gfx.h` /
`screen.cpp` descendent), OverShifted/LiquidGlass (MIT), cJSON (MIT), stb_image (domaine
public / MIT), FreeType (FTL), SDL2 (zlib), libcurl (curl), Inter (OFL 1.1) et l'algorithme
BlurHash (MIT) si on en reprend une implémentation.

---

## 3. Architecture de Régie5

### 3.1 Arborescence

```
src/
  core/            aucun include plateforme, aucun include SDL / GL / curl
    app.{h,cpp}        boucle logique : input → navigation → écrans → dessin
    model/             Media, QueueItem, Request, Session, Task, Health, DiskSpace…
    store.{h,cpp}      état partagé + versions (les écrans redessinent quand la version change)
    i18n.{h,cpp}       FR / EN
    config.{h,cpp}     lecture de config.json (cJSON) ; les clés ne sont jamais loggées
    log.{h,cpp}        journal avec masquage des clés (X-Api-Key, Token="…")
  api/
    http.h             interface : requête → future/callback (voir 3.3)
    sonarr.{h,cpp}  radarr.{h,cpp}  seerr.{h,cpp}  jellyfin.{h,cpp}
    parse_*.cpp        JSON → modèles (purs, testés sur fixtures/)
    mock_http.cpp      sert fixtures/ pour --mock
  ui/
    gfx.h              API de rendu (issue de Jelly5)
    anim.h  drop.{h,cpp}  glass.{h,cpp}  ambient.{h,cpp}  poster.{h,cpp}  widgets…
    art.{h,cpp}        cache d'images (URL → texture), budget mémoire, fondu
    text.{h,cpp}       FreeType → textures en cache
    screens/           home, downloads, requests, jellyfin, settings
  platform/
    platform.h         fenêtre/boucle, horloge, chemins, langue système, input
    linux/
      main_linux.cpp   SDL2 : fenêtre, contexte GL, événements, vsync
      gfx_gl.cpp       implémentation de ui/gfx.h en OpenGL 3.3 core
      shaders/         ui2d.vert/.frag, blur.frag, liquid_glass.frag (GLSL 330)
      input_sdl.cpp    clavier + SDL_GameController (DualSense)
      http_curl.cpp    libcurl, pool de threads
      paths_xdg.cpp    ~/.config/regie5/
    ps5/               phase 6 seulement
assets/   fonts/Inter-*.ttf, icons/, (shaders sources si partagés)
tests/    test_parse_*.cpp sur fixtures/, lanceur sans dépendance (ou doctest, header unique)
fixtures/ réponses JSON anonymisées + quelques jaquettes JPEG
scripts/  run-linux.sh, (phase 6) build-ps5.sh
docs/     ARCHITECTURE.md
```

Règle vérifiable : `grep -r "SDL\|GL/\|curl" src/core src/api src/ui` ne doit rien
renvoyer (sauf `mock_http.cpp`, qui ne touche que le système de fichiers à travers `platform.h`).

### 3.2 Les interfaces plateforme (le contrat pour la phase 6)

| Interface | Linux (phases 1–5) | PS5 (phase 6) |
|---|---|---|
| `gfx.h` (rendu) | OpenGL 3.3 core via SDL2, rendu à l'échelle de la fenêtre, avec un framebuffer hors écran pour le flou | Voie A : `gfx_agc.cpp` de Jelly5 · Voie B : le même `gfx_gl.cpp` sur GLES 3 via SDL2+Mesa (§4) |
| `input` → masque de bits | SDL : clavier (flèches, Entrée, Échap/Retour arrière, Q/E, R, M) + GameController | `scePad`, d'après `nuvio_input.c` |
| `http` | libcurl dans un pool de 4 threads | libcurl de pacbrew, ou `jf_http_avio.cpp` de Jelly5 |
| `paths` | `$XDG_CONFIG_HOME/regie5/config.json`, `--config` | Dossier de données de l'app (§5) |
| `clock` | `SDL_GetPerformanceCounter` / `clock_gettime` | `clock_gettime` / `sceKernelGetProcessTime` |
| `lang` | `--lang fr|en`, sinon `$LANG` | Langue système (`sceSystemServiceParamGetInt`) |
| fichiers ressources | lus depuis `assets/` | intégrés dans le binaire (`.incbin`), comme Jelly5 |

### 3.3 HTTP asynchrone (l'UI ne bloque jamais)

- `http::submit(Request{method, url, headers, body, timeout}, callback)` place la requête
  dans une file. Des threads de travail exécutent un `http_request` **bloquant**
  (l'interface de Jelly5). La réponse est remise sur une file de résultats, vidée par le
  thread principal au début de chaque image. **Les callbacks s'exécutent donc toujours sur
  le thread principal.** Pas de mutex dans les écrans.
- Le parsing JSON se fait sur le thread de travail : le résultat livré est déjà un modèle.
- Chaque service a un client (Sonarr, Radarr…) qui construit URL et en-têtes (`X-Api-Key`
  ou `Authorization: MediaBrowser Token="…"`) et ne fait rien d'autre.
- Le mode `--mock` remplace le backend HTTP par `mock_http`, qui associe une URL à un
  fichier de `fixtures/` (délai artificiel optionnel pour voir les squelettes de chargement).
- Les erreurs sont typées : `Unreachable`, `Timeout`, `Unauthorized`, `HttpError(code)`,
  `BadJson`. C'est ce qu'affichent la pastille et le message de l'écran Réglages.

### 3.4 Boucle et rendu

- 60 fps visés : vsync activée, `dt` plafonné à 0,1 s, et on redessine seulement si
  (input) ou (animation en cours) ou (version du store changée) ou (image arrivée).
- L'espace logique est 1920×1080. Le backend applique `scale = largeur_fenêtre / 1920`
  (2 en 4K). Le texte est rastérisé à `taille × scale`, donc net en 4K.
- Ordre d'une image : fond ambiant → contenu → panneaux de verre (chacun floute ce qui est
  dessous) → goutte de focus → libellés nets → barre d'onglets.
- **Flou sur OpenGL** : la scène est dessinée dans un FBO. Pour chaque panneau de verre, on
  copie la zone (`glBlitFramebuffer`) à demi-résolution, on applique deux passes de flou
  gaussien, puis le shader `liquid_glass` dessine dans le FBO principal. À la fin, le FBO
  est blitté à l'écran. Le nombre de panneaux floutés par image sera limité (une dizaine) ;
  les petits éléments (puces, cartes) utilisent le verre « sans flou » de Jelly5
  (`draw_glass_placeholder`).

### 3.5 Navigation et écrans

- `Screen` reprend l'interface de Jelly5 (`activate`, `input(pressed)`, `draw(now, dt)`,
  `animating()`, `modal()`).
- La barre d'onglets du haut porte sa propre goutte. L1/R1 changent d'onglet avec un fondu
  croisé (pas de glissement en V1).
- La confirmation (approuver, refuser, arrêter une session, scanner) est une feuille modale
  en verre avec deux boutons. La goutte part par défaut sur « Annuler ».
- Rafraîchissement : △ relance les requêtes de l'écran. Téléchargements se met à jour toutes
  les 5 s **seulement si l'onglet est visible** (minuterie suspendue sinon).

### 3.6 Données, images et règles du périmètre

- La file Sonarr/Radarr est convertie en `QueueItem { media_title, episode_label, progress,
  time_left, quality, status }`. **Le titre de release, l'indexeur, le client de
  téléchargement et `outputPath` ne sont jamais copiés dans le modèle.** Ils ne peuvent donc
  pas être affichés par erreur. Un test unitaire vérifiera qu'aucun de ces champs ne sort du parsing.
- Jaquettes : `mediacover` Sonarr/Radarr (avec `X-Api-Key`), images Jellyfin
  (`/Items/{id}/Images/Primary?fillWidth=…&format=Jpg`), Seerr via **`/imageproxy/tmdb/…`**.
  Jelly5 confirme que ce proxy est servi par Seerr ; c'est le serveur Seerr qui va sur
  internet, pas la PS5. En cas d'échec : carte générée (titre + dégradé déterminé par le
  titre), jamais d'image cassée.
- Aucune URL absolue renvoyée par une API n'est suivie si son hôte n'est pas celui d'un
  service configuré (garde-fou « hors ligne »).

---

## 4. Le portage PS5 : deux voies, et ce que ça change

**Constat important.** Le dépôt pacbrew de ps5-payload-dev contient maintenant :

- **Mesa 26.2** avec **RADV** (Vulkan sur le GPU de la PS5) et **zink** (OpenGL 4.6 et
  OpenGL ES 3.2 au-dessus de Vulkan), via EGL sur une plateforme `videoout` ;
- **SDL2** patché (`0001-ps5-opengl-opengles-and-vulkan-through-mesa.patch`), qui crée des
  fenêtres OpenGL, GLES ou Vulkan sur PS5.

Une seule base SDL2 + OpenGL (ES) pourrait donc tourner **aussi** sur PS5. Mais :

- le PKGBUILD de Mesa avertit lui-même que ces patches sont générés et non relus, et qu'ils
  contiennent probablement beaucoup de problèmes ;
- la sortie est forcée en 3840×2160 (il faudra rendre en 4K, ou en 1080p puis agrandir) ;
- Jelly5 épingle pacbrew **v0.39** et n'utilise pas ce chemin ; aucun port homebrew.page
  connu ne le valide encore pour une app 2D à 60 fps ;
- zink sur RADV sur PS5 : performances et stabilité inconnues.

| | Voie A : backend AGC de Jelly5 | Voie B : SDL2 + Mesa (zink) |
|---|---|---|
| Code à écrire en phase 6 | `gfx_agc.cpp` (adapté), runtime EVO, shaders recompilés avec `amdllpc` (Docker), input `scePad`, HTTP | Presque rien côté rendu : on recompile `gfx_gl.cpp` en GLES 3 |
| Risque | Faible : déjà en production sur PS5, verre compris | Élevé : expérimental, non relu |
| Poids | Gros : il faut embarquer une partie du moteur EVO | Mesa + libs dans le binaire (taille à mesurer) |

**Recommandation :**

1. Écrire `gfx_gl.cpp` **en n'utilisant que le sous-ensemble commun à OpenGL 3.3 core et
   GLES 3.0** (pas de `glMapBuffer` exotique, shaders compatibles `#version 330` /
   `#version 300 es` grâce à un préfixe). La voie B reste ainsi ouverte gratuitement.
2. Garder `gfx.h` **identique à celui de Jelly5** (mêmes signatures). La voie A reste ainsi
   ouverte : `gfx_agc.cpp` se branchera sans toucher aux écrans.
3. Au début de la phase 6, faire un **test de faisabilité d'une demi-journée** : un
   rectangle animé avec SDL2+Mesa sur la PS5, en mesurant le framerate. Si ça tient 60 fps,
   on prend la voie B ; sinon la voie A.

---

## 5. Risques et points à vérifier

| # | Risque | Impact | Mesure |
|---|---|---|---|
| R1 | Voie B (Mesa sur PS5) instable ou lente | Phase 6 plus longue | Garder les deux voies ouvertes (§4) ; test de faisabilité en début de phase 6 |
| R2 | **Persistance des données sur PS5** : le commentaire de `evo_data_path.h` indique que `/download0/…` **ne survit pas toujours à un redémarrage** sans « auto-unjail » de l'app (issue EVO #46). `/data/…` est durable une fois le sandbox levé. Le README de Jelly5 dit pourtant `/download0/jelly5`. | Le `config.json` pourrait disparaître | Piste retenue : **`/data/regie5/config.json`**, persistant et déposable par FTP comme les homebrews dans `/data/homebrew`. À vérifier en phase 6 (accès à `/data` depuis l'app). Le chemin passe déjà par `platform::default_config_path()` et peut être forcé par `--config` : le changer ne touche qu'au backend PS5 |
| R3 | **DNS sur PS5** : la toolchain fournit son propre `getaddrinfo` (`evo_dns.c`) | Un nom d'hôte local (`nas.lan`) pourrait ne pas se résoudre | Documenter « utilisez des adresses IP » ; tester un nom d'hôte en phase 6 |
| R4 | HTTP par FFmpeg avio (si on reprend celui de Jelly5) : les codes d'erreur sont regroupés (4xx → 499) et le corps des erreurs est perdu | Messages d'erreur moins précis | Préférer libcurl de pacbrew sur PS5 ; sinon accepter la dégradation |
| R5 | Coût du flou avec beaucoup de panneaux en 4K | Saccades | Flou à demi-résolution, verre sans flou pour les petits éléments, budget de panneaux par image ; mesure en phase 4 avec 50 jaquettes |
| R6 | Le titre de release fuit par un champ oublié (`title` de la queue Sonarr/Radarr = nom de release) | Refus homebrew.page | Le modèle ne contient pas ce champ + test unitaire dédié (§3.6) |
| R7 | Images Seerr : si le serveur Seerr n'a pas internet, `imageproxy` échoue | Pas de jaquettes Seerr | Carte générée ; on pourra aussi croiser avec la jaquette Radarr/Sonarr (même `tmdbId`/`tvdbId`) quand le média y est |
| R8 | Formats de réponse réels différents de la doc (Seerr v3, Jellyfin 12, Sonarr v4) | Parsing cassé | Phase 2 : demander à Vivian des `curl` réels pour constituer `fixtures/` avant d'écrire chaque parseur |
| R9 | Fuseau horaire et dates sur PS5 (calendrier « 14 prochains jours » en UTC) | Jour décalé | Calcul en UTC + décalage local fourni par `platform` ; à tester en phase 6 |
| R10 | Une app PS5 ne peut pas se quitter elle-même (`exit()` plante) | Pas de bouton « Quitter » | Ne pas en prévoir ; on ferme avec le bouton PS |
| R11 | Collision de Title ID (Jelly5 = `PPSA99505`) | Deux apps se marchent dessus | Vérifier `homebrew.page/catalog/v1.json` en phase 6 |
| R12 | Licence : reprendre du code GPL impose la GPL à Régie5 | Aucun, si c'est assumé | GPL-3.0-or-later dès le départ, `THIRD_PARTY_NOTICES.md` à jour |

---

## 6. Choix techniques pour Linux (phases 1–5)

- **Langage** : C++17 (+ C pour cJSON et stb), comme Jelly5. Pas d'exceptions dans le code
  partagé, pas de RTTI requis (pour faciliter la toolchain PS5).
- **Build** : CMake + Ninja, compilé avec clang (le même compilateur que la toolchain PS5,
  LLVM 21 est déjà installé).
- **Dépendances Linux** : SDL2, OpenGL 3.3 (Mesa radeonsi présent, OpenGL 4.6), libcurl
  (déjà installé), FreeType. cJSON et stb_image sont intégrés au dépôt.
  À installer :

  ```bash
  sudo apt install libsdl2-dev libfreetype-dev pkg-config
  ```
- **Polices** : Inter (Regular, Medium, SemiBold, Bold), OFL 1.1.
- **Tests** : petit exécutable `regie5_tests` lancé par `ctest`, sans réseau, sur `fixtures/`.

## 7. Découpage confirmé des phases

1. Squelette : fenêtre SDL2/GL, `gfx_gl.cpp` (fill, dégradés, texte, ombre, liseré,
   `backdrop_blur` + shader verre), input clavier + manette, goutte de focus, 5 onglets vides,
   `scripts/run-linux.sh`.
2. Données : HTTP asynchrone, clients, parseurs, tests, `--mock`, Réglages avec test réel.
3. Écrans et actions.
4. Finition et performances.
5. Recette Linux.
6. PS5 (sur feu vert) : test de faisabilité de la voie B, puis voie A ou B, packaging.

---

## 8. Mise à jour après la phase 1

### 8.1 Le fork Jelly5-Seer

Le fork Jelly5-Seer de Vivian (branches `feature/seerr`, `pr/linux-build`,
`pr/seerr-integration`) a été étudié. Son « build Linux » (commit `08b9328`) permet de
**compiler le binaire PS5 depuis Linux** : `setup-toolchain.sh` utilise le LLVM de la
distribution, `build.sh` trouve l'adresse de log avec `ip route`. Il ne contient **pas
de backend desktop** de l'API de dessin : `gfx_agc.cpp` reste le seul backend, et l'interface
ne peut pas tourner dans une fenêtre Linux. Le backend OpenGL de Régie5 a donc été écrit pour
ce projet (`src/platform/linux/gfx_gl.cpp`).

Ce fork servira en **phase 6** : son `setup-toolchain.sh` pour Linux est exactement ce qu'il
faut pour compiler Régie5 pour la PS5 sur le laptop de Vivian.

### 8.2 Écarts entre `gfx.h` de Régie5 et celui de Jelly5

Ils sont minimes, pour que `gfx_agc.cpp` puisse se brancher en phase 6 (voie A) :

- `texture_from_pixels` prend des octets RGBA prémultipliés au lieu de `uint32_t` ;
- ajout de `image_tint` (une texture utilisée comme masque, dans une couleur) : le texte
  passe par là ;
- pas encore de `begin_overlay`, `push_fade_mask` ni `image` depuis un `ui_image` ; ils
  seront ajoutés quand un écran en aura besoin ;
- **le texte est portable** : `ui/font.cpp` (FreeType) et `ui/gfx_text.cpp` (atlas de
  glyphes depuis la phase 4, §12.3) ne dépendent que de `texture_from_pixels`,
  `texture_update`, `image_tint_uv` et `set_solid_source`. Un backend n'a pas de texte à
  fournir.

Le backend OpenGL dessine des quads groupés. Chaque quad porte sa forme (rectangle, rayon),
et un seul shader en tire coins arrondis anti-aliasés, liseré lumineux (`rim`) et ombre douce
(`shadow`) à partir de la même distance signée. Il n'y a pas de textures en 9 tranches comme
dans Jelly5. La scène est dessinée dans une texture hors écran ; `backdrop_blur` en copie la
zone à demi-résolution, la floute en deux passes, puis applique le shader du verre de Jelly5
(`gl_shaders.h`). Les shaders n'utilisent que le sous-ensemble commun à GLSL 3.30 et GLSL ES
3.00 (`gfx_gl::init(es)`), ce qui garde la voie B ouverte.

### 8.3 Outils de développement (Linux uniquement)

- `REGIE5_FPS=1` : une ligne par seconde sur stderr (images, pire écart entre deux images,
  pire temps CPU, temps GPU moyen et pire dont la part du flou, textes rastérisés, images
  envoyées au GPU, textes en cache) et, pour tout écart de plus de 25 ms, une ligne
  « slow frame » qui détaille l'image d'avant et celle-ci (voir §12.1).
- `REGIE5_SCRIPT="1 right right 0.8 shot:/tmp/a.ppm quit"` : rejoue des appuis et enregistre
  des captures (PPM) de l'image complète, à la résolution de la fenêtre. Sert aux captures d'écran et
  aux vérifications sans manette.
- `REGIE5_RES=3840x2160` : dessine à cette taille, puis réduit l'image dans la fenêtre
  (mesurer la 4K sur un écran plus petit, §12.4).
- `scripts/capture-states.sh [états…]` : capture chaque écran dans chaque état (données,
  chargement, vide, hors ligne, clé refusée, mixte, désactivé, sans config), en FR et en EN,
  avec une planche par état (`build/states/`). En `--mock`, avec `REGIE5_MOCK_EMPTY` et
  `REGIE5_MOCK_OFF` en plus de `REGIE5_MOCK_FAIL`.
- `scripts/list-strings.py` : toutes les chaînes FR / EN en tableau Markdown, pour la relecture.

---

## 9. Mise à jour après la phase 2 (données)

### 9.1 Ce qui a été vérifié contre les serveurs de Vivian

Requêtes en lecture seule (GET) uniquement. Versions : Sonarr 4.0.20, Radarr 6.4.4, Seerr 3.5.0,
Jellyfin 12.2.0. Endpoints confrontés aux specs officielles (OpenAPI de Sonarr, Radarr et
Jellyfin 12.2.0, `seerr-api.yml`).

| Besoin | Endpoint retenu | Remarque |
|---|---|---|
| Test de connexion Sonarr/Radarr | `GET /api/v3/system/status` | 401 avec une mauvaise clé |
| Test de connexion Seerr | `GET /api/v1/settings/about` | `/api/v1/status` est **public** : il ne vérifie pas la clé (403 ici avec une mauvaise clé) |
| Test de connexion Jellyfin | `GET /System/Info` | 401 avec une mauvaise clé |
| Calendrier | `GET /api/v3/calendar?start&end&unmonitored=false` (+ `includeSeries` pour Sonarr) | Les images du calendrier n'ont qu'une `remoteUrl` (internet) : **on ne s'en sert pas** |
| Jaquettes Sonarr/Radarr | `GET /api/v3/mediacover/{id}/poster-500.jpg`, `fanart-360.jpg` | Servies par le serveur local, avec `X-Api-Key` |
| Jaquettes Seerr | `GET /imageproxy/tmdb/t/p/w342{posterPath}` | Servies par Seerr (c'est lui qui va sur internet, pas la PS5) |
| File d'attente | `GET /api/v3/queue?includeSeries&includeEpisode` / `includeMovie` | `timeleft` est déprécié : on préfère `estimatedCompletionTime` |
| Demandes Seerr | `GET /api/v1/request?filter=pending\|approved\|all` | Pas de titre dans la réponse : un `GET /api/v1/{movie,tv}/{tmdbId}` par média (à mettre en cache) |
| Ajouts récents Jellyfin | `GET /Items?SortBy=DateCreated&…&IncludeItemTypes=Movie,Episode` | `/Items/Latest` exige un `userId` en 12.x (400 sans) ; les épisodes d'une même série sont regroupés |
| Sessions | `GET /Sessions?activeWithinSeconds=960` | Seules celles avec `NowPlayingItem` sont gardées ; `TranscodingInfo` absent en Direct Play |
| Tâches | `GET /ScheduledTasks?isHidden=false` | |
| Actions (non appelées en réel) | `POST /api/v1/request/{id}/approve\|decline`, `POST /Sessions/{id}/Playing/Stop`, `POST /Library/Refresh` | Testées uniquement en `--mock` |

### 9.2 Règles de périmètre appliquées dans le code

- **File d'attente** : les champs `title` (nom de release), `statusMessages`, `errorMessage`,
  `downloadClient`, `indexer`, `outputPath`, `downloadId` ne sont **jamais lus** par le parseur.
  Le modèle (`core/model.h`) n'a aucun champ pour eux. Le test `queue_never_keeps_release_names`
  le vérifie.
- **Santé** : le message d'une alerte Sonarr/Radarr peut nommer le client de téléchargement
  (constaté sur le serveur de Vivian). Pour les sources `DownloadClient*`, `Indexer*`,
  `RemotePathMapping*`, `ImportMechanism*` et Prowlarr/Jackett, le texte est retiré et l'écran
  affichera un message générique.
- **Seerr** : `downloadStatus` (qui contient des titres de release pendant un téléchargement)
  n'est pas lu.

### 9.3 Organisation

- `src/api/http.{h,cpp}` : `Transport` (une requête bloquante) et `Pool` (4 threads de
  travail). Le parsing a lieu sur le thread de travail ; le résultat est livré sur le thread
  principal par `Pool::pump()`. Une requête terminée réveille la boucle (événement SDL).
- `src/api/{arr,seerr,jellyfin}.{h,cpp}` : chemins + parseurs purs, testés.
- `src/api/services.{h,cpp}` : un objet pour les 4 services (URL, en-tête d'auth,
  classification des erreurs : injoignable, délai, clé refusée, adresse incorrecte, HTTP,
  réponse inattendue, désactivé).
- `src/api/mock_transport.{h,cpp}` : `--mock`. Les actions (POST) sont seulement journalisées
  et répondent 204 : rien n'est jamais modifié.
- `src/platform/linux/curl_transport.{h,cpp}` : libcurl, un handle par thread.
  `REGIE5_HTTP_LOG=1` journalise méthode, adresse (sans la query), statut et durée, jamais les
  en-têtes.
- Les bibliothèques sont séparées : `regie5_data` (core + api + cJSON, sans aucun affichage),
  `regie5_ui`, et l'exécutable Linux. Les tests ne lient que `regie5_data`.

### 9.4 Fixtures

`scripts/make-fixtures.py` les régénère depuis les vrais serveurs, en GET uniquement. Il
anonymise (utilisateurs, appareils, identifiants, chemins, adresses), retire les champs
interdits, puis **refuse d'écrire** si un fichier contient encore une clé d'API, l'adresse
d'un serveur, une IP privée, un nom d'utilisateur ou d'appareil d'origine, ou quelque chose
qui ressemble à un nom de release ou de fichier. Les cas absents des serveurs (file
d'attente, demandes en attente, session en transcodage, tâche en cours) sont dérivés des
vraies réponses et marqués `"_regie5_synthetic": true`.

Variables pour essayer les états en `--mock` : `REGIE5_MOCK_DELAY_MS` (latence, 250 ms par
défaut) et `REGIE5_MOCK_FAIL=radarr:unreachable,seerr:unauthorized,…`.

---

## 10. Mise à jour après la phase 3 (écrans)

### 10.1 Les écrans

| Écran | Données | États | Actions |
|---|---|---|---|
| Accueil | santé et disques (Sonarr + Radarr), calendrier des 14 prochains jours (fusionné, trié), ajouts récents Jellyfin | squelettes au chargement ; rangée vide → message propre (« Rien de prévu… », « Rien d'ajouté… ») ; service en échec → message d'erreur à la place de la rangée, ou pastille rouge à côté du titre si l'autre service répond ; service désactivé → message | ✕ sur le bandeau : liste des alertes |
| Téléchargements | file Sonarr + Radarr fusionnée (en cours, puis à surveiller, puis en attente) | rafraîchie toutes les 5 s **uniquement quand l'onglet est affiché** ; la liste reste affichée pendant un rafraîchissement ; le focus suit le même élément | aucune (lecture seule) |
| Demandes | `En attente` / `Approuvées` / `Toutes` ; titre et affiche via un appel de détail par média, mis en cache, chargé seulement autour des lignes visibles | squelettes, vide par filtre, erreur Seerr | ✕ sur une demande en attente : Approuver / Refuser / Annuler (Annuler par défaut), puis confirmation (Annuler par défaut) |
| Jellyfin | sessions en cours (rafraîchies toutes les 5 s), tâches planifiées (10 s) | vide (« Personne ne regarde »), erreur, désactivé | ✕ sur une session : arrêter (confirmation) ; « Scanner les bibliothèques » (confirmation) |

Après une action, une note en haut de l'écran donne le résultat (« Demande refusée », ou
« Échec : Clé d'API refusée »…) et les listes concernées sont rechargées. En `--mock`, les
actions sont seulement journalisées (`mock: POST … (nothing done)`).

### 10.2 Images

- `ui/art` : cache d'images à la Jelly5. Téléchargement et décodage (`stb_image`, JPEG/PNG) sur
  un pool de 3 threads **séparé** de celui des API, mise à l'échelle par moyenne de zone, envoi
  au GPU de 3 textures par image au plus, budget de 192 Mo (LRU), fondu de 0,3 s.
- Sources, toutes locales : `mediacover` Sonarr/Radarr (`poster-500`, `poster-250`,
  `fanart-360`), Jellyfin (`/Items/{id}/Images/Primary|Backdrop?fillWidth&format=Jpg`), Seerr
  (`/imageproxy/tmdb/t/p/w185|w300…`, vérifié sur le serveur de Vivian).
- En cas d'échec (404, serveur injoignable, service désactivé, ou `--mock` qui n'a pas
  d'images) : **jaquette générée** (dégradé déterminé par le titre, titre écrit dessus ; sur
  les vignettes, l'initiale). Jamais d'image cassée. △ retente les images en échec.
- Fond ambiant : le fanart de l'élément focalisé, en 192×108, flouté sur CPU puis étiré. Le
  fondu ne démarre qu'une fois l'image arrivée et après 0,35 s de focus stable.

### 10.3 Divers

- Une feuille (`Sheet`) ouverte capte toutes les touches, L1/R1 compris (`Screen::modal()`).
- Les listes sont coupées au-dessus des indications de boutons. Un fondu en haut et en bas
  évite que le contenu passe brutalement sous la barre d'onglets.
- La file d'attente utilise `estimatedCompletionTime` tant qu'elle est dans le futur, sinon
  `timeleft` (une estimation dépassée est périmée).
- À voir en phase 4 : un pic isolé de ~80 ms observé une fois au passage d'une rangée à
  l'autre (dessin CPU < 1 ms ; à mesurer avec 50 jaquettes).

---

## 11. Mise à jour : alertes santé et Bibliothèque

### 11.1 Alertes santé

- Le champ `source` de `/api/v3/health` (le nom de la vérification) et le niveau (`error` /
  `warning` / `notice`) désignent un **type d'alerte** (`model::Health::Kind`, `arr::health_kind`).
  Le catalogue suit les sources de Sonarr et Radarr (`HealthCheck/Checks`). Pour les
  vérifications « tous / un seul » (clients, indexeurs, listes, notifications), le niveau
  `error` correspond à « tous » et `warning` à « un seul ».
- L'interface rédige elle-même la phrase de chaque type, en FR et en EN. Le texte du serveur
  n'est **jamais** montré pour une vérification connue. Pour une vérification inconnue (ou une
  annonce), il n'est gardé que si `arr::shareable` le juge sûr : aucun mot désignant un client,
  un indexeur ou un tracker, aucun nom de logiciel de téléchargement, aucun chemin, aucune adresse.
  Sinon, une phrase générique le remplace.
- **Depuis quand** : l'API ne donne aucune date. Les seuls statuts horodatés
  (`indexerstatus`…) ne sont plus documentés en v4. L'app affiche donc l'heure où **elle**
  a constaté l'alerte pour la première fois (« constatée depuis 14:34 »), en mémoire pour la
  session. Pour les indisponibilités longues, la phrase dit « depuis plus de 6 h », comme le
  serveur.
- La liste s'ouvre avec ✕ sur le bandeau de l'Accueil, qui l'indique (« ✕ Détails »). Les
  erreurs sont en tête.

### 11.2 Barre d'onglets

Accueil · Bibliothèque · Téléchargements · Demandes · Jellyfin, puis Réglages en engrenage
de verre à droite, avant l'horloge (comme l'avatar de Jelly5). L1/R1 parcourent les six.

### 11.3 Bibliothèque (lecture seule)

| Sous-vue | Source | Contenu |
|---|---|---|
| Séries | `GET /api/v3/series` (Sonarr) | grille de jaquettes (`poster-500`) ; tri titre / date d'ajout / prochaine diffusion ; filtre tous / surveillées / avec épisodes manquants / terminées |
| Films | `GET /api/v3/movie` (Radarr) | idem ; le filtre « Terminés » devient « Téléchargés » (fichier présent) pour les films |
| Manquants | `GET /api/v3/wanted/missing` (les deux, surveillés seulement, 250 premiers) | liste : d'abord ce qui est diffusé ou sorti (du plus récent au plus ancien), puis ce qui est à venir |
| Fiche détail | les objets ci-dessus + `GET /api/v3/qualityprofile` + `GET /api/v3/episodefile?seriesId=` | statut, surveillance, profil, taille, qualité des fichiers (par qualité, jamais par nom), prochaine diffusion ou sorties, saisons avec épisodes présents / manquants |

- **Aucune action** : pas de recherche (automatique ou interactive), pas de changement de
  surveillance ni de profil. Le seul ✕ ouvre une fiche. Les parseurs ne lisent jamais `path`,
  `relativePath`, `sceneName`, `releaseGroup(s)` ni `originalFilePath`.
- **Sous-vues** : ←/→ déplacent le focus sur les pilules, ✕ ouvre la sous-vue. Un passage sur
  « Manquants » n'emporte donc pas Tri et Filtre.
- **L2/R2** (Z/C) : grille triée par titre → lettre précédente / suivante (titre plié en
  minuscules sans accents : « À l'ouest » va sous A), avec la lettre affichée en grand un
  instant ; autre tri → page de deux rangées. Maintenu, il se répète comme le D-pad.
- **Jaquettes progressives** : `ui/art` garde les demandes en file. Au plus 8 téléchargements
  à la fois, les plus récemment demandées d'abord (celles à l'écran). Une demande pas encore
  envoyée et non redemandée depuis 20 images (défilée) est abandonnée. Seules les rangées
  visibles sont dessinées ; la rangée au-delà de chaque bord est demandée juste après.
- Mesuré sur la bibliothèque de Vivian (436 films, 53 séries) en défilement rapide : 85 à
  105 images/s ; dessin CPU le plus souvent < 5 ms, avec des pointes à 8–14 ms quand beaucoup
  de légendes nouvelles apparaissent (rastérisation du texte). À traiter en phase 4.

### 11.4 Fixtures

`scripts/make-fixtures.py --only library` régénère seulement les fichiers de bibliothèque
(séries, films, manquants, profils, fichiers d'épisodes par série), réduits aux champs lus.
Le test `fixtures_hold_no_forbidden_field` échoue si un champ interdit réapparaît dans
`fixtures/`. Il a été ajouté après la découverte de `statistics.releaseGroups` (un nom de
groupe de release) dans deux fixtures Radarr de la phase 2, désormais retiré, et ajouté à
la liste des champs supprimés par le script.


---

## 12. Phase 4 (finition)

### 12.1 Mesure par image (étape 4.2)

`core/perf` (portable : une horloge `std::chrono`, rien d'autre) garde les 8 dernières
images. Chacune porte :

| Champ | Mesuré par | Contenu |
|---|---|---|
| `events`, `pump`, `input`, `draw` | la boucle Linux | événements SDL ; réponses HTTP et images remises au thread principal ; input et écrans ; dessin CPU (`Shell::draw`) |
| `submit`, `present` | `gfx_gl` (`end_frame`) | envoi du dernier lot et copie à l'écran ; `SDL_GL_SwapWindow` (attente de vsync ou du compositeur) |
| `texts` / `text_ms` | `ui/gfx_text` | textes nouveaux rastérisés par FreeType, et leur coût |
| `uploads` / `upload_ms` | `ui/art` | jaquettes envoyées au GPU |
| `blurs` | `gfx_gl` | panneaux de verre floutés |
| `gpu_ms` / `blur_gpu_ms` | `gfx_gl` | horodatages GPU (`glQueryCounter`) au début et à la fin de l'image et autour de chaque flou, relus 4 images plus tard sans jamais attendre le GPU |

L'écart entre deux images va du début de l'image précédente au début de celle-ci. La ligne
« slow frame » imprime donc les deux, cinq images après coup pour que les temps GPU soient
connus. Les horodatages GPU ne sont actifs qu'en OpenGL desktop. En GLES (voie B de la
phase 6), `glQueryCounter` n'existe que par `EXT_disjoint_timer_query` : il faudra le
détecter ou s'en passer.

Premier constat (Linux, Wayland, écran 120 Hz, `--mock`) : les pics de 35 à 85 ms vus en
passage automatique sont **entièrement dans `present`**, avec 0,3 ms de GPU et moins de 1 ms
de CPU. À confirmer en usage réel, fenêtre au premier plan (étape 4.3).

### 12.2 Le pic au changement de rangée (étape 4.3)

Mesuré par Vivian, fenêtre au premier plan : les deux écarts au-delà de 25 ms (38 et 39 ms)
sont **entièrement dans `present`** (`SDL_GL_SwapWindow`), avec 0,1 à 0,2 ms de dessin, 0,3 à
0,4 ms de GPU, aucun texte ni aucune jaquette dans ces images. C'est l'attente du
compositeur ou de la vsync du laptop. **Rien n'est corrigé côté Linux** : sur PS5,
l'affichage passe par un autre pilote, et on mesurera là-bas en phase 6.

### 12.3 L'atlas de glyphes (étape 4.4)

Avant, chaque texte (chaîne, style, échelle) était rastérisé en entier par FreeType dans
**une texture à lui**. Chaque nouvelle légende coûtait donc une rastérisation et une
création de texture, et chaque texte coupait le lot de dessin. Maintenant :

- `font::layout` place les glyphes au pixel de sortie près, avec le même arrondi
  qu'avant : les captures sont identiques au pixel près.
- `gfx_text` range chaque glyphe (graisse, taille, caractère) **une seule fois** dans un
  atlas : des pages de 1024² (2048² au-delà de l'échelle 1,5), rangées par étagères, avec
  1 px d'écart. Les glyphes s'écrivent d'abord dans une copie CPU de la page (1 octet par
  pixel). La zone modifiée part au GPU en un seul envoi, avant le premier dessin qui la lit.
- La page 0 contient un bloc blanc opaque. Avec `set_solid_source`, les aplats, liserés
  et ombres lisent ce texel au lieu de leur propre texture blanche : texte et aplats se
  dessinent **dans le même lot**.
- Un texte en cache n'est plus qu'une liste de quads. L'atlas repart de zéro (dans
  `collect`, entre deux images) s'il dépasse 48 Mo, ou si l'échelle de sortie change de
  catégorie. Un parcours complet de l'app en 4K occupe environ 10 % d'une page.

Trois primitives ont été ajoutées au backend pour cela : `texture_update` (une région d'une
texture), `image_tint_uv` (une partie d'un masque) et `set_solid_source`. Elles sont à
fournir dans `gfx_agc.cpp` si la phase 6 prend la voie A. Attention à `texture_update` :
une région de l'atlas n'est jamais réécrite tant qu'elle peut être lue (l'atlas ne fait que
grandir, puis repart de zéro entre deux images). Un backend qui écrit directement dans la
mémoire GPU n'a donc pas à attendre le GPU.

Mesures (laptop, vraie bibliothèque : 436 films, Films en défilement rapide puis sauts de
page R2/L2) :

| | Avant | Atlas |
|---|---|---|
| Appels de dessin par image (pire) | 75 à 139 | 28 à 57 |
| Texte par image en défilement (pire) | 1,0 à 1,8 ms | 0,2 à 0,6 ms |
| Rastérisations en régime établi | une par légende (~170/s) | une par glyphe nouveau (4 à 30/s) |
| Première image, 1080p (à froid) | 6,7 ms | 7,5 ms |
| Première image, 3200×1800 (à froid) | 30 à 33 ms | 19 à 55 ms (création de la page 2048², envois) |

La première image reste chère en 4K, mais une seule fois : création de la page par le
pilote, puis rastérisation de quelque 300 glyphes en grande taille. Les pics de 8 à 14 ms
notés en phase 3 ne se reproduisent plus sur le laptop.

### 12.4 Le coût du flou en 4K (étape 4.5)

`REGIE5_RES=3840x2160` dessine en 4K hors écran, puis réduit l'image à la taille de la
fenêtre (`gfx_gl::set_render_size`). Cela permet de mesurer la 4K sur l'écran du laptop.

Constat : le flou se faisait à demi-résolution **de sortie**, avec un noyau en texels
proportionnel à l'échelle. En 4K, il traitait 4 fois plus de pixels avec un noyau 2 fois
plus large, soit environ 8 fois le coût en 1080p. Sur un parcours complet (Accueil,
Bibliothèque en défilement, Téléchargements, Demandes, Jellyfin, Réglages), on mesurait en
4K jusqu'à **21 ms de GPU par image, dont 15,8 ms de flou** (grands panneaux de Jellyfin et
Réglages).

Correction : la région est réduite sur une grille de demi-pixels **logiques**
(`down = max(2, 2·scale)`, donc au quart en 4K) et le noyau est exprimé dans cette
grille. Le coût du flou ne dépend plus de la résolution de sortie. Rien ne change en
1080p ; en 4K, les captures avant / après diffèrent de 0,07 % (RMSE), sans écart visible.

| Parcours complet, GPU par image | 1080p | 4K avant | 4K après |
|---|---|---|---|
| Moyenne | 0,6 ms | 2,1 à 10 ms | 2,1 à 2,9 ms |
| Pire | 0,6 à 8,5 ms (1re seconde) | 21 ms (flou 15,8 ms) | 3 à 5 ms (flou ≤ 3,1 ms) |

En 4K, la moyenne de 2,3 ms comprend la réduction finale vers la fenêtre, qui n'existe
pas sur une vraie sortie 4K.

### 12.5 États et textes (étapes 4.6 et 4.7)

- `REGIE5_MOCK_EMPTY=sonarr,…` vide les listes des fixtures sans changer la forme des
  réponses (tableaux vides, compteurs à 0 ; testé). `REGIE5_MOCK_OFF=…` désactive des
  services dans la config de `--mock`.
- Règles d'affichage retenues : voir `ROADMAP.md`, « Décisions › Interface ».
- `fmt::labelled` (« Sonarr : … » / « Sonarr: … ») et `fmt::count` (singulier pour 0 et 1
  en français, pour 1 seulement en anglais) remplacent les chaînes construites à la main.

---

## 13. Phase 6 (portage PS5) : les deux voies revues

### 13.1 Mesa de pacbrew : inutilisable dans une app

pacbrew v0.41.1 (10 octobre 2026) livre Mesa 26.2 (RADV + zink) et un SDL2 OpenGL/GLES,
mais **en bibliothèques partagées** (`libEGL.so`, `libGLESv2.so`, `libgallium-26.2.0.so`,
`libvulkan.so`) que SDL2 ouvre par `dlopen`. Ce chargeur n'existe que pour les **payloads**
(le `crt1.o` du SDK) ; dans une **app native** (tuile, `eboot.bin`), `__dlopen` est une
fonction vide (boîte à outils de Jelly5, `stubs/libc_ext.c`). De plus ces `.so` dépendent de
`libkernel_web.sprx`, qu'une app ne charge pas. La voie B telle que décrite au §4 est donc
fermée pour une app.

### 13.2 La voie retenue pour la sonde : ps5-opengl (statique)

[ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl) 1.0.1 (BlackBearReloaded,
GPL-3.0-or-later) est un **SDK statique** d'OpenGL 4.6 Core sur Mesa/Gallium pour les apps
natives : contexte EGL, 1080p / 1440p / 4K à 60 ou 120 Hz, suite de conformité OpenGL 4.6
de Khronos passée sur PS5. Deux apps du catalogue homebrew.page l'utilisent (« PS5 OpenGL
Showcase », « Homebrew UI Lab », interface en verre dépoli 4K). Le backend
`src/platform/gl/gfx_gl.cpp` (GL 3.3 core) tourne **sans changement** dans un contexte 4.6
Core : c'est une voie B, sans SDL.

| Pièce | PS5 |
|---|---|
| Rendu | `platform/gl/gfx_gl.cpp` (le même que Linux), contexte par `platform/ps5/display_egl` |
| Entrée | `platform/ps5/pad_ps5` (scePad direct ; stick gauche = croix) |
| Fichiers | `/app0` = le dossier du titre (`/data/homebrew/<TITLE_ID>`, écrit depuis un PC) : assets, et `config.json` (résout R2 sans accès à `/data`) |
| Journal | stderr → `/download0/regie5/dev/app.log` (non tamponné, lisible en FTP sous `/mnt/sandbox/<TITLE_ID>_000/download0/…` pendant que le titre tourne) ; copie UDP vers le laptop en développement |
| Mémoire | tas fixe (`runtime/app_heap.c`) : 256, sinon 192, sinon 128 Mio (512 refusé sur 13.60, 128 accordé) ; aucun tas accordé = arrêt immédiat, le malloc du système étant bien trop petit. Budgets selon le tas (jaquettes 64 / 48 / 32 Mio, glyphes 24 / 24 / 16 Mio) ; la zone de rendu du SDK est de 256 Mio |
| Threads | 64 Kio de pile par défaut : les threads de travail de `http::Pool` (pthreads) en demandent 512 Kio (`http::kWorkerStack`) |
| Fin | jamais `exit()` ni retour de `main` : `sceSystemServiceLoadExec("exit")` |
| Empaquetage | chaîne de ps5-homebrew-ui (boilerplate natif, FSELF, `libc.prx`), clonée à un commit fixé par `scripts/build-ps5.sh` |

Écarts corrigés pour la console : aucun flux C++ (`<sstream>`, `<fstream>` tirent des
fonctions de locale absentes du runtime d'app) ; aucune fonction de date de la libc de la
console (`localtime`, `mktime`, `time`, `strftime`, jamais appelées par une app qui tourne
déjà) : `platform/ps5/time_ps5.c` fait le calcul de calendrier lui-même et ne demande au
noyau que le décalage horaire (`sceKernelConvertUtcToLocaltime`).

Journal : chaque ligne de `ps5::log` part en UDP **avant** l'étape suivante ; un
gestionnaire de signaux envoie un plantage (signal, adresse fautive, adresses de retour
probables), à relire avec `build/ps5/<nom>/llvm-pie.elf` (repère : l'adresse de `ps5::log` ;
adresse dans l'ELF = adresse sur la console − (adresse de `ps5::log` sur la console −
celle dans l'ELF), soit − `0x400000` jusqu'ici). Le `ucontext_t` de la console semble
différer de l'en-tête du SDK : le rip vient de l'adresse fautive pour SIGILL / SIGFPE /
SIGTRAP, et la pile est parcourue depuis le gestionnaire lui-même. Un `new` qui échoue
exécute `ud2` (pas d'exceptions) : signal 4, code 5, dans `operator new`.

**Pas de `glBlitFramebuffer`** avec ps5-gallium : il le fait au processeur
(`software-blit` dans le journal, ~470 ms par image en 4K, probe-5). La scène est posée à
l'écran et la zone à flouter réduite par un quad texturé (`kCopyFrag`), comme le canevas de
ps5-homebrew-ui. Règle pour la suite : toute copie d'image passe par un dessin.

**Voie B validée** (probe-6, 12 octobre 2026) : 60 images/s en 4K sur 13.60. Les
horodatages GPU de ps5-opengl comptent l'attente de l'image suivante (≈ 16,4 ms quelle que
soit la charge) : ils ne mesurent pas le coût réel du GPU sur la console.

### 13.3 HTTP sur la console

`platform/posix/socket_transport` : HTTP/1.1 sur sockets, une connexion par requête
(`Connection: close`), sans compression, délais par `poll`, annulation à la fermeture ;
le protocole (adresse, requête, réponse à longueur fixe, `chunked` ou jusqu'à la
fermeture) est dans `api/http_wire`, testé sans réseau. Utilisable sous Linux par
`REGIE5_HTTP=socket`. Choisi plutôt que libcurl + OpenSSL de pacbrew : environ 80
fonctions système de plus pour la console (dont `strftime`, `dlopen`, `getcontext`,
`getifaddrs`), chacune liée à NULL sans prévenir si la console ne l'exporte pas. Les
fonctions réseau utilisées sont vérifiées au démarrage ; sans `getaddrinfo`, seules les
adresses numériques sont tentées. `https://` répond « non pris en charge » (erreur
`Unsupported`, message dans Réglages) en attendant le TLS (6.2 bis).

| Fichier | Linux | PS5 |
|---|---|---|
| `config.json` | `~/.config/regie5/` | dossier du titre (`/app0` = `/data/homebrew/PPSA99520`) |
| `settings.json` | à côté de `config.json` | `/download0/regie5/` (données de l'app) |
| copie de `config.json` | — | `/download0/regie5/config.json`, utilisée si le dossier du titre n'en a plus |

### 13.5 Configurer depuis un téléphone, HTTPS

- `core/config_web` (portable, testé) : vue de `config.json` sans clés, application des
  modifications (clé vide = gardée, champs inconnus conservés), page HTML/JS sans ressource
  externe (`core/config_page.cpp`).
- `platform/posix/config_server.cpp` : serveur HTTP minimal sur un thread (port 8085 à
  8090), seulement le réseau local, code à 6 chiffres comparé en temps constant, écrit
  `config.json` à côté puis renomme ; `bind`, `listen`, `accept`, `getsockname` en imports
  faibles (bouton masqué s'ils manquent). Adresse de la console : un `connect` UDP vers un
  des serveurs puis `getsockname`.
- HTTPS : Mbed TLS dans `socket_transport.cpp` (canal `Tls` au-dessus du même socket non
  bloquant), générateur aléatoire unique sous verrou (la chaîne PS5 n'a pas
  `__cxa_thread_atexit_impl` : pas de `thread_local` avec destructeur), `mbedtls_ms_time`
  et l'aléa fournis par `tls_glue.cpp`. Sur PS5 l'aléa vient de `/dev/urandom` ou de
  RDRAND : `libSceRandom` aurait été un module requis de plus, non vérifié sur console.

### 13.4 L'heure de la console

Constat (journaux de probe-3 à probe-6, firmware 13.60, heure d'été de Paris) :
`clock_gettime(CLOCK_REALTIME)` donne l'UTC **plus une heure**, et
`sceKernelConvertUtcToLocaltime` un décalage de +1 h qui laisse l'heure d'été de côté
(rendue à part, `dst`). L'heure locale tombait juste, l'UTC non : tout horaire d'un serveur
s'affichait une heure trop tôt. `time_ps5.c` prend l'en-tête `Date` des serveurs comme
UTC : l'avance de l'horloge, au quart d'heure, est retirée de `time()` et ajoutée au
décalage, si bien que l'heure locale reste celle de la console. Au-delà de 14 h d'écart,
c'est une horloge mal réglée : rien n'est corrigé. Le reste de l'avance (la console n'a
pas de serveur de temps sans internet : ~1 min de dérive constatée) est retiré lui aussi,
à la seconde : l'heure affichée est celle des serveurs.

La voie A (backend AGC de Jelly5) reste le recours si la sonde échoue.
