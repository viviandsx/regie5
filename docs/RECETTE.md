# Régie5 — Recette V1 (phase 5)

Tout ce qu'il faut vérifier, sous Linux, contre le vrai serveur, avant de déclarer la V1
terminée. Chaque point dit **comment** le vérifier et **ce qu'on doit voir**. Cocher au fur
et à mesure ; noter à côté tout écart (capture, ligne de journal).

Les actions réelles (section 7) ne se font que dans l'ordre et avec les précautions
indiquées : utilisateur Seerr de test, sa propre session Jellyfin, et pour le Ménage un
élément choisi par Vivian, supprimé par Vivian lui-même.

## Passe automatique du 11 octobre 2026

Faite contre les vrais serveurs (Sonarr 4.0.20, Radarr 6.4.4, Seerr 3.5.0, Jellyfin
12.2.0), **en lecture seule** : uniquement des `GET`, aucune action déclenchée. Ce qui
reste à faire en personne est dans **`docs/RECETTE-EXPRESS.md`** (15 points).

| Vérifié | Résultat |
|---|---|
| Hors ligne (§1) | 4 hôtes seulement, ceux de `config.json` ; 171 requêtes, toutes `GET`, toutes 200 |
| Clés (§0) | Aucune clé dans le journal HTTP |
| Périmètre (§1) | Aucun nom de release, d'indexeur ni de client affiché ; alertes rédigées par l'app ; Journal sans IP ni appareil |
| Chiffres (§3 à §6) | 55 séries, 439 films, 158 épisodes et 23 films manquants, 2 alertes, disques 5,6 / 7,9 To et 35 / 124 Go (une fois chacun), file vide, 0 demande en attente, 1 lecture en Direct Play : identiques aux API |
| File d'attente (§5) | Interrogée seulement quand elle est affichée |
| Erreurs (§8) | Clé fausse, port faux, service désactivé, JSON cassé relu par △ (configuration précédente gardée), fichier absent : messages justes, sur une **copie** de `config.json` |
| Langues (§9) | FR / EN corrects (dates, tailles, « Sonarr: »), langue par défaut = `$LANG` |
| Dépôt (§11) | Ni IP, ni clé, ni nom de compte ou d'appareil réels ; `THIRD_PARTY_NOTICES.md` complet ; README toujours à écrire |
| Perfs 1080p (§10) | `cpu worst` ≤ 5,4 ms hors première image, `draws worst` 57, pics seulement dans `present` |
| Perfs 4K (§10) | Non concluant : le laptop swappait (3,5 Go) ; mesure à refaire (fin de `RECETTE-EXPRESS.md`) |

Corrigés pendant la passe : pic de 15 ms à l'ouverture du Journal (changement de taille de
police FreeType à chaque texte → objets de taille mis en cache, 3 ms) ; Guide : une sortie
cinéma passée sans fichier affichait « À venir » (→ « En salles »).

---

## 0. Préparation

```bash
cmake --build build/linux && ctest --test-dir build/linux --output-on-failure
```

```bash
REGIE5_HTTP_LOG=1 REGIE5_FPS=1 scripts/run-linux.sh 2>&1 | tee /tmp/regie5-recette.txt
```

- [ ] Les tests passent (44 au moment de la mise à jour, phase 5).
- [ ] `~/.config/regie5/config.json` contient les quatre services (et Glances s'il est
      installé), voir `docs/CONFIG.md`. Une instance 4K de Radarr/Sonarr porte `"is4k": true`.
- [ ] Le journal ne contient **aucune clé d'API** :
      `grep -ci "apikey\|x-api-key\|token=" /tmp/regie5-recette.txt` doit donner 0.
- [ ] Une DualSense est branchée (USB ou Bluetooth) pour la section 2.

## 1. Hors ligne et périmètre (conditions de publication)

- [ ] **Aucune requête vers internet.** Après un tour de tous les onglets (dont les
      affiches de Demandes) :
      `grep -oE 'https?://[^/ ]*' /tmp/regie5-recette.txt | sort -u` ne liste **que** les
      adresses des services de `config.json` (Glances compris).
