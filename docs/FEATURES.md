# Régie5 — Étude de faisabilité : cinq fonctionnalités

Étude du 11 octobre 2026, avant implémentation. Endpoints vérifiés **en lecture seule (GET)**
sur les serveurs de Vivian (Sonarr 4.0.20, Radarr 6.4.4, Seerr 3.5.0, Jellyfin 12.2.0), sur
l'OpenAPI que sert Jellyfin 12.2.0, et sur les specs officielles de Radarr et Sonarr
(`openapi.json` de leurs dépôts). Les endpoints d'écriture (`DELETE`) sont lus dans les
specs et **jamais appelés**.

Ordre d'implémentation demandé : C, E, A, B, D.

---

## Vue d'ensemble : où ranger tout ça

La barre garde **cinq libellés et l'engrenage**. Proposition :

```
Accueil · Bibliothèque · Activité · Demandes · Jellyfin · ⚙
```

| Fonctionnalité | Emplacement proposé |
|---|---|
| C. Guide des programmes | **Bibliothèque › Guide** (nouvelle pilule). La rangée « À venir » de l'Accueil reste ; ✕ sur une de ses cartes ouvre le Guide à cette semaine |
| E. Journal du serveur | L'onglet **Téléchargements devient « Activité »**, avec deux pilules : **File d'attente** · **Journal** |
| A. Diagnostic de lecture | **Jellyfin** : ✕ sur une session ouvre sa **fiche diagnostic** (avec « Arrêter la lecture » dedans) ; bandeau « Charge du serveur » en haut si Glances est configuré |
| B. Grand ménage | **Bibliothèque › Ménage** (dernière pilule, séparée des autres) |
| D. Ouvrir dans Jelly5 | Bouton **Options (M)** : menu contextuel de l'élément sélectionné |

