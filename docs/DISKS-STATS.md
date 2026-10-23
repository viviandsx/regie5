# Régie5 — Étude : santé des disques et statistiques de visionnage (0.8.0)

Étude du 23 octobre 2026, **sans code**. Deux services optionnels, en **lecture seule**, sur le
modèle de Glances : s'ils ne sont pas configurés, rien ne s'affiche.

Sources lues (code et documentation officiels, pas de mémoire) :

- **[SC1]** Scrutiny, dépôt `AnalogJ/scrutiny` (MIT) : `webapp/backend/pkg/web/server.go`
  (routes), `web/handler/*.go`, `models/device.go`, `models/device_summary.go`,
  `models/measurements/smart*.go`, `pkg/constants.go`, `models/settings.go`,
  `frontend/src/app/shared/device-status.pipe.ts`, aux tags **v0.8.1** (avril 2024) et
  **v0.9.0 → v0.9.5** (avril → 30 septembre 2026).
- **[JD1]** Jellydash, dépôt `themartz90/jellydash` (MIT, PHP), branche `main`, **v1.15.3**
  (21 octobre 2026) : `README.md`, `public/api/*.php`, `utils/@api-guard.php`, `src/Router.php`,
  `docs/HISTORY_CSV.md`, `src/Jellyfin/HistoryFilters.php`.
- **[PR1]** Playback Reporting pour Jellyfin, dépôt `jellyfin/jellyfin-plugin-playbackreporting`
  (GPL-3.0), **v19** (8 septembre 2026, `targetAbi` 12.0.0.0) :
  `Api/PlaybackReportingActivityController.cs`, `Data/ActivityRepository.cs`,
  `Model/ReportPlaybackOptions.cs`, `TaskCleanDb.cs`.
- **[PR2]** Playback Reporting pour Emby, dépôt `faush01/playback_reporting` (branche `develop`) :
  `Api/UserActivityAPI.cs`.
- **[JF1]** Jellyfin, `MediaBrowser.Model/Configuration/ServerConfiguration.cs`,
  `Events/Consumers/Session/PlaybackStopLogger.cs`.
- **[TA1]** Tautulli, wiki « Tautulli API Reference ».
- Vérifications en **GET** sur le serveur de Vivian (Jellyfin 12.2.0) : journal d'activité,
  `UserData`, liste des plugins. Scrutiny et Jellydash : **pas trouvés** sur les ports par
  défaut de la machine du Jellyfin ; leurs adresses sont à demander (§ 7).

---

## 1. Scrutiny : santé des disques

### 1.1 Ce qu'est Scrutiny

Un **collecteur** (`smartctl`, lancé par cron, une fois par jour par défaut) envoie les relevés
SMART à un **serveur web** (Go + InfluxDB) qui les garde et les affiche. Image omnibus
`ghcr.io/analogj/scrutiny:…-omnibus`, port **8080** par défaut. Le serveur peut recevoir
plusieurs collecteurs (plusieurs machines).

### 1.2 Authentification

**Aucune** [SC1] : `server.go` ne monte que les middlewares journal / dépôt / config, aucune
garde. Tout le réseau local peut lire *et écrire* (archiver, supprimer un disque, changer les
réglages). Régie5 n'appellera que des `GET`. Derrière un reverse proxy, certains ajoutent un
HTTP Basic : on le prévoit comme pour Glances (`username` / `password` facultatifs).

Le chemin de base est configurable côté Scrutiny (`web.listen.basepath`) : l'`url` de
`config.json` peut contenir un chemin, toutes les URL passent par `Services::url`.

### 1.3 Endpoints (lecture)

| Endpoint | Ce qu'il donne | Usage dans Régie5 |
|---|---|---|
| `GET /api/health` | `{"success": true}` si SQLite et InfluxDB répondent, sinon 500 + `error` | Bouton **Tester** de Réglages |
| `GET /api/summary` | Tous les disques : identité, `device_status`, dernier relevé (`temp`, `power_on_hours`, `collector_date`) **et l'historique de température complet** (`temp_history`, durée `forever`) | Bandeau santé, liste des disques |
| `GET /api/summary/temp?duration_key=week` | Historique de température de tous les disques (`day`, `week`, `month`, `year`, `forever`) | Courbe 7 jours de la fiche disque (au besoin) |
| `GET /api/device/{id}/details?duration_key=week` | Le disque, ses relevés SMART (un point par jour) avec **chaque attribut et son statut**, et une table `metadata` (noms et seuils des attributs) | Fiche disque : attributs en défaut |
| `GET /api/settings` | Réglages de l'interface, dont `metrics.status_threshold` | Appliquer le même seuil que l'interface web |