- [ ] **Aucun nom de release.** Activité › File d'attente : pour chaque élément, Régie5 montre le titre
      de la série ou du film, l'épisode, la qualité, la progression et le temps restant. Le
      nom de release affiché dans Sonarr/Radarr (Activité › File d'attente) n'apparaît
      **nulle part**, ni dans Activité › Journal (les imports montrent le titre et la
      qualité seulement).
- [ ] **Aucun indexeur, client de téléchargement, tracker ni chemin** : ni dans
      Activité, ni dans les alertes santé (✕ sur le bandeau de l'Accueil), ni dans les
      fiches de la Bibliothèque.
- [ ] **Vie privée du Journal** : aucune adresse IP, aucun nom d'appareil dans Activité ›
      Journal (les connexions disent « X s'est connecté », sans appareil).
- [ ] **Aucune action de recherche** : nulle part un bouton « Rechercher », « Recherche
      interactive » ou « Télécharger ».
- [ ] La Bibliothèque est en lecture seule, **sauf le Ménage** : ✕ n'ouvre qu'une fiche ;
      rien ne change la surveillance ni le profil.
- [ ] **Ménage désactivé par défaut** : avec un `settings.json` absent (le renommer), la
      pilule Ménage n'apparaît pas dans la Bibliothèque.

## 2. Navigation et contrôles

Au clavier, puis à la manette.

| Bouton | Clavier | Vérifier |
|---|---|---|
| D-pad / stick | flèches | La goutte de verre glisse avec un rebond ; maintenu, le déplacement se répète |
| ✕ | Entrée | Valide (pilule, fiche, bouton) |
| ○ | Échap / Retour arrière | Ferme la feuille ouverte ; sinon remonte à la barre d'onglets |
| L1 / R1 | Q / E | Onglet précédent / suivant, dans l'ordre Accueil · Bibliothèque · Activité · Demandes · Jellyfin · Réglages (engrenage) |
| L2 / R2 | Z / C | Grille triée par titre : lettre précédente / suivante (la lettre s'affiche en grand) ; autre tri : page ; Guide : semaine précédente / suivante |
| △ | R | Recharge l'écran (et retente les jaquettes en échec) |
| Options | M | Menu de l'élément sélectionné (fiche, ajout récent, demande, candidat du Ménage) ; « M Options » n'apparaît en bas que s'il y a une action |

- [ ] Tous les boutons du tableau, au clavier.
- [ ] Tous les boutons du tableau, à la DualSense.
- [ ] Une feuille ouverte (alertes, demande, session, menu Options) capte L1/R1 : pas de
      changement d'onglet.
- [ ] Les indications de boutons en bas d'écran correspondent à ce que font les boutons.
- [ ] L'horloge en haut à droite est à l'heure locale.

## 3. Accueil

- [ ] **À venir** : mêmes éléments que les calendriers Sonarr + Radarr sur les 14 prochains
      jours (épisodes et sorties de films), triés par date, avec jaquettes.
- [ ] **Ajouts récents** : mêmes éléments que « Ajoutés récemment » dans Jellyfin ; plusieurs
      épisodes d'une même série regroupés (« N nouveaux épisodes »).
- [ ] ✕ sur une carte « À venir » ouvre Bibliothèque › Guide à la semaine de cette date.
- [ ] **Bandeau santé** : espace libre identique à Sonarr/Radarr (Système › État ›
      Espace disque), chaque disque une seule fois.
- [ ] **Alertes** : le nombre correspond à Système › État de Sonarr + Radarr ; ✕ ouvre la
      liste, les erreurs en tête, phrases lisibles en FR et en EN.
- [ ] Le fond flouté change en fondu après un court arrêt sur une jaquette.
- [ ] Défilement d'une rangée longue : fluide, sans à-coup.

## 4. Bibliothèque

- [ ] **Séries** : le nombre affiché = le nombre de séries dans Sonarr.
- [ ] **Films** : le nombre affiché = le nombre de films dans Radarr.
- [ ] Tri : Titre / Date d'ajout / Prochaine diffusion, l'ordre est le bon.
- [ ] Filtres Séries : Tous / Surveillées / Manquants / Terminées ; Films : Tous / Surveillés /
      Manquants / Téléchargés. Les nombres sont cohérents avec Sonarr/Radarr.
- [ ] L2/R2 en tri par titre : « À l'ouest… » est sous A ; la lettre affichée est la bonne.
- [ ] **Fiche** (✕ sur une jaquette) : statut, surveillance, profil, taille, qualité des
      fichiers (par qualité, jamais par nom de fichier), saisons avec présents / manquants.
      Comparer avec la page de la série ou du film dans Sonarr/Radarr.
- [ ] **Manquants** : les nombres d'épisodes et de films correspondent à Wanted › Missing ;
      ce qui est sorti d'abord (plus récent en premier), puis ce qui est à venir.
- [ ] Défilement rapide (↓ maintenu, puis R2 répétés) dans les Films : fluide ; jaquettes en
      fondu, jamais d'image cassée.
- [ ] **Guide** : sept colonnes lundi → dimanche, aujourd'hui en surbrillance ; mêmes
      épisodes et sorties que les calendriers Sonarr/Radarr de la semaine ; états disponible
      / à venir / manquant justes ; L2/R2 change de semaine ; ✕ ouvre la fiche.

### 4 bis. Ménage (lecture)

- [ ] Réglages › « Ménage : désactivé » › ✕ : avertissement, focus sur **Annuler** ;
      Annuler ne change rien. ✕ puis Activer : la pilule Ménage apparaît. Le choix survit à
      un redémarrage (`settings.json` à côté de `config.json`).
- [ ] Candidats : seulement des titres **vus** selon le critère (tous les comptes actifs /
      au moins un), plus vus depuis la durée choisie, jamais en favori ; séries terminées et
      entièrement vues. Vérifier 3 candidats dans Jellyfin (Lu, date, favori, par compte).
- [ ] Un titre absent de Jellyfin n'est jamais proposé ; un compte de `cleanup.ignoreUsers`
      ne compte pas dans « tous ».
- [ ] En-tête « N éléments · X Go récupérables » ; liste triée par taille ; tailles =
      Radarr/Sonarr.
- [ ] Options › **Garder** : le titre passe dans « Gardés » ; Options › Ne plus garder le
      remet dans la liste.
- [ ] Options › Supprimer… › Annuler (deux fois, aux deux feuilles) : **rien n'est envoyé**
      (`grep DELETE /tmp/regie5-recette.txt` vide).
- [ ] Plusieurs instances : un titre présent dans deux Radarr apparaît une fois par instance,
      avec le nom de l'instance.

## 5. Activité (File d'attente et Journal)

- [ ] La file = Activité › File d'attente de Sonarr + Radarr, fusionnée : en cours, puis à
      surveiller, puis en attente.
- [ ] Progression et temps restant ≈ ceux de Sonarr/Radarr.
- [ ] Statuts : en cours / en attente / import / avertissement / erreur, bonnes couleurs.
- [ ] Rafraîchissement toutes les 5 s **seulement onglet affiché** : dans le journal, les
      `GET …/queue` s'arrêtent quand on change d'onglet et reprennent au retour.
- [ ] Le focus reste sur le même élément quand la liste bouge.
- [ ] File vide : « Aucun téléchargement en cours. »
- [ ] **Journal** (pilule Journal, ✕) : 14 jours groupés par jour ; filtres Tout · Imports ·
      Demandes · Jellyfin · Santé. Comparer 3 imports avec l'historique Sonarr/Radarr, 3
      demandes avec Seerr, 3 lectures avec le journal d'activité Jellyfin.
- [ ] Imports de **toutes** les instances (si plusieurs Radarr/Sonarr).

## 6. Demandes et Jellyfin (lecture)

- [ ] **Demandes** : En attente / Approuvées / Toutes = les mêmes listes que dans Seerr ;
      titre, affiche (par le proxy d'images de Seerr), demandeur, date, type, saisons.
- [ ] Une affiche que Seerr ne peut pas fournir est remplacée par une jaquette générée.
- [ ] **Jellyfin** : sessions en cours = Tableau de bord › Activité ; utilisateur, appareil,
      média, progression ; Direct Play ou Transcodage **avec la raison** (lancer une lecture
      qui transcode pour vérifier).
- [ ] Tâches planifiées : même liste et mêmes états que le tableau de bord Jellyfin
      (« En cours · N % » pendant une tâche).
- [ ] **Diagnostic de lecture** (✕ sur une session qui transcode) : méthode, raisons, source
      et envoyé au client, accélération matérielle, « le transcodage suit-il ? » (avance en
      minutes, dite estimation), conseils lisibles. Comparer avec le tableau de bord Jellyfin
      (Activité › info de lecture). Pastille « Ne suit pas » / « À la limite » cohérente.
- [ ] **Glances** (si installé, `docs/GLANCES.md`) : charge CPU / mémoire en haut de
      l'onglet Jellyfin et dans la fiche ≈ `glances` sur le serveur ; rafraîchie seulement
      onglet affiché. Glances arrêté : une ligne discrète « Charge du serveur
      indisponible ». Entrée absente ou désactivée : la section disparaît.
- [ ] **Ouvrir dans Jelly5** (simulé sous Linux) : Options sur une fiche Bibliothèque, un
      ajout récent et une demande disponible › Ouvrir dans Jelly5. Une note « Ouverture de
      « … » dans Jelly5 » s'affiche et le journal contient
      `launch (simulated): PPSA99505 --open-item <Id> --server <Id>` : l'`<Id>` de l'élément
      est celui de la page du titre dans Jellyfin (adresse `…/details?id=<Id>`), le serveur
      celui de Tableau de bord › Général. L'entrée n'apparaît pas sur un titre absent de
      Jellyfin ni sur une demande pas encore disponible.

## 7. Actions réelles (dans cet ordre, avec précaution)

Chacune : la confirmation s'ouvre avec le focus sur **Annuler** ; ○ ou Annuler ne fait rien.

1. [x] **Scanner les bibliothèques** — fait le 11 octobre 2026 (`POST /Library/Refresh` →
       204, la tâche « Analyser la médiathèque » est passée en cours). À refaire une fois en
       fin de recette pour revoir la note et la tâche.
2. [ ] **Arrêter une lecture** — sur **sa propre** session, lancée depuis le téléphone :
       ✕ sur la session › Arrêter. La lecture s'arrête sur le téléphone, la note « Lecture
       arrêtée » s'affiche, la session disparaît.
3. [ ] **Refuser** — avec l'**utilisateur Seerr de test** : créer une demande, puis dans
       Régie5 ✕ › Refuser › confirmer. Note « Demande refusée » ; la demande quitte « En
       attente » ; dans Seerr, elle est refusée.
4. [ ] **Approuver** — même utilisateur de test, nouvelle demande : ✕ › Approuver ›
       confirmer. Note « Demande approuvée » ; elle passe dans « Approuvées » ; dans Seerr,
       elle est approuvée.
5. [ ] Une demande déjà traitée ailleurs entre-temps : message « Cette demande n'est plus en
       attente. », rien n'est envoyé.
6. [ ] **Supprimer avec le Ménage** — **fait par Vivian lui-même**, sur **un seul** élément
       qu'il choisit (un film sans valeur, de préférence petit), après avoir noté son
       titre, son année, sa taille et son instance dans Radarr/Sonarr :
       - Options › Supprimer… : la feuille 1 montre ce titre, cette année, cette taille,
         cette instance, « fichiers supprimés définitivement » ; focus sur Annuler.
         Continuer › feuille 2, focus sur Annuler, Supprimer en rouge › Supprimer.
       - Journal : **un seul** `DELETE …/movie/<id>?deleteFiles=true&addImportExclusion=true`
         (ou `…/series/<id>?deleteFiles=true&addImportListExclusion=true`), vers la bonne
         instance ; rien vers Seerr.
       - Radarr/Sonarr : le titre a disparu, ses fichiers aussi (vérifier l'espace disque) ;
         il est dans Paramètres › Listes › Exclusions.
       - Seerr, **après sa tâche « Media Availability Sync »** (nuit, ou lancée à la main dans
         Seerr › Paramètres › Tâches) : le titre n'est plus « disponible » (dans la bonne
         qualité, 4K ou non) ; il peut être redemandé ; ses anciennes demandes sont là.
       - Régie5 : note « « … » supprimé : X libérés · Seerr le saura à sa prochaine
         synchronisation », la liste se recharge
         sans lui.

## 8. Réglages, configuration et erreurs

- [ ] Quatre pastilles vertes avec la version de chaque serveur (une ligne par instance s'il y
      en a plusieurs ; Glances s'il est configuré) ; version de Régie5 et chemin du fichier
      de configuration affichés.
- [ ] Ligne « Ménage : activé / désactivé » (voir 4 bis).
- [ ] **Clé fausse** (une lettre changée dans `config.json`), « Tout tester » sans relancer :
      ce service passe à « Clé d'API refusée » ; ses écrans affichent l'erreur.
- [ ] **Port faux** : « Injoignable — vérifiez l'adresse et le port ».
- [ ] **JSON cassé** (virgule en trop), « Tout tester » : message rouge « …n'est pas du JSON
      valide. La configuration précédente reste en service. » ; tout reste vert.
- [ ] **Service désactivé** (`"enabled": false`), « Tout tester » : pastille grise, et ses
      écrans disent « … est désactivé dans la configuration. »
- [ ] **Service arrêté** (arrêter réellement Radarr, par exemple) : Accueil « État inconnu »
      en rouge avec la cause ; Films et Manquants affichent l'erreur Radarr ; le reste
      fonctionne. △ après redémarrage : tout revient.
- [ ] **Sans config.json** (`scripts/run-linux.sh --config /tmp/aucun.json`) : Réglages dit
      quoi faire ; les autres écrans disent « Pas de configuration : voir Réglages. »
- [ ] Tout remettre en ordre, « Tout tester » : quatre pastilles vertes.

### 8 bis. Plusieurs instances (si une 2ᵉ instance Radarr/Sonarr est disponible)

- [ ] Réglages : une ligne par instance, chacune avec sa pastille et sa version.
- [ ] Accueil : alertes et disques des deux instances ; une alerte dit de quelle instance
      elle vient (« Radarr 4K ») ; un film suivi par les deux n'apparaît qu'une fois dans
      « À venir ».
- [ ] Bibliothèque › Films : les films des deux instances, le nom de l'instance sous chaque
      jaquette ; la fiche montre la puce de l'instance, son profil et sa jaquette.
- [ ] Activité › File d'attente : téléchargements des deux, avec la puce de l'instance.
- [ ] Une instance arrêtée : pastille rouge « Radarr 4K : Injoignable… », l'autre reste
      affichée.
- [ ] Sans 2ᵉ instance réelle : `REGIE5_MOCK_MULTI=1 scripts/run-linux.sh --mock` montre
      le même parcours sur les fixtures.

## 9. Langues

- [ ] `--lang fr` puis `--lang en` : tous les écrans, feuilles et notes dans la langue choisie.
- [ ] Sans `--lang`, la langue suit `$LANG`.
- [ ] Anglais : « Sonarr: … » (sans espace avant les deux-points), dates « Thu, Oct 15 »,
      tailles « 5.6 TB ».
- [ ] Français : « 0 série », « 1 film », « 2 films » ; dates « jeu. 15 oct. », tailles « 5,6 To ».
- [ ] Aucun texte coupé de façon gênante (à part les titres longs, terminés par « … »).

## 10. Performances et affichage

- [ ] Plein écran (`--fullscreen`) : net, à la bonne échelle.
- [ ] `REGIE5_FPS=1` en usage normal : `cpu worst` reste sous 8 ms, `draws worst` sous 70.
      Les seuls pics notables sont dans `present` (compositeur du laptop ; mesure PS5 en phase 6).
- [ ] Rendu 4K simulé : `REGIE5_RES=3840x2160 REGIE5_FPS=1 scripts/run-linux.sh`, tour de
      tous les onglets : `gpu … worst` reste sous 8 ms.
- [ ] Une heure ouverte, onglet Téléchargements affiché : pas de ralentissement, mémoire
      stable (`ps -o rss= -C regie5` au début et à la fin).

## 11. Dépôt et publication

- [ ] `git status` propre ; ni `config.json`, ni `build/`, ni aucun des fichiers locaux listés dans `.git/info/exclude` ne sont suivis.
- [ ] Aucune clé, adresse privée, nom d'utilisateur ou d'appareil réel dans le dépôt :
      `git grep -nE '192\.168\.|10\.[0-9]+\.[0-9]+\.[0-9]+|apiKey"\s*:\s*"[0-9a-f]{20,}'`
      ne doit trouver que `config.example.json` et `docs/` (adresses d'exemple).
- [ ] `scripts/make-fixtures.py` relancé si les fixtures ont changé ; le test
      `fixtures_hold_no_forbidden_field` passe.
- [ ] `THIRD_PARTY_NOTICES.md` à jour (Jelly5, LiquidGlass, cJSON, stb, FreeType, SDL2,
      libcurl, Inter…).
- [ ] Un `README.md` existe (installation Linux, `config.json`, touches) — il n'existe pas
      encore au moment de la rédaction.

---

**V1 terminée** quand toutes les cases sont cochées ou que chaque écart restant est noté
et accepté dans `docs/ROADMAP.md`.
