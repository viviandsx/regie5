# Régie5 — Feuille de route

État d'avancement du projet, tenu à jour à la fin de chaque étape. Pour reprendre le
travail, ce fichier suffit ; le détail technique est dans `docs/ARCHITECTURE.md` (§8 à §11
pour ce qui a été fait après la phase 0).

**Dernière mise à jour : 13 octobre 2026 : phase 7 en cours : dépôt public, release `v0.2.0` publiée, PR #244 au catalogue en attente de relecture (§ 2 quinquies). Marche à suivre désormais : `docs/RELEASING.md`.**

---

## 1. Où on en est

| Phase | Contenu | État |
|---|---|---|
| 0. Reconnaissance | Étude de Jelly5, `docs/ARCHITECTURE.md` | ✅ fait (`3ccbaf0`) |
| 1. Squelette Linux | SDL2 + OpenGL 3.3, verre et goutte de focus de Jelly5, input clavier + manette, onglets, `scripts/run-linux.sh` | ✅ fait (`bbecb87`) |
| 2. Données | HTTP asynchrone, clients Sonarr / Radarr / Seerr / Jellyfin, parseurs testés, `--mock`, Réglages avec test réel | ✅ fait (`f9cc20e`) |
| 3. Écrans | Accueil, Téléchargements, Demandes, Jellyfin, actions avec confirmation | ✅ fait (`2d6bb36`) |
| 3 bis. Compléments | Renommage Helm5 → Régie5 (`ac95d6c`) ; détail des alertes santé, onglet Bibliothèque en lecture seule, Réglages en engrenage (`4bd6c95`) ; fixtures Radarr nettoyées de `releaseGroups`, historique réécrit (`0703a2c`) | ✅ fait |
| 4. Finition | Perfs, rechargement de la config, états vides / chargement / erreur, textes FR/EN | ✅ fait et commité |
| 4 bis. Fonctionnalités TV | C Guide, E Journal, A Diagnostic de lecture, B Grand ménage, D Ouvrir dans Jelly5 (dans cet ordre) | ✅ fait (C, E, A, B commités dans `1f2cdb9` ; `is4k` et D à commiter) |
| 4 ter. Multi-instances dans les écrans | Accueil, Bibliothèque, Activité › File d'attente sur toutes les instances Radarr/Sonarr | ✅ fait (`fd10118`) |
| 5. Recette V1 sous Linux | Checklist complète contre le vrai serveur | ✅ passe automatique faite et commitée (`e65bea1`). La **recette express** (15 points) est **reportée sur PS5**, en fin de phase 6 (décision de Vivian) |
| **6. Portage PS5** | Feu vert donné le 11 octobre 2026 | ✅ validée le 13 octobre 2026 (6.4 par le repli « Chercher dans Jelly5 ») |
| **7. Publication** | homebrew.page, version 0.2.0 | 🔶 en cours : dépôt public, release `v0.2.0` et PR #244 faits ; contrôles du catalogue verts, **relecture du mainteneur en attente** (§ 2 quinquies) |

Tests : **51** (`regie5_tests`, lancé par `ctest`), tous verts.

---

## 2. Phase 4 — points à traiter

Plan validé le 11 octobre 2026. Une étape = un commit.

- [x] **4.1** « Tout tester » (et △) dans Réglages relit `config.json`. Si le fichier est
      absent ou invalide, la configuration en service est gardée et Réglages affiche
      l'erreur. Si un service a changé, ses jaquettes sont oubliées et les autres onglets
      repartent de zéro sur la nouvelle configuration. Validée.
- [x] **4.2** Mesure détaillée par image (`REGIE5_FPS=1` : phases CPU, textes rastérisés,
      envois de textures, temps GPU et part du flou ; ligne « slow frame » au-delà de 25 ms).
      Voir `ARCHITECTURE.md` §12.1. Premier constat : les pics vus en passage automatique
      sont dans `present` (swap / compositeur), pas dans le dessin ni le GPU. Validée.
- [x] **4.3** Pic au passage d'une rangée à l'autre sur l'Accueil : mesuré par Vivian,
      **uniquement dans `present`** (vsync / compositeur du laptop), dessin et GPU < 0,5 ms.
      **Pas de correction sous Linux** (décision de Vivian) : sur PS5 l'affichage passe par
      un autre pilote, **à mesurer en phase 6**. La phase 4 se concentre sur ce qui vaut
      aussi sur PS5 (texte, envois de textures, flou).
- [x] **4.4** Texte : **atlas de glyphes** (`ARCHITECTURE.md` §12.3). Chaque glyphe est
      rastérisé une fois, et texte et aplats partagent les lots. En défilement rapide :
      appels de dessin divisés par 2 à 3, texte ≤ 0,6 ms par image. Rendu identique au
      pixel près. Validée.
- [x] **4.5** Flou en 4K (`REGIE5_RES=3840x2160`, rendu hors écran) : il coûtait jusqu'à
      15,8 ms par image (21 ms de GPU au total), parce que son coût grandissait comme le cube
      de l'échelle. Il est maintenant calculé dans la résolution logique : pire image en 4K
      ramenée à 3 à 5 ms, rendu inchangé (`ARCHITECTURE.md` §12.4).
- [x] **4.6** États vides / chargement / erreur / désactivé / sans config, revus sur
      150 captures (`scripts/capture-states.sh`, 8 états × 2 langues, nouveaux
      `REGIE5_MOCK_EMPTY` et `REGIE5_MOCK_OFF`). Corrigés :
      bandeau santé « Aucune alerte » alors que Sonarr/Radarr ne répondaient pas (→ « État
      inconnu » en rouge, avec la cause) ; rangée de l'Accueil « Rien de prévu » alors qu'un
      service avait échoué ; Manquants « Rien ne manque » en cachant l'échec de Radarr ;
      sans config.json, tous les écrans disaient « désactivé » (→ « Pas de configuration :
      voir Réglages ») ; goutte en pilule géante sur une rangée vide ou en chargement ;
      message d'erreur tronqué (boîte trop étroite) ; aucune ligne sous « Tâches
      planifiées » vides ; « Rien à afficher avec ce filtre » avec le filtre « Tous » (→
      « Aucun film dans Radarr. ») ; « Seerr : Désactivé… » incohérent ; « Sonarr : … »
      en anglais (→ « Sonarr: … ») ; chemin du vrai config.json affiché en mode démo.
