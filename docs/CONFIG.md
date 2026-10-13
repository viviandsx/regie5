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
| `apiKey` | Sonarr, Radarr, Seerr, Jellyfin | Clé d'API du service. Sans clé, le service est coupé. |
| `name` | tous | Nom affiché de l'instance (« Radarr 4K »). Par défaut, le nom du service. |
| `is4k` | Sonarr, Radarr | `true` pour l'instance que **Seerr** utilise comme serveur 4K. `false` par défaut. Jamais déduit du nom. |
| `insecure` | tous | `true` pour une adresse `https://` dont le certificat est **auto-signé** (réseau local) : il n'est pas vérifié. `false` par défaut : le certificat doit être signé par une autorité reconnue (Let's Encrypt…). |
| `username`, `password` | Glances | Seulement si Glances est lancé avec `--password` (voir `docs/GLANCES.md`). |

Sur PS5, le fichier peut aussi être rempli **depuis un téléphone** : Réglages › « Depuis le
téléphone » affiche un QR code vers une page servie par la console sur le réseau local
(code à 6 chiffres, active tant que la fenêtre est ouverte). Les clés déjà enregistrées ne
sont jamais renvoyées au téléphone : un champ laissé vide garde la clé.

### À quoi sert `is4k`

Seerr suit séparément l'état « normal » et l'état 4K d'un titre. Quand le Ménage supprime
un titre d'une instance marquée `is4k`, Régie5 marque **l'état 4K** du titre comme
« supprimé » dans Seerr ; sinon, l'état normal. Une instance 4K non marquée ferait donc
repasser le mauvais état.

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

Comptes Jellyfin (noms, sans tenir compte de la casse) qui ne comptent pas dans « vu par
tous les comptes actifs ». Le Ménage lui-même s'active dans Réglages, pas ici.