**Jamais appelés** (écritures) : `POST /api/devices/register`, `POST /api/device/{id}/smart`,
`…/selftest`, `…/archive`, `…/unarchive`, `DELETE /api/device/{id}`, `POST /api/settings`,
`POST /api/health/notify`, `POST /api/collector/error`.

### 1.4 Versions : un changement à connaître

| Version | Clé des disques dans `summary` | `{id}` de `/api/device/{id}/details` |
|---|---|---|
| 0.8.x (jusqu'à 0.8.6, février 2026) | le **WWN** | `wwn` |
| **0.9.0 et après** (avril 2026 →) | le **`scrutiny_uuid`** | `scrutiny_uuid` (le WWN reste dans l'objet, « deprecated ») |

Régie5 prend `device.scrutiny_uuid` s'il existe, sinon `device.wwn`, **comme clé opaque** :
jamais affichée, jamais journalisée, jamais écrite dans une fixture réelle. Pas de numéro de
version exposé par l'API : on déduit 0.8 / 0.9 de la forme. Un serveur plus ancien (avant
InfluxDB, 2022) sans `summary` lisible donne « Scrutiny : version non prise en charge
(0.8 ou plus) » dans Réglages.

### 1.5 Forme de `/api/summary` [SC1]

```json
{ "success": true,
  "data": { "summary": {
    "<scrutiny_uuid ou wwn>": {
      "device": { "device_name": "sda", "label": "", "device_label": "", "host_id": "",
                  "capacity": 4000787030016, "device_protocol": "ATA",
                  "rotational_speed": 5400, "archived": false, "device_status": 0,
                  "model_name": "…", "serial_number": "…", "wwn": "…", "firmware": "…",
                  "scrutiny_uuid": "…" },
      "smart": { "collector_date": "2026-10-23T00:00:12Z", "temp": 38, "power_on_hours": 21450 },
      "temp_history": [ { "date": "…", "temp": 37 }, … ] } } } }
```

**Lu** : `device_name` (`sda`, `nvme0`), `label` (nom donné par l'utilisateur), `capacity`,
`device_protocol`, `rotational_speed` (0 = SSD), `archived`, `device_status`, `smart.*`, et la
clé. **Jamais lu** : `serial_number`, `device_serial_id`, `model_name`, `manufacturer`,
`firmware`, `wwn` (sauf comme clé en 0.8), `device_uuid`, `host_id`, `interface_*`.
Un disque s'affiche « Disque sda · 4 To » ou par son `label` s'il en a un.

### 1.6 Quand un disque est en alerte [SC1]

`device_status` est un **masque de bits** :

| Bit | Sens | Phrase de Régie5 (proposée) |
|---|---|---|
| 0 | tout va bien | « OK » (vert) |
| 1 | **SMART** : le disque lui-même signale un attribut sous le seuil du fabricant (`when_failed` = `now` / `past`) | « Le disque signale une panne (SMART) » (rouge) |
| 2 | **Scrutiny** : un attribut dépasse les seuils de Scrutiny (taux de panne observés, données Backblaze) | « Usure ou erreurs anormales (Scrutiny) » (orange → rouge) |

L'interface web n'affiche que `device_status & metrics.status_threshold` (réglage : 1 SMART,
2 Scrutiny, 3 les deux, par défaut 3) ; Régie5 fait **pareil**, pour dire la même chose que la
page de Scrutiny. Sans relevé (`smart` absent) : « État inconnu ».

Dans `details`, chaque attribut a `status` (0 OK, 1 échec SMART, 2 avertissement Scrutiny,
4 échec Scrutiny), `status_reason`, `failure_rate`. Régie5 ne liste **que les attributs
non nuls**, nommés par l'app (le texte du serveur n'est pas repris) :

- ATA : 5 secteurs réalloués, 10 relances de rotation, 184 erreurs de bout en bout,
  187 erreurs non corrigibles, 188 délais de commande, 197 secteurs en attente,
  198 secteurs hors ligne non corrigibles, 199 erreurs de câble (CRC) ;
- NVMe : `critical_warning`, `media_errors`, `percentage_used`, `available_spare` ;
- SCSI : `scsi_grown_defect_list`, erreurs de lecture / écriture non corrigées ;
- autre : « Attribut n° 9 » / « Attribut nvme `xyz` ».

**Température** : Scrutiny **n'a pas de seuil** de température (il l'affiche, c'est tout).
Régie5 fixe les siens, documentés : disque dur ≥ 50 °C « chaud » (orange), ≥ 60 °C rouge ;
SSD NVMe ≥ 70 °C orange, ≥ 80 °C rouge (SSD SATA : comme un disque dur). Pas de réglage en
0.8.0.

**Relevé ancien** : `collector_date` de plus de 48 h → « Pas de relevé depuis 3 jours »
(orange) : le collecteur ne tourne plus, ce qui est aussi une alerte.

Disques archivés (`archived: true`) : ignorés.

### 1.7 Points à mesurer sur un vrai Scrutiny

- **Taille de `/api/summary`** : il embarque tout l'historique de température de chaque
  disque. Sur PS5 (tas de 256 Mio, budgets par réponse), il faut la mesurer avec plusieurs
  disques et des mois de données. Si elle est trop grosse : plafond de réponse et message
  clair, ou demander `/api/summary/temp` à part (la courbe) et ignorer `temp_history`.
- **Taille de `details`** : la table `metadata` (≈ tous les attributs ATA connus) est jointe
  à chaque réponse. Demandée seulement à l'ouverture d'une fiche, avec `duration_key=week`.
- Forme des dates (`collector_date` en RFC 3339 UTC attendu).

---

## 2. Statistiques de visionnage

### 2.1 Jellydash : pas d'API pour une app tierce [JD1]

- Les statistiques et le récap mensuel sont des **pages HTML rendues par PHP**
  (`?page=statistics`, `statistics/recap`), pas des réponses JSON.
- Les fichiers `public/api/*.php` sont les **appels internes** de sa propre interface, non
  documentés comme API. Aucun ne donne les statistiques agrégées ; le plus proche est
  `GET /api/history-export.php` (filtres `range=7|30|all|custom`, `start`, `end`, `user`…),
  qui renvoie **tout l'historique filtré en CSV**. Le format CSV, lui, est documenté et
  versionné (`docs/HISTORY_CSV.md`, version 2, 36 colonnes).
- **Authentification** : aucune par défaut ; avec `AUTH_ENABLED=true` (recommandé dès qu'il
  sort du réseau local), chaque appel exige une **session ouverte par mot de passe** (cookie),
  sans jeton d'API. Régie5 ne saisit pas de mot de passe : Jellydash protégé serait inutilisable.
- Projet très jeune et très actif (15 versions en deux mois) : un appel interne peut changer
  à chaque version.
- Le CSV contient `client` et `device` (Régie5 ne les lirait pas), et Jellydash suit aussi des
  clients de téléchargement (Régie5 n'en lirait rien, § 3 du projet).
- Jellyfin seulement (il importe des sauvegardes Playback Reporting d'Emby, sans plus).

**Conclusion** : Jellydash n'est **pas** une base fiable pour Régie5. Piste communautaire :
proposer à son auteur un point d'entrée JSON en lecture seule avec jeton (issue sur son dépôt,
décision de Vivian). Si un jour il existe, une clé `"jellydash"` s'ajoutera comme Glances.

### 2.2 Les trois sources possibles

| | **A. Playback Reporting** (plugin) | **B. Jellyfin seul** | **C. Jellydash** (CSV) |
|---|---|---|---|
| À installer | le plugin, depuis le catalogue Jellyfin (dépôt officiel `jellyfin/…`) | rien | Jellydash (Docker) |
| Auth | la **clé d'API admin** déjà dans `config.json` (`RequiresElevation`) | la même | aucune, ou session par mot de passe (inutilisable) |
| API | oui, JSON, `/user_usage_stats/…` | `/System/ActivityLog/Entries`, `UserData` | non (appel interne, CSV complet) |
| Heures par utilisateur | **oui**, durée réelle de lecture (`PlayDuration`) | estimation (début → arrêt, pauses comprises) | oui |
| Titres les plus vus | **oui**, films et séries (nombre + durée) | nombre de lectures | oui (à calculer sur tout le CSV) |
| Par jour | **oui** (`PlayActivity`, par utilisateur et par jour) | oui | oui |
| Semaine / mois / année | additions faites par Régie5 | 30 jours max | oui |
| Profondeur | **3 mois par défaut** (`MaxDataAge = 3`), « pour toujours » possible dans les réglages du plugin | **30 jours** (`ActivityLogRetentionDays = 30`) | tout ce que Jellydash a enregistré |
| Avant l'installation | rien (le plugin commence à son installation) | 30 jours | depuis son installation |
| Emby | **plugin équivalent** (`faush01/playback_reporting`, mêmes routes, à vérifier) | journal d'Emby (déjà lu par le Journal) | non |
| Plex | non → **Tautulli** | historique Plex (lectures terminées seulement) | non |

**Recommandation : A (Playback Reporting)**, détecté tout seul. B et C : non retenus en 0.8.0
(B est trop court et trop approximatif pour un « Wrapped » ; C n'a pas d'API).

### 2.3 Playback Reporting : endpoints (lecture) [PR1]

Tous sous `/user_usage_stats/`, avec l'en-tête `Authorization: MediaBrowser Token="…"`.
Paramètres communs : `days`, `endDate` (`yyyy-MM-dd`), `timezoneOffset` (heures).

| Endpoint | Réponse | Usage |
|---|---|---|
| `user_activity?days=N` | par utilisateur : `user_id`, `user_name`, `total_count`, `total_time` (s), `latest_date` | Heures par utilisateur |
| `PlayActivity?days=N&dataType=time` (ou `count`) | par utilisateur : `user_usage` = `{ "2026-10-01": secondes, … }` (jours manquants à 0) + une ligne `labels_user` | Barres par jour ; semaine / mois en additionnant |
| `MoviesReport?days=N` | `[{label, count, time}]` : films par nom | Films les plus vus |
| `GetTvShowsReport?days=N` | `[{label, count, time}]` : séries par nom (préfixe de `ItemName` avant « - ») | Séries les plus vues |
| `UserId/BreakdownReport?days=N`, `ItemType/…`, `PlaybackMethod/…` | `[{label, count, time}]` | Répartition (films / épisodes / musique ; Direct Play / transcodage) |
| `HourlyReport?days=N` | `{ "<jour de la semaine 0-6>-<heure HH>": secondes }` | « À quelle heure on regarde » (Wrapped) |
| `type_filter_list` | types connus | Test de présence |

**Jamais appelés** :
- `user_manage/prune`, `user_manage/add`, `user_manage/remove`, `load_backup`, `save_backup` :
  ce sont des **GET qui écrivent** ;
- `POST submit_custom_query` : exécute du **SQL arbitraire** ;
- `ClientName/BreakdownReport`, `DeviceName/BreakdownReport`, `{userId}/{date}/GetItems` et
  les champs `client_name`, `item_name` de `user_activity` : appareils et clients (vie privée,
  même règle que le Journal).

Les rapports films / séries ne donnent que des **noms** (pas d'identifiant) : la jaquette se
trouve par le nom dans Radarr / Sonarr (déjà chargés), sinon jaquette générée. Les rapports
films / séries ne se filtrent pas par utilisateur (on n'en a pas besoin pour la V1).

Le plugin respecte sa propre **liste d'utilisateurs ignorés** (réglage du plugin) : c'est là
qu'un membre du foyer se retire des statistiques.

### 2.4 Emby et Plex

- **Emby** : le plugin d'origine (`faush01/playback_reporting`) expose les mêmes routes
  (`user_activity`, `PlayActivity`, `BreakdownReport`, `HourlyReport`, réservées aux admins) [PR2].
  Les formes de réponse sont à vérifier sur le serveur Emby de test (installer le plugin
  demande un téléchargement depuis le catalogue Emby : **accord de Vivian**). Proposé :
  Emby « bêta » pour les statistiques, comme Emby lui-même.
- **Plex** : pas de Playback Reporting. **Tautulli** (`/api/v2?cmd=…`, clé d'API en en-tête
  `X-Api-Key`) a tout : `get_home_stats` (`top_movies`, `top_tv`, `top_users`…),
  `get_plays_by_date`, `get_user_watch_time_stats` [TA1]. C'est aussi la piste du Ménage avec
  Plex (`docs/PLEX.md` § 2.10). **Hors 0.8.0**, à étudier ensuite (une seule intégration pour
  deux fonctions). Sans Tautulli, Plex n'a pas de statistiques dans Régie5.

---

## 3. Où les afficher

Principe : rien de nouveau dans la barre d'onglets ; deux pilules sur l'onglet du serveur
multimédia, comme Activité et Demandes.

```
Jellyfin   [ Serveur ]  [ Statistiques ]
```

La pilule **Statistiques** n'existe que si Playback Reporting répond. Sans Scrutiny ni
statistiques, l'onglet reste exactement comme aujourd'hui (pas de pilules).

### 3.1 Disques

**Accueil, bandeau santé** (déjà là : espace disque + alertes Sonarr / Radarr) : les disques
en alerte s'ajoutent aux alertes, avec la même règle (« Aucune alerte » seulement si tous les
services surveillés ont répondu). Exemples de lignes de la liste (✕ sur le bandeau) :

- « Disque sdb : le disque signale une panne (SMART) » (rouge)
- « Disque sda : 58 °C » (orange)
- « Scrutiny : pas de relevé depuis 3 jours » (orange)

Rien de plus sur l'Accueil : un disque sain ne prend aucune place.

**Vue détaillée : Jellyfin › Serveur, section « Disques »**, juste sous la charge Glances (le
matériel du serveur au même endroit). Une ligne par disque :

```
●  Disque sda · 4 To · disque dur        38 °C    OK
●  Disque sdb · 8 To · disque dur        41 °C    Panne signalée (SMART)
●  nvme0 · 1 To · SSD                    52 °C    OK
```

✕ sur un disque ouvre une **fiche** (même style que la fiche diagnostic) : état et sa phrase,
température et courbe des 7 derniers jours, heures de fonctionnement, date du dernier relevé,
attributs en défaut (nommés par l'app, § 1.6). Pas d'action (lecture seule). Un disque en
alerte depuis l'Accueil ouvre la même fiche.

### 3.2 Statistiques : Jellyfin › Statistiques

Un écran, une période choisie par des pilules : **7 jours · 30 jours · 12 mois** (← / →, ✕).

1. En tête : **total d'heures** sur la période, nombre de lectures.
2. **Par jour** (7 et 30 j) ou **par mois** (12 mois) : une rangée de barres.
3. **Par utilisateur** : nom, heures, barre proportionnelle.
4. **Films les plus vus**, **séries les plus vues** : deux rangées de jaquettes (5 à 10),
   avec le nombre de lectures et les heures.

Rafraîchi à l'ouverture et par △ (pas de minuterie : ça change lentement).

### 3.3 Le futur « Wrapped »

Prévu dès maintenant, codé plus tard : une carte « Récap d'octobre » en haut de
Statistiques (le mois précédent, puis l'année en janvier), ✕ ouvre une suite de 4 ou 5
écrans plein cadre (heures totales, utilisateur le plus assidu, top films, top séries, jour
et heure préférés), L1/R1 ou ← / → pour avancer. Les mêmes appels avec `endDate` au dernier
jour du mois et `days` = 31 / 365. Limite à dire à l'écran : sans « garder pour toujours »
dans le plugin, l'année est **coupée à 3 mois**.

---

## 4. Configuration

### 4.1 `config.json`

```json
"scrutiny": {
  "enabled": false,
  "url": "http://192.168.1.10:8080",
  "username": "",
  "password": ""
}
```

Comme `glances` : pas de clé d'API (Scrutiny n'en a pas), `username` / `password` facultatifs
pour un reverse proxy en HTTP Basic, `insecure` pour un certificat auto-signé. Présente,
désactivée, dans `config.example.json`. `docs/CONFIG.md` et un guide court `docs/SCRUTINY.md`
(sur le modèle de `GLANCES.md` : installer, vérifier par `curl …/api/health`, le déclarer).

**Statistiques : aucune clé.** Playback Reporting tourne dans Jellyfin et utilise sa clé :
Régie5 le détecte (`GET /Plugins`, nom « Playback Reporting », état `Active`). Une clé
`"statistics": false` dans l'entrée du serveur multimédia permet de masquer la pilule
(facultative, absente par défaut). Pas de clé `"jellydash"` (§ 2.1).

### 4.2 Page du téléphone

- Un bloc **Scrutiny** comme celui de Glances : adresse (port 8080 proposé), utilisateur et
  mot de passe facultatifs, case « activer ».
- Statistiques : une ligne d'aide sous le serveur multimédia, « Statistiques : installez le
  plugin Playback Reporting dans Jellyfin, rien à saisir ici ».

### 4.3 Réglages

- Ligne **Scrutiny** : « Optionnel, non configuré » (comme Glances), sinon pastille et
  « 4 disques » ; erreurs lisibles (injoignable, version non prise en charge, InfluxDB en
  panne côté Scrutiny : `/api/health` en 500).
- Sous la ligne du serveur multimédia, une sous-ligne **Statistiques** :
  « Playback Reporting 19 » (vert) · « Plugin non installé (optionnel) » (gris) ·
  « Compte non administrateur : statistiques indisponibles » (Quick Connect avec un compte
  simple) · Plex : « Pas encore avec Plex ».

---

## 5. Vie privée et fixtures

- **Disques** : jamais de numéro de série, de WWN, de modèle exact, de fabricant, de firmware
  ni de nom d'hôte, ni à l'écran, ni dans le journal, ni dans le dépôt. Le WWN / l'UUID sert
  seulement de clé en mémoire.
- **Statistiques** : noms d'utilisateurs (déjà montrés par Jellyfin › Utilisateurs), titres,
  durées. Jamais d'appareil, de client ni d'adresse.
- **Fixtures inventées** (`fixtures/scrutiny/`, `fixtures/jellyfin/user_usage_stats/`) :
  4 disques (`sda` sain, `sdb` panne SMART avec attributs 5 et 197, `sdc` chaud à 57 °C,
  `nvme0` sans relevé depuis 4 jours) ; numéros de série vides, `model_name` « Disque de
  démonstration », WWN `0x5000000000000001…`, UUID `00000000-0000-4000-8000-00000000000N` ;
  utilisateurs et titres des fixtures existantes (déjà anonymes). Les deux formes de
  Scrutiny (0.8 par WWN, 0.9 par UUID) ont chacune leur fixture.
- Le test `fixtures_hold_no_forbidden_field` est étendu : `serial_number` toujours vide,
  `model_name` dans une liste blanche, aucun `client_name` / `DeviceName` rempli.
- Les réponses réelles lues pendant le développement restent dans le dossier temporaire.

---

## 6. Estimation

| Lot | Contenu | Taille |
|---|---|---|
| D1 | Scrutiny : config, client, parseurs 0.8 / 0.9, `--mock`, Réglages, page du téléphone, tests | moyen |
| D2 | Disques à l'écran : bandeau santé, pilules Serveur · Statistiques, section Disques, fiche disque | moyen |
| S1 | Playback Reporting : détection, client, parseurs, fixtures, Réglages, tests | moyen |
| S2 | Écran Statistiques : périodes, barres, utilisateurs, rangées de jaquettes (nom → Radarr / Sonarr) | moyen à gros |
| F | 4 langues, `SCRUTINY.md`, `CONFIG.md`, README, Nouveautés, `capture-states.sh`, mesures mémoire | petit |

Environ **12 tests** de plus (parseurs Scrutiny 0.8 / 0.9, seuil d'état, relevé ancien,
températures, attributs nommés, Playback Reporting par jour / mois, top titres, champs
interdits). Wrapped : **hors 0.8.0**, sauf décision contraire.

**Test console : un build**, une quinzaine d'étapes : Scrutiny saisi par le téléphone, ligne
verte dans Réglages, bandeau de l'Accueil avec un disque simulé en alerte (fichier de mock
servi par le Linux de test, rien à casser sur ton serveur), section et fiche disque, pilule
Statistiques avec le plugin, lisibilité en 4K, et la mémoire après `/api/summary` lue sur ton
vrai Scrutiny.

---

## 7. Questions pour Vivian

1. **Statistiques** : d'accord pour Playback Reporting au lieu de Jellydash (pas d'API) ?
   Il faudra l'installer sur ton Jellyfin (catalogue officiel) et, pour un récap annuel, régler
   « garder les données » sur « pour toujours ». Il ne reprend pas l'historique de Jellydash.
2. **Adresses** de ton Scrutiny et de ton Jellydash (je ne les ai pas trouvés sur les ports par
   défaut, et je ne balaie pas plus ton serveur) : pour mesurer `/api/summary` en GET.
3. **Emplacement** : pilules Serveur · Statistiques sur l'onglet du serveur multimédia, et
   disques dans Serveur, ça te va ?
4. **Emby** : installer Playback Reporting sur le serveur Emby de test (téléchargement depuis
   le catalogue Emby) pour vérifier les formes, ou Emby sans statistiques en 0.8.0 ?
5. **Seuils de température** (50 / 60 °C disques durs, 70 / 80 °C NVMe) : d'accord ?
