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
multimédia (Jellyfin, Emby ou aucun), la même page du téléphone (qui ne montre que le
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

## Jellyfin ou Emby

Le serveur multimédia est **Jellyfin** (clé `"jellyfin"`) **ou Emby** (clé `"emby"`, mêmes
champs), l'un ou l'autre :

```json
"emby": { "url": "http://192.168.1.10:8096", "apiKey": "…" }
```

Clé d'API Emby : dans les réglages du serveur Emby, *Advanced › Api Keys › New Api Key*
(libellés de l'interface en anglais, vérifiés avec Emby 4.10). Avec Emby,
l'onglet et les textes disent « Emby », et Options propose « Chercher dans Emby5 » (le titre à
chercher dans Emby5 ; aucune app n'est lancée). Emby 4.8 ou plus récent (testé avec 4.10.1.0).
Le support d'Emby est en **bêta** depuis la 0.4.0 (pastille dans Réglages) : retours bienvenus
sur l'issue #1 du dépôt.

Si `"jellyfin"` et `"emby"` sont tous deux actifs, seul Jellyfin est utilisé, et Réglages le
signale sur sa ligne. Pour passer d'Emby à Jellyfin, mettez `"enabled": false` dans
`"emby"` (ou retirez-le).

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
