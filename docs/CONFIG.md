# Régie5 — config.json

Régie5 lit `config.json` au démarrage et quand on lance « Tout tester » (ou △) dans
Réglages. Sous Linux : `~/.config/regie5/config.json`, ou `--config <chemin>`. Partez de
`config.example.json`.

Les clés d'API ne sont jamais écrites dans un journal.

## Un service

| Champ | Services | Rôle |
|---|---|---|
| `enabled` | tous | `false` pour couper le service sans effacer sa configuration. Vrai par défaut. |
| `url` | tous | Adresse du service. Peut contenir un chemin (`https://exemple.lan/radarr`) derrière un reverse proxy ou avec l'`urlBase` des *arr. |
| `apiKey` | Sonarr, Radarr, Seerr, Jellyfin, Emby | Clé d'API du service. Sans clé, le service est coupé. |
| `token` | Plex | Jeton Plex, écrit par la connexion depuis le téléphone (ou collé à la main). Pour le propriétaire, c'est le jeton du compte Plex. Voir « Plex » plus bas. |
| `name` | tous | Nom affiché de l'instance (« Radarr 4K »). Par défaut, le nom du service. |
| `is4k` | Sonarr, Radarr | `true` pour l'instance que **Seerr** utilise comme serveur 4K. `false` par défaut. Jamais déduit du nom. |
| `insecure` | tous | `true` pour une adresse `https://` dont le certificat est **auto-signé** (réseau local) : il n'est pas vérifié. `false` par défaut : le certificat doit être signé par une autorité reconnue (Let's Encrypt…). |
| `username`, `password` | Glances | Seulement si Glances est lancé avec `--password` (voir `docs/GLANCES.md`). |

Sur PS5, le fichier peut aussi être rempli **depuis un téléphone** : Réglages › « Depuis le
téléphone » affiche un QR code vers une page servie par la console sur le réseau local
(code à 6 chiffres, active tant que la fenêtre est ouverte). Les clés déjà enregistrées ne
sont jamais renvoyées au téléphone : un champ laissé vide garde la clé.

Au **premier lancement** (`config.json` absent, illisible, ou sans aucun service principal
actif : Glances seul ne compte pas), Régie5 ouvre son **assistant** : choix du serveur
multimédia (Jellyfin, Emby, Plex ou aucun), la même page du téléphone (qui ne montre que le
serveur choisi), puis le test de chaque service. « Plus tard » le ferme pour de bon
(`settings.json` : `"welcome": "later"`) ; l'Accueil propose alors « Configurer », et
Réglages › « Assistant » le rouvre à tout moment. Une mise à jour qui garde un `config.json`
utilisable ne l'affiche jamais. Un `config.json` illisible est gardé à côté, sous le nom
`config.json.invalid`, au premier enregistrement depuis le téléphone (jamais par-dessus une
copie existante).

### À quoi sert `is4k`