- [x] **4.7** Textes FR / EN relus (245 chaînes, `scripts/list-strings.py`) et vérifiés
      sur les captures des deux langues. Corrigés : pluriels (« 1 séries », « 0 séries »,
      « 1 épisodes manquants », « 1 affichés ») avec `fmt::count` ; double deux-points
      (« Sonarr : Injoignable : vérifiez… » → « Sonarr : Injoignable — vérifiez… ») ;
      espace avant « : » en anglais. Dates (« Thu, Oct 15 » / « jeu. 15 oct. ») et tailles
      (« 5.6 TB » / « 5,6 To ») déjà correctes.

---

## 2 bis. Phase 4 bis — cinq fonctionnalités

Étude et décisions : `docs/FEATURES.md` (validée le 11 octobre 2026). Ordre : C, E, A, B, D.

- [x] **C. Guide des programmes** — Bibliothèque › Guide : une semaine en 7 colonnes
      (lundi → dimanche), épisodes Sonarr et sorties Radarr, état disponible / à venir /
      manquant, aujourd'hui en surbrillance, L2/R2 semaine précédente / suivante, ✕ ouvre la
      fiche. ✕ sur une carte « À venir » de l'Accueil ouvre le Guide à sa semaine
      (`Context::go`). Règles dans `core/schedule` (testées). Fixtures : calendriers sur trois
      semaines (`make-fixtures.py --only guide`), complétés d'épisodes synthétiques marqués.
- [x] **E. Journal du serveur** — l'onglet Téléchargements devient **Activité** (pilules
      File d'attente · Journal, ✕ pour changer, comme la Bibliothèque). Journal : 14 jours,
      groupés par jour, filtres Tout · Imports · Demandes · Jellyfin · Santé à droite de la
      même rangée. Imports Sonarr/Radarr (`history/since`, `downloadFolderImported`
      seulement, `sourceTitle` jamais lu), demandes Seerr (créée, approuvée / refusée par,
      disponible), activité Jellyfin (lectures, connexions, comptes ; phrases rédigées par
      l'app à partir du type, jamais `Name` / `Overview` ; pauses et reprises regroupées),
      alertes santé constatées ou disparues pendant la session. Widget `draw_pills`
      partagé. Tests : imports sans nom de release, journal Jellyfin sans texte.
- [x] **A. Diagnostic de lecture** — ✕ sur une session Jellyfin ouvre sa fiche : méthode,
      raisons, source / envoyé au client (vidéo, audio, sous-titres, conteneur, codecs lisibles),
      accélération matérielle, « le transcodage suit-il ? » (avance du transcodage sur la
      lecture, estimation dite comme telle, throttling pris en compte, baisse détectée entre
      deux lectures), conseils FR/EN pour les 28 raisons regroupées en 11 conseils,
      « Arrêter la lecture » (avec confirmation) depuis la fiche. Pastille « Ne suit pas » /
      « À la limite » sur la liste. **Glances** optionnel (`/api/4/quicklook`, HTTP Basic si
      mot de passe) : charge en haut de l'onglet Jellyfin et dans la fiche ; absent ou
      désactivé, la section disparaît. Logique dans `core/diagnosis` (testée), fixtures
      `make-fixtures.py --only diag` (deux transcodages synthétiques), `docs/GLANCES.md`.
- [x] **B. Grand ménage** — Bibliothèque › Ménage, **désactivé par défaut** : Réglages ›
      « Ménage : désactivé » → avertissement (Annuler par défaut) → activé ; choix gardé dans
      `settings.json` (géré par l'app, à côté de `config.json` ; rien d'écrit en `--mock`).
      Candidats : rapprochement Jellyfin ↔ Radarr/Sonarr par TMDB / TVDB / IMDb, titres
      inconnus de Jellyfin jamais proposés ; critères « Vu par : tous les comptes actifs /
      au moins un », « Non vu depuis 3 / 6 / 12 / 24 mois », jamais favori, séries terminées
      et entièrement vues ; comptes actifs = non désactivés, activité < 6 mois, hors
      `cleanup.ignoreUsers` (config.json) ; triés par taille, total récupérable. Actions (✕
      ou Options) : **Garder** (liste « Gardés », réversible) et **Supprimer…** : double
      confirmation (titre, taille, instance, « fichiers supprimés définitivement »,
      Annuler par défaut les deux fois), une suppression à la fois, Radarr/Sonarr `DELETE`
      avec `deleteFiles` et **exclusion d'import toujours**, puis Seerr : média marqué
      « supprimé » (`POST /api/v1/media/{id}/deleted`, statut 6 ; **retiré le 13 octobre 2026** :
      Seerr ignore ce statut, sa tâche « Media Availability Sync » s'en charge) pour pouvoir être redemandé
      sans perdre l'historique des demandes. Config **multi-instances** (objet ou liste
      nommée), Réglages une ligne par instance, Journal et Ménage sur toutes les instances.
      Logique dans `core/cleanup` (testée), fixtures `--only library,cleanup`.
      **Aucune suppression réelle faite** (seulement en `--mock`).
      Retour de Vivian : décisions 1, 2, 4, 5 validées ; marquage Seerr validé, mais la 4K
      ne se devine plus au nom : champ **`"is4k"`** par instance (faux par défaut,
      `docs/CONFIG.md`, `config.example.json`), qui choisit le côté 4K dans Seerr.
- [x] **D. Ouvrir dans Jelly5** — Options (M) › « Ouvrir dans Jelly5 » sur une fiche
      Bibliothèque (rapprochement TMDB/TVDB/IMDb par un index Jellyfin lu une fois), un
      ajout récent (un épisode, ou sa série si plusieurs sont regroupés) et une demande
      disponible (`jellyfinMediaId`, `…4k` pour une demande 4K). Arguments
      `--open-item/--server` + `open.json` (`core/jelly5`, testé) ; `platform::launch_app`
      simulé sous Linux (une ligne de journal), à écrire pour PS5 en phase 6. Le menu
      Options des Demandes porte aussi Approuver / Refuser. Une demande 4K suit maintenant
      l'état 4K du média (`status4k`), y compris dans le Journal.
- [x] **Fin de 4 bis** — consignes du projet (périmètre, écritures autorisées, Ménage, Glances,
      Activité, contrôles, vie privée du Journal), `docs/RECETTE.md` (Guide, Journal,
      diagnostic, Glances, Ménage, Jelly5, **suppression réelle par Vivian** en §7.6),
      `capture-states.sh` (Ménage, menus Options, diagnostic, état `multi` ;
      `REGIE5_MOCK_CLEANUP=1`).

## 2 ter. Étape 4 ter — multi-instances dans les écrans existants

- [x] Accueil : calendrier, santé et disques de toutes les instances ; alertes nommées par
      instance (« Radarr 4K »), aussi dans le Journal.
- [x] Bibliothèque : séries / films / manquants / guide de toutes les instances ; un titre
      présent dans deux instances apparaît deux fois, avec le nom de l'instance (cartes,
      Manquants, puce sur la fiche), comme dans le Ménage. Profils, fichiers d'épisodes et
      jaquettes lus sur la bonne instance.
