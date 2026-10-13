# Charge du serveur avec Glances (optionnel)

Jellyfin ne donne ni l'occupation du processeur ni celle de la mémoire du serveur. Régie5
peut les lire chez **Glances**, un moniteur système léger qui expose une API web en
**lecture seule**. C'est facultatif : sans Glances, la charge n'apparaît simplement pas
(en-tête de l'onglet Jellyfin, fiche diagnostic d'une lecture).

Régie5 lit un seul point d'entrée, `GET /api/4/quicklook` (CPU, mémoire, swap, charge, en
pourcentages), toutes les 5 secondes, et seulement quand l'onglet Jellyfin est affiché. Il
faut **Glances 4** (API v4).

## 1. Installer Glances sur le serveur

### Avec Docker

```bash
docker run -d --name glances --restart unless-stopped \
  -p 61208:61208 -e GLANCES_OPT="-w" --pid host \
  nicolargo/glances:latest
```

Avec Docker Compose :

```yaml
services:
  glances:
    image: nicolargo/glances:latest
    restart: unless-stopped
    pid: host
    ports:
      - "61208:61208"
    environment:
      - GLANCES_OPT=-w
```

### Sans Docker

```bash
pipx install 'glances[web]'
glances -w
```

Pour qu'il démarre avec le serveur, un service systemd (`/etc/systemd/system/glances.service`) :

```ini
[Unit]
Description=Glances (web)
After=network.target

[Service]
ExecStart=/usr/bin/env glances -w
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl enable --now glances
```

### Vérifier

Depuis un autre ordinateur du réseau local :

```bash
curl http://192.168.1.10:61208/api/4/quicklook
```

La réponse doit être un JSON avec `cpu`, `mem`, `swap` et `load`.

## 2. Le déclarer dans Régie5

Dans `config.json`, l'entrée `glances` (déjà présente, désactivée, dans `config.example.json`) :

```json
"glances": {
  "enabled": true,
  "url": "http://192.168.1.10:61208"
}
```

- Derrière un reverse proxy, l'adresse peut contenir un chemin :
  `"url": "https://media.example/glances"`.
- Si Glances est lancé avec `--password`, ajoutez `"username"` et `"password"` (HTTP Basic).
  Ils sont lus depuis le fichier et ne sont jamais journalisés.

Puis **Réglages › Tout tester** : la ligne Glances passe au vert, avec le nom du processeur.
La charge apparaît en haut de l'onglet Jellyfin.

## Sécurité

- L'API de Glances ne fait que **lire** l'état de la machine : Régie5 ne peut rien y changer.
- Par défaut, Glances écoute sur toutes les interfaces, sans mot de passe. Gardez le port
  61208 **sur le réseau local** (pas de redirection de port depuis internet), ou protégez-le
  avec `--password`.
