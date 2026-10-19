# Régie5 — Feuille de route

État d'avancement du projet, tenu à jour à la fin de chaque étape. Pour reprendre le
travail, ce fichier suffit ; le détail technique est dans `docs/ARCHITECTURE.md` (§8 à §11
pour ce qui a été fait après la phase 0).

**Dernière mise à jour : 19 octobre 2026. **0.7.0 publiée**, fiche homebrew.page en 0.7.0 avec la nouvelle description (PR #432 du catalogue, fusionnée).**

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
| **7. Publication** | homebrew.page, version 0.2.0 | ✅ fait le 13 octobre 2026 : dépôt public, release `v0.2.0`, PR #244 fusionnée, Régie5 listé sur homebrew.page (§ 2 quinquies) |
| 8. Version 0.3.0 | reverse proxy, pastilles, signalements Seerr, bibliothèques et utilisateurs Jellyfin, 120 Hz | ✅ publiée le 13 octobre 2026 (§ 2 sexies) ; page du téléphone cassée |
| 9. Version 0.3.1 | correctif : page du téléphone (rangée Jeux), 120 Hz expérimental | ✅ publiée le 14 octobre 2026 (§ 2 septies) |
| 10. Version 0.3.2 | 120 Hz par défaut (Auto), dates des films ; Multimédia écarté | ✅ publiée le 14 octobre 2026 (§ 2 septies, « 0.3.2 ») |
| 11. Version 0.4.0 : Emby + assistant | Emby demandé dans la première issue ; assistant de première configuration | ✅ publiée (Emby en beta) |
| 12. Version 0.5.0 | Nouveautés, espagnol et allemand, demandes Seerr, Quick Connect, communauté | ✅ publiée (§ 2 decies) |
| 13. Correctif 0.5.1 | Plantage du testeur Emby : images Emby, rapport de plantage, garde-fou de démarrage | ✅ publiée le 17 octobre 2026 ; catalogue en attente de la PR automatique (§ 2 undecies) |
| 14. Version 0.6.0 : Plex | Plex Media Server, troisième serveur multimédia (beta) | ✅ publiée ; catalogue en attente (§ 2 duodecies) |
| 15. Audit d'optimisation | Performances PS5, mémoire, réseau, robustesse, ergonomie, code | validé par Vivian le 18 octobre 2026 (§ 2 terdecies) |
| 16. Version 0.7.0 : optimisation | Les points de l'audit, en 3 lots (§ 2 quattuordecies) | ✅ publiée ; catalogue en 0.7.0 (19 octobre 2026) |

Tests : **99** (`regie5_tests` : 92, `regie5_ui_tests` : 7, lancés par `ctest`), tous verts.

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
      › « Chercher dans Jelly5 » validé sur console (note « Ouvrez Jelly5 et cherchez « titre » »).
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
      **Ménage** activé puis **suppression réelle d'une série terminée** (chemin Sonarr :
      un seul `DELETE /api/v3/series/43` → 200). Approbation non testée volontairement
      (même mécanisme que le refus, pas de téléchargement voulu). Vérifié en GET : la série
      absente de Sonarr, présente dans ses exclusions d'import, absente de
      Jellyfin. Images : 60/s en régime normal, `cpu worst` 14,8 ms, 4 « slow frame » (dont
      la première image et un pic isolé de 119 ms).
      **Défaut trouvé et corrigé** : le `POST /api/v1/media/{id}/deleted` envoyé à Seerr
      répondait 200 sans rien changer (Seerr 3.5.0 ne connaît pas ce statut sur cette
      route : `available`, `partial`, `processing`, `pending`, `unknown` seulement), et la
      note disait « Seerr à jour » à tort. Seerr marque lui-même le titre `DELETED`
      (demandes gardées, redemandable) dans sa tâche « Media Availability Sync » (05:00
      chez Vivian) : Régie5 n'écrit plus rien dans Seerr, la note dit « Seerr le saura à sa
      prochaine synchronisation » (une écriture de moins ; `FEATURES.md` le prévoyait déjà).
      **Vérifié le 14 octobre 2026** (GET) : après la synchronisation de 05:00, la série est en
      statut 7 (`DELETED`) dans Seerr, toutes ses saisons aussi, et sa demande est gardée.
      Ménage validé de bout en bout.

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
      adresse du réseau de Vivian, fixtures anonymisées, les fichiers de consignes locaux, `_ref/`
      exclus. Mentions retirées de la doc. **L'historique** garde 4 mentions dans d'anciens
      diffs : publication conseillée en **historique neuf** (un seul commit).
- [x] **Vivian, 13 octobre 2026** : dépôt public https://github.com/viviandsx/regie5, poussé en
      **historique neuf** (branche locale `public` → `main` du dépôt, un commit « Régie5 0.2.0 ») ;
      tag `v0.2.0` et release GitHub « Régie5 0.2.0 » avec `PPSA99520.zip` ; PR #244 sur
      `blackbearreloaded/ps5-homebrew-catalog` (fichier `apps/PPSA99520.json`, branche
      `viviandsx-patch-1` du fork de Vivian). Contrôles « Scan the release » et « Validate
      submission » verts.
- [x] **PR #244 validée et fusionnée** (13 octobre 2026) : Régie5 0.2.0 est sur homebrew.page.
      Le catalogue suit seul chaque nouvelle release GitHub, pré-releases comprises
      (`docs/RELEASING.md` §2).
- [ ] Règle ufw du port 5555 : **gardée ouverte** par Vivian pour continuer les tests ; à
      fermer quand ils seront finis.
- [x] Branches locales renommées par Vivian : `main` suit `origin/main`, l'ancien historique est
      dans `archive-privee` (jamais poussée), `push.default=upstream`.

## 2 sexies. Version 0.3.0 (en préparation)

Demandé par Vivian le 13 octobre 2026. Une étape = un commit. **Aucune release ni
pré-release tant que la 0.3.0 n'est pas finie et testée sur la console** : le catalogue
publie seul toute release du dépôt.

- [x] **Fixtures anonymes** : les fixtures publiées montraient les vrais titres de la
      bibliothèque de Vivian. `scripts/anonymize_titles.py` remplace titres, titres
      d'épisodes, résumés, studios, chaînes, chemins d'images TMDB et identifiants TMDB /
      TVDB / IMDb par des valeurs inventées, de façon déterministe (le même titre donne le
      même titre inventé dans tous les services ; Seerr renommé d'après les identifiants
      inventés). `make-fixtures.py` l'appelle et refuse d'écrire si un titre d'origine
      reste. Tests inchangés (titres attendus renommés). Vérifié sur tout l'arbre : aucun
      titre ni identifiant d'origine. Le tag `v0.2.0` et l'ancien historique les gardent.
- [x] **Jeu de démo et captures** : `regie5 --demo` (= `--mock` sur `demo/`). `demo/` est
      construit depuis les fixtures par `scripts/make-demo.py` : les titres les plus visibles
      deviennent les films libres de la Blender Foundation (Big Buck Bunny, Elephants Dream,
      Sintel, Tears of Steel, Cosmos Laundromat, Spring, Coffee Run, Sprite Fright, Charge ;
      Hero et Glass Half sans image) et la série Caminandes, métadonnées en anglais. 20
      images de Wikimedia Commons (CC BY 3.0 / 4.0, une CC0) redimensionnées en JPEG
      (`demo/images/`, 1,6 Mo ; Caminandes ép. 3 recadré pour l'affiche), crédits fichier par
      fichier dans `demo/CREDITS.md`, résumés dans le README et `THIRD_PARTY_NOTICES.md`.
      Le transport de démo sert les images listées dans `demo/images.json` (mediacover,
      images Jellyfin, proxy Seerr) ; `fixtures/` n'en a toujours aucune. Hors du zip PS5.
      Captures du README refaites par `scripts/capture-readme.sh` (anglais, 1920 × 1080).
- [x] **a) Versions minimales** (`core/versions`, testé) : Sonarr 4.0, Radarr 5.0, Seerr 2.0
      (Jellyseerr 2.x accepté, Overseerr 1.x non : pas de lien Jellyfin), Jellyfin 10.9
      (`/Items?userId=`, en-tête `MediaBrowser Token`) ; Glances : son chemin `/api/4` suffit.
      Comparaison nombre par nombre ; une version illisible (build de développement) passe.
      Trop ancienne : pastille orange dans Réglages, « Version trop ancienne » et « v3.0.10 —
      Régie5 demande la 4.0 ou plus récente » ; les écrans essaient quand même. Tableau
      « Minimum / Testé avec » dans le README. `REGIE5_MOCK_OLD=sonarr,jellyfin` et l'état
      `old` de `capture-states.sh` pour le voir sans serveur.
- [x] **b) Une langue = un fichier** : `src/core/lang/en.cpp`, `fr.cpp` (textes par clé,
      dans n'importe quel ordre) avec les règles de la langue (pluriel, séparateur décimal,
      jours, mois, unités, modèles de dates) ; `languages.cpp` les liste, CMake prend tout
      le dossier. Une clé absente s'affiche en anglais. `--lang <code>`, langue système de la
      PS5 lue pour 30 langues (anglais si Régie5 ne l'a pas), textes de la page du téléphone
      inclus. Test : clés valides, pas de doublon, mêmes `%d`/`%s` que l'anglais dans le même
      ordre, en et fr complets. `list-strings.py [code]`, `LANGS=de capture-states.sh`,
      **`docs/TRANSLATING.md`** (en anglais, pour les contributeurs). Rendu FR/EN identique
      au pixel près. **Défaut corrigé** au passage (présent depuis la 0.2.0) : les textes
      `ConfigMissing` et `ConfigFromBackup` étaient inversés dans l'ancienne table par
      position (« absent : la copie gardée par l'app est utilisée » affiché quand
      `config.json` manque).
- [x] **c) Reverse proxy** — testé le 13 octobre 2026 : nginx 1.27 en Docker sur
      `127.0.0.1:8443`, HTTPS auto-signé, chemins `/sonarr`, `/radarr/` (avec `/` final),
      `/requests` (Seerr), `/media/jellyfin` (deux niveaux), retirés par le proxy, vers les
      serveurs de Vivian sans rien y changer. **Lecture seule garantie par nginx**
      (`limit_except GET`) : 215 GET, aucune écriture. Tous les onglets et les jaquettes
      (mediacover, images Jellyfin, proxy d'images Seerr) marchent. Trouvé et corrigé :
      sous Linux, un certificat refusé s'affichait « Injoignable » (libcurl : erreurs 60 et
      83 jamais traduites) ; Réglages ne montrait que l'hôte (`127.0.0.1:8443` quatre fois),
      il montre l'adresse avec son chemin ; une redirection (portail de connexion devant le
      proxy, mauvais service) donnait « Erreur du serveur (HTTP 302) », maintenant
      « Redirigé (HTTP 302) — page de connexion du proxy, ou mauvaise adresse ? » (jamais
      suivie : la clé partirait avec). Test `services_behind_a_reverse_proxy`.
      `docs/CONFIG.md` : section « Derrière un reverse proxy » ; section `is4k` remise à
      jour (Régie5 n'écrit plus dans Seerr depuis le 13 octobre). Conteneur arrêté.
      Non testé : chemin gardé par le proxy + `URL Base` réglée dans le service (demande de
      changer le serveur de Vivian).
- [x] **d) Pastilles de compteur** sur les onglets : Activité (téléchargements dans la file
      de toutes les instances ; rouge s'il y a une erreur, orange un avertissement, gris
      sinon), Demandes (demandes en attente, total de Seerr sur toutes les pages, bleu),
      Jellyfin (lectures en cours, vert). Rien à 0 ni quand le compte est inconnu (service
      coupé ou muet). `ui/BadgeWatch` relit les trois toutes les 30 s (lectures seules,
      appelé à chaque tour de boucle, même au repos) ; un écran qui charge les mêmes
      données publie son compte aussitôt (après une approbation, la pastille suit). Apparition
      avec un petit rebond, disparition en fondu, en fonction du temps écoulé ; texte à taille
      fixe (l'atlas ne rastérise rien de plus). Logique dans `core/badges` (testée). Les
      captures du README les montreront à la prochaine série (0.3.0).
- [x] **e) Signalements Seerr** — onglet Demandes en deux vues (pilules **Demandes ·
      Signalements**, filtres de la vue à droite, ✕ pour choisir, comme Activité) ; filtres
      Ouverts · Résolus · Tous. Ligne : jaquette, titre, type (vidéo / audio / sous-titres /
      autre), épisode ou saison, qui l'a signalé, quand, nombre de réponses, dernier message.
      ✕ : le fil des commentaires (les auteurs arrivent avec `GET /issue/{id}` : la liste ne
      les joint pas, vérifié dans le code de Seerr). Options : **Résoudre…**
      (`POST /api/v1/issue/{id}/resolved`) et **Commenter…** (`POST …/comment`, quatre phrases
      toutes faites FR/EN : taper à la manette est inutilisable), chacun reconfirmé, Annuler
      par défaut. Seuls les noms affichés des utilisateurs sont lus (jamais l'e-mail). Cache
      de titres Seerr partagé avec les demandes (`ui/SeerrTitles`), `ListSheet` peut servir
      de choix. Fixtures inventées (`scripts/make-issue-fixtures.py`, 6 signalements),
      `demo/` régénéré, `capture-states.sh` (liste, fil, menu). Endpoints vérifiés contre
      `seerr-api.yml` et `server/routes/issue.ts` (branche develop). **Testé en `--mock`
      seulement** (les deux POST journalisés, rien envoyé) ; lecture vérifiée sur le vrai
      Seerr le 13 octobre 2026 (GET seulement : liste, filtres, fil avec les auteurs) ; aucune
      écriture réelle sans accord de Vivian. Ajouté aux écritures autorisées des consignes du projet (§3).
- [x] **f) Jellyfin : bibliothèques et utilisateurs** — deux sections de l'onglet Jellyfin,
      entre « Scanner les bibliothèques » et les tâches. **Bibliothèques**
      (`/Library/VirtualFolders`) : films, séries + épisodes, ou éléments pour les autres types
      (`/Items?ParentId=…`, `Limit=0` pour compter). **Taille** : Jellyfin n'en donne aucune
      (pas de champ dans `BaseItemDto`, seulement les `MediaSources` de chaque fichier, trop
      lourdes) ; les films et séries de la bibliothèque sont retrouvés dans Radarr / Sonarr
      (TMDB / TVDB / IMDb, comme le Ménage) et leurs tailles additionnées. Un titre dans deux
      instances compte la copie dont le dossier racine (`rootFolderPath`) porte le nom d'un
      dossier de la bibliothèque, sinon la plus petite ; « taille de 49 titres sur 50 » quand
      tout n'est pas trouvé. Chemins lus pour leur dernier nom seulement, jamais affichés.
      **Utilisateurs** (`/Users`, déjà lu par le Ménage) : nom, Admin, Désactivé, « Actif il y a
      2 j » (pastille verte sous 24 h), du plus récent au plus ancien ; ni IP ni appareil (pas
      dans `/Users`). Logique dans `core/libraries` (testée), `ui/JellyfinStats`. Fixtures
      inventées (`scripts/make-library-fixtures.py` : trois bibliothèques depuis l'index déjà
      anonyme ; il donne aussi des noms distincts aux trois utilisateurs que l'anonymiseur
      avait appelés comme d'autres). **Vérifié sur le vrai serveur le 13 octobre 2026** (GET
      seulement, accord de Vivian) : formes conformes, réponses en 20 à 160 ms ; tailles
      trouvées pour 288 films sur 334 et 50 séries sur 51. Corrigé : une bibliothèque
      **Collections** (`boxsets`) comptait les fichiers de ses films (déjà comptés ailleurs) ;
      elle compte maintenant ses collections (`IncludeItemTypes=BoxSet`).