- [x] Activité › File d'attente : files de toutes les instances, puce d'instance.
- [x] « À venir » et Guide : un épisode ou une sortie suivi par deux instances n'apparaît
      qu'une fois (la copie qui a son fichier), `schedule::drop_duplicates` (testé).
- [x] Échecs partiels : une instance qui ne répond pas est nommée dans une pastille rouge,
      les autres restent affichées (`merge_parts`, `ui/load.h`). Note « seule la première
      instance est affichée » retirée de Réglages.

---

## 2 quater. Phase 6 — portage PS5 (plan proposé, à valider)

Title ID choisis, libres dans `homebrew.page/catalog/v1.json` le 11 octobre 2026 :
**Régie5 = `PPSA99520`**, **sonde = `PPSA99521`** (`PPSA99505`–`99507` sont Jelly5 et ses
sondes, `PPSA99515` est pris). Installation comme Jelly5 : dossier
`/data/homebrew/<TITLEID>/`, tuile créée par ShadowMount+.

Chaîne : `ps5-payload-sdk` v0.44 (9 Mo) + pacbrew **v0.41.1** (228 Mo), qui contient pour
la première fois Mesa 26.2 (RADV + zink) et le SDL2 OpenGL/GLES. Publié le 10 octobre
2026, patches annoncés « non relus » par le mainteneur : c'est ce que la sonde vérifie.
LLVM 21 du laptop suffit (même méthode que `setup-toolchain.sh` de Jelly5-Seer).

- [x] **6.0 Chaîne** — `scripts/setup-ps5-toolchain.sh` (SDK v0.44 + pacbrew v0.41.1,
      empreintes officielles vérifiées), puis ps5-opengl 1.0.1 et la chaîne de
      ps5-homebrew-ui (commit fixé) par `scripts/build-ps5.sh`. Le code portable compile
      pour PS5 ; corrigé : plus de flux C++ (locale absente), `localtime_r`/`gmtime_r`.
      **Constat** (`ARCHITECTURE.md` §13) : le Mesa de pacbrew est en `.so`, chargeables
      seulement par un payload, pas par une app : la voie B passe par **ps5-opengl**
      (statique, OpenGL 4.6 Core, déjà utilisé par deux apps du catalogue), sans SDL.
- [x] **6.1 Sonde voie B** (`PPSA99521`, « Régie5 Probe ») — **validée le 12 octobre
      2026 avec probe-6** (firmware 13.60) : 60 images/s en 4K, plus aucun `software-blit`,
      mémoire stable (~20 Mio sur un tas de 256 Mio), manette, fermeture propre. Étapes :
      probe-2/3 dates refaites (libc de la console), probe-4/5 tas (512 Mio refusé, 256
      accordé) et piles des threads, probe-6 copie par quad au lieu de `glBlitFramebuffer`
      (fait au processeur par ps5-gallium). **Voie B retenue.**
      Constats de probe-6 : le « gpu avg ≈ 16,4 ms » inclut l'attente de l'image suivante
      (il ne bouge pas de 2 à 5 panneaux ni de 5 à 26 appels de dessin, tombe à ~9,5 ms
      quand l'app ne dessine que 3 images/s ; la même scène coûte 0,4 ms de GPU sous Linux) :
      ce n'est pas la charge réelle du GPU. Pics à 20-23 ms à l'ouverture d'un écran
      (100+ glyphes rastérisés) : glyphes courants **préchauffés** au démarrage (8 styles),
      budget de l'atlas porté à 48 Mio (24 Mio ne tenaient qu'une page 4K : une deuxième
      faisait tout recommencer), remises à zéro de l'atlas journalisées. Lignes `[ps5-…]`
      du pilote (compilées dans le SDK, pas de réglage) rangées par `ps5-log.sh` dans un
      fichier à part.
- [x] **6.2 App complète sur PS5** (`PPSA99520`, « Régie5 ») — **validée le 12 octobre
      2026** (app-3) avec les vrais serveurs : Réglages au vert, jaquettes, 60 images/s,
      tous les onglets. `config.json` dans `/data/homebrew/PPSA99520/` (= `/app0`),
      `settings.json` dans `/download0/regie5/`. HTTP : client HTTP/1.1 sur sockets
      (`platform/posix/socket_transport`, protocole testé dans `api/http_wire`) plutôt que
      libcurl + OpenSSL (≈ 80 fonctions système de plus). `http://` seulement.
