# Santé des disques avec Scrutiny (optionnel)

[Scrutiny](https://github.com/AnalogJ/scrutiny) relève chaque jour l'état **SMART** des
disques du serveur (secteurs réalloués, erreurs, température…) et le garde. Régie5 peut le
lire, en **lecture seule** : c'est facultatif ; sans Scrutiny, rien ne change à l'écran.

Avec Scrutiny, Régie5 montre :

- **Accueil**, bandeau santé : un disque en panne, trop chaud, ou dont le dernier relevé date
  de plus de 48 h, compte parmi les alertes ; ✕ sur l'alerte ouvre le disque ;
- **Jellyfin › Serveur**, section **Disques** : chaque disque, sa température et son état ;
  ✕ ouvre sa fiche (état, température des 7 derniers jours, heures de fonctionnement, dernier
  relevé, attributs SMART en défaut).

L'état est celui que donne la page de Scrutiny (selon son réglage « Device status
threshold » : SMART, Scrutiny ou les deux). Scrutiny ne fixe aucun seuil de température ;
ceux de Régie5 : disque dur et SSD SATA **chaud dès 50 °C**, **trop chaud dès 60 °C** ;
SSD NVMe dès 70 °C et 80 °C.

Régie5 lit `GET /api/summary` (une fois à l'ouverture de l'Accueil et de l'onglet Jellyfin,
puis toutes les 5 minutes), `GET /api/settings`, et `GET /api/device/…/details` à l'ouverture
d'une fiche. Il faut **Scrutiny 0.8 ou plus récent** (essayé avec 0.9.5).

Régie5 n'affiche ni ne garde le numéro de série, le modèle, le fabricant ou le firmware des
disques : seulement leur nom (`sda`, ou celui que vous leur avez donné dans Scrutiny), leur
taille et leur type.

## 1. Installer Scrutiny

Voir [son guide](https://github.com/AnalogJ/scrutiny#docker). L'image « omnibus » (interface
web, base et collecteur dans un seul conteneur) suffit ; par défaut, son interface est sur le
port **8080**. Le collecteur a besoin de voir les disques (`--cap-add SYS_RAWIO`, chaque
`/dev/sdX` en `--device`).

### Vérifier

Depuis un autre ordinateur du réseau local :

```bash
curl http://192.168.1.10:8080/api/health
```

La réponse doit être `{"success":true}`.

## 2. Le déclarer dans Régie5

Dans `config.json`, l'entrée `scrutiny` (déjà présente, désactivée, dans `config.example.json`),
ou depuis le téléphone (Réglages › « Depuis le téléphone », section Scrutiny) :

```json
"scrutiny": {
  "enabled": true,
  "url": "http://192.168.1.10:8080"
}
```

- Derrière un reverse proxy, l'adresse peut contenir un chemin :
  `"url": "https://media.example/scrutiny"` (le `web.listen.basepath` de Scrutiny).
- Si le proxy demande un mot de passe (HTTP Basic), ajoutez `"username"` et `"password"`.
  Ils ne sont jamais journalisés.

Puis **Réglages › Tout tester** : la ligne Scrutiny passe au vert, avec le nombre de disques.

## Sécurité

- Scrutiny n'a **aucune authentification** : quiconque atteint son port peut lire et aussi
  supprimer des disques de son tableau ou changer ses réglages. Régie5 ne fait que des `GET`.
- Gardez son port **sur le réseau local** (pas de redirection depuis internet), ou mettez-le
  derrière un reverse proxy avec un mot de passe.