- [x] **g) Interface à 120 Hz** — fait. Réglages › « Fréquence d'affichage : Auto (60 Hz) /
      60 Hz / 120 Hz » (**décision de Vivian, 13 octobre 2026 : en 0.3.0, Auto demande 60 Hz**,
      le 120 Hz se choisit à la main, à cause des 90 images/s mesurées en 4K ; appliqué au
      prochain lancement :
      ps5-opengl fixe la fréquence avant `eglInitialize`), ligne « Affichage » en entier sur
      deux lignes au plus (`REGIE5_DISPLAY=3840x2160@60/120` pour la voir sous Linux), journal
      `display: … at … Hz` et la ligne `[ps5-output-mode]` du pilote.
      **Essais du 13 octobre 2026 sur le moniteur 144 Hz de Vivian** (sortie PS5 en 1080p) :
      avec le `param.json` de la 0.2.0 (app-9, app-10b), `sceVideoOutIsOutputSupported(15)` = 1
      puis `sceVideoOutConfigureOutput(15)` → `0x80290016` (mode non pris en charge), retour à
      60 Hz ; Jelly5 annonçait aussi un refus. **Avec le `param.json` de Jelly5 (app-10d),
      accepté** : `result=00000000`, `refresh_id=13` (119,88 Hz), « 3840 × 2160, 120 Hz »
      dans Réglages. **Cause pas encore établie** : un champ de `param.json` (catégorie,
      `attribute`, `attribute3`, blocs `amm` / `kernel`) ou un changement côté écran entre les
      essais (menu de l'écran). Essai prévu : app-10b (ancien `param.json`, Jeux) relancé ;
      s'il obtient 120 Hz, c'était l'écran ; sinon app-10e (Jeux avec les valeurs de Jelly5)
      dit si c'est la catégorie ou les attributs.
      **Mesures** : à 60 Hz (banc par phases, app-10b, 2 352 images) dessin CPU 4,7 ms en
      moyenne, p95 7,7, pire 17,3 ; le GPU n'est pas séparable (ps5-opengl exécute les dessins
      à l'échange : `deferred=1`, `gpu-present=1`). **À 120 Hz en 4K (app-10d) : 90 à 91
      images/s**, intervalles de 12 à 14 ms, GPU 9 à 14 ms par image (horodatages, l'attente
      de l'écran n'y est plus) : **la 4K ne tient pas le 120 Hz**. À 90 images/s sur un écran
      à 120 Hz, les images durent tantôt une, tantôt deux périodes (saccade).
      **Piste : dessiner moins de pixels à 120 Hz.** VideoOut annonce `3840x2160` à l'app
      même quand le signal HDMI est en 1080p (dans les deux cas, 60 et 120 Hz) : la
      résolution réelle de sortie n'est pas lisible par ce moyen, et `ConfigureOutput` ne
      prend que le mode. Faisable autrement : à 120 Hz, rendre en 2560 × 1440 ou 1920 × 1080
      (`eglSetDisplayModePS5` ; la PS5 met à l'échelle), ou un réglage « Résolution de
      rendu ». Le travail du GPU suit à peu près le nombre de pixels (1440p ≈ 44 %, 1080p ≈ 25 %
      de la 4K), le dessin CPU (4,7 ms) ne change pas : 1440p ou 1080p devraient tenir les
      8,33 ms. À mesurer sur console (`output-1440p`, `output-1080p` existent déjà).
- [ ] **0.3.1 : Auto à 120 Hz avec un rendu 1440p** (décidé par Vivian le 13 octobre 2026).
      Quand le 120 Hz est demandé, rendre en 2560 × 1440 (`eglSetDisplayModePS5`) ; mesurer
      sur console qu'on tient 120 images/s (banc `bench` + `output-1440p`, ou directement le
      nouveau mode) ; si oui, Auto demande 120 Hz (repli 60 Hz si l'écran refuse) et la 4K
      reste pour 60 Hz. Libellé « Auto (60 Hz) » à revoir alors.
- [x] **h) Catégorie Multimédia** — **fait avec le `param.json` de Jelly5** (app-10d démarre,
      tuile dans Multimédia, tas de 256 Mio accordé comme avant, 120 Hz accepté) :
      `applicationCategoryType: 65536`, `contentBadgeType: 1`, intention `launchActivity`,
      `attribute: 0`, `attribute3: 0x80040`, **sans** blocs `amm` ni `kernel` ; ne diffèrent de
      Jelly5 que le titre, le Title ID, le `conceptId`, le `contentId` et la version. Échecs
      (« Impossible de démarrer le jeu ou l'application », journal vide : refus avant la
      première ligne) : app-10 (profil ps5-homebrew-ui : insigne 2, sans `gameIntent`) et
      app-10c (65536 avec nos anciens `attribute` / `amm` / `kernel`). La tuile passe d'une
      catégorie à l'autre par simple copie par-dessus (pas de cache bloquant). `build-ps5.sh`
      élargit le contrôle de ps5-homebrew-ui à ce profil (copie temporaire) et garde
      `REGIE5_PARAM`. `INSTALL.txt`, README et notes de version : « under Media ».

**Sortie de la 0.3.0** (préparée le 13 octobre 2026) :

- [x] Numéros : `contentVersion` `00.003.000`, `project(… VERSION 0.3.0)` ; la version
      « -dev » de Linux suit maintenant `project(VERSION)` même dans un dossier de build
      ancien (elle restait « 0.2.0-dev » dans le cache CMake).
- [x] Auto à 60 Hz (option A de Vivian) : `refresh_asked(Auto)` = 60, libellé « Auto (60 Hz) »,
      README, notes de version et recette en conséquence.
- [x] README : signalements, bibliothèques et utilisateurs, pastilles, 120 Hz, reverse proxy,
      tableau des services, deux captures de plus (`issues.jpg`, `libraries.jpg`) dans
      `capture-readme.sh` (ligne « Display » fixée à la taille des captures).
- [x] `make-demo.py` : bibliothèques Jellyfin de la démo réduites aux titres de la démo
      (sinon « taille de 16 titres sur 414 ») ; `--out` pour regarder avant de remplacer.
- [x] Notes de version `build/publish/release-notes-v0.3.0.md` ; recette console courte
      `docs/RECETTE-0.3.0.md` (lecture seule).
- [ ] Vivian : `make-demo.py` et `capture-readme.sh` (ils réécrivent `demo/` et
      `docs/images/`), commits, puis `release-ps5.sh`, recette, tag, release
      (`docs/RELEASING.md` §3).

## 2 septies. Version 0.3.1 (correctif, ouvert le 14 octobre 2026)

**Défaut de la 0.3.0** : Réglages › Depuis le téléphone affiche « La page n'a pas pu être
ouverte », « listen : permission denied ». La sonde du démarrage le disait déjà dans le
journal d'app-10d (13 octobre, 22:05 : `listening port 8085: refused: Permission denied`),
passé inaperçu, et la recette 0.3.0 ne passait pas par la page. Seul changement côté
console entre la 0.2.0 (écoute OK) et la 0.3.0 : le `param.json`.

Ce qui est établi (docs de ps5-opengl et de ps5-native-app-boilerplate, essais) :

- le 120 Hz ne dépend que de `attribute3 & 0x80040` (`0x40` : 120 Hz permis ; `0x80000` : VRR
  à 120 Hz) ; l'outil de ps5-opengl garde les autres bits ;
- `attribute 0x62000000` = sortie HDR gardée (pas d'écran noir au lancement),
  `attribute3 0x100000` = VRR coupé pour le titre : rien sur le réseau d'après ces docs ;
- restent suspects : la **catégorie Multimédia** (65536) elle-même, les blocs `amm` / `kernel`,
  et ces deux attributs. app-10e (Jeux avec les valeurs de Jelly5) n'a jamais été lancé.

- [x] **Diagnostic et garde-fous** : la sonde du démarrage journalise `bind` et `listen`
      séparément (port 8085 et port choisi par le système) et le profil du `param.json`
      installé ; l'erreur de la page dit « bind : … » ou « listen : … ».
      `ps5/app/param-profiles.json` garde ce que chaque profil a fait sur la console
      (rangée, démarrage, écoute, 120 Hz) ; `scripts/check-param.py` avertit dans
      `build-ps5.sh` et **refuse** dans `release-ps5.sh` un profil jamais vu démarrer **et**
      écouter. `RELEASING.md` : trois points à vérifier à chaque version (rangée et démarrage,
      page du téléphone, fréquence obtenue).
- [x] **Mode stress 120 Hz** (fichier `stress` dans le dossier du titre, minutes dedans,
      10 par défaut) : 120 Hz forcé, tour en boucle, mémoire toutes les 10 s, une ligne par
      tour (écarts entre images), seules les requêtes GET partent (transport `GetOnly`).
- [x] **Build de test `app-12`** (journal UDP vers le laptop, à la maison) et `params/` : un champ changé
      à la fois par rapport à la 0.3.0 (p1 catégorie Jeux, p2 `kernel`, p3 `amm`,
      p4 `attribute`, p5 `attribute3` + `0x100000`), p0 contrôle, p6 la 0.2.0 entière.
      Marche à suivre : `build/ps5/app-12/ESSAIS.txt` (série A : 7 lancements ; série B :
      stress 4K puis 1440p, 10 min chacun). `app-11` : même build, journal vers une mauvaise
      adresse (laptop hors de la maison), à jeter.
- [x] **120 Hz « expérimental »** dans Réglages (ligne « Affichage », quand 120 Hz est choisi),
      en attendant le résultat du stress.
- [x] **0.3.1 rapide** (décision de Vivian, 14 octobre 2026) : le code actuel avec le
      `param.json` de la 0.2.0 (profil `games-0.2.0`, vu démarrer et écouter : 0.2.0, app-7c,
      app-10b avec le code 0.3.0), `contentVersion` `00.003.001`, `project(VERSION 0.3.1)`.
      Tuile dans Jeux, **pas de 120 Hz** : `build-ps5.sh` écrit `REGIE5_HZ120` (bit `0x40` de
      `attribute3`) ; sans lui l'app ne demande jamais 120 Hz, Réglages ne le propose plus
      (Auto ↔ 60 Hz) et un « 120 Hz » gardé de la 0.3.0 s'affiche « 120 Hz indisponible dans
      cette version » en jaune (`DisplayMode::can_120`, `REGIE5_NO_120=1` pour le voir sous
      Linux). README (avertissement « Known issue in 0.3.0 … Fixed in 0.3.1 », Jeux, 120 Hz),
      `INSTALL.txt`, notes `build/publish/release-notes-v0.3.1.md`, recette
      `docs/RECETTE-0.3.1.md`. Zip `build/ps5/release-0.3.1/upload/PPSA99520.zip` (accepté par
      `check-param.py --release`) et build de test `app-13` (même `param.json`, journal UDP).
- [x] **Variante retenue : 120 Hz rendu** (app-14, 14 octobre 2026 au soir) : même profil
      Jeux avec `attribute3 0x180040` (`0x80040` ajouté, `0x100000` gardé) ; démarrage, page
      du téléphone et 120 Hz vérifiés sur la console (profil `games-hz120-0.3.1`). Le 120 Hz
      reste « expérimental » dans Réglages, Auto à 60 Hz. `release-ps5.sh` accepte
      `REGIE5_RELEASE_NAME` (deux candidats d'une même version).
- [x] **0.3.1 publiée** par Vivian le 14 octobre 2026 (release `v0.3.1` à 17:34 UTC, tag sur
      `abc31b2`, zip `14101f04…48e7`, le digest de GitHub est identique ; la release 0.3.0 dit
      « Fixed in 0.3.1 » sous son avertissement). Profil **`games-hz120-0.3.1` vérifié sur la
      console** (app-14 puis le zip : démarre dans Jeux, écoute, page du téléphone ouverte et
      enregistrement fait, 120 Hz obtenu). Catalogue : PR automatique attendue vers 00:00 UTC
      le 15 octobre.
- [x] **Journal du 14 octobre (app-14)** : `param.json` relu par l'app (`attribute3 0x180040`),
      écoute possible sur 8085 et sur un port choisi par le système ; à 120 Hz,
      `[ps5-output-mode] … result=00000000`, « 3840x2160 at 120 Hz, accepted ». **120 images/s
      tenues en 4K** pendant les ~17 s mesurées (écrans légers : GPU ~8,1 ms par image, pire
      33,7 ms à l'ouverture d'un écran, 5 « slow frame » en tout), alors qu'app-10d faisait
      90 images/s sur l'Accueil (GPU 11 ms) : la charge dépend de l'écran (flou, jaquettes
      envoyées). Mémoire stable (21,8 Mio, pic 31,7). Aucun plantage, aucune erreur du pilote.
      Séance trop courte pour conclure sur la tenue ou le plantage : c'est la série B.

### 0.3.2 — Multimédia, écoute et 120 Hz stable (plan du 14 octobre)

Ce qu'on sait : le 120 Hz ne demande que `attribute3 0x80040`, aussi en catégorie Jeux
(app-14). Le profil de Jelly5 (Multimédia) démarre mais n'écoute pas ; Multimédia avec nos
anciens `attribute` / `amm` / `kernel` (app-10c) ne démarre pas.

- [x] **Série A' (rangée Multimédia)** — app-15, 14 octobre 2026 au soir (journal
      `build/ps5/logs/console-20261014-203506.log`). Profil de Jelly5 + un élément de la 0.3.1 :

      | essai | ajout | rangée | démarre | écoute (8085) | 120 Hz |
      |---|---|---|---|---|---|
      | a0 | (0.3.1 publiée, contrôle) | Jeux | oui | oui | accepté, **tenu** (120 images/s) |
      | a1 | bloc `kernel` | Multimédia | **non** (aucune ligne de journal) | — | — |
      | a2 | bloc `amm` | Multimédia | oui | **non** : `bind` refusé (errno 13) | accepté, **non tenu** (≈ 87 images/s) |
      | a3 | `attribute 0x62000000` | Multimédia | oui | **non** : `bind` refusé | accepté, non tenu (≈ 86) |
      | a4 | bit `0x100000` d'`attribute3` | Multimédia | oui | **non** : `bind` refusé | accepté, non tenu (≈ 84) |

      Les cinq sont dans `ps5/app/param-profiles.json` (a1–a4 refusés par `check-param.py
      --release`). **Rangée Multimédia : impossible avec la page du téléphone, sauf découverte
      future.** Deux constats nouveaux : (1) en Multimédia, seul le **port 8085** est refusé ;
      la sonde du démarrage a pu ouvrir **un port choisi par le système** (`bind` sur 0) dans
      a2, a3 et a4 : une page du téléphone sur un port libre (le QR code porte déjà le port)
      pourrait marcher en Multimédia, **jamais essayé** ; (2) en Multimédia, le 120 Hz est
      accepté mais **pas tenu en 4K** (84 à 87 images/s, 1 200 à 1 900 « slow frames » en
      25 à 36 s), alors que le même code tient 120 en Jeux : les 90 images/s d'app-10d
      venaient de la rangée, pas de la 4K. Multimédia voudrait donc dire 60 Hz (ou 1440p à
      essayer). Rien ne presse : on reste dans Jeux.
- [x] **Série B (stress 120 Hz, rangée Jeux)** — 10 min chacun, sans toucher la manette, 8 tours
      de tous les onglets, resté ouvert jusqu'au bout, aucune erreur du pilote, 0 échec
      d'allocation.

      | | B1 4K | B2 1440p |
      |---|---|---|
      | images/s quand l'app dessine | 120,3 | 120,2 |
      | écart moyen entre images (tours 2 à 8) | 8,24 à 8,31 ms | 8,23 à 8,31 ms |
      | pire écart (tours 2 à 8) | 15,3 ms | 14,7 ms |
      | écarts > 17,5 ms (tours 2 à 8) | 0 | 0 |
      | « gpu avg » (attente de l'écran comprise) | 8,1 ms | 8,2 ms |
      | tas au début → à la fin | 19,9 → 22,8 Mio (plat dès 2 min, pic 32,6) | 16,4 → 19,9 Mio (plat dès 2 min, pic 29,8) |

      Le 1er tour seul a des écarts (démarrage, jaquettes reçues). « gpu avg » ne sépare pas le
      GPU de l'attente de l'image (ps5-opengl), donc on ne voit pas la marge gagnée en 1440p :
      **la 4K tient, le rendu 1440p n'est pas utile** (on garde `output-1440p` pour mesurer).
- [x] ~~Rendu 1440p à 120 Hz~~ : inutile (série B).
- [x] **Décision 120 Hz** (proposée le 14 octobre, à confirmer par Vivian) : **Auto demande
      120 Hz** (`refresh_asked(Auto)` = 120), repli 60 Hz si l'écran refuse ; libellé
      « Auto » sans « (60 Hz) » ; mention « expérimental » retirée ; si l'écran refuse, la ligne
      « Affichage » dit « Auto : l'écran reste à 60 Hz » en gris (en jaune seulement pour
      120 Hz choisi à la main). README et notes : une TV qui ne prend le 120 Hz qu'en plus
      basse définition peut recevoir cette définition : 60 Hz pour garder la 4K.
- [x] **Fiche d'un film** (signalé par Vivian) : « Sorties : Au cinéma … · Sortie numérique … ·
      Sortie physique … » coupé par « … » ; une ligne par date (« Au cinéma », « Sortie
      numérique », « Sortie physique »), FR et EN. Fiches revues : la ligne « Qualité des
      fichiers » d'une série passe sur deux lignes si besoin ; `list_scroll` réduit sa marge
      quand la vue est basse (la saison sélectionnée sortait de la liste « Saisons »).
- [x] README : ligne « Known issue in 0.3.0 » retirée, 120 Hz réécrit.
- [x] Version `00.003.002` / `0.3.2`, profil `games-hz120-0.3.1` inchangé ; notes
      `build/publish/release-notes-v0.3.2.md` ; recette `docs/RECETTE-0.3.2.md` (page du
      téléphone, 120 Hz, fiches) ; zip `build/ps5/release-0.3.2/upload/PPSA99520.zip`
      (`482f0a39c0547769fe778c6b139596c839949a0f0345cdf22e32b582d7b82504`) ; patches
      `build/patches-10/`.
- [x] Vivian, 14 octobre 2026 : recette console OK, patches commités, captures du README
      refaites, tag `v0.3.2`, release publiée (sha256 vérifié).
- [x] Catalogue : PR automatique groupée #287 (« Update 6 apps… »), fusionnée le 15 octobre 2026
      à 00:09 UTC ; `version`, `artifact_url`, `sha256` (`482f0a39…`) et `icon_url`
      (`/v0.3.2/…/icon0.png`) justes, sur la PR et sur `main` (vérifié en lecture seule par la
      tâche programmée). La fiche est passée de 0.3.0 à 0.3.2 directement.

## 2 octies. Version 0.4.0 — Emby (ouverte le 14 octobre 2026)

Première issue du dépôt : Emby à côté de Jellyfin. Décisions de Vivian : clé `"emby"` à côté
de `"jellyfin"`, l'une ou l'autre ; « Chercher dans Emby5 » ; serveur Emby de test en Docker
sur le laptop (127.0.0.1:18096, données dans `build/emby-test/`), **jamais** le serveur de
Vivian ; version de test pour l'auteur de l'issue : zip joint à un commentaire, **jamais** en
release (le catalogue publie toute release). Patches : `build/patches-11/`.

- [x] **E1. Serveur de test** — `scripts/emby-test-server.sh` (`start` / `setup` / `stop` /
      `status`) : image `emby/embyserver:latest` (Emby **4.10.1.0**, 381 Mo à télécharger,
      1,35 Go sur le disque ; accord de Vivian), conteneur `regie5-emby-test` sur
      `127.0.0.1:18096` (`REGIE5_EMBY_ADDR` pour la console), 4 films et 3 épisodes de 15 à 20 s
      dessinés par ffmpeg (noms des films libres de Blender, un en HEVC 4K pour le
      transcodage), affiches et fonds dessinés, `.nfo` (rien d'internet).
      `scripts/emby-test-setup.py` : assistant de premier lancement, administrateur de test,
      clé d'API « Regie5 », bibliothèques Films / Séries, utilisateur Lea (films vus, un favori),
      `build/emby-test/config.json` (Emby seul) ; mots de passe générés, rangés dans
      `build/emby-test/credentials.json` (non versionné, jamais affiché).
- [x] **E2. Configuration et noms** — `"emby"` (objet ou liste, mêmes champs) prend la place
      du serveur multimédia si Jellyfin n'est pas actif (`config::MediaServer`) ; les deux
      actifs : Jellyfin, et Réglages le dit (« seul Jellyfin est utilisé »). En-tête
      `X-Emby-Token` (Emby 4.10 accepte aussi `Authorization: MediaBrowser`, vérifié).
      `i18n::set_emby` : dans tous les textes, « Jellyfin » devient « Emby » et « Jelly5 »
      « Emby5 » (traductions inchangées, `TRANSLATING.md`). Options : « Chercher dans Emby5 »
      (jamais de lancement). Version minimale Emby 4.8 (testé 4.10). Page du téléphone :
      section « Emby ». `docs/CONFIG.md` : « Jellyfin ou Emby ».
- [x] **E3. Ce qu'Emby fait autrement** (relevé sur le serveur de test, GET ; lecture
      transcodée simulée par Lea) : tous les chemins de Jellyfin répondent (avec ou sans
      `/emby`) ; écarts traités :
      journal d'activité aux types `playback.start` / `playback.stop` / `user.authenticated` /
      `user.created` (traduits) et `UserId` = numéro interne d'Emby, absent de `/Users` →
      `GET /Users/{numéro}` une fois par numéro ; `LastPlayedDate` seulement avec
      `Fields=UserDataLastPlayedDate` ; **aucune date pour une série, même vue en entier : le
      Ménage l'aurait prise pour « jamais vue récemment »** → la date d'une série est celle de
      son dernier épisode vu (`IsPlayed=true`), et sans cette réponse la liste ne s'affiche
      pas ; accélération matérielle lue dans `VideoPipelineInfo` de la session (Emby ne
      nomme pas la méthode ; `HardwareAccelerationMode` 0 = coupée) ; conseil « accélération
      matérielle » propre à Emby (Paramètres › Transcodage, Emby Premiere).
      Pas journalisé par Emby 4.10 : un mot de passe refusé (rien à cacher de plus).
      `fixtures/emby/` (22 fichiers, `scripts/make-emby-fixtures.py`, sans adresses ni
      identifiants d'appareil), `REGIE5_MOCK_EMBY=1`, état `emby` de `capture-states.sh`.
      Tests : config, noms, versions, page du téléphone, parseurs et `Services` sur les
      fixtures Emby (62 tests).
      Vu contre le vrai Emby de test : Réglages (v4.10.1.0), Accueil (ajouts récents et
      jaquettes servies par Emby), onglet Emby (lecture en cours, diagnostic), Journal,
      bibliothèques, utilisateurs, confirmation du scan.
- [x] README de la 0.4.0 (patch à part : **à commiter seulement avec la release**, le README de
      GitHub change dès le push).
- [x] Zip de test `build/ps5/emby-test-1/upload/PPSA99520.zip` (sha256 `d1e8a247…410c`),
      comme une release (sans journal UDP), `contentVersion` de test `00.003.900` (« 0.3.900 »,
      jamais publié, la vraie 0.4.0 passera au-dessus) ; `param.json` du dépôt inchangé.
      Essai : `build/ps5/emby-test-1/ESSAIS.txt` (A : non-régression Jellyfin ; B facultatif :
      l'Emby de test depuis la console).
- [x] **Essai console d'`emby-test-1`** (Vivian, 14 octobre 2026) : essai A (non-régression
      Jellyfin) OK ; essai B (Emby depuis la console) non fait. Patches 01 à 04 et 06 de
      `build/patches-11/` commités ; le 05 (README) attend la 0.4.0. Zip posté sur l'issue.
- [x] Testeur de l'issue, **@ihatefillingthisout** sur GitHub (15 octobre 2026, avant d'essayer) : **Emby 4.10.1.0** (la version de
      l'Emby de test) et **Emby Premiere** (l'accélération matérielle d'Emby pourra être vue
      dans le diagnostic de lecture, ce que l'Emby de test ne permet pas).
- [ ] Son vrai retour (ce qui marche, captures), puis la 0.4.0.
- **Décision de Vivian (15 octobre 2026)** : on n'attend plus le testeur. Emby est optionnel et
  ne touche pas au chemin Jellyfin (validé sur la console) : si l'**essai B** passe, la 0.4.0
  sort avec Emby marqué **beta** (notes de version, README, mention dans l'app si simple ;
  commentaire à poster sur l'issue #1, remerciement à @ihatefillingthisout sans dire qu'il a
  testé).
- [x] **Essai B sur la console** (Vivian, 15 octobre 2026 : **les 34 étapes conformes** ; fin faite : 0.3.2 réinstallée, Emby de test remis sur 127.0.0.1, règle ufw du port 18096 supprimée ; journal dans `build/ps5/logs/`) : build `app-17` (journal UDP, `test-config` et
      `config-test.json` dans son dossier : Emby seul, l'Emby de test 4.10.1.0 du laptop sur
      192.168.1.15:18096), étapes dans `build/ps5/app-17/ESSAIS.txt` : connexion, onglet Emby,
      lecture transcodée depuis Firefox, diagnostic, journal, bibliothèques et utilisateurs,
      « Chercher dans Emby5 », assistant avec le choix Emby ; port 18096 ouvert dans ufw
      pendant l'essai, Emby remis en écoute locale à la fin. Vu avant sous Linux contre le même
      serveur : Réglages (v4.10.1.0), Accueil, onglet Emby.

### Assistant de première configuration (0.4.0, conception validée le 15 octobre 2026)

Patches : `build/patches-12/`. Captures FR/EN : `build/welcome-captures/` (mode démo).

- [x] **Page du téléphone** : si les ports 8085 à 8090 sont refusés, **un port choisi par le
      système** (le QR code le porte). **Note : ça pourrait rouvrir la piste Multimédia**
      (app-15 : en Multimédia, seul le port 8085 était refusé, un port choisi par le système
      passait la sonde) ; il resterait le 120 Hz non tenu en 4K dans cette rangée. Un
      `config.json` illisible est gardé dans `config.json.invalid` au premier enregistrement
      (jamais par-dessus une copie existante). `?m=jellyfin|emby|none` : la page ne montre
      que le serveur multimédia choisi (Emby alors activé d'office). Testé : ports 8085 à 8090
      occupés, fichier illisible (deux tests).
- [x] **Quand l'afficher** (`core/welcome`, testé) : `config.json` absent, vide, invalide ou
      sans aucun service principal actif (Glances seul ne compte pas) ; jamais si la copie de
      l'app a servi (mise à jour 0.3.x), jamais en `--mock` (sauf `REGIE5_WELCOME=1`), jamais
      après « Plus tard » (`settings.json` : `"welcome": "later"`, gardé quelle que soit la
      sortie de l'assistant).
- [x] **Écrans** (`ui/welcome_flow`) : Bienvenue → serveur multimédia (Jellyfin · Emby · Aucun ;
      les textes passent en Emby/Emby5 aussitôt) → QR code et code à 6 chiffres (repli FTP si
      la page ne s'ouvre pas, avec l'erreur) → vérification (mêmes lignes que Réglages :
      `ui/service_rows`, extraites de Réglages) ; « Vérifier » relit `config.json` (cas FTP),
      un enregistrement du téléphone y passe tout seul ; « Corriger depuis le téléphone »,
      « Commencer » (Accueil, écrans refaits). « Plus tard » partout. Réglages : bouton
      « Assistant » ; Accueil : carte « Configurer » tant que rien n'est configuré.
      Code partagé avec Réglages : `ui/phone_share` (page et QR), `ui/config_reload`.
- [x] Vu sous Linux : config absente (parcours complet, enregistrement simulé par `curl`
      avec `REGIE5_SHARE_LOG_CODE=1`, Emby de test connecté, Accueil rempli), config remplie
      (pas d'assistant), « Plus tard » (carte sur l'Accueil, pas d'assistant au lancement
      suivant, la carte le rouvre), config illisible (`config.json.invalid`), page du téléphone
      au format mobile avec `m=emby`, `--mock` FR/EN.
- [x] **Build de test `app-16`** (journal UDP vers 192.168.1.15 ; version affichée 0.3.2) :
      mode d'essai des builds de développement, **fichier `test-config` dans le dossier du
      titre** → l'app tourne sur `config-test.json` et `/download0/regie5/settings-test.json`,
      sans la copie de secours : le `config.json` de Vivian n'est pas touché.
- [x] **Essai console d'`app-16`** (Vivian, 15 octobre 2026) : tout OK (config remplie : pas
      d'assistant ; config absente par `test-config` : « Plus tard », carte de l'Accueil,
      parcours complet, vérification, « Corriger depuis le téléphone » ; retour à sa config).
      Patches 01 à 04 de `build/patches-12/` commités et poussés.

### Pour publier la 0.4.0 (préparé le 15 octobre 2026)

Recette console : `docs/RECETTE-0.4.0.md` (mise à jour depuis la 0.3.2, Réglages à cinq
boutons, page du téléphone avec sa section Emby, assistant rouvert depuis Réglages,
non-régression Jellyfin ; Emby depuis la console en option). Une release n'a pas le mode
`test-config` : le cas « config absente » reste celui d'`app-16`.

1. [x] ~~Retour du testeur Emby~~ : remplacé par l'essai B (décision de Vivian) ; son retour
       viendra après la 0.4.0, sur l'issue #1. **Mention « bêta »** dans l'app : pastille à côté
       du nom Emby dans les lignes de services (Réglages, vérification de l'assistant), jamais
       pour Jellyfin (`i18n::EmbyBeta`).
2. [x] **README** (15 octobre) : `build/patches-14/03` remplace le patch 05 de
       `build/patches-11/` (Emby, chemin des clés d'API d'Emby corrigé : réglages du serveur ›
       **Advanced › Api Keys › New Api Key**) ; `docs/CONFIG.md` corrigé de même
       (`build/patches-14/01`). Appliqué avec la release, avec la mention **beta** (Features,
       tableau des services « 4.10.1 », limites connues avec le lien vers l'issue #1) ; « (bêta) »
       aussi dans `INSTALL.txt`.
3. [x] **README** : section « At the first start » (assistant, « Later », « Set up » de
       l'Accueil, Settings › « Setup », `config.json.invalid`), Emby dans Features, capture
       `docs/images/welcome.jpg` (choix du serveur, par `capture-readme.sh`, dans le patch 03).
       Les autres captures refaites ne diffèrent que par l'heure, les « N d ago » et le bouton
       « Setup » de `settings.jpg` (pas dans le README) : gardées telles quelles.
4. [x] **`INSTALL.txt`** (FR/EN) : l'assistant au premier lancement, « Plus tard » et comment
       le rouvrir, Emby (clé `"emby"`, chemin de sa clé d'API, « Find in Emby5 »)
       (`build/patches-14/01`).
5. [x] **`config.example.json`** : une ligne de `_comment` sur `"emby"` (`build/patches-14/01`).
6. [x] **Numéros** : `contentVersion` `00.004.000`, `project(regie5 VERSION 0.4.0)` ; profil
       `param.json` inchangé (`games-hz120-0.3.1`, accepté par `check-param.py --release`).
7. [x] **Notes de version** `build/publish/release-notes-v0.4.0.md` (anglais) : Emby en beta,
       testé contre Emby 4.10.1.0, retours sur l'issue #1, remerciement à @ihatefillingthisout
       pour la demande (sans dire qu'il a testé), assistant, page du téléphone, limites connues.
8. [x] **`capture-states.sh`** (`build/patches-14/02`) : l'état `emby` dérivait dès la fiche
       série (pas de menu Options avec Emby : un ○ de trop sortait de la Bibliothèque), il
       suit maintenant le tour jusqu'à Réglages ; `noconfig` était recouvert par l'assistant
       (il tourne désormais avec un `settings.json` « Plus tard » : carte « Configurer » de
       l'Accueil) ; nouvel état `welcome` (les six écrans, Jellyfin puis Emby, mode démo) ;
       capture des boutons de Réglages (« Assistant » sélectionné). Vu en FR/EN : `data`,
       `emby`, `noconfig`, `welcome`. Remarqué en passant, inchangé : en `noconfig`,
       Bibliothèque › Manquants reste sur ses squelettes au lieu de « Pas de configuration ».
9. [~] Tests (`ctest`, 65 verts), `scripts/release-ps5.sh` → `build/ps5/release-0.4.0/upload/PPSA99520.zip`
       (37 fichiers, ni `config.json` ni `test-config`), sha256
       `2c83be9b394dc892a5ce23ebd0a43a1366da5baffc7b03956521f45924d57640` ; reste la recette
       `RECETTE-0.4.0.md` sur la console (raccourcie : mise à jour, Réglages, téléphone,
       Jellyfin inchangé ; Emby et l'assistant déjà vus avec `app-16` et `app-17`).
10. [ ] Vivian : commit « Régie5 0.4.0 », push, tag `v0.4.0`, release GitHub (pièce jointe
        `PPSA99520.zip`, pas « pre-release »), empreinte vérifiée ; patches `build/patches-15/`.
11. [ ] Catalogue : la PR automatique du lendemain (`sha256`, `icon_url` en
        `/v0.4.0/ps5/app/sce_sys/icon0.png`) ; une tâche programmée de vérification comme pour
        la 0.3.2.
12. [ ] Issue Emby : poster `build/publish/issue-1-comment-v0.4.0.md` (prêt, anglais) à la
        sortie ; laisser l'issue ouverte pour ses retours, la fermer quand il confirme.

- Non vérifié : Ménage avec Emby de bout en bout (sans Radarr /
  Sonarr de test : couvert par les tests seulement) ; Emby 4.8 et 4.9 (minimum supposé).

**Hypothèses pour le plantage en 120 Hz** (**non reproduit** : 20 min de stress le 14 octobre
au soir sans fermeture ; aucun rapport dans les journaux ; aucune session
ne finit par une ligne de fermeture, car l'app est quittée par le bouton PS) : à 120 Hz en 4K
le GPU met 9 à 17 ms par image (pire 39 ms) pour 8,33 ms disponibles ; ps5-opengl s'arrête
par `_Exit` quand une image n'est pas confirmée dans son délai (« fail-stop »), ce qui ne
laisse pas de signal (le build de développement le journalise, pas la release). Autres
pistes : relance rapprochée après une session à 120 Hz (ps5-opengl impose 5 s entre deux
présentateurs 120 Hz, dans un même processus seulement) ; mémoire (rien vu : tas stable à
~20 Mio sur les sessions courtes). Après un plantage de la release : relancer Régie5 une fois
et copier `app.prev.log` (`scripts/ps5-fetch-log.py <console> 2121 PPSA99520`).

## 2 nonies. Après la 0.4.0 (ordre décidé par Vivian le 15 octobre 2026)

Rien qui touche au code Emby. Bazarr et Lidarr retirés de la suite (Vivian, 17 octobre 2026) :
ils ne seront pas implémentés.

1. [ ] **Petits correctifs** : un message visible à l'écran (ou une notification système) si
       la console refuse la mémoire demandée, au lieu du seul journal (§ 3, « Taille du tas ») ;
       Bibliothèque › Manquants qui reste sur ses squelettes sans configuration au lieu de
       « Pas de configuration » (vu avec l'état `noconfig` de `capture-states.sh`).

## 2 decies. Version 0.5.0 (ouverte le 16 octobre 2026)

La 0.4.0 est publiée. Cinq chantiers, décidés par Vivian ; les chantiers 3 et 4 passent par
une conception validée avant d'être codés. Patches : `build/patches-16/` (chantiers 1, 2, 5) et `build/patches-17/` (3, 4, puis les lignes des Nouveautés et le build de test).
Un seul build de test console à la fin, puis la release (pas sans l'accord de Vivian).
Le § 2 nonies (petits correctifs) vient après.

- [x] **1. Écran « Nouveautés » après une mise à jour** — `core/whats_new` (testé) : s'ouvre
      une fois au premier lancement d'une nouvelle **série** de versions (0.5 ; un correctif
      0.5.1 ne le rouvre pas), `settings.json` : `"seenVersion"` ; jamais à une première
      installation (PS5 : aucun lancement compté avant dans `/download0/regie5/launches` ;
      Linux : pas encore de `settings.json`), jamais avec l'assistant, jamais en `--mock`
      (sauf `REGIE5_WHATSNEW=1`). Une mise à jour depuis la 0.4.x (pas de `seenVersion`) le
      voit. La version est notée à chaque lancement, montrée ou non. Écran : panneau de verre
      au-dessus des écrans, 3 à 5 lignes (`WnItem1` à `WnItem5`, `-` = pas de ligne ; une ligne
      qui nomme Jellyfin est masquée pour Emby, `i18n::plain`), bouton OK (✕ ou ○). Réglages :
      sixième bouton « Nouveautés » (les boutons rétrécissent ensemble s'ils ne tiennent pas en
      largeur). État `whatsnew` de `capture-states.sh`. Lignes de la 0.5.0 relues le 16 octobre
      contre ce qui est livré (demandes Seerr avec la 4K, Quick Connect — masquée pour Emby —,
      espagnol et allemand, cet écran dans Réglages).
- [x] **2. Espagnol et allemand** — `src/core/lang/es.cpp` et `de.cpp` : toutes les clés (page du
      téléphone comprise), jours, mois, dates, décimale `,`, pluriel « 1 seul », unité de jour
      (`d`, `T`). Faits avec l'aide de l'IA (dit dans le README et `TRANSLATING.md`, dont
      l'exemple passe à l'italien). Langue système PS5 : `es_ES`, `es_419` → `es`, `de` → `de`
      (testé). Vu en captures (`data`, `noconfig`, `welcome`, `whatsnew`) : deux débordements
      corrigés dans le code, pour toute langue — les pilules de la Bibliothèque rétrécissent
      ensemble si elles ne tiennent pas (Ménage en allemand), et les puces de charge du
      diagnostic s'arrêtent avant ses boutons (« Wiedergabe beenden », « Detener la
      reproducción »). Barre d'onglets, boutons de Réglages, fiches et assistant : rien ne
      déborde. Dates, unités et mise en page aussi vues en espagnol et en allemand.
- [x] **3. Demandes Seerr depuis Régie5** (conception validée le 16 octobre 2026, option A) —
      Demandes › pilule **Découvrir** (`ui/screens/discover`) : champ de recherche (clavier
      système PS5 `sceImeDialog`, porté de Jelly5 : `platform/ps5/text_ps5.cpp`, stub
      `libSceCommonDialog` fait par `build-ps5.sh` ; sous Linux, saisie SDL, étape de script
      `text:`), rangées Tendances, Films / Séries populaires, Films / Séries à venir, résultats ;
      fiche (état standard et 4K, saisons, « Demander… », « Chercher dans Jelly5 / Emby5 ») ;
      feuille de demande (saisons à cocher, qualité 4K si Seerr a une instance 4K, serveur,
      profil, dossier préréglés comme dans Seerr), puis confirmation Annuler d'abord ;
      `POST /api/v1/request` au nom du compte de la clé d'API. Appels repris de Jelly5-Seerr
      (`/search`, `/discover/*`, `/movie|tv/{id}`, `/service/radarr|sonarr[/{id}]`,
      `/settings/public`), `is4k` ajouté. Fixtures inventées (`scripts/make-discover-fixtures.py`,
      fixtures et démo), recherche filtrée par le transport `--mock`, tests (chemins, analyseurs,
      corps de la demande, mock). □ (X sous Linux) efface la recherche. État `discover` de
      `capture-states.sh`. Conformité au catalogue : aucune recherche de release, aucun
      indexeur, aucun nom de release ; écriture ajoutée aux règles du projet. **Aucune vraie demande
      faite** (mock seulement, accord de Vivian nécessaire).
      Piste future (option B) : « Demander pour… » un utilisateur de Seerr (en-tête
      `X-Api-User`, ses droits et quotas).
- [x] **4. Quick Connect pour Jellyfin** (conception validée le 16 octobre 2026, option A) —
      `ui/quick_connect` : Réglages › ligne Jellyfin › Options (M) › « Quick Connect… », et bouton
      « Quick Connect » de la vérification de l'assistant quand Jellyfin a une adresse sans clé.
      `/QuickConnect/Enabled` → `POST /QuickConnect/Initiate` → code à 6 chiffres sur la télé →
      `/QuickConnect/Connect?secret=` toutes les 2 s → `POST /Users/AuthenticateWithQuickConnect`.
      Session écrite dans `config.json` (`accessToken`, `userName`, `isAdmin` ;
      `config::with_jellyfin_session`, écriture atomique ; rien en `--mock`), jamais montrée par la
      page du téléphone, effacée si l'adresse change. En-tête `MediaBrowser Client="Regie5",
      Device, DeviceId, Version, Token` ; identifiant d'appareil tiré une fois (`settings.json`
      `"deviceId"`). Une clé d'API passe avant. 401 avec la session → « Session Jellyfin terminée :
      reconnectez-vous » (`Error::Revoked`) ; 403 pour un compte non administrateur → « Demande un
      compte administrateur Jellyfin » (`Error::NeedsAdmin`), dans chaque section concernée ; ligne
      orange « Quick Connect · <nom> · non administrateur ». Emby : pas d'équivalent local (Emby
      Connect passe par internet), rien ne change. `REGIE5_MOCK_QC=1`, fixtures
      `jellyfin/quickconnect_*`, mock `forbidden`, tests, état `quickconnect` de
      `capture-states.sh`. À voir sur la console : contre le Jellyfin de Vivian (lecture seule ; la
      connexion ajoute Régie5 à ses appareils), avec `config-test.json`.
- [x] **Essai console `app-18`** (build de développement, journal UDP, `test-config`, version
      affichée `0.5.0-test` par `REGIE5_SHOW_VERSION`, demandes Seerr **jamais envoyées** par
      `REGIE5_DRY_REQUESTS=1` ; `release-ps5.sh` force les deux à vide / 0) : nouveautés, assistant,
      Quick Connect sur le Jellyfin de Vivian, Découvrir sur son Seerr (lecture seule, arrêt sur
      Annuler), espagnol puis allemand (langue de la console, puis retour au français), session
      révoquée et compte non administrateur (facultatif). Étapes : `build/ps5/app-18/ESSAIS.txt`.
      **Fait par Vivian (16 octobre) : tout OK.** A : au lancement, l'assistant (config d'essai
      vide), pas les Nouveautés ; B : Quick Connect sur son Jellyfin 12, compte admin ; C :
      Nouveautés depuis Réglages, pas de réaffichage ; D : Découvrir, clavier système, fiche,
      feuille, Annuler ; E : espagnol puis allemand, rien ne déborde ; F : session révoquée, retour
      au vert. Fin : `test-config` et `config-test.json` supprimés, appareil « Regie5 » retiré de
      Jellyfin, 0.4.0 réinstallée. Étape 45 fausse : `/download0` n'est visible
      (`/mnt/sandbox/PPSA99520_000/download0/`) que Régie5 ouvert ; `settings-test.json` reste
      sur la console, sans gêne (jamais lu par une release ; seul contenu sensible,
      l'identifiant d'appareil, dont l'appareil est retiré de Jellyfin). Chemins pour les
      prochaines listes : `docs/RELEASING.md`, « Essais console ».

### Pour publier la 0.5.0 (préparé le 16 octobre 2026)

1. [x] **Numéros** : `contentVersion` `00.005.000`, `project(regie5 VERSION 0.5.0)` ; profil
       `param.json` inchangé (`games-hz120-0.3.1`, accepté par `check-param.py --release`).
2. [x] **Zip** : `scripts/release-ps5.sh` → `build/ps5/release-0.5.0/upload/PPSA99520.zip`
       (37 fichiers, ni `config.json` ni `test-config`), sha256
       `32fd8c094b30a6c2cae671471ea2940c1b324cc396e720c78d703846874a0bf8`. Vérifié dans
       `llvm-pie.elf` : pas de `REGIE5_DRY_REQUESTS` (le message « NOT sent » absent), version
       `0.5.0` (pas `0.5.0-test`), pas d'adresse de journal (le mode `test-config` est donc
       inactif). Tests Linux : 71 verts.
3. [x] **Notes de version** `build/publish/release-notes-v0.5.0.md` (anglais).
4. [x] **README** : capture `docs/images/discover.jpg` (ajoutée à `capture-readme.sh`), Seerr
       (Découvrir, demandes) et Jellyfin (Quick Connect) dans le tableau des services, limite
       « demandes au nom du compte de la clé d'API ».
5. [x] **Recette** `docs/RECETTE-0.5.0.md` (mise à jour depuis la 0.4.0, Nouveautés, Réglages,
       page du téléphone, Découvrir en lecture seule).
6. [x] **Brouillon Reddit** `build/publish/reddit-r-ps5-jailbreak.md` mis à jour (0.5.0).
7. [x] **`is4k`** : vérifié dans le code de Seerr (`server/entity/MediaRequest.ts`, branche
       `develop`) : le corps est lu tel quel, `is4k: !!requestBody.is4k`, droits 4K
       (`REQUEST_4K`, `REQUEST_4K_MOVIE` / `_TV`) vérifiés, approbation automatique par
       `AUTO_APPROVE_4K` ou `MANAGE_REQUESTS` ; `serverId`, `profileId`, `rootFolder`,
       `seasons`, `tvdbId` aux mêmes noms que les nôtres. Pas de vraie demande faite.
8. [x] Vivian : recette `docs/RECETTE-0.5.0.md` sur la console.
9. [x] Vivian : commit « Régie5 0.5.0 » (`build/patches-18/`), push, tag `v0.5.0`, release
       GitHub (pièce jointe `PPSA99520.zip`, pas « pre-release »), empreinte vérifiée.
10. [ ] Catalogue : la PR automatique du lendemain (`sha256`, `icon_url` en
        `/v0.5.0/ps5/app/sce_sys/icon0.png`).
11. [ ] Reddit : poster le brouillon une fois la fiche du catalogue en 0.5.0.
- [x] **5. Communauté** — `docs/CONTRIBUTING.md` (anglais : bug, idée, traduction, code, règles
      du projet ; lié depuis le README) ; `docs/images/demo.gif` (600 px, ~3 Mo, 10 images/s :
      Accueil, Bibliothèque et fiche de Caminandes, Activité, Demandes, Jellyfin et diagnostic,
      mode démo) en tête des fonctionnalités du README, refait par `scripts/capture-gif.sh` ;
      brouillon `build/publish/reddit-r-ps5-jailbreak.md` (posté par Vivian ; à ajuster selon la
      version au moment de poster). GitHub Discussions : conseillé, à activer par Vivian
      (Settings › General › Features › Discussions). La mention de l'IA reste au README
      (« How Régie5 was made », traductions) et à `TRANSLATING.md`.

## 2 undecies. Plantage du testeur Emby (issue #1, 16 octobre 2026)

Signalé par Vivian : @ihatefillingthisout (firmware 12.40, webkit, ShadowMount+, kstuff)
configure Emby par la page du téléphone, « Enregistrer » (message vert), revient sur la
console : plantage, puis **à chaque lancement**. Écran de la console : SIGSEGV, « page fault
(user read data, page not present) », adresse `0x7eedf8000`, RIP `0x470750`, backtrace[0]
`0x80000cc3f`. Patches : `build/patches-19/`.

- [x] **Quel binaire** (app chargée à `0x400000`, donc décalage `0x70750` dans `llvm-pie.elf`) :
      0.4.0 et 0.5.0 : `0x70750` tombe **au milieu d'une instruction** (`launch_app`, `libraries::measure`),
      impossible. **emby-test-1** (zip de test posté sur l'issue le 14 octobre, version affichée
      0.3.900) : pile sur `mov (%rbp,%r15,8),%r8` de `ps5::on_crash` (+0xf0), la lecture de la pile
      par le **rapport de plantage**. C'est donc emby-test-1 qui tourne chez lui.
- [x] **Ce qui plante** : un premier plantage (inconnu) appelle `on_crash`, qui écrit sa ligne
      « CRASH » dans `app.log`, puis lit 1536 mots (12 Kio) au-dessus de son cadre pour chercher
      des adresses de retour. Sur un thread dont la pile est peu remplie (un worker HTTP : la pile
      se termine juste au-dessus), la lecture **dépasse le haut de la pile** (adresse alignée sur
      une page, `0x7eedf8000`) : second SIGSEGV dans le gestionnaire, et c'est lui que la console
      affiche. Défaut présent depuis la 0.2.0 (`system_ps5.cpp` jamais modifié), donc aussi en
      0.4.0 et 0.5.0 ; il masque tout plantage d'un worker. Pas lié au firmware 12.40 (la taille
      du tas accordée pourrait l'être : `app.prev.log` le dira, ligne « heap: N MiB granted »).
- [x] **Cause probable du premier plantage** (à confirmer par `app.prev.log`) : **Emby ignore
      `fillWidth`** (vérifié sur l'Emby de test : `fillWidth=200` → 500×750, l'original ;
      `maxWidth=200` → 200×300). Sur un vrai Emby (affiches 2000×3000, fonds 4K), l'Accueil
      décode les originaux dès le démarrage, trois à la fois (`images`, 3 workers), environ
      7 octets par pixel le temps du décodage : le tas de la PS5 (128 à 256 Mio) s'épuise, un `new`
      échoue (`ud2`, signal 4, vu en probe-4), sur un worker → le rapport plante comme ci-dessus.
      L'Emby de test n'a que des images de 500×750 : rien vu à l'essai B. Ni la lecture de
      `config.json` (relue sans souci sous Linux, avec AddressSanitizer), ni la page du téléphone.
- [x] **Reproduit sous Linux** (dans le scratchpad, sans toucher `build/emby-test/`) : code
      d'emby-test-1 (`git archive 062524b`) + petit proxy devant l'Emby de test qui sert des
      images à la taille d'un vrai serveur quand on demande `fillWidth` + `LD_PRELOAD` qui mesure
      le tas : pic **24 Mio** avec les images de test, **116 Mio** avec les vraies tailles (une
      allocation de 31 Mio : un fond 4K décodé en entier), pour 5 titres. Avec la correction :
      **23 Mio** ; et si un serveur ignorait aussi `maxWidth`, **64 Mio** (garde-fou de décodage).
      Le plantage lui-même dépend du tas de la console (Linux n'a pas de tas fixe).
- [x] **Corrections** (patch 01) : `jellyfin::emby_image_path` (`fillWidth` → `maxWidth`, Emby
      seulement : rien ne change pour Jellyfin), appliqué dans `art::submit` ; `art::decode` lit
      d'abord la taille (`stbi_info_from_memory`) : au-delà de 4096×2304 l'image n'est pas décodée
      (jaquette dessinée), au-delà de 1920×1080 une seule à la fois.
- [x] **Rapport de plantage borné** (patch 02) : `note_thread_stack()` (main, workers HTTP par
      `http::set_thread_start`) note le haut de la pile de chaque thread dans une table fixe
      (pas de `thread_local` : la chaîne PS5 l'émule par `__emutls_get_address`, qui peut
      allouer) ; ailleurs la lecture s'arrête à la fin de la page de `sp`. Même borne pour
      `abort()`.
- [x] **Garde-fou de démarrage** (patch 02, `core/boot_guard`, testé) : un plantage (signal ou
      `abort`) dans les 30 s qui suivent la mise en service d'une configuration (démarrage ou
      relecture) écrit `/download0/regie5/crash-mark` (empreinte FNV-1a du texte). Au démarrage,
      `/download0/regie5/boot-guard` compte les plantages de suite avec le même texte ; au 2ᵉ, le
      lancement suivant **met le fichier de côté** : `LoadError::SetAside`, rien de chargé, ni
      copié, ni modifié ; message orange dans l'Accueil (carte « Configurer ») et Réglages,
      assistant ouvert même après « Plus tard » ou avec la copie, texte propre sur son premier
      écran (FR/EN/ES/DE). Un texte nouveau (page du téléphone, FTP) est chargé ; « Tout tester »
      / △ / « Vérifier » relisent le fichier, et une relecture qui tient 30 s efface la mise de
      côté. La copie de `/download0` est jugée comme `config.json` quand elle le remplace.
      Linux : mêmes fichiers à côté de `settings.json`, `REGIE5_TEST_CRASH=N` (plantage à N s)
      pour l'essayer ; rien en `--mock`. **Vu sous Linux** : 2 plantages → 3ᵉ lancement sur
      l'assistant avec le message, Accueil et Réglages, △ → Emby au vert, `boot-guard` effacé
      après 30 s.
- [x] **Contournement pour le testeur** (dans `issue-1-crash-reply.md`) : **ne rien supprimer**.
      Supprimer `config.json` ne suffit pas (la copie de `/download0/regie5/config.json`, mise à
      jour au lancement qui a planté, donc identique, prendrait le relais) ; supprimer la copie
      est impossible en pratique (`/download0` n'existe que Régie5 ouvert, et il plante en
      quelques secondes). Donc : garder `config.json` sur le PC, le remplacer sur la console par
      un fichier `{}` (valide : la copie n'est plus lue, puis écrasée par `{}`), lancer **une**
      fois, récupérer `app.prev.log` (le lancement d'avant, celui qui a planté) pendant que
      Régie5 est ouvert.
- [x] **Zip de test** `build/ps5/emby-fix-1/upload/PPSA99520.zip` (sha256
      `ef0d93d2ab5fa0808e4f683d4db2fd280b52592ebb032e1125a31546bbd593e3`) : la 0.5.0 + patches
      01-02, comme une release (pas de journal UDP), version affichée `0.5.1-test1`,
      `contentVersion` 00.005.000 inchangé (une 0.5.1 passera au-dessus). `emby-test-2` : build
      intermédiaire écarté (`thread_local` émulé), à ne pas poster.
- [x] Vivian (16 octobre) : essai console d'`emby-fix-1` avec son Jellyfin **OK** ; patches
      01-02 de `build/patches-19/` commités et poussés (`762e998`, `3ca77de`) ; contournement
      posté sur l'issue ; ligne « Known issue » ajoutée au texte de la release v0.5.0 ; zip de
      test posté sur l'issue. Le garde-fou ne se déclenche pas à la demande sur la console.
- [ ] Retour du testeur (aucune réponse au 17 octobre ; message de sortie posté) : `config.json` (clés en `xxx`), version exacte, `app.prev.log` (ligne
      « CRASH signal … », « ALLOCATION FAILED », « heap: N MiB granted »).
- [x] **0.5.1 décidée** (Vivian) : publiée dès que le testeur confirme `emby-fix-1`, ou au plus
      tard le 18 octobre 2026 sans réponse. Pas de 0.4.1 (le catalogue ne propose que la
      dernière). Préparation ci-dessous.
- À surveiller : le journal de l'essai B (`build/ps5/logs/console-20261015-153949.log`) finit
      par un plantage d'`app-17` (« CRASH signal 11 … fault address 0xa8 », sans ligne de pile :
      même défaut du rapport) juste avant le lancement de la 0.3.2 réinstallée. Probablement la
      copie de la 0.3.2 par-dessus l'app encore ouverte ; à garder en tête si une adresse 0xa8
      revient.

### Pour publier la 0.5.1 (préparé le 16 octobre 2026)

Le code est celui d'`emby-fix-1` (déjà commité) ; la 0.5.1 n'ajoute que les numéros, le README,
`INSTALL.txt`, la recette et la roadmap. Patch : `build/patches-20/`.

1. [x] **Numéros** : `contentVersion` `00.005.001`, `project(regie5 VERSION 0.5.1)` ; profil
       `param.json` inchangé (`games-hz120-0.3.1`, accepté par `check-param.py --release`). Les
       Nouveautés ne se rouvrent pas (série 0.5) : pas de ligne à écrire.
2. [x] **Zip** : `scripts/release-ps5.sh` → `build/ps5/release-0.5.1/upload/PPSA99520.zip`
       (37 fichiers, ni `config.json` ni `test-config`), sha256
       `75f6fced1dd15c1d16c31d5632c053801374514a23a70ca1fcbbada812abfb2b`. Vérifié dans
       `llvm-pie.elf` : version `0.5.1` (ni `-test1` ni `-dev`), pas de « NOT sent », pas
       d'adresse de journal, `emby_image_path` / `note_thread_stack` / `boot_guard` présents ;
       `INSTALL.txt` du zip avec le paragraphe du garde-fou. Tests Linux : 72 verts.
3. [x] **Notes de version** `build/publish/release-notes-v0.5.1.md` (anglais) : correctif Emby,
       écran de plantage, garde-fou, que faire si la 0.4.0 / 0.5.0 se ferme à chaque lancement,
       remerciement à @ihatefillingthisout pour le signalement.
4. [x] **README** (« At the first start ») et **`INSTALL.txt`** (FR/EN) : un paragraphe sur le
       garde-fou. Captures inchangées.
5. [x] **Recette** `docs/RECETTE-0.5.1.md` (5 min : mise à jour, Réglages, page du téléphone,
       relance).
6. [x] **Déclencheur** : pas de réponse du testeur, release faite le 17 octobre. Règle d'origine : le testeur confirme qu'`emby-fix-1` marche, **ou** le 18 octobre 2026
       sans réponse. S'il répond que ça plante encore : ne pas publier, regarder son
       `app.prev.log` d'abord.
7. [x] Vivian : recette `docs/RECETTE-0.5.1.md` sur la console.
8. [x] Vivian : commit « Régie5 0.5.1 » (`build/patches-20/`), push, tag `v0.5.1`
       (`git push origin v0.5.1`), release GitHub « Régie5 0.5.1 » (texte = les notes, pièce
       jointe `PPSA99520.zip`, pas « pre-release »), empreinte vérifiée.
9. [x] Release v0.5.0 : retirer la ligne « Known issue » (`build/publish/release-0.5.0-known-issue.md` :
       quoi supprimer, ligne « Fixed in 0.5.1 » à mettre à la place). Texte seulement : ni le tag
       ni le zip.
10. [x] Issue #1 : sans réponse, `issue-1-release-no-reply.md` posté, issue ouverte. Règle : testeur OK → `build/publish/issue-1-closing.md`, puis fermer l'issue ; sans
        réponse → `build/publish/issue-1-release-no-reply.md`, issue laissée ouverte.
11. [ ] Catalogue (encore en 0.5.0 le 17 octobre) : la PR automatique du lendemain (`version` 0.5.1, `sha256` de l'étape 2,
        `icon_url` en `/v0.5.1/ps5/app/sce_sys/icon0.png`) ; sinon chemin manuel
        (`docs/RELEASING.md` §3).
12. [ ] Après la sortie (facultatif, si le testeur confirme) : la ligne « Emby is in beta » du
        README peut dire qu'Emby a été essayé sur un vrai serveur Emby 4.10.1 avec Premiere.

## 2 duodecies. Version 0.6.0 — Plex (ouverte le 17 octobre 2026)

Demandé par Vivian : Plex comme troisième serveur multimédia (clé `"plex"`, un seul serveur
multimédia à la fois, mention beta). Étape 1 : étude sans code ; étape 2 (implémentation)
seulement après validation. Patches : `build/patches-21/`.

- [x] **Étude de faisabilité** `docs/PLEX.md` (17 octobre 2026), vérifiée sur la
      spécification officielle de l'API de Plex Media Server (developer.plex.tv, OpenAPI 1.2.3),
      le guide officiel « Authenticating with Plex », les articles d'aide de Plex, l'image
      Docker officielle, le code de Seerr, de Tautulli et de python-plexapi, et une mesure des
      en-têtes CORS de plex.tv (requêtes `OPTIONS`, aucun compte). Conclusions :
      connexion **par le téléphone** (PIN plex.tv fait par la page locale : CORS `*` mesuré,
      comme Seerr le fait dans le navigateur ; seul le jeton du serveur, trouvé par
      `resources`, arrive sur la console), jeton collé en secours, accès sans jeton accepté ;
      jeton **classique** (le JWT expire en 7 jours : impossible hors ligne) ; compte
      **propriétaire** nécessaire pour sessions, arrêt, scan, tâches, historique de tous ;
      **Plex Pass** pour l'accélération matérielle et sans doute l'arrêt d'une lecture ;
      raisons de transcodage absentes des sessions (déduites) ; Journal réduit aux lectures
      terminées ; Ménage réduit (« vu par tous », preuve dans l'historique) ou reporté avec
      Tautulli ; images par `/photo/:/transcode`, **taille à mesurer** (leçon d'Emby) et
      plafond de décodage gardé ; minimum PMS 1.41.9 ; serveur de test sans compte possible
      (`ALLOWED_NETWORKS`, image officielle de 163 Mo).
- [x] **Décisions de Vivian** (17 octobre 2026) : (1) authentification validée : PIN plex.tv par
      le téléphone via la page locale (jeton classique propre au serveur, jamais celui du compte),
      jeton collé en secours, accès sans jeton accepté mais déconseillé à l'écran ; (2) **pas de
      Ménage avec Plex en 0.6.0** : message « non disponible avec Plex pour l'instant » dans le
      Ménage ; piste Tautulli plus tard (§ 3) ; (3) image Docker téléchargée ; (4) agent Plex sur
      le serveur de test ; (5) compte Plex de test : à la séance de connexion par le téléphone,
      créé par Vivian avec une adresse dédiée. Implémentation en autonomie, une séance à la fois ;
      arrêt pour le compte Plex ou pour le test console. Patches : `build/patches-22/`.
- [x] **P1. Serveur de test** — `scripts/plex-test-server.sh` (PMS 1.43.4, 127.0.0.1:32400, sans
      compte, `ALLOWED_NETWORKS`), `scripts/plex-test-setup.py` (bibliothèques par l'API, agents
      Plex), `scripts/plex-test-play.sh` (lecture transcodée ou directe sans Plex Web). Relevé dans
      `docs/PLEX.md` § 7 : JSON partout ; **images à la taille demandée** (2000×3000 → 300×450,
      3840×2160 → 960×540), `height` obligatoire ; `Media` d'une session = ce qui est envoyé, la
      source dans les métadonnées ; arrêt → 401 sans Plex Pass ; Butler presque sans titres ;
      `POST /library/sections` (la spécification dit `/all`).
- [x] **P2. Configuration et noms** — clé `"plex"` (`url`, `token`, `userName`, `owner` ; une
      adresse sans jeton est acceptée), `config::MediaServer::Plex`, un seul serveur multimédia
      (Jellyfin, puis Emby, puis Plex ; « Plusieurs serveurs multimédias… : seul X est utilisé »),
      `i18n::set_media` (Jellyfin et Jelly5 deviennent « Plex » : onglet, « Chercher dans Plex ») ;
      `api/plex` (en-têtes `X-Plex-*` sur chaque requête, `X-Plex-Token` en en-tête seulement,
      test de connexion sur `/`), erreurs propres à Plex (jeton absent, refusé, pas le
      propriétaire) ; version minimale 1.41.9 ; Réglages : ligne Plex « bêta », version courte,
      « Sans jeton : … (déconseillé) », compte et « pas le propriétaire » en orange ; assistant :
      Jellyfin · Emby · **Plex** · Aucun (`m=plex`) ; page du téléphone : section Plex, jeton
      collé (champ avancé, guide, avertissement sans jeton), jeton jamais renvoyé, effacé si
      l'adresse change. `REGIE5_MOCK_PLEX=1` (configuration seulement, fixtures en P3).
      `docs/CONFIG.md` (« Jellyfin, Emby ou Plex »), `config.example.json`. Vu contre le serveur de
      test : Réglages au vert (v1.43.4.10903), assistant, page au format téléphone. 74 tests.
- [x] **P3. Client Plex et écrans** — `api/plex` complet : sessions (ce qui est envoyé, et la
      source lue dans les métadonnées de l'élément ; compte nommé par `/accounts` quand Plex ne le
      nomme pas), raisons de transcodage **déduites** et dites comme telles (« déduit par
      Régie5 »), accélération matérielle par son nom, avance du transcodage ; arrêt
      (`POST /status/sessions/terminate`, refus → « arrêter une lecture demande un Plex Pass »,
      prévenu dans la confirmation sans Plex Pass) ; scan de chaque bibliothèque ; Butler
      (« Planifiée », « Tous les N jours ») et activités en cours ; ajouts récents (par
      bibliothèque, épisodes regroupés) ; Journal = lectures terminées (« Regardé par … ») ;
      bibliothèques, nombres, utilisateurs (« Propriétaire », dernière activité d'après
      l'historique) ; images toujours par le transcodeur (`Services::media_image`) ; Ménage :
      « non disponible avec Plex pour l'instant » (rien demandé, ne s'active pas dans Réglages).
      Jamais lus : adresses, `deviceID`, chemins. `fixtures/plex/` (21 fichiers,
      `scripts/make-plex-fixtures.py`, anonymisés, un 2ᵉ compte « Lea » synthétique),
      `REGIE5_MOCK_PLEX=1`, état `plex` de `capture-states.sh` (vu en FR, EN, ES, DE : rien ne
      déborde), test anti-fuite étendu (`address`, `remotePublicAddress`, `file`, `deviceID`,
      jeton). Vu contre le serveur de test : Accueil, onglet Plex, diagnostic d'un transcodage
      4K HEVC → H.264, scan (Annuler par défaut), arrêt refusé (pas de Plex Pass), Journal.
      Emby et Jellyfin inchangés (états `data` et `emby`). 76 tests.
- [~] **P4. Connexion par le téléphone** — code fait : bouton « Se connecter à Plex » de la page
      (PIN fort sur plex.tv, page de connexion de Plex dans un nouvel onglet, attente du jeton une
      fois par seconde, `/api/v2/user`, `clients.plex.tv/api/v2/resources`), serveur trouvé par
      son adresse parmi ses connexions (sinon liste à choisir), seul le **jeton du serveur** envoyé
      à la console avec le compte et `owner` ; identifiant client tiré par la page (« Regie5 »).
      Relevé : le serveur Plex n'accepte en CORS que `app.plex.tv`, la page ne peut donc pas lire
      son `/identity`. Essayé dans le navigateur avec plex.tv **simulé** (aucun appel réel) : les
      deux cas (adresse trouvée, choix dans la liste), `config.json` avec le jeton du serveur, rien
      du jeton du compte, rien dans le journal. Test `share_plex` étendu. **Reste : l'essai réel avec
      le compte Plex de test de Vivian** (connexion, `resources`, jeton refusé après retrait de
      l'appareil).
- [x] **Changement de plan (Vivian, 17 octobre 2026)** : pas de zip de test pour un utilisateur de
      Plex ; la **0.6.0 sort directement avec Plex en BETA**, et les utilisateurs de Plex sont invités à
      ouvrir une issue pour tout retour (modèle « Plex feedback (beta) »). Avant la release : l'essai de
      la connexion avec le compte Plex de test de Vivian (sous Linux), et la recette console de
      non-régression Jellyfin avec sa config. Pas d'essai Plex sur la console.
- [~] **Essai avec le compte Plex de test** — préparé : `scripts/plex-test-server.sh claim` (code de
      réclamation demandé sans écho, jamais gardé ; les réseaux Docker n'entrent plus sans jeton) et
      `unclaim` (jeton du compte retiré des réglages du serveur par l'outil de Plex dans le conteneur,
      retour sans compte) ; accès à distance coupé et nom « Regie5 test » réglés avant toute liaison ;
      la page écrit dans la console du navigateur si le jeton du serveur est celui du compte (jamais
      les jetons). Étapes : `build/plex-test/ESSAI-COMPTE.txt` (21 étapes). En attente de Vivian.
 P5 fait dans P3 (message du Ménage).

### Pour publier la 0.6.0 (préparé le 17 octobre 2026)

Plex en **BETA** ; pas de zip de test pour un utilisateur de Plex (décision de Vivian). Patches :
`build/patches-22/` (05 : essai avec le compte, 06 : la release).

1. [x] **Numéros** : `contentVersion` `00.006.000`, `project(regie5 VERSION 0.6.0)` ; profil
       `param.json` inchangé (`games-hz120-0.3.1`).
2. [x] **Nouveautés** de la série 0.6 (FR/EN/ES/DE, vues sans débordement) : Plex bêta ; retours
       des utilisateurs de Plex dans une issue, même si tout marche ; Réglages › Nouveautés.
3. [x] **README** (Plex bêta : fonctionnalités, tableau des services 1.41.9 / 1.43.4, connexion
       depuis le téléphone, limites connues, lien vers le formulaire), **`INSTALL.txt`** (FR/EN),
       modèle d'issue **`.github/ISSUE_TEMPLATE/plex_feedback.yml`** (« Plex feedback (beta) » :
       version de PMS, Plex Pass, compte utilisé, autres comptes, connexion, ce qui marche, ce qui
       ne marche pas, captures ; étiquettes `plex` et `beta`).
4. [x] **Notes de version** `build/publish/release-notes-v0.6.0.md` (anglais : appel aux
       utilisateurs de Plex en tête, ce qui marche, limites connues).
5. [x] **Brouillon Reddit** `build/publish/reddit-r-ps5-jailbreak-v0.6.0.md` (appel aux
       utilisateurs de Plex).
6. [x] **Recette console** `docs/RECETTE-0.6.0.md` (22 étapes : mise à jour, Nouveautés, Accueil,
       onglet Jellyfin, Réglages, page du téléphone, ligne Affichage ; aucune écriture, pas de Plex).
7. [x] **Zip** : `scripts/release-ps5.sh` → `build/ps5/release-0.6.0/upload/PPSA99520.zip`
       (37 fichiers, ni `config.json` ni `test-config`), sha256
       `c14c91997ec55d1a0b55a32ca5c3064428d9e8787fb80f01539884dd0f3ff385`. Vérifié dans
       `llvm-pie.elf` : version `0.6.0` (ni `-test` ni `-dev`), pas de « NOT sent », pas d'adresse de
       journal ; profil accepté par `check-param.py --release`. Tests Linux : 76 verts.
8. [x] Vivian : essai avec le compte Plex de test (17 octobre 2026) : liaison, refus sans jeton,
       connexion par le téléphone (serveur trouvé directement), ligne verte, `unclaim` OK.
       **Deux constats** : pour le propriétaire, le jeton « du serveur » est **le jeton du compte** ;
       retirer l'appareil « Regie5 » **ne le révoque pas** (encore accepté 5 minutes après).
       Recherche (`docs/PLEX.md` § 8) : rien de mieux pour une console hors ligne (jeton transitoire :
       48 h, perdu au redémarrage du serveur ; JWT d'appareil : 7 jours, rafraîchi par internet).
       Corrigé : avertissement sur la page du téléphone sous « Se connecter à Plex » (4 langues),
       README, `docs/CONFIG.md`, `docs/PLEX.md`, notes de version, brouillon Reddit ; révocation
       réelle = changer le mot de passe sur plex.tv avec « Sign out connected devices » ; voie sans
       jeton (liste « allowed without auth » réduite à l'adresse de la console) décrite. Ligne de
       diagnostic de la console du navigateur retirée. Nouveau zip `release-0.6.0b` ci-dessous.
9. [x] Vivian : recette `docs/RECETTE-0.6.0.md` faite sur le premier zip (`c14c9199…`) ; **à refaire**
       avec `build/ps5/release-0.6.0b/upload/PPSA99520.zip`, sha256
       `6757ea3b5918e58be14c593922c895d804b074e7ffb4d061ce41fcd16345a080` (mêmes contrôles : 37
       fichiers, version 0.6.0, ni « NOT sent » ni ligne de diagnostic). C'est lui qui sera publié.
10. [x] Vivian : patch `06-regie5-0.6.0-v2` de `build/patches-22/` (le 05 est commité ; l'ancien 06 n'est plus à utiliser), commit « Régie5 0.6.0 », push.
11. [x] Vivian : tag `v0.6.0` et push du tag seul.
12. [x] Vivian : release GitHub « Régie5 0.6.0 » (texte = les notes, pièce jointe `PPSA99520.zip`,
        pas « pre-release »), empreinte vérifiée.
13. [ ] Vivian : étiquettes `plex` et `beta` créées sur le dépôt (le formulaire les pose).
14. [ ] Catalogue (pas encore en 0.6.0 le 18 octobre ; Vivian vérifie le 19, sinon `docs/RELEASING.md` § 3) : la PR automatique du lendemain (`version` 0.6.0, `sha256`, `icon_url` en
        `/v0.6.0/ps5/app/sce_sys/icon0.png`).
15. [ ] Reddit : poster le brouillon une fois la fiche du catalogue en 0.6.0.

## 2 terdecies. Audit d'optimisation (18 octobre 2026, sans code)

Demandé par Vivian après la 0.6.0 : code et journaux console (`build/ps5/logs/`, 35 fichiers,
11 au 15 octobre ; aucun journal Emby ou Plex sur console). Intérêt : ★★★ fort, ★★ moyen,
★ faible. Difficulté : F facile, M moyenne, D difficile. « Console » : demande un essai PS5.

**Ce qui va bien (mesuré)** : 120 Hz tenu en rangée Jeux (série B : 0 écart > 17,5 ms) ; tas
plat sur 10 min de stress (19,9 → 22,8 Mio, pic 32,6, 0 échec), pic de 35 Mio au plus dans
tous les journaux pour 256 Mio accordés : **aucune fuite vue** ; aucun « glyph atlas started
afresh » ; glyphes préparés au démarrage : 952 en 45 ms.

### Performances PS5

| # | Constat (preuve) | Gain | Int. | Diff. | Console |
|---|---|---|---|---|---|
| P1 | Images de 90 à 173 ms sur Demandes alors que les phases mesurées font < 4 ms (`console-20261013-210345.log`, #1904 à #1951, en même temps que les réponses Seerr `/movie/N` et les jaquettes). Aucune priorité ni affinité n'est donnée aux 7 threads (4 HTTP + 3 images, qui parsent, décodent, réduisent, floutent ; `main_ps5.cpp:718`) : hypothèse, ils prennent le cœur du thread principal | plus de saccades à l'ouverture des écrans chargés | ★★★ | F | oui (mesurer avant/après) |
| P2 | Rastérisation des glyphes à l'ouverture d'un écran : 6 à 15 ms sur une image (image #2 : 81 à 87 glyphes, 9 à 11 ms ; pire 25,5 ms) ; 44 secondes sur 1 763 ont une image > 8,3 ms à cause d'eux | 1 à 2 images sautées en moins à chaque écran à 120 Hz | ★★ | M (budget de glyphes par image, ou préchauffage étendu) | oui |
| P3 | Envois de textures : 3 par image (`art.cpp` `kUploadsPerFrame`), pire 10,5 ms > 8,3 ms à 120 Hz | défilement plus régulier pendant l'arrivée des jaquettes | ★★ | F (budget en temps) | oui |
| P4 | Jaquettes retéléchargées : 576 requêtes d'image pour 236 adresses (59 % de redites, même journal). La clé du cache contient la taille (`key_of`) : une affiche est téléchargée pour chaque taille (80, 160, 360, 500) et après chaque éviction (64 Mio) | jaquettes immédiates au retour sur un écran, moins de réseau | ★★ | M (cache des JPEG compressés par adresse, ~16 Mio ≈ 250 affiches) | non (mesure console ensuite) |
| P5 | Démarrage à 120 Hz : le pilote passe 1,7 s à changer de mode vidéo (`[ps5-prepare-startup] video_ms=1701` ; 10 à 45 ms à 60 Hz), la 1re image reste figée 1,7 s après le splash. Les étapes ne sont pas horodatées : durée totale du démarrage inconnue | sans doute incompressible (TV) ; cacher le splash après la 2e image ; horodater les étapes | ★ | F | oui |
| P6 | `art::frame()` et `art::busy()` parcourent tout le cache à chaque image (3 passes et un tri) | négligeable aujourd'hui, à surveiller avec des milliers de titres | ★ | F | non |

### Mémoire

| # | Constat (preuve) | Gain | Int. | Diff. | Console |
|---|---|---|---|---|---|
| M1 | **Grosses bibliothèques (même mécanisme qu'Emby)** : `/api/v3/movie` et `/api/v3/series` arrivent en entier (texte + arbre cJSON, 4 à 8 fois le texte), plafond `kMaxBody` 64 Mio. Les fixtures font 720 o par film ; un vrai Radarr fait plutôt 3 à 6 Ko : 5 000 films ≈ 15 à 30 Mo de JSON → pic estimé à 80–200 Mio. La liste est demandée **trois fois** séparément (Bibliothèque `library.cpp:253-271`, Ménage `cleanup.cpp:54-64`, tailles de l'onglet Jellyfin `jellyfin_stats.cpp:101-112`) et gardée en double (parts + fusion). Le journal ne donne pas la taille des réponses | évite le prochain plantage « grosse config » | ★★★ | M (taille dans le journal, cache partagé, garde-fou avec message clair) à D (parseur en flux) | mesure sous Linux (proxy + `LD_PRELOAD`, comme pour Emby), puis console |
| M2 | Les entrées « Failed » du cache d'images et `SeerrTitles` ne sont jamais purgées | aucun en pratique | ★ | F | non |

### Réseau

| # | Constat (preuve) | Gain | Int. | Diff. | Console |
|---|---|---|---|---|---|
| R1 | Serveur lent ou éteint : connexion 5 s par adresse (`kConnectMs`), délai 10 s, 4 workers partagés par tous les services. Au démarrage, Sonarr a répondu en 5,5 à 6,7 s (`console-20261014-192427.log:72-87`) et ses 3 requêtes ont occupé 3 workers sur 4 | les autres services s'affichent sans attendre le lent | ★★★ | M (au plus 2 requêtes en vol par hôte ; hôte « injoignable » 30 s après un échec de connexion) | Linux (port filtré) puis console |
| R2 | `Connection: close` à chaque requête (`http_wire.cpp:129`) et résolution DNS à chaque requête (`socket_transport.cpp:82`, pas de cache) : en HTTPS derrière un reverse proxy avec un nom (le cas du § 2 bis), chaque jaquette paie DNS + TCP + poignée TLS. Jamais mesuré (Vivian est en HTTP sur IP). Seule requête HTTPS des journaux : Glances en **34,7 s** pour un délai de 10 s (`console-20261013-161003.log:268`, build sans détail TLS) : la poignée mbedTLS ne vérifie le délai qu'en attente réseau | HTTPS utilisable pour la communauté | ★★ | F (cache DNS) / D (keep-alive) | oui (un proxy HTTPS de test) |
| R3 | Pas de `Accept-Encoding: gzip` : grosses listes non compressées (voir M1) | réseau ÷ 5 à 10 sur ces listes | ★ | M (miniz) | non |
| R4 | Tailles des bibliothèques (onglet Jellyfin) : `ask_arr()` ne redemande jamais, même avec △ (`jellyfin_stats.cpp:91`) ; après un échec, à confirmer : tailles absentes jusqu'au redémarrage | petite correction | ★ | F | non (`REGIE5_MOCK_FAIL`) |

Rafraîchissements : seulement écran visible (5 s file d'attente et sessions, 10 s tâches,
5 s Glances) avec garde contre les doublons : rien à reprendre.

### Robustesse

| # | Constat (preuve) | Gain | Int. | Diff. | Console |
|---|---|---|---|---|---|
| B1 | = M1 : le plus probable des prochains plantages | | ★★★ | | |
| B2 | Parseurs testés sur des fixtures valides seulement : aucun test avec JSON tronqué, `null`, chaîne au lieu d'un nombre, nombre géant (`json::i64` fait `llround` puis un cast : comportement indéfini sur 1e300 ou NaN, `json.cpp:27`) | moins de plantages sur des serveurs inhabituels | ★★ | F (un test « chaque parseur × corps hostiles », un fuzz court sous ASan) | non |
| B3 | Une seule police (Inter : latin, grec, cyrillique) : un titre japonais, chinois, coréen, arabe ou hébreu (anime, Découvrir, noms d'utilisateurs) s'affiche vide | lisible pour toute la communauté | ★★ | M (police de repli sous-ensemble, budget d'atlas) | oui (mémoire) |
| B4 | Firmware : le journal ne note pas la version du système ; le plantage `0xa8` du 15 octobre reste inexpliqué | diagnostics des issues plus rapides | ★ | F | oui |

### Ergonomie (à compléter par Vivian à la manette)

| # | Constat | Int. | Diff. | Console |
|---|---|---|---|---|
| E1 | 1,7 s figé au démarrage à 120 Hz (P5) | ★ | F | oui |
| E2 | Pendant les 5 à 10 s d'attente d'un serveur lent (R1), l'écran montre des squelettes sans dire qui est lent : nommer le service après 3 s | ★★ | F | non |
| E3 | Pas de mesure sur ce qui est lent ou confus à la manette : une séance de 15 min avec relevé par Vivian (écrans trop chargés, textes) | ★★ | — | oui |

### Code

| # | Constat (preuve) | Int. | Diff. |
|---|---|---|---|
| C1 | `services.cpp` (855 lignes) : 22 branches `if (m_cfg.plex())` ; Emby est un drapeau dans `jellyfin.cpp`, relu dans `art.cpp`, `format.cpp`, `session_diag.cpp`, `cleanup.cpp`. Piste : une interface « serveur multimédia » (Jellyfin, Emby, Plex) ; tests de fixtures déjà là pour les trois | ★★ | D |
| C2 | Fichiers gros : `library.cpp` 1 008, `main_ps5.cpp` 947 (sonde, bench et stress dans la boucle principale), `discover.cpp` 909, `test_api.cpp` 1 371 | ★ | M |
| C3 | Trois chargements des listes *arr (M1) : un cache partagé dans `Context` | ★★ | M |
| C4 | Pas de test sur `art` (plafond de décodage, éviction, budget : la leçon d'Emby), ni sur les délais de `socket_transport` (serveur muet, réponse coupée) | ★★ | F |

### Recommandation : les 5 premiers

1. **M1 + C3** — mesurer la taille des listes (taille dans le journal, mesure sous Linux avec un
   gros Radarr simulé), un seul cache partagé, garde-fou avec message clair.
2. **P1** — priorité des threads de travail sous celle du thread principal ; avant/après sur
   console (Demandes, Bibliothèque).
3. **B2 + C4** — tests hostiles des parseurs, tests de `art` et des délais HTTP (Linux seul).
4. **R1 + cache DNS de R2** — limite par hôte, hôte injoignable mis de côté, avec E2.
5. **P2 + P3** — budget de glyphes et d'envois de textures par image à 120 Hz.

Ensuite : P4 (cache des JPEG), R2 keep-alive après une mesure HTTPS, B3, C1.

## 2 quattuordecies. Version 0.7.0 — optimisation (ouverte le 18 octobre 2026)

Décidé par Vivian : tous les points de l'audit (§ 2 terdecies), dans cet ordre, en autonomie.
Un patch et un message de commit par point (`build/patches-23/`, arbre de travail déjà dans
l'état final : `git apply --cached`). Lot 1 sous Linux ; lot 2 sur console ; un seul build de
test console (journal UDP) à la fin des lots 1 et 2 ; pas de release sans accord. Lot 3 : B3
seulement après validation des tailles, C1 en dernier (comportement et captures identiques).
Mesures et fichiers intermédiaires dans le scratchpad.

### Lot 1 (Linux)

- [x] **M1. Grosses bibliothèques** (patch 01) — mesuré sous Linux avec une bibliothèque
      synthétique de **5 000 films** (18,5 Mio de JSON, ~3,9 Ko par film comme un vrai Radarr
      v5) et **500 séries** (2,0 Mio), compteur de tas en `LD_PRELOAD` :

      | | avant | après |
      |---|---|---|
      | lecture des 5 000 films (`arr::parse_movies`) | pic **113 Mio** | pic **24 Mio** (texte 18,5 + titres 4,6) |
      | lecture des 500 séries | pic 11,5 Mio | pic 2,6 Mio |
      | app entière en `--mock` (Accueil, Bibliothèque, Ménage, Jellyfin) | pic **130 Mio** | pic **48,5 Mio** |
      | app entière, fixtures normales | 14,0 Mio | 13,8 Mio |

      (1) `json::each` lit une longue liste **un élément à la fois** (un arbre cJSON fait ~5 fois
      le texte) : films et séries Radarr/Sonarr, `Items` Jellyfin/Emby (bibliothèques, ce que
      chaque utilisateur a vu, index de Jelly5), `Metadata` Plex. (2) **Un seul cache partagé**
      (`ui/arr_lists`, dans `Context::lists`) : Bibliothèque, Ménage et tailles de l'onglet
      Jellyfin lisaient chacun les mêmes listes (et la Bibliothèque les gardait en double) ;
      avec une instance, plus aucune copie (`cleanup::Library` pointe sur la liste partagée) ;
      △ redemande sans doublon en vol ; un titre supprimé par le Ménage n'y revient jamais.
      (3) **Limite** `api::set_list_limit` (Linux 64 Mio ; PS5 32 / 24 / 16 Mio de texte selon le
      tas de 256 / 192 / 128 Mio, soit ~8 000 films) : au-delà, la réception s'arrête
      (`Response::TooLarge`) et l'écran dit « Radarr : Bibliothèque trop grande pour Régie5
      (plus de 32 Mo) » (FR/EN/ES/DE), au lieu d'un plantage ; `REGIE5_MOCK_FAIL=radarr:toolarge`
      pour le voir. (4) **Taille de chaque réponse** dans le journal HTTP (PS5 et
      `REGIE5_HTTP_LOG=1`) : « -> 200 (59 ms, 18.5 MiB) ». Vérifié : 80 tests ; captures
      data / multi / mixed / emby / plex / loading / offline / empty / unauthorized identiques
      à la 0.6.0 (hors horloge, heures relatives et curseur). Constat de l'audit R4 (tailles
      jamais redemandées) : faux, △ les redemandait déjà ; c'est maintenant le cache partagé.
- [x] **B2 + C4. Réponses abîmées, comportements indéfinis, tests** (patch 02) — test
      `every_parser_survives_hostile_answers` : les **62 parseurs** (Sonarr, Radarr, Seerr,
      Jellyfin, Emby, Plex, Glances) × 30 versions de leur fixture (coupée au tiers, à moitié,
      aux deux tiers, à un octet de la fin ; nombres à 1e300, −1e300, 9,3e18, 1e400 et −1e400
      (infini) ; chaînes changées en nombres géants, nombres en chaînes, toutes les valeurs à
      `null`, objets et listes échangés, chaînes vides ou de 5 000 caractères ; et du bruit :
      vide, HTML, `[[[[…`, 1 000 crochets, octets invalides), soit plus de 1 800 lectures.
      **Sous UBSan, la 0.6.0 s'arrête dès la première file d'attente** (`arr.cpp:567`, 1e300
      converti en entier : comportement indéfini) ; corrigé, plus rien sous ASan + UBSan.
      Corrections : `json::whole` (arrondi refusé hors de `int64_t`, infini, NaN : la valeur par
      défaut) pour `json::i64` (l'ancien `json.cpp:27`), `json::integer` (hors d'un `int` : défaut),
      le `whole` et le `text` de Plex ; `json::num` et le `number` de Plex refusent l'infini ;
      `json::int_of` remplace `atoi` (identifiants TMDB/TVDB écrits en texte, Plex et Jellyfin) ;
      taille de file d'attente lue en entier ; progression des tâches bornée à 0–100 (Jellyfin,
      Plex) ; port d'adresse limité à 5 chiffres, statut HTTP lu sur 3 chiffres exactement ; index
      `from` de la page du téléphone borné. Tests nouveaux : `json_numbers_out_of_range…` ;
      **cache d'images** (nouvel exécutable `regie5_ui_tests`, rendu factice qui compte les
      textures, `tests/gfx_stub.cpp`, images PNG fabriquées par le test) : taille demandée,
      jamais agrandie ; une image de 5 000 × 3 000 jamais décodée (carte dessinée) ; échec gardé
      jusqu'à △ ; budget tenu (les plus anciennes rendues) ; `forget` rend tout ; **délais HTTP**
      du transport de la PS5 contre des serveurs locaux qui se taisent, envoient un octet toutes
      les 100 ms, coupent la réponse, refusent la connexion, parlent HTTP au lieu de TLS ou
      négocient TLS au compte-gouttes : chaque requête finit dans son délai (+ 0,6 s), l'annulation
      en moins d'une seconde. Ces tests passent aussi sur le code actuel : la négociation TLS
      respecte déjà le délai **pendant les attentes réseau** ; les 34,7 s du 13 octobre (build
      d'avant la 0.2.0, sans détail TLS) restent à expliquer (R2).
      ASan + UBSan : `scratchpad/asan.sh` (copie de l'arbre, `-fsanitize=address,undefined`).
- [x] **R1 + cache DNS. Un serveur lent ou éteint ne bloque plus les autres** (patch 03) —
      (1) `http::Pool` : **au plus 2 requêtes en vol par service** (`Request::server` = l'adresse
      du service, mise par `Services` et par le cache d'images ; sinon schéma + hôte + port) : un
      Sonarr lent n'occupe plus 3 des 4 workers, les autres services passent. (2) **Mise de
      côté** : un service qui n'a pas répondu à la connexion (refus, pas de route, délai de
      connexion) est mis de côté **10 s** ; ses requêtes échouent tout de suite (« Injoignable »)
      au lieu d'attendre 5 s chacune ; la première réponse le remet en service ; « Tester » et
      « Tout tester » passent quand même (`Request::fresh`). Un délai de connexion dépassé est
      maintenant « injoignable » (avant : « pas de réponse à temps », faux), aussi avec libcurl.
      (3) **Le service lent est nommé** en bas à gauche après 3 s : « En attente de Sonarr,
      Radarr (5 s)… » (FR/EN/ES/DE ; `Pool::waiting`, `Services::slow`), vu avec
      `REGIE5_MOCK_DELAY_MS=8000`. (4) **Cache DNS** du transport de la PS5 : un nom trouvé est
      gardé 5 min, un nom introuvable 30 s (sans serveur DNS, chaque essai peut prendre plusieurs
      secondes) ; les adresses numériques ne sont jamais cherchées. Tests : tour de chaque
      serveur (le lent jamais plus de 2, l'autre servi pendant ce temps), mise de côté (essayé une
      fois, puis réponse immédiate ; `fresh` passe), service lent nommé, nom cherché une fois
      (trouvé ou non). Captures data / offline / mixed / multi identiques.
- [x] **R2. Délais partout, connexions HTTPS gardées** (patch 04) — (1) **Délai** : la
      connexion ne dépasse plus le délai de la requête (avant : 5 s par adresse, en plus) ; la
      négociation TLS vérifie le délai et l'annulation **à chaque étape**, pas seulement pendant
      les attentes réseau (le travail de mbedTLS compte aussi). Les 34,7 s du 13 octobre ne se
      reproduisent pas : toutes les attentes respectaient déjà le délai sur le code actuel (tests
      du patch 02), il venait d'un build d'avant la 0.2.0 ; à revoir sur console avec la ligne
      « connected …, TLS … » (build de test, étape HTTPS). Le mode restartable de mbedTLS (ECC
      découpé) a été écarté : il ne couvre que ECDHE-ECDSA, pas les certificats RSA courants.
      (2) **Connexions gardées, HTTPS seulement** (le gain est la négociation TLS ; en HTTP sur
      le réseau local, une connexion coûte ~1 ms : rien de changé, `Connection: close` comme
      avant). Gardée seulement si sûr : réponse HTTP/1.1, sans `Connection: close`, fin connue
      (longueur ou morceaux), rien après ; au plus 2 par serveur, 10 s ; une connexion que le
      serveur a fermée est écartée avant usage ; **une lecture** envoyée sur une connexion gardée
      que le serveur a lâchée est renvoyée une fois sur une neuve ; **une action (POST, DELETE)
      ne prend jamais une connexion gardée** (elle ne doit jamais partir deux fois). Journal :
      « (42 ms, 550 B: kept connection) ». Tests : 3 lectures sur 1 connexion (longueur et
      morceaux), jamais gardée avec `Connection: close`, HTTP/1.0 ou des octets en trop, une
      action sur une connexion neuve, une connexion lâchée remplacée sans erreur, connexion vers
      une adresse muette (192.0.2.1) arrêtée au délai. Vu en vrai TLS sous Linux (serveur
      Python HTTPS, transport de la PS5) : 6 requêtes, **1 seule connexion**, 1 négociation.
      ASan + UBSan : rien.
- [x] **P4. Une jaquette reçue n'est plus redemandée** (patch 05) — le cache d'images garde
      aussi **les fichiers tels qu'ils sont arrivés** (JPEG, PNG : 30 à 80 Kio une affiche), par
      adresse : la même affiche à une autre taille (cartes 80, 160, 360, 500 px, copie floutée du
      fond), ou de nouveau après que sa texture a été rendue, est **décodée depuis le fichier
      gardé** sur un worker (`Pool::submit_local`), sans requête ; deux tailles demandées dans la
      même image n'envoient qu'une requête (la seconde attend le fichier). Budget : PS5 12 / 8 /
      4 Mio selon le tas (≈ 250 affiches), Linux 32 Mio, les moins récents partent ; un fichier de
      plus de 1 Mio ou qui ne se décode pas n'est pas gardé ; config.json change → oubliés.
      Journal console du 13 octobre : 576 requêtes d'images pour 236 adresses (59 % de redites) ;
      à remesurer sur le build de test. Test `art_asks_each_file_once` (2 tailles dans la même
      image : 1 requête ; 3ᵉ taille et copie floutée : 0 ; texture rendue puis redessinée : 0 ;
      service oublié : redemandé ; budget des fichiers tenu). Captures `--demo` (vraies affiches)
      identiques à la 0.6.0. Aussi : dans `http::Pool`, le compte par serveur se fait sous le même
      verrou que le choix de la requête (une course pouvait laisser passer une 3ᵉ requête).

### Lot 2 (console)

- [x] **P1. Threads de travail sous le thread principal** (patch 07 ; **mesuré : pas gardé**,
      voir l'analyse de l'A/B ci-dessous) — l'audit a vu des images de 90 à 173 ms que rien de mesuré n'explique
      pendant que les workers parsent et décodent. On ne sait pas comment la console ordonne
      ses threads : le build de test le **journalise** au démarrage (« threads (start): main:
      policy …, priority … (range …), CPUs … », idem pour un worker) et **essaie** d'abaisser
      les 7 workers (`ps5::lower_workers`) : échelle de la console détectée (256 à 767, plus
      grand = moins prioritaire : le worker passe 32 sous le principal), sinon échelle POSIX
      (un cran sous le principal) ; relu après coup et journalisé ; rétabli à la demande. Les 5
      fonctions sont des imports **faibles** (un import absent reste NULL sans planter, vérifié
      dans l'ELF). Pas activé hors du test A/B tant que la mesure ne l'a pas justifié.
      **Test A/B automatique** (build de développement, fichiers `stress` et `ab` dans le
      dossier du titre) : le tour du mode stress tourne en boucle, le 1ᵉʳ tour chauffe, puis
      chaque tour **repart à froid** (jaquettes et textes oubliés, glyphes préchauffés comme au
      démarrage) sous **A** (comme la 0.6.0), **B** (budgets P2 + P3) ou **C** (budgets + workers
      abaissés), à tour de rôle ; une ligne par tour : images, moyenne, pire, **images manquées à
      120 Hz (> 12,5 ms)**, > 17,5 / 25 / 50 ms, glyphes et pire image de texte, envois et pire
      image d'envois. Les requêtes restent en GET seulement (mode stress).
- [x] **P2 + P3. Budgets par image** (patch 06 ; **mesuré : gardés**, voir l'analyse de l'A/B) —
      **P2** : une image ne passe pas plus de **35 % de sa durée** (2,9 ms à 120 Hz, 5,8 ms à
      60 Hz) à mettre en page et rastériser des textes nouveaux ; au-delà, les textes nouveaux
      suivants sont dessinés à l'image suivante (leur largeur est juste quand même, la mise en
      page autour ne bouge pas ; l'app redessine tant qu'il en reste). Constat de l'audit : 81 à
      87 glyphes en 9 à 15 ms à l'ouverture d'un écran. **P3** : envois de jaquettes au GPU dans
      **25 % de l'image** (2,1 ms à 120 Hz), une au moins, trois au plus comme avant (pire relevé
      10,5 ms). Sur PS5 seulement (`set_frame_budgets`, après la première image, selon la
      fréquence obtenue) ; Linux sans budget (captures inchangées). Tests : 12 textes avec un
      budget minuscule → un par image, largeur juste, `text_waiting` ; 3 jaquettes avec un budget
      minuscule → une par image ; sans budget, tout dans la même image.
- [x] **P5. Démarrage figé 1,7 s à 120 Hz : rien de simple, non codé** — les journaux du
      pilote montrent que ces 1,7 s sont le **changement de mode vidéo** (`[ps5-prepare-startup]
      video_ms=1701` à 120 Hz, 10 à 46 ms à 60 Hz), fait par le pilote à la 2ᵉ image : l'écran
      se resynchronise, il n'y a rien à dessiner pendant ce temps. Le seul levier (Réglages ›
      Affichage › 60 Hz) existe déjà. Vu au passage : le test `&getaddrinfo != nullptr` du
      démarrage est un import ordinaire (non faible), que le compilateur peut considérer toujours
      vrai ; sans effet aujourd'hui (toutes les consoles vues l'exportent), à rendre faible un
      jour comme ceux de P1.
- [x] **Build de test console `opt-1`** (lots 1 et 2, patches 01 à 07) — build de
      développement, version affichée `0.7.0-test1`, journal UDP vers le laptop (192.168.1.15) ;
      `build/ps5/opt-1/PPSA99520.zip` (sha256
      `ce4dc9c482865429be13c8be6896ca507571bdf4ac680dcbfbd6fd667a845436`) ; les deux fichiers du
      test A/B dans `build/ps5/opt-1/ab-files/` (`stress` = 12, `ab` vide) ; marche à suivre
      `build/ps5/opt-1/ESSAIS.txt` (25 min : installer, utilisation normale avec les budgets,
      R1 facultatif en arrêtant Sonarr, A/B automatique de 12 min sans toucher la manette, puis
      **supprimer `stress` et `ab`**). Vérifié : les 7 patches appliqués dans l'ordre sur la
      0.6.0 redonnent l'arbre de travail ; compilation PS5 à chaque point.
- [x] Vivian (18 octobre 2026) : patches 01 à 08 commités et poussés ; essai `opt-1` fait
      (journal `build/ps5/logs/console-20261018-175142.log`) : A et B OK (rien de différent, les
      jaquettes reviennent tout de suite), C allé au bout sans plantage ; R1 avec Sonarr arrêté
      pas essayé ; `stress` et `ab` supprimés, 0.6.0 réinstallée.
- [x] **Analyse de l'A/B** (120 Hz, 4K, 9 tours comptés après le tour d'échauffement, chacun à
      froid) :

      | | A (comme la 0.6.0) | B (budgets P2 + P3) | C (B + workers abaissés) |
      |---|---|---|---|
      | images manquées (> 12,5 ms), tours 1 / 2 / 3 | 5 / 10 / 8 | **0 / 0 / 0** | 1 / 0 / 0 |
      | > 17,5 ms | 2 / 2 / 2 | 0 / 0 / 0 | 0 / 0 / 0 |
      | pire écart | 24,6 / 25,4 / 25,4 ms | **11,7 / 12,0 / 12,5 ms** | 13,2 / 11,1 / 12,0 ms |
      | pire image de texte | 11,7 / 19,2 / 19,1 ms | 8,3 / 7,1 / 7,1 ms | 7,0 / 7,2 / 7,2 ms |
      | pire image d'envois | 5,0 / 5,9 / 7,1 ms | **3,6 / 3,6 / 3,6 ms** | 3,6 / 3,6 / 3,6 ms |
      | moyenne | 8,33 / 8,28 / 8,28 ms | 8,25 / 8,28 / 8,28 ms | 8,33 / 8,28 / 8,27 ms |

      **Décision : P2 et P3 gardés** (déjà actifs par défaut depuis le patch 06) : plus aucune
      image manquée à 120 Hz sur les tours B, le pire écart passe de ~25 ms (3 images) à ~12 ms.
      Un texte seul peut encore dépasser le budget (le premier d'une image passe toujours :
      7 à 8 ms), sans image manquée. **P1 pas gardé** : C ne fait pas mieux que B (1 image
      manquée sur 3 tours contre 0). Le journal dit pourquoi : le thread principal et les
      workers ont **la même politique (1) et la même priorité (700, échelle 256 à 767)** ;
      l'abaissement à 732 marche (relu 732) mais ne change rien : les saccades venaient du
      travail fait dans l'image (textes, envois), pas des workers. Affinité : non lisible
      (`pthread_getaffinity_np` : erreur 34, taille d'ensemble refusée) ; sans objet. Le code de
      mesure et le test A/B restent (builds de développement seulement, aucun effet en release).
      **Partie B du même journal** : `/api/v3/movie` de Vivian = **2,8 Mio** (823 ms), lu **une
      seule fois** (cache partagé) ; `/api/v3/series` 188 Kio ; images : **149 requêtes pour 147
      adresses (1 % de redites, contre 59 % le 13 octobre)** ; tas : pic 32,0 Mio.

### Lot 3

- [x] **B3. Polices non latines** (patch 10 ; validé par Vivian le 18 octobre 2026 :
      sous-ensemble CJK dans le zip, chargé à la demande, □ pour l'introuvable, arabe et hébreu
      écartés en V1). `scripts/make-cjk-font.py` fait `assets/fonts/NotoSansCJK-Subset.otf`
      **sans rien télécharger** : Noto Sans CJK JP Regular 2.004 du système (`fonts-noto-cjk`),
      la bibliothèque de sous-ensemble de HarfBuzz (`libharfbuzz-subset0`, par `ctypes`), les
      jeux de caractères des codecs de Python : kana, ponctuation CJK, formes pleine chasse,
      idéogrammes de GB 2312, JIS X 0208 niveau 1 et **Big5 niveau 1** (traditionnels, ajoutés
      puisque la taille le permettait), 2 350 hangul de KS X 1001 ; tables de mise en forme et
      hints retirés (l'app ne fait pas de mise en forme : avec elles, 18 294 glyphes et 4,2 Mio).
      Résultat : **12 378 glyphes, 2,30 Mio** (9 237 idéogrammes), licence OFL
      (`assets/fonts/NotoSansCJK-LICENSE.txt`, `THIRD_PARTY_NOTICES.md`). `font.cpp` : la police
      n'est lue qu'au **premier caractère absent d'Inter** (2,3 Mio de tas, jamais pris sinon ;
      une ligne « font: … read » dans le journal), un seul poids, épaissi par FreeType pour
      SemiBold et Bold ; ce que ni Inter ni elle n'ont devient **□** (celui d'Inter) ; les
      caractères sans forme (espace de largeur nulle, sélecteurs de variante, contrôles) ne
      dessinent rien. **Correction de l'audit** : avant, un caractère absent n'était pas sauté
      mais dessiné en `.notdef` d'Inter (une case pleine avec un « ? »). Le crénage d'Inter ne
      s'applique qu'entre glyphes d'Inter. Zip PS5 9,7 → **11,7 Mo**, dossier 44 → 47 Mio.
      Vu sous Linux (fixtures copiées avec des titres japonais, chinois, coréens, thaï et arabe) :
      CJK dessinés, thaï et arabe en □ ; captures latines identiques. Test
      `fonts_fall_back_for_cjk_and_box_the_rest` (latin sans lecture du repli ; あ ア 日 这 這 한
      dessinés en Regular et Bold ; thaï, arabe = □ ; largeur nulle sans forme ; pas de crénage
      entre polices). À dire dans le README et les notes de la 0.7.0. Proposition d'origine : Mesuré sur
      les polices du laptop (FreeType) : Inter (l'app) n'a ni kana, ni idéogrammes, ni hangul, ni
      arabe, ni hébreu (2 937 glyphes, 0,4 Mio par graisse, 4 graisses). Noto Sans CJK (toutes les
      langues CJK) : **18,6 Mio** par graisse, 65 535 glyphes, ~300 o par glyphe, 20 976
      idéogrammes, 189 kana, 11 172 hangul ; Droid Sans Fallback : 3,8 Mio, 20 902 idéogrammes,
      **pas de hangul** ; Noto Sans Arabic 0,24 Mio, Hebrew 27 Kio, Thai 38 Kio. Aujourd'hui un
      caractère absent est **sauté** (le titre paraît tronqué, sans signe).
      **Proposition légère** : (1) **repli chargé à la demande** : la police de repli n'est lue
      qu'au premier caractère absent d'Inter (0 octet de tas pour qui n'en voit jamais),
      une seule graisse (le gras simulé par FreeType) ; (2) **sous-ensemble CJK** fait une fois
      par un script (Noto Sans CJK, OFL) : kana, ~7 000 idéogrammes courants (GB 2312, qui
      contient les jōyō japonais) et 2 350 hangul courants (KS X 1001) ≈ 9 600 glyphes ≈ **2,8 Mio**
      (zip +2,3 Mio environ, 44 → ~47 Mio), **+2,8 Mio de tas** une fois chargé ; atlas : un
      idéogramme en 4K tient ~80 px, une page de 16 Mio en garde ~600, le budget actuel (48 Mio)
      ~1 800 avant de repartir à neuf : suffisant pour une bibliothèque ; (3) **arabe, hébreu,
      devanagari : pas en V1** (sens de lecture de droite à gauche et lettres liées : il faudrait
      HarfBuzz, plusieurs centaines de Kio et du travail) ; (4) dans tous les cas, un caractère
      introuvable s'affiche en **□** au lieu de disparaître. Variante plus légère encore : (2) en
      « paquet de polices » facultatif (un dossier à copier), le zip ne change pas.
      **À valider : (2) dans le zip ou en paquet facultatif ; sous-ensemble proposé ; (3) écarté.**
- [x] **C1. Interface commune Jellyfin / Emby / Plex** (patch 11, après l'analyse de l'A/B) —
      `api::MediaServer` (`src/api/media_server.{h,cpp}`) : en-tête d'authentification, en-têtes
      envoyés à chaque requête, lecture des refus (session terminée, pas le propriétaire, jeton
      refusé), test de connexion, chemin d'image et chemin de téléchargement d'image, et les 17
      appels du serveur multimédia (sessions, tâches, ajouts récents, journal, bibliothèques,
      titres et compte d'une bibliothèque, utilisateurs, éléments, encodage, ce qu'a vu un
      utilisateur, index de Jelly5, Id du serveur, arrêt d'une lecture, scan). Trois classes :
      **Jellyfin**, **Emby** (dérivée : son en-tête, `maxWidth` pour les images, la date des
      séries en deux appels) et **Plex** ; le code de chaque branche a été **déplacé tel quel**.
      `Services` garde les trois objets toute sa vie (une réponse en vol après un changement de
      config.json retrouve le sien) et choisit par `media()`. `services.cpp` : **855 → 533
      lignes, 22 → 0** branches `plex()` / `emby()` (une seule sélection reste, dans `media()`) ;
      `Services::call` dans un en-tête interne (`services_call.h`) ; le cache d'images demande
      le chemin au serveur (`media_fetch_path`) au lieu de tester `emby()`. Quick Connect reste
      dans `Services` (Jellyfin seul, appelé même service éteint). Les écarts d'affichage propres
      à chaque serveur dans l'interface (`format.cpp`, `session_diag.cpp`, `settings.cpp`…) ne
      changent pas : ce sont des textes différents, pas du code en double.
      **Comportement inchangé, vérifié** : 99 tests verts (Emby et Plex sur leurs fixtures, Quick
      Connect) ; nouveau test `media_servers_read_refusals_their_own_way` (Jellyfin par clé,
      Emby, Plex non propriétaire / jeton refusé / sans jeton, arrêt refusé au propriétaire =
      Plex Pass), **passé aussi sur le code de la 0.6.0** ; captures de 9 états (data, emby, plex,
      multi, mixed, offline, unauthorized, quickconnect, old) : 119 identiques, le reste = heure,
      curseur, heures relatives, et deux écarts dus à l'ordre d'arrivée des réponses (focus de
      l'Accueil Emby, ordre de deux alertes de la même minute) qui **varient déjà d'un lancement
      à l'autre en 0.6.0** : sur 3 lancements de chaque version, mêmes images. Compilation PS5,
      ASan + UBSan : rien.
- [ ] **Séance ergonomie** : **pas pour la 0.7.0** (décision de Vivian, 18 octobre 2026) ; la
      liste reste pour plus tard. À observer, en notant
      ce qui gêne (même un détail) :
      1. Du lancement au premier écran utile : combien de secondes, qu'est-ce qui arrive en dernier ?
      2. Onglets (L1/R1, barre du haut) : sait-on toujours où est le focus (la goutte) ?
      3. Bibliothèque : retrouver un film précis — combien de temps ? L2/R2 par lettre compris ?
      4. Les pilules en haut (Séries, Films, Guide…) : passer de la grille aux pilules et revenir.
      5. Options (M) : devine-t-on qu'il existe ? le « M Options » du bas se voit-il ?
      6. Textes coupés par « … » qui gênent (titres, messages d'erreur) à 3 m de la TV.
      7. Onglet Jellyfin : qu'est-ce qu'on regarde vraiment ? qu'est-ce qui est en trop ?
      8. Confirmations (focus sur Annuler) : rassurant ou pénible ?
      9. ○ Retour : revient-on toujours là où on s'attend ?
      10. Messages d'erreur (un service arrêté) : compris sans réfléchir ?
      11. Nouveau en 0.7.0 : « En attente de Sonarr (5 s)… » en bas à gauche : utile, gênant ?
      12. Petites infos (pastilles, dates, tailles) : lisibles de loin ?

### Pour publier la 0.7.0 (préparé le 18 octobre 2026)

Patch : `build/patches-23/12-regie5-0.7.0` (arbre de travail dans l'état final : `--cached`).

1. [x] **Numéros** : `contentVersion` `00.007.000`, `project(regie5 VERSION 0.7.0)` ; profil
       `param.json` inchangé (`games-hz120-0.3.1`, accepté par `check-param.py --release`).
2. [x] **Nouveautés** de la série 0.7 (FR/EN/ES/DE, vues sans débordement) : plus fluide à
       120 Hz ; jaquette déjà reçue revenue tout de suite ; grandes bibliothèques et serveur lent ;
       titres japonais, chinois et coréens ; Réglages › Nouveautés.
3. [x] **README** : interface plus fluide, grandes bibliothèques et serveurs lents, titres CJK
       (fonctionnalités) ; autres écritures en □ et bibliothèque trop grande (limites) ; Noto
       Sans CJK (crédits). `INSTALL.txt` inchangé (rien de nouveau à installer).
4. [x] **Notes de version** `build/publish/release-notes-v0.7.0.md` (anglais, chiffres mesurés
       sur la console : 5 à 10 images manquées par tour → 0, pire écart 25 → 12 ms ; 59 % → 1 %
       d'images redemandées ; 113 → 24 Mio pour lire 5 000 films).
5. [x] **Recette console** `docs/RECETTE-0.7.0.md` (23 étapes : mise à jour, Nouveautés,
       Accueil, Bibliothèque avec défilement rapide, Activité, Demandes › Découvrir, Jellyfin,
       Réglages, page du téléphone ; aucune écriture).
6. [x] **Zip** : `scripts/release-ps5.sh` → `build/ps5/release-0.7.0/upload/PPSA99520.zip`
       (39 fichiers : ceux de la 0.6.0, plus la police CJK et sa licence ; ni `config.json` ni
       `test-config`, ni `stress` ni `ab`), sha256
       `c54af57f7c6fe7ff8a2322be5a09e0620f51c8c10473d5b58908fa84d6a3e023`. Vérifié dans
       `llvm-pie.elf` : version `0.7.0`, mêmes chaînes « -test » / « -dev » que la 0.6.0 publiée
       (celles de bzip2 et de Mbed TLS seulement), pas d'adresse de journal. Tests Linux : 99 verts.
7. [x] Vivian : recette `docs/RECETTE-0.7.0.md` avec ce zip.
8. [x] Vivian : patch `12-regie5-0.7.0` de `build/patches-23/` (`git apply --cached`), commit
       « Régie5 0.7.0 », push.
9. [x] Vivian : tag `v0.7.0` et push du tag seul.
10. [x] Vivian : release GitHub « Régie5 0.7.0 » (texte = les notes, pièce jointe `PPSA99520.zip`,
        pas « pre-release »), empreinte vérifiée (`sha256sum`).
11. [x] Catalogue : la PR automatique du lendemain (`version` 0.7.0, `sha256`, `icon_url` en
        `/v0.7.0/ps5/app/sce_sys/icon0.png`) ; sinon le chemin manuel de `docs/RELEASING.md` § 3.
        Vu par Vivian le 19 octobre 2026 : homebrew.page affiche la 0.7.0 (fiche : `sha256`
        `c54af57f…`, `icon_url` en `/v0.7.0/`).
12. [x] **Nouvelle description sur le catalogue** (demandée par Vivian le 19 octobre 2026) :
        « A TV remote for your self-hosted media stack: Sonarr, Radarr, Seerr and Jellyfin, Emby or
        Plex at a glance, with request approval, playback diagnosis and an optional cleanup. »
        (174 caractères ; règles de `docs/metadata.md` du catalogue : 200 au plus, une phrase en
        texte brut, NFC, sans espace en bord). `build/publish/PPSA99520.json` mis à jour (état
        publié de la 0.7.0, catégorie `video`, nouvelle description). Vivian : PR manuelle depuis
        le fork `viviandsx/ps5-homebrew-catalog`, ne changeant que `description` dans
        `apps/PPSA99520.json` ; attendre « Validate submission » au vert et la fusion. Le README
        dit déjà « Jellyfin (or Emby, or Plex) » : rien à changer. **Fait** : PR #432 sur
        `blackbearreloaded/ps5-homebrew-catalog`, fusionnée le 19 octobre 2026.

## 3. En attente

| Sujet | Qui | Détail |
|---|---|---|
| Port 5555 (ufw) | Vivian | Gardé ouvert pour les tests console (décision du 17 octobre 2026). |
| PR Jelly5 | Vivian | Sans lanceur depuis une app, Régie5 ne s'en sert pas sur PS5 : la proposer ou non est une décision à part (`docs/JELLY5-PR.md`). |
| Ménage avec Plex | plus tard | Pas en 0.6.0 (décision de Vivian, 17 octobre 2026) : Plex ne montre pas les lectures en cours des autres comptes. Piste : **Tautulli** (optionnel, lecture seule, réseau local), qui enregistre chaque lecture par utilisateur (`docs/PLEX.md` § 2.10). |
| Taille du tas sur PS5 | 6.2 | 256 Mio accordé et l'affichage démarre (probe-5) : on garde 256 ; l'usage mesuré est ~20 Mio. En 6.2 : message visible à l'écran (ou notification système) si la mémoire est refusée, au lieu du seul journal. |

### Actions réelles

Aucune action réelle sans accord explicite de Vivian.

1. [x] **Scanner les bibliothèques** (Jellyfin) — fait une fois le 11 octobre 2026 depuis
   Régie5 : `POST /Library/Refresh` → 204, la tâche « Analyser la médiathèque » est passée
   en cours (0 %, puis 65 %), et la note « Scan des bibliothèques lancé » s'est affichée.
2. [x] **Arrêter une lecture** — 13 octobre 2026, sur console (recette 6.7).
3. [x] **Refuser** (Seerr) — 13 octobre 2026, utilisateur de test. **Approuver** : non testé
   volontairement (même appel, autre chemin).
4. [x] **Supprimer** (Ménage, Sonarr) — 13 octobre 2026, une série terminée. Radarr : non
   testé en vrai (même code, autre chemin).

---

## 3 bis. Pour la communauté

Régie5 est fait pour la communauté (PS5 jailbreakée + serveur auto-hébergé), pas pour une
seule installation. Ces règles s'appliquent à tous les choix. Chaque point est intégré quand
il touche ce qui est codé, sinon il est planifié ici.

| Point | Où on en est | Quand |
|---|---|---|
| HTTPS | Linux (libcurl) et PS5 (mbedTLS), `"insecure": true` pour un certificat auto-signé, « Certificat refusé » sinon | Fait (6.2 bis ; Linux corrigé en 0.3.0 c) |
| Sous-chemins de reverse proxy, `urlBase` des *arr | Testé derrière nginx (chemins retirés, HTTPS auto-signé), redirections signalées, `docs/CONFIG.md` | Fait (0.3.0 c) |
| Versions plus anciennes | Minimums vérifiés au test de connexion, message clair dans Réglages, tableau dans le README | Fait (0.3.0, a) |
| Plusieurs instances (Radarr 1080p + 4K…) | Config : objet **ou** liste nommée, `is4k` par instance ; tous les écrans parcourent toutes les instances | Fait (4 ter) |
| Fonctions destructives désactivées par défaut | — | 4 bis (B) : Ménage activé seulement dans Réglages, après un avertissement |
| Configuration sans FTP | Étudiée dans `FEATURES.md` (annexe) : clavier système, QR + page locale, Quick Connect Jellyfin | Décision en phase 6 |
| Ajouter une langue | Une langue = un fichier (`src/core/lang/`), `docs/TRANSLATING.md` | Fait (0.3.0, b) |
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
scripts/run-linux.sh --demo                # sans serveur, films libres avec affiches (captures)
scripts/run-linux.sh --mock                # sans serveur, sur fixtures/
scripts/run-linux.sh                       # avec ~/.config/regie5/config.json
scripts/run-linux.sh --lang en --mock      # en anglais
cmake --build build/linux && ctest --test-dir build/linux --output-on-failure
```

Variables de développement : `REGIE5_FPS=1` (temps d'image chaque seconde),
`REGIE5_SCRIPT="…"` (rejoue des touches, captures PPM), `REGIE5_HTTP_LOG=1`,
`REGIE5_MOCK_DELAY_MS`, `REGIE5_MOCK_FAIL=radarr:unreachable,…`, `REGIE5_MOCK_DIR=<dossier>` (autres données de `--mock`/`--demo`), `REGIE5_DISPLAY=3840x2160@60/120` (ce que répondrait une console : taille, fréquence obtenue, demandée),
`REGIE5_WORKER_STACK_KIB=64` (pile des threads de travail, comme sur PS5),
`REGIE5_HTTP=socket` (le client HTTP de la PS5 au lieu de libcurl, avec Mbed TLS pour `https://`),
`REGIE5_NO_LAUNCH=1` (une console qui ne lance pas d'app : « Chercher dans Jelly5 »),
`REGIE5_MOCK_OLD=sonarr,jellyfin` (versions trop anciennes en `--mock`),
`REGIE5_MOCK_EMBY=1` (Emby à la place de Jellyfin en `--mock`, `fixtures/emby/`),
`REGIE5_MOCK_PLEX=1` (Plex à la place de Jellyfin en `--mock`, `fixtures/plex/`),
`REGIE5_WELCOME=1` (l'assistant même avec une config ou en `--mock`),
`REGIE5_WHATSNEW=1` (l'écran « Nouveautés » au lancement, même en `--mock`),
`REGIE5_SHARE_LOG_CODE=1` (le code de la page du téléphone dans le terminal, pour un essai
par `curl`).
Plex de test : `scripts/plex-test-server.sh start`, puis `setup`, puis
`scripts/run-linux.sh --config build/plex-test/config.json` ; une lecture : `scripts/plex-test-play.sh transcode 4`.
Emby de test : `scripts/emby-test-server.sh start`, puis `setup`, puis
`scripts/run-linux.sh --config build/emby-test/config.json`.
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
| 13 oct. 2026 | Recette express (réduite) validée sur console : arrêt de lecture, refus, suppression réelle d'une série (Sonarr) ; POST Seerr `…/deleted` inopérant retiré. **Phase 6 validée.** Publication préparée. |
| 13 oct. 2026 | Publication préparée : version 0.2.0, zip au format du catalogue, README, INSTALL, modèles d'issues, fiche `PPSA99520.json`, audit du dépôt. |
| 13 oct. 2026 | **Phase 7** : dépôt public (historique neuf), release `v0.2.0` avec `PPSA99520.zip`, PR #244 au catalogue (contrôles verts, relecture en attente). `docs/RELEASING.md` : branches, ce qui est gelé, sortie d'une version. |
| 13 oct. 2026 | 0.3.0 : fixtures anonymes (titres et identifiants inventés, `anonymize_titles.py`). |
| 13 oct. 2026 | 0.3.0 : jeu de démo (`--demo`, films Blender sous CC BY), captures du README refaites. |
| 13 oct. 2026 | 0.3.0 a) : versions minimales par service, message dans Réglages, tableau du README. |
| 13 oct. 2026 | 0.3.0 b) : une langue = un fichier, `TRANSLATING.md` ; textes ConfigMissing / ConfigFromBackup remis à l'endroit. |
| 13 oct. 2026 | **Phase 7 terminée** : PR #244 fusionnée, Régie5 0.2.0 sur homebrew.page. `RELEASING.md` : le catalogue suit chaque release, aucune avant la 0.3.0 testée sur console. |
| 13 oct. 2026 | 0.3.0 c) : reverse proxy nginx (sous-chemins, HTTPS auto-signé, lecture seule) ; certificat refusé sous Linux, adresse avec chemin et redirections dans Réglages. |
| 13 oct. 2026 | 0.3.0 d) : pastilles de compteur sur Activité, Demandes, Jellyfin. |
| 13 oct. 2026 | 0.3.0 e) : signalements Seerr (lecture, résoudre, commenter ; `--mock` seulement). |
| 13 oct. 2026 | 0.3.0 f) : bibliothèques (nombres, taille d'après Radarr / Sonarr) et utilisateurs Jellyfin. |
| 13 oct. 2026 | 0.3.0 g) : 120 Hz (réglage, journal, banc de mesure) ; build `app-9` prêt pour la console. |
| 13 oct. 2026 | app-9 : 120 Hz refusé (`0x80290016`, sortie HDMI 2160p inchangée en 1440p), banc à refaire ; e) et f) vérifiés en lecture sur les vrais serveurs, Collections corrigées ; h) catégorie Multimédia ; build `app-10`. |
| 13 oct. 2026 | app-10 refusé au lancement (Multimédia sans `launchActivity`) ; profil de Jelly5 ; builds `app-10b` (Jeux) et `app-10c` (Multimédia). |
| 13 oct. 2026 | 120 Hz : Jelly5 refusé aussi sur cet écran ; banc par phases ; app-10c refusé, app-10d (profil Jelly5 entier) ; 0.3.0 préparée (version, README, notes, recette). |
| 13 oct. 2026 | app-10d démarre en Multimédia (profil Jelly5 entier) et obtient le 120 Hz ; 4K à 120 Hz : 90 images/s ; dépôt en Multimédia, 0.3.0 prête. |
| 13 oct. 2026 | Option A : Auto à 60 Hz en 0.3.0, 120 Hz à la main ; 0.3.1 : Auto à 120 Hz en rendu 1440p. Patches finaux dans `build/patches-5/`. |
| 13 oct. 2026 | **0.3.0 publiée** (tag, release, zip `77cc9963…`) ; catalogue mis à jour par la PR automatique #251 (14 oct., 00:01 UTC). |
| 14 oct. 2026 | 0.3.1 ouverte : page du téléphone cassée par le `param.json` de Jelly5. Sonde `bind`/`listen`, profils vérifiés (`param-profiles.json`, `check-param.py`), mode stress 120 Hz, 120 Hz « expérimental » ; build `app-12` et 7 variantes de `param.json`. |
| 14 oct. 2026 | 0.3.1 rapide préparée : `param.json` de la 0.2.0 (Jeux, sans 120 Hz), 120 Hz retiré de Réglages et signalé s'il était gardé, README / INSTALL / notes / recette, zip et build `app-13`. Séries A et B d'`app-12` reportées en 0.3.2. |
| 14 oct. 2026 | app-14 validé (Jeux, écoute, 120 Hz) ; **0.3.1 publiée** avec le 120 Hz expérimental. Seerr : série supprimée en statut 7 (`DELETED`), demande gardée : Ménage validé. Première issue : Emby (étude). Plan de la 0.3.2. |
| 14 oct. 2026 | Essais app-15 : Multimédia sans écoute sur 8085 et sans 120 Hz tenu (5 profils dans `param-profiles.json`) ; stress 120 Hz propre en 4K et 1440p. 0.3.2 préparée : Auto à 120 Hz, dates des films, README, notes, recette, zip, `build/patches-10/`. |
| 14 oct. 2026 | 0.3.2 publiée. 0.4.0 (Emby) : serveur de test, clé `emby`, noms, écarts d'API (journal, dates du Ménage, accélération), fixtures et mock, README préparé, zip de test `emby-test-1`, `build/patches-11/`. |
| 15 oct. 2026 | Assistant de première configuration : port choisi par le système, `config.json.invalid`, règle `core/welcome`, écrans, Réglages et Accueil ; build `app-16`, `build/patches-12/`. |
| 15 oct. 2026 | `app-16` validé sur la console ; catalogue en 0.3.2 (PR #287) ; testeur Emby : 4.10.1.0 + Premiere. Recette 0.4.0 et liste « Pour publier la 0.4.0 ». |
| 15 oct. 2026 | En attendant le testeur Emby : README 0.4.0 (Emby, assistant, capture `welcome.jpg`), `CONFIG.md`, `INSTALL.txt`, `config.example.json`, `capture-states.sh` (états `emby` et `noconfig` réparés, état `welcome`), brouillon des notes de version ; `build/patches-14/`. |
| 15 oct. 2026 | Décision : 0.4.0 avec Emby en beta si l'essai B passe. Build `app-17` et étapes de l'essai B (Emby de test depuis la console). |
| 15 oct. 2026 | Essai B réussi (34 étapes). 0.4.0 préparée avec Emby en beta : pastille « bêta » dans Réglages, README (patch 03 + beta), notes, numéros, zip (`2c83be9b…`), recette courte, commentaire de l'issue #1 ; ordre de la suite (§ 2 nonies). `build/patches-15/`. |
| 16 oct. 2026 | 0.4.0 publiée. 0.5.0 ouverte (cinq chantiers). Chantier 1 : écran « Nouveautés » (`core/whats_new`, `seenVersion`, bouton de Réglages). |
| 16 oct. 2026 | Chantier 2 : espagnol et allemand (`es.cpp`, `de.cpp`), langue système PS5 testée, pilules et puces de charge qui s'adaptent à la longueur des textes. |
| 16 oct. 2026 | Chantier 5 : `CONTRIBUTING.md`, GIF de démonstration (`capture-gif.sh`), brouillon Reddit. Conceptions des chantiers 3 et 4 proposées. |
| 16 oct. 2026 | Chantier 3 : Demandes › Découvrir (recherche au clavier système, rangées, fiche, demande Seerr avec saisons / 4K / profil / dossier), fixtures et tests ; lien « Questions » (Discussions). |
| 16 oct. 2026 | Chantier 4 : Quick Connect pour Jellyfin (assistant, Réglages, session dans `config.json`, compte non administrateur accepté avec des messages clairs), tests. |
| 16 oct. 2026 | Lignes des Nouveautés relues ; build de test `app-18` (demandes jamais envoyées, version affichée 0.5.0-test) et ses étapes. |
| 16 oct. 2026 | `app-18` validé sur la console (A à F). 0.5.0 préparée : numéros, zip (`32fd8c09…`), notes, README (capture Découvrir), recette courte, brouillon Reddit ; `is4k` vérifié dans le code de Seerr ; emplacement de `/download0` vu par FTP (`RELEASING.md`). `build/patches-18/`. |
| 16 oct. 2026 | Plantage du testeur Emby (issue #1) : RIP dans le rapport de plantage d'emby-test-1 (lecture au-delà de la pile), cause probable les images Emby en taille d'origine (`fillWidth` ignoré), reproduit sous Linux (pic de tas ×5). Corrigé : `maxWidth` pour Emby, décodage plafonné, rapport borné, garde-fou de démarrage (`core/boot_guard`). Zip de test `emby-fix-1`, réponses à l'issue, `build/patches-19/`. |
| 16 oct. 2026 | `emby-fix-1` validé sur la console (Jellyfin), patches-19 commités, contournement et zip de test postés sur l'issue. 0.5.1 préparée : numéros, zip (`75f6fced…`), notes, README et `INSTALL.txt` (garde-fou), recette courte, textes de sortie (release v0.5.0, issue #1), `build/patches-20/`. Publication : testeur OK ou 18 octobre au plus tard. |
| 17 oct. 2026 | **0.5.1 publiée** (tag, release, zip `75f6fced…`) après la recette console OK ; testeur Emby sans réponse, message posté sur l'issue #1 (laissée ouverte) ; release v0.5.0 corrigée « Fixed in 0.5.1 ». Catalogue encore en 0.5.0. Bazarr et Lidarr retirés de la suite ; port 5555 gardé ouvert. |
| 17 oct. 2026 | 0.6.0 ouverte (Plex). Étude de faisabilité `docs/PLEX.md` (sources officielles de Plex, Seerr, Tautulli ; CORS de plex.tv mesuré) ; décisions attendues ; `build/patches-21/`. |
| 17 oct. 2026 | Décisions Plex de Vivian (pas de Ménage avec Plex en 0.6.0). P1 : serveur Plex de test (Docker, sans compte), simulateur de lecture, relevé (images à la taille demandée, arrêt refusé sans Plex Pass). |
| 17 oct. 2026 | P2 Plex : configuration `"plex"`, un seul serveur multimédia, noms, Réglages, assistant et page du téléphone (jeton collé), erreurs propres à Plex. |
| 17 oct. 2026 | P3 Plex : client complet, écrans (sessions, diagnostic, arrêt, scan, tâches, ajouts récents, Journal, bibliothèques, utilisateurs), Ménage indisponible avec Plex, fixtures anonymisées et `--mock` (76 tests). |
| 17 oct. 2026 | P4 Plex (code) : connexion par le téléphone (PIN plex.tv, jeton du serveur seulement), essayée avec plex.tv simulé ; essai réel en attente du compte de test. |
| 17 oct. 2026 | Plan changé : 0.6.0 directe avec Plex en BETA. Essai avec le compte de test préparé (`claim` / `unclaim`, 21 étapes) ; 0.6.0 préparée : numéros, Nouveautés, README, `INSTALL.txt`, formulaire « Plex feedback (beta) », notes, brouillon Reddit, recette console, zip (`c14c9199…`). |
| 17 oct. 2026 | Essai Plex avec compte : le jeton du propriétaire est celui du compte, et retirer l'appareil ne le révoque pas. Rien de mieux chez Plex hors ligne (`docs/PLEX.md` § 8) : avertissements partout, vraie révocation expliquée. Zip `release-0.6.0b` (`6757ea3b…`). |