- [x] **6.2 bis HTTPS sur PS5** — **validé le 13 octobre 2026** (app-7c) : faux Glances en
      HTTPS auto-signé passé « Connecté », les 4 autres services restés verts. Décision de Vivian : mbedTLS.
      Mbed TLS 3.6.7 (LTS, Apache-2.0, `scripts/fetch-mbedtls.sh`, empreinte figée) compilé
      dans l'app avec `src/platform/posix/mbedtls_config.h` (client TLS 1.2, ECDHE + AES-GCM /
      ChaCha20-Poly1305 / AES-CBC, certificats RSA et ECDSA) ; autorités de Mozilla
      (`cacert-2026-09-25.pem`, curl.se, 121 autorités) dans `assets/certs/` ; champ
      **`"insecure": true`** par service pour un certificat auto-signé (aussi pour libcurl
      sous Linux) ; erreur « Certificat refusé » dans Réglages. Aléa : `/dev/urandom`, sinon
      l'instruction RDRAND du processeur (aucun import système de plus). Vérifié sous Linux
      contre des serveurs HTTPS locaux (signé par une autorité : OK ; auto-signé : refusé,
      puis OK avec `insecure`). Sur console : `scripts/https-test-server.py` (faux Glances
      en HTTPS auto-signé).
      Reste à comprendre (non bloquant) : cette requête a pris **34,7 s** sur la console (45 ms sous Linux,
      dont ~0 de calcul : l'attente venait de Nagle). app-8 : `TCP_NODELAY`, et le journal
      détaille connexion / poignée TLS (« connected …, TLS … ») pour le prochain service HTTPS.
      app-8 n'a rien mesuré (serveur de test arrêté, puis Glances désactivé).
- [x] **6.3 Vraies données** — **validée le 13 octobre 2026** : heure locale et horaires des
      serveurs justes, redémarrage, copie du zip par-dessus (app-6) ; dérive de l'horloge
      (+58 s mesurées) recalée à la seconde sur l'en-tête `Date` des serveurs (app-7c) : la
      minute change en même temps que sur le téléphone.
- [x] **6.4 Ouvrir dans Jelly5** — **close le 13 octobre 2026 avec le repli** : app-8 a
      obtenu la poignée du module, mais `sceKernelDlsym` répond `0x80020003` (introuvable)
      pour les deux fonctions : **pas de lancement d'app depuis une app** sur 13.60. Options
      › « Chercher dans Jelly5 » validé sur console (note « Ouvrez Jelly5 et cherchez 1917 »).
      Décision de Vivian : on garde ce repli (`docs/JELLY5-PR.md`). Historique : app-7c : **aucune** des deux fonctions liée, entrée
      masquée, donc menu Options vide et indication « Options » absente (comportement
      voulu : l'indication ne s'affiche que si le menu a une entrée, sur tous les écrans).
      **Cause trouvée** : les deux fonctions sont dans la bibliothèque de liaison du SDK,
      mais le convertisseur de ps5-homebrew-ui range chaque import sous la bibliothèque
      « libSceSystemService » ; la console les publie sous une autre bibliothèque du même
      module, la recherche échoue et la référence faible vaut 0. Les homebrews qui lancent
      un titre (ps5-homebrew-dev-protocol, shsrv/hbldr, ShadowMount+) sont des payloads qui
      résolvent **par nom**. **app-8** fait pareil depuis l'app : `sceKernelLoadStartModule`
      (poignée du module déjà chargé) puis `sceKernelDlsym` pour
      `sceSystemServiceLaunchApp` et `sceLncUtilLaunchApp`, tout journalisé. Si la console
      refuse à une app (code dans `launch: … -> 0x…`), repli **« Chercher dans Jelly5 »** :
      note « Ouvrez Jelly5 et cherchez « titre » » (8 s), aussi après un refus définitif
      (`0x80940005`). `REGIE5_NO_LAUNCH=1` le montre sous Linux. Le fichier de passage
      (`/data/jelly5/open.json`) reste hors de portée d'une app (pas de `/data`) : le
      passage par argument reste la voie si le lancement marche.
- [x] **6.5 Configuration sans FTP** — **validée le 13 octobre 2026** (QR scanné, page ouverte,
      code accepté, enregistrement OK). Décision de Vivian : QR code
      (le test de port d'app-6 a réussi, dossier du titre inscriptible). Réglages › « Depuis
      le téléphone » : QR code (Nayuki, MIT) vers `http://<console>:8085/?c=<code>`, page
      locale (`core/config_page.cpp`) servie tant que la fenêtre est ouverte, réseau local
      seulement, code à 6 chiffres (10 essais faux : arrêt) ; services en listes
      d'instances, clés jamais renvoyées au téléphone (champ vide = clé gardée), le reste de
      `config.json` conservé ; à l'enregistrement, Régie5 relit et teste. Logique testée
      (`tests/test_share.cpp`, serveur réel sur la boucle locale), page essayée dans un
      navigateur au format téléphone.
- [x] **6.6 Paquet** — fait (zip final : § 2 quinquies) : `scripts/release-ps5.sh` → `build/ps5/release-<v>/
      Regie5-<v>.zip` (dossier `PPSA99520` + `config.example.json`, `INSTALL.txt` FR/EN,
      `LICENSE`, notices, licences de ps5-opengl/Mesa), version tirée de `param.json`
      (0.1.0), sans journal UDP ni mesures par image, empreinte SHA-256. Icône dessinée à
      chaque build (`scripts/make-icons.py`). **Règle (demande de Vivian, pour la
      communauté)** : le zip ne contient **jamais** de `config.json` (le script refuse de
      le produire), et une mise à jour par copie du dossier par-dessus garde le
      `config.json` de l'utilisateur (plus la copie de secours de l'app). `release-0.1.1`
      (logo B, version passée à 0.1.1 dans `param.json`) sert au test d'installation, pas à
      une publication.
      **Logo** : B (« R5 » en verre), choisi et **validé sur la tuile** le 13 octobre 2026. `scripts/make-icons.py` dessine
      `icon0.png`, `pic0.dds` (tuile sélectionnée, « R5 » à droite) et `pic1.dds` (lancement,
      « R5 » et « Régie5 » au centre) à chaque build ; les DDS (3840 × 2160, BC7_UNORM, en-tête
      DX10, sans mipmaps) sortent de `scripts/bc7.py` (encodeur BC7 mode 6, texconv étant
      Windows seulement), relus par le décodeur de Pillow (PSNR 54 dB) ; `validate-assets.sh`
      de ps5-homebrew-ui passe dans `build-ps5.sh`, donc aussi pour le zip de release.
- [x] **6.7 Recette sur PS5** — **validée le 13 octobre 2026** (recette réduite, app-8,
      `docs/RECETTE-EXPRESS.md`). Parties B, C, D non refaites : déjà vues dans les tests
      précédents (tuile, horloge, navigation, 60 images/s, Bibliothèque ; scan réel en
      phase 4). Faites en vrai : **arrêt d'une lecture** (`POST …/Playing/Stop` → 204, arrêtée
      sur le téléphone), **refus** d'une demande de l'utilisateur de test (`…/decline` → 200),
      **Ménage** activé puis **suppression réelle de la série « Hannibal »** (chemin Sonarr :
      un seul `DELETE /api/v3/series/43` → 200). Approbation non testée volontairement
      (même mécanisme que le refus, pas de téléchargement voulu). Vérifié en GET : Hannibal
      absent de Sonarr, présent dans ses exclusions d'import (tvdb 259063), absent de
      Jellyfin. Images : 60/s en régime normal, `cpu worst` 14,8 ms, 4 « slow frame » (dont
      la première image et un pic isolé de 119 ms).
      **Défaut trouvé et corrigé** : le `POST /api/v1/media/{id}/deleted` envoyé à Seerr
      répondait 200 sans rien changer (Seerr 3.5.0 ne connaît pas ce statut sur cette
      route : `available`, `partial`, `processing`, `pending`, `unknown` seulement), et la
      note disait « Seerr à jour » à tort. Seerr marque lui-même le titre `DELETED`
      (demandes gardées, redemandable) dans sa tâche « Media Availability Sync » (05:00
      chez Vivian) : Régie5 n'écrit plus rien dans Seerr, la note dit « Seerr le saura à sa
      prochaine synchronisation » (une écriture de moins ; `FEATURES.md` le prévoyait déjà).
      **À vérifier après le 14 octobre 05:00** (GET) : Hannibal en statut 7 (`DELETED`) dans
      Seerr, sa demande gardée.

