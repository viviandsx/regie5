# Régie5 — Plex : étude de faisabilité (0.6.0)

Étude du 17 octobre 2026, **sans code**. But : Plex Media Server comme troisième serveur
multimédia, à côté de Jellyfin et d'Emby (un seul à la fois).

Sources lues (rien de mémoire) :

- **[S1] Documentation officielle de l'API de Plex Media Server** : https://developer.plex.tv/pms/
  (spécification OpenAPI « Plex Media Server » **1.2.3**, publiée par Plex depuis septembre 2025 ;
  les sections « API Info », « Authenticating with Plex » et chaque opération). Plex dit lui-même
  qu'elle ne couvre pas encore tout ce que fait le serveur.
- **[S2] Guide officiel « Authenticating with Plex »** : https://forums.plex.tv/t/authenticating-with-plex/609370
  (repris dans [S1]).
- **[S3] Articles d'aide de Plex** : « Finding an authentication token / X-Plex-Token »
  (204059436), « Authentication for local network access » (200890058), « Using
  Hardware-Accelerated Streaming » (115002178853), sur https://support.plex.tv/articles/.
- **[S4] Image Docker officielle** `plexinc/pms-docker` (README du dépôt
  https://github.com/plexinc/pms-docker, tags de Docker Hub lus le 17 octobre 2026).
- **[S5] Code de Seerr** (branche `develop`) : `src/utils/plex.ts` (connexion Plex faite **dans
  le navigateur**), `server/api/plexapi.ts`, `server/lib/scanners/plex/index.ts`,
  `server/entity/Media.ts`, `server/lib/availabilitySync.ts`.
- **[S6] Code de Tautulli** (branche `nightly`, `plexpy/pmsconnect.py`) : la référence de fait
  pour les sessions Plex (`TranscodeSession`), que [S1] ne décrit pas dans `/status/sessions`.
- **[S7] python-plexapi** (`plexapi/server.py`) : `/accounts`, `/butler`, historique.
- **[S8] Mesure directe** (17 octobre 2026, depuis le laptop, requêtes `OPTIONS` seulement,
  aucun compte) : en-têtes CORS de `plex.tv` et `clients.plex.tv`.

Ce que [S1] ne documente pas mais que [S5], [S6] ou [S7] utilisent depuis des années est
marqué **« hors contrat »** : à vérifier sur le serveur de test, et jamais indispensable.

---

## 0. En bref

| Sujet | Verdict |
|---|---|
| Faisabilité globale | **Oui.** Tous les écrans de Régie5 ont un équivalent, sauf deux réductions (Journal, Ménage). |
| Authentification hors ligne | **Oui, par le téléphone** : le téléphone (qui a internet) fait la connexion plex.tv ; la console ne parle qu'au serveur Plex du réseau local. CORS de plex.tv ouvert (`*`, mesuré). |
| Compte nécessaire | Le **propriétaire** du serveur pour presque tout : Plex n'a pas de rôle « administrateur » pour un autre compte. |
| JSON | Oui partout avec `Accept: application/json` [S1]. |
| Images | `/photo/:/transcode?width=&height=` [S1] ; **à mesurer** sur de grandes images (leçon d'Emby), plafond de décodage gardé dans tous les cas. |
| Plex Pass | Nécessaire pour l'**accélération matérielle** [S3] et, d'après [S6], pour **arrêter une lecture**. Régie5 le lit (`myPlexSubscription`) et le dit. |
| Journal | **Réduit** : Plex n'a pas de journal d'activité ; seulement les lectures terminées (historique). |
| Ménage | **Réduit et prudent** sans Tautulli (§ 2.10) ; décision à prendre. |
| Serveur de test | Image officielle, **163 Mo** compressés (PMS 1.43.4) ; **sans compte Plex** possible pour l'essentiel. |
| Travail | **6 à 7 séances** + 1 essai console, puis un zip de test pour un vrai utilisateur de Plex. |

---

## 1. Authentification — le point critique

### 1.1 Ce que Plex attend

Chaque requête vers le serveur porte [S1, « Headers », « Auth »] :

- `X-Plex-Token` : le jeton (« obtained from plex.tv ») ;
- `X-Plex-Client-Identifier` : identifiant opaque de l'app, tiré une fois et gardé
  (« A random string or UUID is sufficient » [S2]) ;
- conseillés : `X-Plex-Product` (« Regie5 »), `X-Plex-Version`, `X-Plex-Platform`,
  `X-Plex-Device-Name`. Le nom de produit s'affiche dans les « appareils autorisés » du compte.

Tous les `X-Plex-*` peuvent aussi passer en paramètre d'URL [S1] : Régie5 les met **toujours
en en-tête** (un jeton dans une URL finirait dans un journal).

Un serveur « réclamé » (lié à un compte) exige un jeton pour tout [S3 200890058]. La
vérification du jeton est faite **par le serveur**, pas par la console : que la PS5 soit hors
ligne ne gêne pas l'usage d'un jeton, seulement son **obtention**.

### 1.2 Deux sortes de jetons (important pour une console hors ligne)

[S1, « Authenticating with Plex »] décrit :

- **JWT** (« recommandé ») : clé Ed25519 propre à l'appareil, jeton **valable 7 jours**, à
  rafraîchir auprès de `clients.plex.tv` (nonce, JWT signé). **Impossible pour une console
  hors ligne** : il faudrait refaire la connexion chaque semaine.
- **Jeton classique** (« Legacy ») : obtenu par le même PIN, **sans durée de vie annoncée** ;
  invalidé seulement si l'utilisateur retire l'appareil, ou change son mot de passe avec
  « déconnecter les appareils » [S3 204059436]. Plex ne le dit pas abandonné, mais le présente
  comme l'ancienne méthode.

→ Régie5 utilise le **jeton classique**. **Risque** : si Plex retire un jour les jetons
classiques, Plex demandera une reconnexion hebdomadaire depuis le téléphone (assumable, mais
à surveiller). Un jeton refusé (401) donne « Session Plex terminée : reconnectez-vous depuis
le téléphone » (le `Error::Revoked` de Quick Connect).

### 1.3 Les options étudiées

**A. Coller le jeton dans la page du téléphone.** Faisable (un champ comme la clé d'API).
Mais : la seule méthode officielle pour le trouver (Plex Web › un élément › « Obtenir des
infos » › « Afficher le XML », lire `X-Plex-Token=` dans l'adresse) donne, selon Plex, un
jeton **« valid temporarily »** [S3 204059436], et c'est fastidieux sur téléphone (vue XML,
URL à copier). Utile en **secours** (avec un guide clair), pas comme méthode principale.

**B. PIN plex.tv fait par le téléphone, depuis notre page locale.** **Faisable, recommandé.**

- Mesuré le 17 octobre 2026 [S8] : `plex.tv/api/v2/pins`, `clients.plex.tv/api/v2/pins`,
  `plex.tv/api/v2/user` et `clients.plex.tv/api/v2/resources` répondent au pré-vol CORS
  d'une origine du réseau local (`Origin: http://192.168.1.50:8085`) par
  `access-control-allow-origin: *`, méthodes GET/POST, et autorisent les en-têtes `accept`,
  `x-plex-product`, `x-plex-client-identifier`, `x-plex-version`, `x-plex-token`.
- C'est exactement ce que fait **Seerr** dans le navigateur [S5 `src/utils/plex.ts`] :
  `POST https://plex.tv/api/v2/pins?strong=true` avec les en-têtes `X-Plex-*`, ouverture de
  `app.plex.tv/auth`, puis `GET https://plex.tv/api/v2/pins/{id}` jusqu'au jeton.
- Une page `http://` qui appelle `https://` n'est pas du contenu mixte bloqué (c'est l'inverse
  qui l'est) ; une page du réseau privé qui appelle internet n'est pas visée par les
  restrictions « Private Network Access » (elles protègent le réseau privé contre internet).

Déroulé proposé (Réglages › « Depuis le téléphone », ou l'assistant avec le choix Plex) :

1. La console lit `GET /identity` du serveur Plex : **sans jeton** [S1 : `security: [{}]`], il
   donne `machineIdentifier`, `version`, `claimed`. Elle passe à la page l'identifiant du
   serveur et son propre `X-Plex-Client-Identifier` (tiré une fois, `settings.json`, comme le
   `deviceId` de Quick Connect).
2. Sur le téléphone, bouton « Se connecter à Plex » : la page crée un PIN fort
   (`POST https://plex.tv/api/v2/pins?strong=true`, en-têtes `X-Plex-Product: Regie5`,
   `X-Plex-Client-Identifier: <celui de la console>`), puis ouvre dans un nouvel onglet
   `https://app.plex.tv/auth#?clientID=…&code=…&context%5Bdevice%5D%5Bproduct%5D=Regie5`
   (sans `forwardUrl` : on ne compte pas sur un retour vers une adresse `http://` locale).
3. La page interroge `GET https://plex.tv/api/v2/pins/{id}` une fois par seconde
   (« once per second » [S2]) jusqu'à `authToken` (ou l'expiration du PIN).
4. Avec ce jeton de compte, `GET https://clients.plex.tv/api/v2/resources?includeHttps=1`
   [S1, « Talking to PMS »] : la page cherche **le serveur dont l'identifiant est celui donné
   par la console**, et prend **son** `accessToken` (le jeton propre à ce serveur pour ce
   compte) et `owned` (propriétaire ou non). Serveur absent : « Ce compte Plex n'a pas accès à
   ce serveur ».
5. La page envoie à la console **seulement** ce jeton de serveur, `owned` et le nom du compte
   (même code à 6 chiffres que la page actuelle). **Le jeton du compte ne quitte jamais le
   téléphone.** La console l'écrit dans `config.json`, relit et teste.

Conséquences : la console ne touche jamais internet ; Régie5 apparaît dans les appareils
autorisés du compte sous le nom « Regie5 » ; l'identifiant client envoyé à plex.tv est celui
que la console utilisera avec le serveur. Pas de PIN à 4 caractères (`plex.tv/link`) : la
console ne peut pas le créer elle-même, hors ligne.

Limite : une seule connexion à la fois par PIN ; plex.tv limite le débit des PIN (429) [S1] :
un bouton, pas de nouvelle tentative automatique en boucle.

**C. « Réseau local sans authentification » de Plex Media Server.** Réglages du serveur ›
Réseau › « List of IP addresses and networks that are allowed without auth » (préférence
cachée `allowedNetworks`), IP ou IP/masque séparés par des virgules [S3 200890058]. Plex
avertit que c'est risqué pour la sécurité du serveur et du réseau. Une adresse listée obtient
un accès **sans jeton**, d'après plusieurs retours de la communauté avec les droits du
propriétaire et sans les restrictions des comptes enfants. L'image Docker le recommande
« seulement si vous ne connectez pas votre serveur » [S4, `ALLOWED_NETWORKS`].
→ **Pris en charge** (une adresse sans jeton est acceptée : Régie5 essaie, et Réglages dit
« sans jeton : accès réseau local autorisé par le serveur »), **jamais conseillé** dans la doc
utilisateur, sinon avec l'avertissement de Plex. Utile surtout pour le serveur de test (§ 4).

### 1.4 Recommandation

1. **B (PIN depuis le téléphone)** comme méthode principale, dans l'assistant et Réglages.
2. **A (jeton collé)** en secours dans la page du téléphone, champ « Jeton Plex (avancé) »,
   avec la note de Plex sur la durée limitée de ce jeton.
3. **C** accepté sans être conseillé.

### 1.5 Compte propriétaire ou non, fonction par fonction

Plex n'a **pas d'administrateur autre que le propriétaire** : la spécification [S1] marque
`admin` les opérations réservées. Un compte partagé ou un compte de Plex Home donne les
« écrans réduits » comme Quick Connect avec un compte non administrateur (ligne orange
« Plex · <nom> · pas le propriétaire », messages « Demande le compte propriétaire du serveur
Plex » dans chaque section concernée : le `Error::NeedsAdmin` existant).

| Fonction | Opération | Propriétaire | Compte partagé |
|---|---|---|---|
| Test, version | `GET /identity` (sans jeton), `GET /` | oui | oui |
| Ajouts récents, images | `/library/sections/{id}/all`, `/photo/:/transcode` | oui | ses bibliothèques partagées |
| Sessions, diagnostic | `GET /status/sessions` | **admin** | non |
| Arrêter une lecture | `POST /status/sessions/terminate` | **admin** (+ Plex Pass, § 2.3) | non |
| Scanner | `POST /library/sections/{id}/refresh` | **admin** | non |
| Tâches | `GET /butler` | **admin** | non |
| Activités en cours | `GET /activities` | toutes | les siennes |
| Bibliothèques | `GET /library/sections/all` | oui | les siennes |
| Historique (Journal, Ménage) | `GET /status/sessions/history/all` | **tous les comptes** | le sien seulement |
| Comptes du serveur | `GET /accounts` (hors contrat) | à vérifier | à vérifier |

---

## 2. Correspondance des fonctions

Version minimale proposée : **PMS 1.41.9**, la première version de l'API publiée (« 1.0.0
(Supported in PMS >= 1.41.9) » [S1, « API Versioning »]). Les opérations utilisées existent
depuis bien avant (Tautulli et Seerr s'en servent), mais rien ne permet de le vérifier : plus
ancien = pastille orange « Version trop ancienne » (le mécanisme de `core/versions`), les
écrans essaient quand même. Régie5 n'envoie pas `X-Plex-Pms-Api-Version` (version 0.0 par
défaut, l'API historique), sauf si un écart l'impose sur le serveur de test.

### 2.1 Infos et version du serveur

- `GET /identity` (sans jeton) : `machineIdentifier`, `version`, `claimed` [S1].
- `GET /` (jeton) : `friendlyName`, `version`, `platform`, `myPlexSubscription` (Plex Pass),
  `transcoderActiveVideoSessions`, `myPlexUsername` [S1]. **Test de connexion** de Réglages :
  `/identity` puis `/` (le premier dit si l'adresse est un Plex, le second si le jeton
  passe ; 401 → « Jeton refusé »).
- Rien de nouveau en version minimale : 1.41.9 (ci-dessus).

### 2.2 Sessions en cours et diagnostic de lecture

- `GET /status/sessions` (**admin**) [S1] : `Metadata[]` avec `title`, `type`,
  `grandparentTitle`, `parentIndex`/`index`, `viewOffset`, `duration`, `User.title`,
  `Player` (`title`, `platform`, `state`, `local`), `Session.id`, `Session.bandwidth`,
  `Media[].Part[].Stream[]` (`streamType` 1/2/3, codec, résolution, `decision`).
- **`TranscodeSession` n'est pas décrit dans `/status/sessions` par [S1]** (seulement dans la
  file de téléchargement, avec `throttled`, `progress`, `speed`, `transcodeHwRequested`,
  `transcodeHwFullPipeline`). **Hors contrat** mais lu par Tautulli [S6] dans chaque session :
  `videoDecision` / `audioDecision` / `subtitleDecision` (`copy`, `transcode`, `burn`…),
  `speed`, `throttled`, `progress`, `maxOffsetAvailable`, `container`, `videoCodec`,
  `audioCodec`, `protocol`, `transcodeHwRequested`, `transcodeHwDecoding(Title)`,
  `transcodeHwEncoding(Title)`, `transcodeHwFullPipeline`. Pas de `TranscodeSession` = lecture
  directe.

Ce que devient la fiche diagnostic :

| Fiche Régie5 (Jellyfin) | Plex |
|---|---|
| Méthode (Direct Play / Direct Stream / Transcodage) | décisions vidéo + audio : tout `copy` / absent = lecture directe ; vidéo `copy`, audio `transcode` = « flux direct » ; vidéo `transcode` = transcodage. Même présentation. |
| Source / envoyé au client | flux source dans `Media/Part/Stream` ; cible dans `TranscodeSession` (`videoCodec`, `audioCodec`, `container`) |
| Accélération matérielle | `transcodeHwDecoding` / `transcodeHwEncoding` (nom du matériel dans `…Title`) ; demandée mais absente → conseil. Plex Pass requis [S3 115002178853] : conseil propre à Plex (« le propriétaire du serveur doit avoir un Plex Pass ») si `myPlexSubscription` est faux. |
| « Le transcodage suit-il ? » | `speed` (≥ 1 = suit), `throttled` (en avance, ralenti exprès), `maxOffsetAvailable` contre `viewOffset` : même estimation qu'avec Jellyfin, dite comme telle. |
| **Raisons** (28 raisons Jellyfin → 11 conseils) | **Plex ne donne pas de raison dans la session.** Raisons **déduites** et dites comme telles : sous-titres `burn` → « sous-titres incrustés » ; codec cible ≠ codec source → « format non lu par l'appareil » ; même codec, débit / résolution plus bas → « qualité choisie dans l'appareil ou limite de débit ». Le texte de décision détaillé de Plex n'existe que dans la réponse de `/…/transcode/universal/decision` (au moment de la lecture, par le client), pas dans les sessions. |
| Vie privée | comme avec Jellyfin : l'onglet montre l'appareil (`Player.title`, `Player.platform`) des sessions en cours, **jamais** `Player.address` ni `remotePublicAddress` (adresses IP), qui ne sont pas lus (test anti-fuite étendu). |

Version minimale : 1.41.9 (champs `TranscodeSession` hors contrat, présents bien avant d'après
[S6]).

### 2.3 Arrêter une lecture

- `POST /status/sessions/terminate?sessionId=<Session.id>&reason=<texte>` (**admin**,
  réponses 200 / 401 / 403 / 404) [S1]. `reason` est montré à la personne qui regarde :
  phrase fixe de l'app (« Lecture arrêtée par l'administrateur du serveur », langue de
  l'app).
- **Plex Pass** : [S1] n'en dit rien, mais Tautulli refuse l'arrêt sans Plex Pass
  (« No Plex Pass subscription » [S6]) et un retour du forum Plex va dans le même sens. Régie5
  lit `myPlexSubscription` dans `GET /` : sans Plex Pass, l'action reste proposée mais la
  confirmation le dit (« Plex demande sans doute un Plex Pass »), et un refus (401 / 403)
  donne un message clair. **Non vérifiable sur le serveur de test** (pas de Plex Pass) : à
  confirmer par un utilisateur de Plex.
- Écriture nouvelle → liste des écritures autorisées des règles du projet, confirmation, Annuler
  par défaut.

### 2.4 Scanner les bibliothèques

- `POST /library/sections/{id}/refresh` (**admin**) pour chaque bibliothèque [S1] (sans
  `force` : recherche des fichiers nouveaux, comme le « Scan » de Jellyfin). [S1] a aussi
  `POST /library/sections/refresh`, décrit comme « refresh all section metadata » (avec
  `force`) : ambigu, donc pas utilisé.
- Progression visible dans `GET /activities` (activités du serveur, avec `progress`) : la
  note « Scan lancé » pourra suivre l'avancement, comme avec Jellyfin.
- Écriture nouvelle → règles du projet (écritures autorisées).

### 2.5 Tâches planifiées

- **Butler** : `GET /butler` (**admin**) [S1] : `name`, `title`, `description`, `interval`
  (jours), `enabled`, `scheduleRandomized`. **Pas d'état ni de date de dernière exécution**
  (Jellyfin en donne).
- Section « Tâches planifiées » avec Plex : titre, « tous les N jours », activée ou non, et
  « en cours · N % » quand une activité correspondante est dans `GET /activities` (à vérifier
  sur le serveur de test : quelles tâches du Butler y apparaissent).
- Lecture seule (Régie5 ne lance pas de tâche, comme avec Jellyfin).

### 2.6 Ajouts récents et images

- **Ajouts récents** : par bibliothèque de films et de séries,
  `GET /library/sections/{id}/all?type=1&sort=addedAt:desc` (films) et `type=4` (épisodes,
  regroupés par série comme aujourd'hui), avec `X-Plex-Container-Start=0` /
  `X-Plex-Container-Size=20` [S1, « Pagination », « Media Queries »]. `/library/recentlyAdded`
  et `/hubs/home/recentlyAdded` existent (Tautulli [S6]) mais sont **hors contrat**.
- **Images** : `GET /photo/:/transcode?url=<thumb>&width=W&height=H&minSize=1&upscale=0`
  [S1, documenté depuis l'API 1.1.0 / PMS 1.42.0, l'opération existe bien avant d'après S6].
  [S1] : « By default (false) the image is scaled to fit within the width/height specified »,
  `upscale` à 0 par défaut, `format` jpg par défaut, `quality`. `url` est le chemin local de
  l'image (`thumb` / `art` de l'élément, ex. `/library/metadata/123/thumb/1699999999`), encodé ;
  le jeton reste dans l'en-tête de la requête externe.
- **Leçon d'Emby (0.5.1)** : Emby ignorait `fillWidth` et envoyait l'original. Pour Plex :
  1. vérifier sur le serveur de test, avec des affiches **2000×3000** et des fonds **3840×2160**
     dessinés par ffmpeg (taille d'un vrai serveur), que la réponse a bien la taille demandée
     (dimensions lues dans le JPEG reçu) ; même mesure du tas que pour Emby (`LD_PRELOAD`,
     scratchpad) ;
  2. **le plafond de décodage reste** quoi qu'il arrive (`art::decode` : au-delà de 4096×2304
     pas de décodage, au-delà de 1920×1080 une seule à la fois) : il ne dépend pas du serveur ;
  3. jamais l'image d'origine (`/library/metadata/{id}/thumb/…` seule) : toujours par
     `/photo/:/transcode`.

### 2.7 Journal d'activité

- **Plex n'a pas de journal d'activité** comparable à celui de Jellyfin (aucune opération de
  [S1] ; connexions, créations de comptes et mots de passe refusés ne sont pas exposés).
  `/activities` ne liste que les tâches en cours du serveur ; les notifications
  (`/:/websocket/notifications`, `/:/eventsource/notifications`) ne sont qu'en direct.
- Ce qui reste : **l'historique des lectures**, `GET /status/sessions/history/all` [S1]
  (propriétaire : tous les comptes), paginé, filtrable par `viewedAt>=…`. Champs lus :
  `type`, `title`, `grandparentTitle`, `viewedAt`, `accountID`, `ratingKey`. **Jamais**
  `deviceID` (ni rien sur l'appareil). Phrase rédigée par l'app : « Léa a regardé « titre » ».
- Filtre « Jellyfin » du Journal → « Plex » (lectures seulement). Les imports, demandes Seerr
  et alertes santé ne changent pas.
- Limite à dire dans l'écran (une ligne grise) : « Plex ne note que les lectures terminées ».

### 2.8 Bibliothèques et utilisateurs

- **Bibliothèques** : `GET /library/sections/all` [S1] : `key`, `title`, `type` (`movie`,
  `show`, `artist`, `photo`), `Location[].path` (dernier nom seulement, comme aujourd'hui,
  jamais affiché). **Nombres** : `X-Plex-Container-Size=0` sur
  `/library/sections/{id}/all` donne `totalSize` sans les éléments [S1, « Pagination »] ;
  épisodes par `type=4`. **Taille** : comme pour Jellyfin, d'après Radarr / Sonarr (Plex
  donne bien `Part.size` pour les films, mais les séries demanderaient tous les épisodes :
  on garde une seule méthode).
- **Utilisateurs** : `GET /accounts` (**hors contrat**, utilisé par python-plexapi [S7]) :
  `id`, `name` des comptes connus du serveur (propriétaire = `id` 1 d'après S7, à vérifier ;
  comptes partagés et Plex Home y apparaissent quand ils ont utilisé le serveur, à vérifier).
  Pas de champ « admin » ni « désactivé » ni de dernière activité : **dernière activité
  déduite** de l'historique (dernier `viewedAt` du compte) et des sessions en cours. Le
  propriétaire est marqué « Propriétaire » (à la place de « Admin »). La liste complète des
  amis et du Plex Home n'est que sur plex.tv (internet) : hors de portée de la console, et
  pas nécessaire.
- Jamais d'IP ni d'appareil.

### 2.9 Correspondance avec Radarr / Sonarr

- Agents actuels de Plex : tableau `Guid[]` avec `imdb://tt…`, `tmdb://…`, `tvdb://…` [S1,
  « Guid Array », fournisseurs « imdb », « tmdb », « tvdb »]. Dans les listes, il faut
  `includeGuids=1` (**hors contrat** dans S1, utilisé par Seerr [S5 `plexapi.ts`]).
- Anciens agents (`com.plexapp.agents.imdb://tt…?lang=…`, `com.plexapp.agents.thetvdb://…`,
  `themoviedb://`) et HAMA : Seerr les reconnaît par expressions régulières [S5 scanner] ;
  Régie5 fera de même pour imdb / tmdb / tvdb (HAMA et AniDB : non, un titre non reconnu
  n'est simplement jamais rapproché, donc jamais proposé au Ménage).
- Agents personnalisés (`tv.plex.agents.custom.…`) : leur `Guid[]` s'il existe, sinon rien.

### 2.10 Ménage (données de visionnage par utilisateur)

Ce que Plex donne sans internet :

- **le propriétaire** : `viewCount`, `lastViewedAt`, `viewOffset` sur chaque élément (pour le
  compte du jeton), y compris les « vu » marqués à la main ;
- **les autres comptes** : seulement **l'historique** (`/status/sessions/history/all`,
  `accountID`, `viewedAt`, filtrable par `metadataItemID` d'une série pour tous ses épisodes)
  [S1]. Il ne contient que les **lectures terminées** : pas les lectures en cours chez un
  autre compte, et (à vérifier sur le serveur de test) pas un « vu » marqué à la main. Un
  utilisateur peut aussi en effacer des lignes (`DELETE …/history/{id}` existe).
- **pas de favori** dans Plex (les notes `userRating` ne sont visibles que pour le compte du
  jeton ; la liste de suivi est sur plex.tv).

Conséquence : sans autre source, Régie5 **ne peut pas savoir** qu'un autre compte est en
train de revoir un film. Le critère « au moins un compte » est impossible à rendre sûr ;
« tous les comptes » l'est **à moitié** (un « vu » prouvé par l'historique pour chacun, mais
une relecture en cours invisible).

**Tautulli** (optionnel, lecture seule, clé d'API, réseau local) enregistre chaque session,
partielles comprises, par utilisateur : il rendrait le Ménage Plex aussi sûr qu'avec Jellyfin.
C'est un service de plus à intégrer (client, Réglages, tests) : pas dans la 0.6.0.

**Proposition pour la 0.6.0 — Ménage Plex réduit** (décision de Vivian) :

1. Seul critère disponible : « **Vu par tous les comptes actifs** » (« au moins un » masqué
   avec Plex). Compte actif = une lecture dans l'historique depuis moins de 6 mois, hors
   `cleanup.ignoreUsers` ; le propriétaire compte toujours.
2. Preuve **positive** exigée pour chaque compte : une ligne d'historique (ou, pour le
   propriétaire, `viewCount > 0`) ; une série : chaque épisode (`leafCount`) vu par chaque
   compte. Rien dans l'historique = **jamais proposé**.
3. Exclu si : une session en cours sur le titre, `viewOffset > 0` chez le propriétaire, une
   lecture dans les N derniers mois, titre non rapproché de Radarr/Sonarr par TMDB/TVDB/IMDb.
4. Bandeau dans le Ménage avec Plex : « Plex ne montre pas les lectures en cours des autres
   comptes : vérifiez avant de supprimer. » ; la double confirmation ne change pas.
5. Suppression : inchangée (Radarr / Sonarr, `deleteFiles`, exclusion d'import). **Aucune
   écriture dans Plex** : Plex retire le titre à son prochain scan (corbeille de Plex selon
   ses réglages). Seerr : sa tâche « Media Availability Sync » marche aussi avec Plex [S5
   `availabilitySync.ts`].

Alternative plus prudente : pas de Ménage avec Plex en 0.6.0 (l'entrée Réglages dit « pas
encore avec Plex »), et Ménage + Tautulli plus tard.

### 2.11 Seerr avec Plex

- Demandes, approbation / refus, signalements, Découvrir et nouvelles demandes : **aucune
  différence** côté Régie5 (API de Seerr, indépendante du serveur multimédia).
- Seerr synchronise Plex lui-même (scanner Plex, `includeGuids=1` [S5]) ; son média garde
  **`ratingKey` / `ratingKey4k`** (Plex) à la place de `jellyfinMediaId` [S5 `Media.ts`].
  Régie5 se servait de `jellyfinMediaId` pour « Ouvrir dans Jelly5 » : avec Plex, ce sera
  « Chercher dans Plex » avec le titre (pas besoin d'identifiant).
- Les noms des demandeurs restent ceux de Seerr (`displayName`, jamais l'e-mail).

### 2.12 JSON ou XML

« The default is XML, so JSON will only be returned if it's explicitly requested (Accept:
application/json). New applications should use JSON. » [S1]. Régie5 envoie
`Accept: application/json` partout. Points à vérifier sur le serveur de test : chaque
opération utilisée répond bien en JSON (`/butler`, `/accounts`, historique) ; les booléens
(Plex a longtemps écrit `1`/`0` dans le XML) : parseurs tolérants (booléen, nombre ou
chaîne) ; un objet seul au lieu d'un tableau d'un élément.

### 2.13 HTTPS

Un Plex en « Connexions sécurisées : obligatoires » n'accepte que `https://`, avec un
certificat `*.plex.direct` dont le nom se résout par internet (DNS). Console hors ligne :
`https://<IP>:32400` avec `"insecure": true` (le nom du certificat ne correspond pas à
l'IP). Par défaut Plex est en « préférées » : `http://` marche. À dire dans `docs/CONFIG.md`.

---

## 3. Intégration dans Régie5

### 3.1 Configuration

```json
"plex": { "url": "http://192.168.1.10:32400", "token": "…" }
```

- Clé `"plex"` à côté de `"jellyfin"` et `"emby"`, objet ou liste (première instance
  utilisée, comme les deux autres). Champs : `enabled`, `url`, `token` (le jeton du serveur),
  `name`, `insecure` ; écrits par la connexion depuis le téléphone : `userName`, `owner`
  (propriétaire ou non). Jamais `apiKey` pour Plex (un `apiKey` présent est lu comme
  `token`, pour qui se tromperait de nom).
- `config::MediaServer` : `Jellyfin`, `Emby`, **`Plex`**. **Un seul actif** : si deux ou trois
  sont actifs, ordre Jellyfin, puis Emby, puis Plex, et Réglages le dit sur la ligne
  (« Plusieurs serveurs multimédias actifs : seul Jellyfin est utilisé »), comme aujourd'hui
  pour Jellyfin + Emby.
- Sans jeton : accepté (option C), essayé tel quel ; 401 → « Jeton manquant ou refusé :
  connectez-vous depuis le téléphone ».
- Port par défaut : **32400** (page du téléphone).
- `X-Plex-Client-Identifier` : tiré une fois, gardé dans `settings.json` (`"plexClientId"`,
  ou le `"deviceId"` existant de Quick Connect : à trancher en séance, le second suffit).

### 3.2 Code

- `api/plex.{h,cpp}` : chemins et parseurs (JSON), qui remplissent **les mêmes modèles** que
  Jellyfin (`model::Session`, `RecentItem`, `Task`, `ActivityEntry`, `JellyfinUser`,
  `JellyfinLibrary`, `UserItem`) : les écrans ne changent presque pas.
- `api/services.cpp` : chaque méthode du serveur multimédia choisit Jellyfin/Emby ou Plex
  (en-têtes `X-Plex-*`, `Accept`, pagination par en-têtes).
- `ui/art.cpp` : chemin d'image Plex (`/photo/:/transcode`), plafond de décodage inchangé.
- `i18n::set_media(...)` (généralise `set_emby`) : « Jellyfin » → « Plex », « Jelly5 » →
  « Plex » (l'app Plex officielle de la PS5) : « Chercher dans Plex », « Ouvrez Plex et
  cherchez « %s » ». Nouveaux textes en FR, EN, ES, DE.
- `core/versions` : minimum Plex 1.41.9.
- Mock : `fixtures/plex/`, `REGIE5_MOCK_PLEX=1`, état `plex` de `capture-states.sh` ; le
  transport de mock sert aussi les images Plex.
- Test anti-fuite étendu : pas de jeton, d'adresse, de `machineIdentifier`, de
  `clientIdentifier`, d'identifiant de compte plex.tv, de `Player.address` /
  `remotePublicAddress` / nom d'appareil dans `fixtures/plex/`.

### 3.3 Page du téléphone et assistant

- Assistant : choix **Jellyfin · Emby · Plex · Aucun** (`?m=plex` : la page ne montre que
  Plex).
- Section Plex de la page : adresse, bouton « Se connecter à Plex » (§ 1.3 B), état
  (« Connecté : <compte> · propriétaire » / « pas le propriétaire : écrans réduits »), et
  « Jeton (avancé) » repliable (§ 1.3 A) avec le guide pour le trouver. Le jeton n'est jamais
  renvoyé au téléphone (champ vide = jeton gardé), comme les clés.
- Changer l'adresse de Plex depuis le téléphone efface le jeton (il appartient à l'autre
  serveur), comme la session Quick Connect.

### 3.4 Noms et mention « beta »

- L'onglet s'appelle « Plex » ; ligne « Plex » dans Réglages avec la pastille **bêta**
  (comme Emby) ; « Chercher dans Plex » dans Options (aucune app lancée).
- README, `INSTALL.txt`, `docs/CONFIG.md` (« Jellyfin, Emby ou Plex »), notes de version :
  Plex en beta, retours sur une issue dédiée.

### 3.5 Écritures nouvelles (règles du projet, à ajouter en séance)

Arrêter une lecture Plex (`POST /status/sessions/terminate`) et scanner les bibliothèques Plex
(`POST /library/sections/{id}/refresh`), chacune derrière une confirmation dont le focus part
sur Annuler. Ménage : aucune écriture dans Plex (§ 2.10). Testées en `--mock` et contre le
serveur de test seulement.

---

## 4. Serveur Plex de test sur le laptop

- **Image officielle** `plexinc/pms-docker` [S4], tag fixé **`1.43.4.10903-e5521bd8c`**
  (= `latest` au 10 septembre 2026), **163 Mo compressés** pour amd64 (taille sur le disque
  une fois décompressée : de l'ordre de 2 à 3 fois plus, mesurée au téléchargement). Les tags
  `public` / `beta` téléchargent PMS au démarrage : non. **J'attends ton accord avant de la
  télécharger.**
- `scripts/plex-test-server.sh` (`start` / `setup` / `stop` / `status`), sur le modèle de
  l'Emby de test : conteneur `regie5-plex-test`, réseau `bridge`,
  **`-p 127.0.0.1:32400:32400`** (seul le laptop l'atteint ; `REGIE5_PLEX_ADDR` pour un essai
  console plus tard, comme pour Emby), données dans **`build/plex-test/`** (`config/`,
  `transcode/`, `media/`), non versionnées (`build/` l'est déjà).
- **Bibliothèque de démo par ffmpeg** : comme pour Emby, 4 films et 3 épisodes de 15 à 20 s
  (titres des films libres de Blender, un en HEVC 4K pour forcer un transcodage), affiches
  **2000×3000** et fonds **3840×2160** dessinés (pour la mesure des images, § 2.6), posés à
  côté des fichiers (`poster.jpg`, `fanart.jpg` : lus par les « médias locaux » de Plex).
  Aucun film téléchargé.
- **Métadonnées** : l'agent « Plex Movie » / « Plex TV Series » va chercher les fiches sur les
  serveurs de Plex (internet, depuis le laptop, sans compte) ; c'est lui qui donne les
  `Guid[]` tmdb/tvdb/imdb utiles au rapprochement. Les films de Blender étant de vrais films
  libres, il les trouvera ; les fixtures seront anonymisées de toute façon. Sinon : agent
  « Personal Media » (aucun internet), mais **pas de GUID** (rapprochement non testable).
  **À choisir** (je conseille l'agent Plex).
- **Faut-il un compte Plex ?** Non pour l'essentiel. Sans `PLEX_CLAIM`, le serveur n'est
  pas lié à un compte (« If not provided, server will not be automatically logged in ») ;
  avec **`ALLOWED_NETWORKS`** il répond sans jeton aux adresses listées [S4]. En `bridge`, le
  laptop arrive depuis la passerelle Docker (« localhost will appear to plex as coming from
  the docker networking gateway » [S4]) : `ALLOWED_NETWORKS=172.16.0.0/12` (plage des réseaux
  Docker), et le port n'est publié que sur 127.0.0.1. La création des bibliothèques se fait
  par l'API (`POST /library/sections/all`), sans l'interface web.

  Testable **sans compte** : `/identity`, `/`, bibliothèques et nombres, ajouts récents,
  images (tailles), Butler, activités, scan, historique et `/accounts` (un seul compte), JSON,
  sessions **si** une lecture peut être simulée sans Plex Web (démarrer un transcodage par
  `/video/:/transcode/universal/start.m3u8` et déclarer la lecture par `POST /:/timeline` :
  à essayer, non garanti), refus de l'arrêt (pas de Plex Pass).

  **Non testable sans compte** : le jeton lui-même (en-têtes, 401), la connexion par le
  téléphone (PIN, `resources`), un compte partagé ou de Plex Home, plusieurs comptes dans
  l'historique, Plex Web pour lancer une vraie lecture.

  **Avec un compte** (le plus simple, si tu le décides) : un **compte Plex gratuit créé
  exprès** (jamais le tien si tu en as un), un jeton de réclamation pris sur
  https://www.plex.tv/claim (durée de vie très courte : quelques minutes), passé en
  `PLEX_CLAIM` au **premier** démarrage. Je ne crée aucun compte et ne tape aucun mot de
  passe : c'est toi qui le fais ; **j'attends ton accord avant toute action liée à un
  compte.** Je conseille de commencer **sans compte**, et de décider du compte quand on
  arrivera à la connexion par le téléphone (séance P4).
- **Rien ne touche ton vrai serveur** : la config de test (`build/plex-test/config.json`)
  ne contient que le Plex de test ; aucun accès à tes serveurs n'est nécessaire pour cette
  version (Jellyfin : seulement l'essai de non-régression sur la console, en lecture).

---

## 5. Estimation et découpage

Une séance = une étape, un patch, un message de commit, la roadmap à jour.

| # | Séance | Contenu | Testé contre |
|---|---|---|---|
| P1 | Serveur de test | `plex-test-server.sh`, médias ffmpeg, bibliothèques par l'API ; relevé des réponses réelles (GET) : JSON, champs, `/accounts`, Butler, activités, **tailles des images**, lecture simulée | serveur de test |
| P2 | Configuration et noms | `"plex"`, `MediaServer::Plex`, un seul serveur, `set_media`, versions, ligne Réglages + bêta, assistant (choix Plex), page du téléphone (adresse + jeton collé) | tests, `--mock` |
| P3 | Client et écrans | `api/plex`, parseurs et fixtures anonymisées, anti-fuite, `REGIE5_MOCK_PLEX`, état `plex` ; Réglages, Accueil, onglet Plex (sessions, diagnostic, arrêt, scan, tâches, bibliothèques, utilisateurs), Journal, images | serveur de test, `--mock` |
| P4 | Connexion par le téléphone | PIN plex.tv depuis la page, `resources`, `owned`, jeton du serveur seulement ; erreurs (PIN expiré, 429, serveur absent du compte) | tests (plex.tv simulé) ; en vrai **seulement avec un compte de test**, sur ton accord |
| P5 | Ménage Plex | version réduite (§ 2.10) **ou** « pas encore avec Plex » | tests, `--mock`, serveur de test |
| P6 | Build console et zip testeur | build de test (journal UDP, `test-config`), étapes une action à la fois : non-régression Jellyfin chez toi, Plex de test depuis la console (port ouvert le temps de l'essai, comme Emby) ; zip de test pour un utilisateur de Plex (jamais une release) | console |

P3 est la plus grosse (une séance et demie à deux). Total : **6 à 7 séances**, **1 essai
console** (P6 ; un second si l'essai trouve un défaut). Jellyfin et Emby : tous les tests
existants restent verts à chaque séance.

### Seulement un vrai utilisateur de Plex pourra vérifier

- l'arrêt d'une lecture et l'accélération matérielle (**Plex Pass**) ;
- les images d'une vraie bibliothèque (affiches et fonds réels, nombre de titres) sur la PS5 ;
- des comptes partagés et Plex Home dans `/accounts` et l'historique, donc le Ménage réduit ;
- de vrais transcodages lancés par de vrais clients (décisions, raisons déduites) ;
- la connexion par le téléphone avec son compte (si on ne crée pas de compte de test) ;
- un Plex en « connexions sécurisées obligatoires », derrière un reverse proxy, ou plus
  ancien que 1.43.

À demander sur l'issue ou Reddit : version de PMS, Plex Pass ou non, comptes partagés / Home,
et un retour par écran avec captures (comme pour Emby).

---

## 6. Risques

| Risque | Effet | Parade |
|---|---|---|
| Plex retire les jetons classiques (JWT seul, 7 jours) | reconnexion hebdomadaire depuis le téléphone | message clair au 401/498 ; à surveiller dans [S1] |
| `TranscodeSession`, `/accounts`, `includeGuids` hors contrat | un champ disparaît | parseurs tolérants ; section qui le dit au lieu de casser |
| `/photo/:/transcode` ignore la taille sur une version | plantage mémoire (cas Emby) | mesure en P1, plafond de décodage gardé |
| Arrêt sans Plex Pass | action refusée | lu dans `GET /`, dit dans la confirmation et l'erreur |
| Ménage : lecture en cours d'un autre compte invisible | titre proposé à tort | version réduite + bandeau, ou pas de Ménage Plex en 0.6.0 |
| plex.tv change sa politique CORS | connexion par le téléphone impossible | repli : jeton collé (§ 1.3 A) |