Il dit quelle instance Seerr utilise comme serveur 4K. Régie5 n'écrit rien dans Seerr : après
une suppression par le Ménage, c'est la tâche « Media Availability Sync » de Seerr qui
marque le titre (état normal ou 4K, selon l'instance). Le champ est lu et gardé pour les
écrans qui distingueront les deux côtés ; il n'est jamais déduit du nom.

## Jellyfin sans clé d'API : Quick Connect

Une adresse sans clé suffit pour Jellyfin : Réglages › ligne Jellyfin › Options (M) ›
« Quick Connect… », ou le bouton « Quick Connect » de la vérification de l'assistant. Le
code affiché se valide dans Jellyfin, sur le téléphone (profil › Quick Connect). Régie5 écrit
alors dans `"jellyfin"` (la première instance d'une liste) :

| Champ | Rôle |
|---|---|
| `accessToken` | La session du compte (secrète comme une clé ; jamais montrée par la page du téléphone, jamais écrite dans un journal). |
| `userName` | Le nom du compte, affiché sur la ligne Jellyfin. |
| `isAdmin` | `false` pour un compte qui n'est pas administrateur : la ligne passe à l'orange, et les écrans qui demandent un administrateur le disent (arrêt de lecture, scan, tâches, journal, utilisateurs, diagnostic, Ménage). |

Une `apiKey` passe avant la session. Changer l'adresse de Jellyfin depuis le téléphone efface
la session (elle appartient à l'ancien serveur). Une session supprimée dans Jellyfin (Tableau
de bord › Appareils) : Réglages le dit, il suffit de refaire Quick Connect. Régie5 se présente
à Jellyfin sous le nom « Regie5 », avec un identifiant d'appareil tiré une fois et gardé dans
`settings.json` (`"deviceId"`). Jellyfin 10.9 ou plus récent, Quick Connect activé (Tableau de
bord › Général). Emby n'a pas d'équivalent sur le réseau local : il garde sa clé.

## Jellyfin, Emby ou Plex

Le serveur multimédia est **Jellyfin** (clé `"jellyfin"`), **Emby** (clé `"emby"`, mêmes
champs) **ou Plex** (clé `"plex"`, ci-dessous), un seul à la fois :

```json
"emby": { "url": "http://192.168.1.10:8096", "apiKey": "…" }
```

Clé d'API Emby : dans les réglages du serveur Emby, *Advanced › Api Keys › New Api Key*
(libellés de l'interface en anglais, vérifiés avec Emby 4.10). Avec Emby,
l'onglet et les textes disent « Emby », et Options propose « Chercher dans Emby5 » (le titre à
chercher dans Emby5 ; aucune app n'est lancée). Emby 4.8 ou plus récent (testé avec 4.10.1.0).
Le support d'Emby est en **bêta** depuis la 0.4.0 (pastille dans Réglages) : retours bienvenus
sur l'issue #1 du dépôt.

Si plusieurs sont actifs, le premier de Jellyfin, Emby, Plex est utilisé, et Réglages le
signale sur sa ligne. Pour passer de l'un à l'autre, mettez `"enabled": false` dans celui
qu'on quitte (ou retirez-le).

### Plex (bêta, 0.6.0)

```json
"plex": { "url": "http://192.168.1.10:32400" }
```

| Champ | Rôle |
|---|---|
| `url` | Adresse du serveur Plex (port 32400 par défaut). |
| `token` | Le jeton que Plex donne pour ce serveur. **Pour le propriétaire, Plex donne le jeton du compte lui-même** (vérifié le 17 octobre 2026) : il ouvre tout le compte Plex, pas seulement ce serveur. Secret : jamais renvoyé au téléphone, jamais écrit dans un journal, mais en clair dans `config.json`. |
| `userName`, `owner` | Écrits avec le jeton : le compte, et s'il est le propriétaire du serveur. `"owner": false` : la ligne passe à l'orange, et les écrans réservés au propriétaire le disent (sessions, arrêt, scan, tâches, journal, utilisateurs). |

Le jeton s'obtient depuis la page du téléphone : « Se connecter à Plex » ouvre la page de
connexion de Plex (le téléphone doit avoir internet, la console non) ; la page trouve le
serveur à l'adresse indiquée (sinon, elle demande lequel) et envoie à la console le jeton que
Plex donne pour ce serveur : pour un compte partagé, un jeton propre au serveur ; **pour le
propriétaire, le jeton du compte**. Régie5 apparaît sous le nom « Regie5 » dans les appareils
autorisés du compte Plex, mais **l'en retirer ne révoque pas le jeton** (essayé : encore accepté
5 minutes après). Pour le révoquer vraiment : sur plex.tv, changer le mot de passe du compte en
cochant « Déconnecter les appareils connectés » (d'après l'aide de Plex, tous les jetons du compte
sont alors invalidés, et toutes les apps et tous les serveurs du compte déconnectés). Plex n'offre
pas de jeton limité à un serveur qu'une console hors ligne pourrait garder (`docs/PLEX.md` § 8).
Autre voie, sans aucun jeton sur la console : la liste « allowed without auth » du serveur
réduite à l'adresse de la console (voir plus bas), que Plex déconseille. En secours, le jeton
peut être collé à la main
(Plex Web › un film › ⋯ › Obtenir des infos › Afficher le XML : la valeur de `X-Plex-Token`
dans l'adresse ; Plex dit qu'un jeton trouvé ainsi peut ne durer qu'un temps). Sans jeton,
Régie5 n'entre que si le serveur Plex laisse le réseau de la console entrer sans
authentification (Paramètres › Réseau › « List of IP addresses and networks that are allowed
without auth ») : Plex le déconseille, Réglages l'affiche. Changer l'adresse depuis le
téléphone efface le jeton (il appartient à l'autre serveur). Plex Media Server 1.41.9 ou plus
récent (testé avec 1.43.4). Un Plex en « Connexions sécurisées : obligatoires » :
`https://<IP>:32400` avec `"insecure": true` (son certificat est au nom de `*.plex.direct`).
Options propose « Chercher dans Plex » (le titre à chercher dans l'app Plex de la PS5). Le
Ménage n'est pas disponible avec Plex pour l'instant. Étude : `docs/PLEX.md`.

## Derrière un reverse proxy

L'`url` peut contenir un chemin, d'un ou plusieurs niveaux, avec ou sans `/` final :

```json
"sonarr":   { "url": "https://media.lan/sonarr",         "apiKey": "…" },
"jellyfin": { "url": "https://media.lan/media/jellyfin", "apiKey": "…", "insecure": true }
```

Deux montages marchent :

- le proxy **retire** le chemin (`location /sonarr/ { proxy_pass http://serveur:8989/; }`) :
  rien à régler sur le service ;
- le proxy **garde** le chemin : il faut alors le même chemin dans le service (`URL Base` de
  Sonarr / Radarr, « URL de base » de Jellyfin). Seerr n'a pas ce réglage : pour lui, seul
  le premier montage marche.

Régie5 ne suit pas les redirections (la clé d'API partirait avec). Une redirection s'affiche
dans Réglages : « Redirigé (HTTP 302) — page de connexion du proxy, ou mauvaise adresse ? ».
Causes habituelles : une page de connexion (Authelia, Authentik…) devant le service, à
contourner pour les chemins `/api` ; ou `http://` alors que le proxy force `https://`.
Testé avec nginx (chemins retirés, HTTPS auto-signé avec `insecure`, refus du certificat
sans).

## Plusieurs instances

Un service peut être un objet (une instance) ou une liste d'instances nommées :

```json
"radarr": [
  { "name": "Radarr",    "url": "http://192.168.1.10:7878",       "apiKey": "…" },
  { "name": "Radarr 4K", "url": "https://media.example/radarr4k", "apiKey": "…", "is4k": true }
]
```

Tous les écrans lisent toutes les instances. Un titre présent dans deux instances
apparaît une fois par instance, avec son nom (« Radarr 4K ») ; dans « À venir » et le
Guide, le même épisode ou la même sortie n'apparaît qu'une fois.

## Ménage

```json
"cleanup": { "ignoreUsers": ["invité", "test"] }
```

Comptes Jellyfin ou Emby (noms, sans tenir compte de la casse) qui ne comptent pas dans « vu par
tous les comptes actifs ». Le Ménage lui-même s'active dans Réglages, pas ici.