## 2 quinquies. Publication sur homebrew.page (13 octobre 2026)

Règles lues : `docs/submitting.md`, `docs/review-policy.md`, `artifact-formats.md`,
`metadata.md`, `versioning.md` du dépôt `blackbearreloaded/ps5-homebrew-catalog`.

- [x] **Version 0.2.0** : `contentVersion` `00.002.000` (0.1.0 et 0.1.1 ont servi aux zips de
      test et ne se réutilisent pas), `project(… VERSION 0.2.0)`, tag **`v0.2.0`**.
- [x] **Zip** `build/ps5/release-0.2.0/upload/PPSA99520.zip` : un seul dossier `PPSA99520/` en
      tête (règle du catalogue ; `INSTALL.txt`, `LICENSE`, notices et licences dedans), pas
      de `config.json`, pas de journal UDP, visuels validés. SHA-256
      `e06ed92efc69626ba02372d96744307c34a1c8b677893adf2b7a5d6482151b7f`.
      `release-ps5.sh --repack` refait le zip sans reconstruire ; le zip est rangé dans
      `upload/` (l'outil de build laisse un `PPSA99520.zip` à lui à côté du dossier).
- [x] **Icône versionnée** `ps5/app/sce_sys/icon0.png` (à côté de `param.json` : le catalogue
      la lie au tag et y lit `contentVersion`) ; le build ne dessine plus que `pic0`/`pic1`.
- [x] **README.md** (anglais, 10 captures du mode démo dans `docs/images/`, sans affiche de
      film), **INSTALL.txt** réécrit (téléphone, HTTPS, Jelly5), `config.example.json`
      (PS5, `insecure`), **modèles d'issues** (`.github/ISSUE_TEMPLATE/`).
- [x] **Fiche du catalogue** `build/publish/PPSA99520.json` (non versionnée, vérifiée contre
      `metadata.md`) et **notes de version** `build/publish/release-notes-v0.2.0.md`. Title ID
      et nom libres dans le catalogue (64 apps) le 13 octobre 2026.
- [x] **Audit** : aucune clé (comparées à `config.json`, arbre, historique et zip), aucune
      adresse du réseau de Vivian, fixtures anonymisées, `CLAUDE.md`, `.claude/`, `_ref/`
      exclus. Mentions retirées de la doc. **L'historique** garde 4 mentions dans d'anciens
      diffs : publication conseillée en **historique neuf** (un seul commit).
- [x] **Vivian, 13 octobre 2026** : dépôt public https://github.com/viviandsx/regie5, poussé en
      **historique neuf** (branche locale `public` → `main` du dépôt, un commit « Régie5 0.2.0 ») ;
      tag `v0.2.0` et release GitHub « Régie5 0.2.0 » avec `PPSA99520.zip` ; PR #244 sur
      `blackbearreloaded/ps5-homebrew-catalog` (fichier `apps/PPSA99520.json`, branche
      `viviandsx-patch-1` du fork de Vivian). Contrôles « Scan the release » et « Validate
      submission » verts.
- [ ] **Relecture de la PR #244** par le mainteneur, puis fusion : Régie5 apparaît sur
      homebrew.page. D'ici là, rien ne bouge dans la release `v0.2.0` (`docs/RELEASING.md` §2).
- [ ] Règle ufw du port 5555 : **gardée ouverte** par Vivian pour continuer les tests ; à
      fermer quand ils seront finis.
- [ ] Branches locales : la `main` locale porte l'**ancien historique** (avec les mentions
      retirées) et ne doit **jamais** être poussée ; renommage proposé dans `docs/RELEASING.md` §1.