Bibliothèque aurait alors cinq pilules : **Séries · Films · Guide · Manquants · Ménage**.
Estimation à la taille actuelle : environ 800 px de pilules et 400 px pour Tri et Filtre, sur
1 760 px disponibles. Ça tient sur une ligne (à confirmer à l'écran).

**Bouton Options (M) = menu contextuel** (décision de Vivian) : une petite feuille de verre
qui liste les actions de l'élément sélectionné, Annuler en dernier. Les actions existantes y
figurent aussi (Approuver / Refuser une demande, Arrêter une session), en plus de ✕. Le
pied d'écran affiche « M Options » seulement quand l'élément a au moins une action.

*Alternative pour E* : un sixième onglet « Journal ». Plus direct, mais la barre passe à six
libellés plus l'engrenage, et le mot « Activité » couvre bien « ce qui arrive maintenant »
(file) et « ce qui est arrivé » (journal).

---

## C. Guide des programmes

**Endpoints (vérifiés)**

| Besoin | Endpoint |
|---|---|
| Épisodes de la semaine | Sonarr `GET /api/v3/calendar?start=<lundi>&end=<lundi+7>&includeSeries=true&unmonitored=false` |
| Sorties de films | Radarr `GET /api/v3/calendar?start=…&end=…&unmonitored=false` (déjà utilisé par l'Accueil) |
| Jaquettes | `mediacover` déjà en place |

**État de chaque case** : `hasFile` vrai → *disponible* ; date passée sans fichier →
*manquant* ; date future → *à venir*. Pour un film, la case indique quelle sortie tombe ce
jour-là (cinéma, numérique, physique), comme l'Accueil.

**Interface** : sept colonnes (lundi → dimanche), jour courant mis en évidence, une carte par
épisode / film (jaquette, titre, S01E05, heure, pastille d'état). ←/→ entre les jours, ↑/↓
dans un jour, ✕ ouvre la fiche Bibliothèque existante. **L2/R2 = semaine précédente /
suivante**, comme « page » ailleurs dans la Bibliothèque.

**Risques** : faibles. Une journée très chargée (10 épisodes et plus) défile dans sa colonne.

**Fixtures** : semaines générées à partir des calendriers déjà anonymisés (dates décalées).

---

## E. Journal du serveur

**Endpoints (vérifiés)**

| Source | Endpoint | Champs **lus** | Champs **jamais lus** |
|---|---|---|---|
| Imports Sonarr | `GET /api/v3/history/since?date=…&eventType=downloadFolderImported&includeSeries=true&includeEpisode=true` (137 événements sur 14 jours) | `date`, `eventType`, `seriesId`, `episodeId`, `quality`, série / épisode inclus | `sourceTitle` (nom de release), `data`, `downloadId` |
| Imports Radarr | `GET /api/v3/history/since?date=…&eventType=downloadFolderImported&includeMovie=true` (55 événements) | `date`, `movieId`, `quality`, film inclus | idem |
| Seerr | `GET /api/v1/request?take=50&sort=modified&filter=all` | `createdAt`, `updatedAt`, `status`, `type`, `media.status`, `media.mediaAddedAt`, `requestedBy.displayName`, `modifiedBy.displayName` | e-mails, identifiants, `downloadStatus` |
| Jellyfin | `GET /System/ActivityLog/Entries?minDate=…&hasUserId=true` | `Type`, `Date`, `UserId` (→ nom via `/Users`), `ItemId` (→ titre via `/Items?ids=…`) | **`Name`, `ShortOverview`, `Overview`** : ils contiennent l'adresse IP et le nom de l'appareil |
| Santé | déjà chargée | apparition / disparition pendant la session | — |

**Seerr n'a pas d'historique** : l'API donne l'état actuel de chaque demande, pas la suite
des changements. Le journal en déduit deux événements au plus par demande : « demandée par
X » (`createdAt`), puis l'état actuel (« approuvée par Y », « refusée par Y » ou
« disponible ») à `updatedAt` / `mediaAddedAt`. Un aller-retour intermédiaire ne se voit
pas. À dire tel quel dans l'interface si besoin.

**Jellyfin** : les types gardés sont `VideoPlayback` (« X regarde Y »),
`VideoPlaybackStopped` (« X a fini Y » si la lecture est allée au bout, sinon « a arrêté »),
`SessionStarted` (« X s'est connecté », **sans appareil**), `UserCreated`. Les autres types,
dont `AuthenticationFailed`, qui contient une IP, sont ignorés. La phrase est **construite
par l'app** à partir du type, jamais copiée du serveur.

**Interface** : une liste chronologique (le plus récent en haut), groupée par jour
(« Aujourd'hui », « Hier », « jeu. 9 oct. »). Une pastille de source et des filtres en
pilules : Tout · Imports · Demandes · Jellyfin · Santé. Fenêtre de 14 jours, △ pour
rafraîchir.

**Risques** : volume (plusieurs centaines d'événements) → on n'affiche que les 300 derniers.
Le risque catalogue est couvert : seuls les événements d'import, jamais les « grabbed », et un
test unitaire du même type que `queue_never_keeps_release_names`.

---

## A. Diagnostic de lecture

**Endpoints (vérifiés sur l'OpenAPI de Jellyfin 12.2.0)**

| Besoin | Source |
|---|---|
| Méthode de lecture | `/Sessions` › `PlayState.PlayMethod` (DirectPlay / DirectStream / Transcode) |
| Raisons | `TranscodingInfo.TranscodeReasons` : 28 valeurs possibles (`SubtitleCodecNotSupported`, `ContainerBitrateExceedsLimit`, `VideoRangeTypeNotSupported`…) |
| Codecs source | `NowPlayingItem.MediaStreams[]` : `Type`, `Codec`, `Profile`, `BitRate`, `Width`, `Height`, `VideoRangeType`, `BitDepth`, sous-titre `Codec` + `IsTextSubtitleStream` |
| Pistes choisies | `PlayState.AudioStreamIndex`, `SubtitleStreamIndex` |
| Sortie | `TranscodingInfo.VideoCodec`, `AudioCodec`, `Container`, `Bitrate`, `Width`, `Height`, `Framerate`, `IsVideoDirect`, `IsAudioDirect`, `AudioChannels` |
| Accélération matérielle | `TranscodingInfo.HardwareAccelerationType` (none, amf, qsv, nvenc, vaapi…), sinon `GET /System/Configuration/encoding` (vérifié) |
| Le transcodage suit-il ? | `TranscodingInfo.CompletionPercentage` comparé à la position (`PositionTicks / RunTimeTicks`) |

**« Le transcodage suit-il ? »** : Jellyfin n'expose ni la vitesse de ffmpeg ni la mémoire
tampon du client. On affiche donc **l'avance du transcodage sur la lecture**, en minutes
(`(CompletionPercentage − position %) × durée`) :
- plus de 1 min → « suit » (vert) ;
- moins de 20 s, ou en baisse entre deux rafraîchissements → « à la limite » (orange) / « ne suit pas » (rouge).

Avec le *throttling* actif, l'avance plafonne volontairement (`ThrottleDelaySeconds`, lu dans
la config d'encodage) : on en tient compte pour ne pas crier au loup. Ce n'est qu'une
**estimation**, présentée comme telle.

**Conseils** : une phrase par raison, en FR et en EN, choisie selon le contexte. Exemples :
- `SubtitleCodecNotSupported` avec un sous-titre PGS ou VobSub → « Sous-titres image (PGS)
  incrustés : choisis une piste SRT, ou désactive les sous-titres ».
- `ContainerBitrateExceedsLimit` / `VideoBitrateNotSupported` → « Débit au-dessus de la
  limite du client : baisse la qualité maximale dans le lecteur, ou relève la limite de
  l'utilisateur ».
- `VideoRangeTypeNotSupported` (HDR / Dolby Vision) → « Écran ou client sans HDR / Dolby
  Vision : la vidéo est convertie en SDR ».
- `AudioCodecNotSupported` (TrueHD, DTS…) → « Son converti : le client ne lit pas ce format ».
- `none` comme accélération pendant un transcodage vidéo → « Transcodage sur le processeur :
  activer l'accélération matérielle soulagerait le serveur ».

Les 28 raisons sont couvertes : une phrase générique pour les plus rares.

**Charge du serveur (optionnelle)** : **Glances** (REST v4, lecture seule).
- Endpoint : `GET http://<serveur>:61208/api/4/quicklook` (CPU, mémoire, swap, charge, en un
  seul appel), rafraîchi toutes les 5 s seulement quand l'onglet Jellyfin est affiché.
- Sur le serveur : `pip install glances[web]` puis `glances -w` (ou le conteneur
  `nicolargo/glances` en mode web). Pas d'authentification par défaut, ou `--password`
  (HTTP Basic). **Lecture seule par nature.**
- `config.json` : une entrée `"glances": {"enabled", "url", "username"?, "password"?}`.
  Absente ou désactivée : **la section disparaît** (pas de message d'erreur). Configurée mais
  injoignable : une ligne discrète « Charge du serveur indisponible ».

**Fixtures** : aucune session ne transcode en ce moment sur le serveur. La fixture de
transcodage existante (synthétique, phase 2) est enrichie selon le schéma OpenAPI 12.2.0 :
PGS, débit, HDR, transcodage qui ne suit pas. Fixture Glances synthétique, d'après la doc.

**Risques** : moyens. L'estimation « suit / ne suit pas » reste une heuristique, à valider en
recette avec une vraie lecture qui transcode. Glances est un service de plus à installer
(optionnel).

**À vérifier chez Vivian** : Glances tourne-t-il déjà sur le serveur ? Sinon, on livre la
section prête et masquée.

---

## B. Grand ménage

**Endpoints (vérifiés en GET, `DELETE` lus dans les specs)**

| Besoin | Endpoint |
|---|---|
| Utilisateurs | Jellyfin `GET /Users` (11 sur le serveur) → `Id`, `Name`, `Policy.IsDisabled`, `LastActivityDate` |
| Vu / dernière lecture / favori, par utilisateur | Jellyfin `GET /Items?userId=<id>&Recursive=true&IncludeItemTypes=Movie,Series&Fields=ProviderIds` → `UserData.Played`, `LastPlayedDate`, `IsFavorite`, `PlayCount`, `UnplayedItemCount` (séries) ; `ProviderIds` (Tmdb, Imdb ; Tvdb pour les séries) |
| Taille et identifiants | Radarr `GET /api/v3/movie` → `sizeOnDisk`, `tmdbId`, `imdbId` ; Sonarr `GET /api/v3/series` → `statistics.sizeOnDisk`, `tvdbId`, `tmdbId`, `imdbId`, `ended` |
| Supprimer un film | Radarr `DELETE /api/v3/movie/{id}?deleteFiles=true&addImportExclusion=…` |
| Supprimer une série | Sonarr `DELETE /api/v3/series/{id}?deleteFiles=true&addImportListExclusion=…` |
| Exclusions existantes | Radarr `GET /api/v3/exclusions` ; Sonarr `GET /api/v3/importlistexclusion` (vides aujourd'hui) |

**Rapprochement** : Radarr ↔ Jellyfin par TMDB, puis IMDb ; Sonarr ↔ Jellyfin par TVDB,
puis TMDB, puis IMDb. Un élément sans correspondance n'est **jamais** proposé : sans
historique de visionnage, on ne peut pas juger.

**Critères** (réglables dans l'écran, avec les valeurs par défaut) :
- Vu par : **tous les utilisateurs actifs** / au moins un. *Actif* = compte non désactivé et
  activité Jellyfin dans les 6 derniers mois. Sans ce garde-fou, un compte de test ou
  abandonné empêcherait tout « vu par tous ».
- Dernière lecture il y a plus de **6** mois (3, 6, 12, 24).
- Jamais mis en favori par personne : toujours actif, non réglable.
- Séries : terminées (`ended`) **et** entièrement vues (`Played`, `UnplayedItemCount = 0`)
  selon le même « vu par ».
- Exclus : tout ce qui est dans la liste « Garder ».

**Désactivé par défaut** (règle « communauté ») : la pilule Ménage n'apparaît pas tant que
l'utilisateur ne l'a pas activé dans **Réglages › Ménage**. L'activation passe par une
feuille d'avertissement : « Le Ménage peut supprimer des films et des séries **avec leurs
fichiers**, définitivement. Chaque suppression demande deux confirmations. », Annuler par
défaut. Le choix est rangé dans un petit fichier géré par l'app (`settings.json`, à côté de
`config.json`), pas dans `config.json` que l'utilisateur édite. On le désactive au même
endroit.

**Liste** : candidats triés par taille décroissante, avec en tête « **N éléments · X Go
récupérables** ». Chaque ligne : jaquette, titre, année, taille, qui l'a vu, dernière lecture.

**Actions** (✕ ou Options) :
- **Garder** : ajouté à `~/.config/regie5/keep.json` (sur PS5, le dossier de données de
  l'app). Réversible depuis une sous-liste « Gardés ».
- **Supprimer** : **double confirmation**.
  1. Feuille 1 : titre, année, taille, « Les fichiers seront supprimés définitivement. »,
     boutons Annuler (focus) / Continuer.
  2. Feuille 2 : « Supprimer définitivement *Titre* (X Go) ? », Annuler (focus) /
     Supprimer (rouge).

  Une seule suppression à la fois : pendant l'appel, les autres actions sont bloquées. Pas
  de sélection multiple. Après : note de résultat, liste rechargée.
- En `--mock` : la suppression est journalisée et ne fait rien (comme les autres actions).

**Proposition : exclusion d'import** (à valider)

| Option | Ce qui se passe |
|---|---|
| **1. Recommandée : supprimer + exclusion d'import** (`addImportExclusion=true` / `addImportListExclusion=true`, toujours) | Le film / la série disparaît de Radarr/Sonarr avec ses fichiers et entre dans leur liste d'exclusions : **aucune liste d'import** (Trakt, IMDb, listes TMDB…) ne le rajoutera. Radarr ne le retéléchargera pas, puisqu'il ne le suit plus. Une **nouvelle demande Seerr reste possible** : c'est un choix humain, pas un retour automatique. L'exclusion se retire dans Radarr/Sonarr si besoin. La feuille 1 l'annonce : « Ne sera pas rajouté par les listes d'import. » |
| 2. Supprimer sans exclusion | Plus simple, mais une liste d'import peut le rajouter et le retélécharger. |
| 3. Supprimer seulement les fichiers et ne plus surveiller (`DELETE /api/v3/moviefile/{id}` + `monitored=false`) | Garde la fiche dans Radarr/Sonarr (historique, profil) et libère l'espace. Mais la fiche reste visible partout comme « fichier absent », ce qui encombre Manquants et la Bibliothèque. |

Je recommande l'option 1, sans réglage : c'est le comportement par défaut le plus sûr pour
« libérer de l'espace » sans surprise.

**Seerr après une suppression** : le média reste « disponible » dans Seerr jusqu'à sa
prochaine synchronisation avec Jellyfin, qui le repasse en non disponible. Régie5 ne touche
pas à Seerr (pas d'écriture de plus). Vérifié sur Seerr 3.5.0 (13 octobre 2026) : c'est la
tâche « Media Availability Sync » qui marque le titre `DELETED` en gardant ses demandes ;
`POST /api/v1/media/{id}/deleted`, essayé un temps, ne fait rien (statut inconnu de la route,
réponse 200 quand même) et a été retiré.

**Risques** : **élevés (destructif)**.
- Rapprochement faux : un mauvais identifiant ferait supprimer le mauvais titre. Parade : la
  feuille affiche le titre, l'année et la taille **lus dans Radarr/Sonarr**, c'est-à-dire
  ce qui sera réellement supprimé. Un test unitaire couvre le rapprochement.
- « Vu par tous » dépend des comptes Jellyfin : à vérifier en recette sur la liste réelle.

**Aucune suppression réelle** sans l'accord tapé de Vivian, sur un élément qu'il choisit.

---

## D. Ouvrir dans Jelly5

**Ce que Jelly5 accepte aujourd'hui** (lu dans Jelly5, commit `552b866`) : **rien**.
- `int main()` ne prend aucun argument.
- `param.json` déclare `"gameIntent": {"permittedIntents": [{"intentType": "launchActivity"}]}`,
  mais le code ne lit aucun *intent* ni paramètre de lancement.
- Ses données sont dans `/download0/jelly5/`, le dossier propre à l'app. Une autre app ne
  peut probablement pas y écrire.

**Ce que permet le système PS5 (homebrew)** : lancer une app par son Title ID avec
`sceLncUtilLaunchApp` (repli : `sceSystemServiceLaunchApp`, dans `libSceSystemService`),
qui accepte des arguments (`argv`). Des outils homebrew l'utilisent pour lancer des jeux
(PS5-Upload-Suite, ps5upload). Mais **depuis un processus d'app**, l'appel peut être refusé
(erreur `0x80940005` signalée). ps5upload le contourne en passant par le processus
SceShellUI. **À tester en phase 6**, sur la console : c'est la principale incertitude.

**Ce qu'il faudrait ajouter à Jelly5 (proposition de PR)** :
1. Au démarrage, lire une demande d'ouverture, par deux voies :
   - les **arguments** : `--open-item <ItemId> --server <ServerId>`, si `sceLncUtilLaunchApp`
     les transmet à `main(argc, argv)` ;
   - un **fichier de passage** `/data/jelly5/open.json`
     (`{"serverId", "itemId", "action": "open"|"play", "createdAt"}`), lu puis **supprimé**,
     ignoré s'il a plus de 60 s. Il marche même si les arguments ne passent pas.
2. Si le serveur du compte courant a le même `ServerId` (celui de `/System/Info`), ouvrir la
   fiche de l'élément. Ne pas lancer la lecture d'office : c'est plus sûr, et `play` peut
   venir plus tard.
3. Sinon, démarrer normalement et l'indiquer par une note.

**Côté Régie5** :
- Options › « Ouvrir dans Jelly5 » sur :
  - une fiche Bibliothèque, si l'élément existe dans Jellyfin (rapprochement par TMDB/TVDB,
    comme le Ménage) ;
  - un ajout récent (identifiant Jellyfin direct) ;
  - une demande disponible (`media.jellyfinMediaId`, vérifié présent dans Seerr).
- Linux : le lancement est **simulé**. On journalise
  `jelly5: launch PPSA99505 open <ItemId> on <ServerId>` et une note s'affiche.
- PS5 : derrière `platform::launch_app(title_id, args)`, écrite en phase 6.

**Risques** : **élevés, hors de notre main** (API système, PR à faire accepter). La partie
Régie5 est petite et isolée : si le lancement n'aboutit pas sur PS5, l'entrée de menu est
masquée et rien d'autre n'est touché.

---

## Ce que la règle « communauté » change pour ces cinq fonctionnalités

- **Adresses** : tous les appels passent par `Services::url`, qui accepte une `url` avec
  chemin (`https://domaine/radarr`, `urlBase` des *arr). Glances aussi (`url` avec chemin,
  derrière un reverse proxy). Sous Linux, HTTPS fonctionne déjà avec libcurl. Les
  certificats auto-signés, fréquents chez soi, demanderont une option par service
  (`"insecure": true`), à ajouter avec l'étape sur les adresses (voir la roadmap).
- **Versions** : chaque fonctionnalité dit ce qu'elle exige. Le diagnostic a besoin de
  `TranscodingInfo.HardwareAccelerationType` (Jellyfin 10.9 et plus : sinon « accélération
  inconnue ») ; le Journal et le Guide utilisent des endpoints `/api/v3` présents depuis
  Sonarr v3 / Radarr v3. Un champ absent donne « inconnu », jamais une erreur.
- **Plusieurs instances** : le Ménage et le Journal **parcourent toutes les instances**
  Radarr/Sonarr dès que la config les décrit. Un film présent dans Radarr et dans Radarr 4K
  apparaît **une fois par instance** dans le Ménage (ce sont deux copies, sur deux disques
  peut-être), avec le nom de l'instance.

**Proposition de structure de config** (rétrocompatible) : un service peut être un objet
(comme aujourd'hui) ou une liste d'instances nommées.

```json
"radarr": [
  { "name": "Radarr",    "url": "http://192.168.1.10:7878", "apiKey": "…" },
  { "name": "Radarr 4K", "url": "https://media.example/radarr4k", "apiKey": "…" }
]
```

## Annexe — Étude : configurer sans FTP (décision en phase 6)

Aujourd'hui, il faut déposer `config.json` sur la console (FTP). Trois pistes, cumulables :

| Piste | Comment | Ce qui existe déjà | Risques |
|---|---|---|---|
| **Écran de configuration dans l'app** | Réglages › Ajouter un service : type, adresse, clé, saisis au **clavier système PS5** (`sceImeDialog*`) | Jelly5 l'utilise déjà (`platform/ime.cpp`), avec une structure de paramètres vérifiée sur console | Taper une clé de 32 caractères à la manette reste pénible : utile pour l'adresse, moins pour la clé |
| **QR code → page locale** | Régie5 ouvre un petit serveur HTTP sur le réseau local (par exemple le port 8085) et affiche un QR `http://<ip-ps5>:8085/` ; depuis le téléphone, un formulaire pour coller adresses et clés, protégé par un **code à 6 chiffres** affiché sur la TV ; le serveur se ferme à la fin | Génération du QR : `qrcodegen` (Nayuki, MIT), déjà dans Jelly5 ; des homebrews PS5 écoutent sur le réseau (serveurs FTP, ps5upload) | À vérifier sur console : une **app** (pas un payload) peut-elle ouvrir un port ? Sécurité : code à usage unique, réseau local seulement, serveur actif seulement tant que l'écran est ouvert |
| **Quick Connect Jellyfin** | Pour Jellyfin seulement : Régie5 affiche un code ; l'utilisateur l'approuve dans Jellyfin, depuis son téléphone ; Régie5 reçoit un jeton **sans clé à taper** | Jelly5 le fait déjà pour se connecter (`ui/login.cpp`) | Le jeton est celui d'un utilisateur : il faut un compte administrateur pour les sessions, le scan et les tâches ; il faut aussi que Quick Connect soit activé sur le serveur |

Il n'y a pas d'équivalent pour Sonarr, Radarr et Seerr : seule la clé d'API marche (ou, pour
Seerr, une connexion avec un compte Jellyfin administrateur, à étudier). La piste QR +
page locale est donc la plus complète, si l'ouverture d'un port est possible.

## Ce qu'il faut installer ou configurer

| Quoi | Où | Obligatoire ? |
|---|---|---|
| Rien pour C, E, B | — | — |
| Glances en mode web (port 61208) | serveur | Non : sans Glances, la section « Charge du serveur » disparaît |
| Entrée `"glances"` dans `config.json` | Régie5 | Seulement si Glances est installé |
| PR « ouverture d'un élément » | Jelly5 | Pour D sur PS5 ; sous Linux, simulation |

## Mises à jour prévues des règles du projet

À faire à la fin (comme demandé) : la Bibliothèque n'est plus strictement en lecture (le
Ménage supprime, avec double confirmation) ; Glances devient un service optionnel autorisé ;
l'onglet Téléchargements devient Activité ; Options = menu contextuel ; nouvelles règles de
vie privée du Journal (aucune IP, aucun appareil, aucun texte du journal Jellyfin).

## Décisions demandées à Vivian

1. **Exclusion d'import** : option 1 (supprimer + exclusion, toujours) ?
2. **Emplacements** : Guide dans Bibliothèque ; Téléchargements → **Activité** (File
   d'attente · Journal) ; Ménage dans Bibliothèque ; diagnostic dans Jellyfin. Ou un onglet
   « Journal » à part ?
3. **Glances** : déjà installé sur le serveur ? Sinon, on le laisse optionnel et masqué.
4. **« Vu par tous »** : seulement les comptes actifs (non désactivés, activité dans les
   6 derniers mois) ?
5. **Plusieurs instances** : j'adopte la structure ci-dessus (objet **ou** liste
   d'instances). Proposition de calendrier :
   - pendant la 4 bis, la lecture de la config l'accepte, et le Ménage et le Journal
     parcourent toutes les instances ;
   - les écrans existants (Accueil, Bibliothèque, Activité) passent au multi-instances dans
     une étape dédiée, juste après la 4 bis ;
   - d'ici là, Réglages affiche chaque instance avec sa pastille, et indique « seule la
     première est affichée dans ces écrans pour l'instant ».
