# Régie5 — Feuille de route

État d'avancement du projet, tenu à jour à la fin de chaque étape. Pour reprendre le
travail, ce fichier suffit ; le détail technique est dans `docs/ARCHITECTURE.md` (§8 à §11
pour ce qui a été fait après la phase 0).

**Dernière mise à jour : 16 octobre 2026 (nuit). 0.4.0 publiée. **0.5.0 prête à publier** (§ 2 decies) : essai `app-18` validé par Vivian (A à F), zip `build/ps5/release-0.5.0/upload/PPSA99520.zip` (`32fd8c09…`), notes, README, recette `docs/RECETTE-0.5.0.md`, patch `build/patches-18/`. Reste : la recette console puis la publication (« Pour publier la 0.5.0 »), pas sans l'accord de Vivian.**

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
| 12. Version 0.5.0 | Nouveautés, espagnol et allemand, demandes Seerr, Quick Connect, communauté | 🧪 codée ; essai console `app-18` à faire (§ 2 decies) |

Tests : **65** (`regie5_tests`, lancé par `ctest`), tous verts.

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

Rien qui touche au code Emby.

1. [ ] **Petits correctifs** : un message visible à l'écran (ou une notification système) si
       la console refuse la mémoire demandée, au lieu du seul journal (§ 3, « Taille du tas ») ;
       Bibliothèque › Manquants qui reste sur ses squelettes sans configuration au lieu de
       « Pas de configuration » (vu avec l'état `noconfig` de `capture-states.sh`).
2. [ ] **Bazarr en lecture seule** : sous-titres manquants (Bibliothèque › Manquants, fiches),
       sans noms de fournisseurs. Endpoints à vérifier dans la doc officielle avant de coder.
3. [ ] **Lidarr en lecture seule** : sorties d'albums dans « À venir » et le Guide, file
       d'attente dans Activité (titres, progression, jamais de noms de releases).

## 2 decies. Version 0.5.0 (ouverte le 16 octobre 2026)

La 0.4.0 est publiée. Cinq chantiers, décidés par Vivian ; les chantiers 3 et 4 passent par
une conception validée avant d'être codés. Patches : `build/patches-16/` (chantiers 1, 2, 5) et `build/patches-17/` (3, 4, puis les lignes des Nouveautés et le build de test).
Un seul build de test console à la fin, puis la release (pas sans l'accord de Vivian).
Le § 2 nonies (correctifs, Bazarr, Lidarr) vient après.

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
8. [ ] Vivian : recette `docs/RECETTE-0.5.0.md` sur la console.
9. [ ] Vivian : commit « Régie5 0.5.0 » (`build/patches-18/`), push, tag `v0.5.0`, release
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

## 3. En attente

| Sujet | Qui | Détail |
|---|---|---|
| Port 5555 (ufw) | Vivian | Ouvert pour les tests, à refermer à la fin. |
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
`REGIE5_WELCOME=1` (l'assistant même avec une config ou en `--mock`),
`REGIE5_WHATSNEW=1` (l'écran « Nouveautés » au lancement, même en `--mock`),
`REGIE5_SHARE_LOG_CODE=1` (le code de la page du téléphone dans le terminal, pour un essai
par `curl`).
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