## 3. En attente

| Sujet | Qui | Détail |
|---|---|---|
| PR #244 au catalogue | mainteneur | Contrôles verts, relecture en attente. Répondre aux demandes en modifiant seulement `apps/PPSA99520.json` sur `viviandsx-patch-1` (`docs/RELEASING.md` §2). |
| Port 5555 (ufw) | Vivian | Ouvert pour les tests, à refermer à la fin. |
| Hannibal dans Seerr | après le 14 oct. 05:00 | Statut 7 (`DELETED`) attendu après « Media Availability Sync » ; vérifier en GET (`/api/v1/tv/40008`). |
| PR Jelly5 | Vivian | Sans lanceur depuis une app, Régie5 ne s'en sert pas sur PS5 : la proposer ou non est une décision à part (`docs/JELLY5-PR.md`). |
| Taille du tas sur PS5 | 6.2 | 256 Mio accordé et l'affichage démarre (probe-5) : on garde 256 ; l'usage mesuré est ~20 Mio. En 6.2 : message visible à l'écran (ou notification système) si la mémoire est refusée, au lieu du seul journal. |

### Actions réelles

Aucune action réelle sans accord explicite de Vivian.

1. [x] **Scanner les bibliothèques** (Jellyfin) — fait une fois le 11 octobre 2026 depuis
   Régie5 : `POST /Library/Refresh` → 204, la tâche « Analyser la médiathèque » est passée
   en cours (0 %, puis 65 %), et la note « Scan des bibliothèques lancé » s'est affichée.
2. [x] **Arrêter une lecture** — 13 octobre 2026, sur console (recette 6.7).
3. [x] **Refuser** (Seerr) — 13 octobre 2026, utilisateur de test. **Approuver** : non testé
   volontairement (même appel, autre chemin).
4. [x] **Supprimer** (Ménage, Sonarr) — 13 octobre 2026, série « Hannibal ». Radarr : non
   testé en vrai (même code, autre chemin).

---

## 3 bis. Pour la communauté

Régie5 est fait pour la communauté (PS5 jailbreakée + serveur auto-hébergé), pas pour une
seule installation. Ces règles s'appliquent à tous les choix. Chaque point est intégré quand
il touche ce qui est codé, sinon il est planifié ici.

| Point | Où on en est | Quand |
|---|---|---|
| HTTPS | Fonctionne sous Linux (libcurl, `http` et `https`). Il manque une option pour les certificats auto-signés (`"insecure": true` par service). Sur PS5 : TLS à porter | Option : étape « adresses » après la 4 bis ; PS5 : phase 6 |
| Sous-chemins de reverse proxy, `urlBase` des *arr | L'`url` de config peut contenir un chemin (`https://domaine/sonarr`) : toutes les URL passent par `Services::url`. Pas encore testé avec un vrai reverse proxy | Fixture + test « url avec chemin » dans l'étape « adresses » |
| Versions plus anciennes | La version est lue (test de connexion) mais aucune version minimale n'est vérifiée | Étape « versions » : minimums par service, message clair dans Réglages, liste publiée dans le README |
| Plusieurs instances (Radarr 1080p + 4K…) | Config : objet **ou** liste nommée, `is4k` par instance ; tous les écrans parcourent toutes les instances | Fait (4 ter) |
| Fonctions destructives désactivées par défaut | — | 4 bis (B) : Ménage activé seulement dans Réglages, après un avertissement |
| Configuration sans FTP | Étudiée dans `FEATURES.md` (annexe) : clavier système, QR + page locale, Quick Connect Jellyfin | Décision en phase 6 |
| Ajouter une langue | Aujourd'hui : FR et EN dans `core/i18n.cpp`, une colonne par langue | Après la 4 bis : une langue = un fichier de table, plus `docs/TRANSLATING.md` |
| Publication | README (captures du mode démo), INSTALL, modèles d'issues : faits pour 0.2.0. Liste des versions prises en charge : avec l'étape « versions » | 0.2.0 publiée ; affiches libres de droits pour les captures : à décider |

---

## 4. Décisions prises

### Produit et périmètre
- Lecture seule partout, sauf : approuver / refuser une demande Seerr, arrêter une lecture
  Jellyfin, scanner les bibliothèques Jellyfin, supprimer depuis le Ménage (double
  confirmation, désactivé par défaut). Chaque action passe par une confirmation, le focus
  part sur « Annuler ».
- Une instance 4K se déclare (`"is4k": true`) ; aucun comportement ne se déduit d'un nom.
- Jamais de nom de release, d'indexeur, de client de téléchargement ni de chemin : les
  parseurs ne lisent pas ces champs, le modèle n'a pas de place pour eux, deux tests le
  vérifient (`queue_never_keeps_release_names`, `fixtures_hold_no_forbidden_field`).
- Alertes santé : la phrase est rédigée par l'app selon le type d'alerte ; le texte du
  serveur n'est repris que s'il est jugé sûr (`arr::shareable`).
- Barre d'onglets : Accueil · Bibliothèque · Activité (ex-Téléchargements) · Demandes · Jellyfin, puis
  Réglages en engrenage avant l'horloge. L1/R1 parcourent les six.
- Bibliothèque : ←/→ déplacent le focus sur les pilules, **✕ change de sous-vue**.
  Filtre « Terminées » pour les séries, **« Téléchargés »** (fichier présent) pour les films.
  L2/R2 : lettre précédente / suivante en tri par titre, sinon page de deux rangées.

### Données
- HTTP simple sur le réseau local, aucune requête vers internet. Jaquettes Seerr par
  `/imageproxy/tmdb/…` (servi par Seerr) ; en cas d'échec, jaquette générée, jamais d'image cassée.
- Jellyfin 12 : uniquement `Authorization: MediaBrowser Token="…"`. Ajouts récents par
  `/Items?SortBy=DateCreated` (`/Items/Latest` exige un `userId`).
- Test de connexion Seerr sur `/api/v1/settings/about` (`/api/v1/status` est public).
- File d'attente : `estimatedCompletionTime` tant qu'il est dans le futur, sinon `timeleft`.
- En `--mock`, les actions sont journalisées et ne font rien.

### Interface
- **Options (M) = menu contextuel** de l'élément sélectionné (décision de Vivian) : il porte
  les nouvelles actions (Ouvrir dans Jelly5, Garder / Supprimer du Ménage…).
- Le bandeau santé ne dit « Aucune alerte » que si **tous** les services surveillés ont
  répondu ; sinon « État inconnu » en rouge, avec la cause.
- Une liste vide alors qu'un service a échoué affiche l'erreur, jamais « rien à
  afficher ». Si une partie a répondu, l'échec est signalé par une pastille rouge.
- Sans config.json lisible, les écrans disent « Pas de configuration : voir Réglages »
  plutôt que « désactivé ».
- En mode démo, Réglages n'affiche pas le chemin du fichier de configuration (il n'est pas
  lu, et apparaîtrait sur les captures publiques).
- « Service : message » passe par `fmt::labelled` (« Sonarr : … » en français,
  « Sonarr: … » en anglais).

### Outils
- `scripts/capture-states.sh` est la référence pour revoir les états : 8 états × 2 langues,
  uniquement en `--mock` (aucune action possible sur un serveur).
- Le tableau des chaînes n'est pas versionné : il se génère (`scripts/list-strings.py`),
  pour ne jamais être en retard sur le code.

### Technique
- Les pics qui ne viennent que de `present` (vsync / compositeur du laptop) ne se corrigent
  pas sous Linux : on mesure l'affichage sur PS5 en phase 6.
- Flou des panneaux de verre calculé sur une grille de demi-pixels logiques : son coût ne
  dépend plus de la résolution de sortie (4K = même aspect que 1080p).
- Texte par atlas de glyphes ; trois primitives de backend en plus (`texture_update`,
  `image_tint_uv`, `set_solid_source`), à fournir aussi par un éventuel backend AGC.
- C++17, CMake + Ninja, clang. Licence **GPL-3.0-or-later** (code repris de Jelly5),
  `THIRD_PARTY_NOTICES.md` à tenir à jour.
- `ui/gfx.h` garde les signatures de Jelly5 (voie A de la phase 6 ouverte) ; le backend
  OpenGL n'utilise que le sous-ensemble GL 3.3 core / GLES 3.0 (voie B ouverte).
- Phase 6 : test de faisabilité SDL2 + Mesa sur PS5 d'abord, puis choix voie A ou B.
  Config PS5 envisagée dans `/data/regie5/config.json` (à vérifier, risque R2).

---

## 5. Commandes utiles

```bash
scripts/run-linux.sh --mock                # sans serveur, sur fixtures/
scripts/run-linux.sh                       # avec ~/.config/regie5/config.json
scripts/run-linux.sh --lang en --mock      # en anglais
cmake --build build/linux && ctest --test-dir build/linux --output-on-failure
```

Variables de développement : `REGIE5_FPS=1` (temps d'image chaque seconde),
`REGIE5_SCRIPT="…"` (rejoue des touches, captures PPM), `REGIE5_HTTP_LOG=1`,
`REGIE5_MOCK_DELAY_MS`, `REGIE5_MOCK_FAIL=radarr:unreachable,…`,
`REGIE5_WORKER_STACK_KIB=64` (pile des threads de travail, comme sur PS5),
`REGIE5_HTTP=socket` (le client HTTP de la PS5 au lieu de libcurl, avec Mbed TLS pour `https://`),
`REGIE5_NO_LAUNCH=1` (une console qui ne lance pas d'app : « Chercher dans Jelly5 »).
`scripts/make-fixtures.py` régénère les fixtures en GET seulement (`--only library` pour
la Bibliothèque).

---

## 6. Journal des étapes

| Date | Étape |
|---|---|
| 11 oct. 2026 | Création de cette feuille de route ; ouverture de la phase 4. |
| 11 oct. 2026 | Plan de la phase 4 validé. Étape 4.1 : relecture de `config.json` depuis Réglages (32 tests). |
| 11 oct. 2026 | Scan Jellyfin réel vérifié (une fois). Étape 4.2 : mesure par image (`core/perf`, horodatages GPU). |
| 11 oct. 2026 | 4.3 : pics uniquement dans `present`, reportés en phase 6. 4.4 : atlas de glyphes, compteur d'appels de dessin dans `REGIE5_FPS`. |
| 11 oct. 2026 | 4.5 : `REGIE5_RES` ; flou dans la résolution logique (pire GPU en 4K : 21 → 5 ms). |
| 11 oct. 2026 | 4.6 : `capture-states.sh`, `REGIE5_MOCK_EMPTY` / `REGIE5_MOCK_OFF`, 11 défauts d'états corrigés (33 tests). |
| 11 oct. 2026 | 4.7 : pluriels, ponctuation FR/EN, `list-strings.py`. Phase 4 terminée ; `docs/RECETTE.md` rédigé pour la phase 5. |
| 11 oct. 2026 | Phase 4 commitée. Nouvelle phase 4 bis (cinq fonctionnalités) : étude de faisabilité `docs/FEATURES.md` (endpoints vérifiés en GET). |
| 11 oct. 2026 | Objectif « communauté » : section 3 bis ; `FEATURES.md` complété (Ménage désactivé par défaut, plusieurs instances, étude configuration sans FTP). |
| 11 oct. 2026 | 4 bis : C, E, A, B commités (`1f2cdb9`, 41 tests). Puis `is4k`, D (Ouvrir dans Jelly5), fin de phase (consignes du projet, `RECETTE.md`, captures) : 43 tests. |
| 11 oct. 2026 | 4 ter : Accueil, Bibliothèque, Guide, File d'attente sur toutes les instances (44 tests). |
| 11 oct. 2026 | Phase 5 : passe automatique contre les vrais serveurs (GET seulement), 2 corrections (tailles FreeType en cache, état « En salles » du Guide) ; recette express rédigée. |
| 11 oct. 2026 | Phase 5 commitée. Recette express reportée en fin de phase 6, sur PS5. Phase 6 ouverte : plan et sonde voie B proposés, Title ID `PPSA99520` / `PPSA99521`. |
| 11 oct. 2026 | 6.0 : chaîne installée ; le Mesa de pacbrew ne se charge pas dans une app → voie B par ps5-opengl (statique). 6.1 : sonde construite (`build/ps5/probe-2`), backend `platform/ps5`, rendu partagé `platform/gl`, mode secours et fermeture autonome. |
| 12 oct. 2026 | Sonde probe-2 sur PS5 (13.60) : plantage juste après la 1ʳᵉ ligne du journal, cause masquée par le relais toutes les 200 ms. probe-3 : journal UDP immédiat, étape par étape, rapport de plantage (signal, adresse, pile), dates sans la libc de la console, tas revenu à 128 Mio (valeur validée), mode secours désactivé par défaut (fichier `safe-mode` pour l'activer). |
| 12 oct. 2026 | probe-3 : dates, EGL et OpenGL 4.6 en 4K OK sur 13.60 ; arrêt silencieux après « step: screens ». probe-4 : tampon de 64 Kio retiré de la pile des threads HTTP (suspect principal), tas journalisé par étape et par écran, rapports d'échec d'allocation / `abort` / `exit` / `_Exit` / libc++, tas 512 Mio, budgets console, Réglages ne teste qu'à l'ouverture. |
| 12 oct. 2026 | probe-4 sur 13.60 : tas de 512 Mio refusé, l'app continue sur le malloc du système et meurt dans « step: fonts » (`new` qui échoue → `ud2`, signal 4 à `operator new`). probe-5 : tas 256 → 192 → 128 Mio, arrêt clair si aucun n'est accordé, échecs d'allocation signalés aussi sur le malloc du système, budgets selon le tas, threads de travail à 512 Kio de pile (`REGIE5_WORKER_STACK_KIB` sous Linux : le tour passe avec 16 Kio), rapport de plantage indépendant de la structure du contexte. |
| 12 oct. 2026 | probe-5 tourne sur la PS5 (13.60) : tour complet, fermeture propre, 256 Mio de tas, ~20 Mio utilisés. ≈ 3 images/s : `glBlitFramebuffer` scène → écran fait au processeur par ps5-gallium (~470 ms en 4K). probe-6 : plus aucun blit, copie par quad texturé (shader `kCopyFrag`), aussi pour la réduction du flou ; option `output-1080p` (fichier dans le dossier du titre). |
| 12 oct. 2026 | probe-6 sur PS5 : 60 images/s en 4K, plus de `software-blit` : **voie B validée**, 6.1 terminée. « gpu avg » ≈ 16,4 ms = attente de l'image suivante, pas la charge. Glyphes préchauffés, atlas 48 Mio, lignes du pilote filtrées par `ps5-log.sh`. 6.2 : client HTTP/1.1 sur sockets (`http_wire` testé, 46 tests), app `PPSA99520` (`ps5/app/sce_sys`, `build-ps5.sh <nom> app`), `config.json` dans le dossier du titre, `settings.json` dans `/download0/regie5/`, erreur « HTTPS pas encore » dans Réglages. `build/ps5/app-3` à tester (`docs/PS5-APP.md`). |
| 12 oct. 2026 | 6.2 validée sur PS5 (app-3). 6.3 : heure de la console recalée sur l'en-tête `Date` des serveurs (R9 : une heure d'écart trouvée dans les journaux), compteur de lancements (R2), copie de secours de `config.json`. 6.4 : lancement de Jelly5 (`sceSystemServiceLaunchApp`). 6.5 : mesures (port en écoute, dossier inscriptible). 6.6 : `release-ps5.sh`, zip 0.1.0 sans `config.json`, `INSTALL.txt`, icône. 47 tests. |
| 12 oct. 2026 | Retour d'app-6 : heure OK, icône OK, redémarrage OK, zip par-dessus OK ; horloge ~1 min en avance, « Ouvrir dans Jelly5 » absent. app-7c : dérive de l'horloge recalée à la seconde, deux fonctions de lancement essayées ; 6.5 (QR code + page locale) ; HTTPS par Mbed TLS (`insecure` par service) ; 3 propositions de logo (`scripts/make-logos.py`). 51 tests. |
| 13 oct. 2026 | Retour d'app-7c : horloge, 6.5 et HTTPS validés ; Jelly5 : aucune fonction liée (cause : nom de bibliothèque des imports). app-8 : lancement par `sceKernelDlsym`, repli « Chercher dans Jelly5 », `TCP_NODELAY` et temps TLS dans le journal ; logo B (icône, `pic0`/`pic1` en BC7 par `scripts/bc7.py`) ; recette express réécrite pour la console. |
| 13 oct. 2026 | app-8 validée : logo B sur la tuile (icône, `pic0`, `pic1`), Glances désactivé depuis le téléphone, « Chercher dans Jelly5 » ; `sceKernelDlsym` → `0x80020003` pour les deux fonctions : pas de lancement depuis une app, repli gardé (6.4 close). Recette express prête. |
| 13 oct. 2026 | Recette express (réduite) validée sur console : arrêt de lecture, refus, suppression réelle de « Hannibal » (Sonarr) ; POST Seerr `…/deleted` inopérant retiré. **Phase 6 validée.** Publication préparée. |
| 13 oct. 2026 | Publication préparée : version 0.2.0, zip au format du catalogue, README, INSTALL, modèles d'issues, fiche `PPSA99520.json`, audit du dépôt. |
| 13 oct. 2026 | **Phase 7** : dépôt public (historique neuf), release `v0.2.0` avec `PPSA99520.zip`, PR #244 au catalogue (contrôles verts, relecture en attente). `docs/RELEASING.md` : branches, ce qui est gelé, sortie d'une version. |
