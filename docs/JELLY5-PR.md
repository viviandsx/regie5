# Proposition pour Jelly5 : ouvrir un élément au lancement

Objectif : depuis une autre app (Régie5), lancer Jelly5 (Title ID `PPSA99505`)
directement sur la fiche d'un film ou d'un épisode. Lu dans Jelly5 au commit `552b866`.

## Constat

- `app/src/main.cpp` : `int main()` ne prend aucun argument.
- `app/sce_sys/param.json` déclare `"gameIntent": {"permittedIntents": [{"intentType":
  "launchActivity"}]}`, mais aucun intent n'est lu dans le code.
- Les données de Jelly5 sont dans `/download0/jelly5/` (dossier propre à l'app).

## Changement proposé (petit, sans effet si rien n'est demandé)

1. **Lire une demande d'ouverture au démarrage**, par deux voies, la première trouvée gagne :
   - les arguments : `main(int argc, char **argv)` et `--open-item <ItemId> --server <ServerId>`
     (si le lanceur système les transmet) ;
   - un fichier de passage **`/data/jelly5/open.json`** :
     `{"serverId": "...", "itemId": "...", "action": "open", "createdAt": 1760000000}`.
     Jelly5 le lit puis **le supprime**, et l'ignore s'il a plus de 60 secondes.
     `/data` est partagé entre apps une fois le jailbreak actif, contrairement à `/download0`.
2. **Après la connexion** (`boot`), si le `ServerId` du compte courant (`/System/Info` →
   `Id`) est celui demandé : ouvrir la fiche de l'élément (même chemin que depuis une
   rangée de l'accueil). Pas de lecture automatique (« play » pourra venir plus tard).
3. Sinon (autre serveur, élément introuvable) : démarrage normal et une note
   (« Élément introuvable sur ce serveur »).

Esquisse :

```cpp
struct OpenRequest { std::string server_id, item_id; };
static std::optional<OpenRequest> take_open_request(int argc, char **argv);
// argv d'abord ; sinon /data/jelly5/open.json (lu, supprimé, < 60 s).
// Dans boot(), une fois le client connecté :
//   if (req && req->server_id == client.server_id()) open_item_detail(req->item_id);
```

## Côté Régie5

- Options (M) › « Ouvrir dans Jelly5 » sur une fiche Bibliothèque (rapprochement TMDB/TVDB
  vers l'élément Jellyfin), un ajout récent, une demande disponible (`jellyfinMediaId`).
- Écrit `/data/jelly5/open.json`, puis lance `PPSA99505` avec `sceLncUtilLaunchApp`
  (repli `sceSystemServiceLaunchApp`) et les arguments ci-dessus.
- Sous Linux : lancement simulé (une ligne de journal et une note).

**Fait (phase 4 bis)** : `core/jelly5` (arguments, contenu de `open.json`, rapprochement
TMDB/TVDB/IMDb par l'index `/Items?…&Fields=ProviderIds`, élément d'un ajout récent ou
d'une demande ; testés), `ui/jelly5_link` (Id du serveur par `/System/Info`, index lu une
fois, note de résultat), `platform::launch_app` (Linux : `launch (simulated): PPSA99505
--open-item <Id> --server <Id>` et le contenu de `open.json`, rien n'est écrit). Un ajout
récent qui regroupe plusieurs épisodes ouvre la série ; une demande 4K ouvre l'élément 4K
(`jellyfinMediaId4k`) et n'est proposée que si la 4K est disponible. Phase 6 : écrire
`launch_app` pour PS5 (fichier de passage puis `sceLncUtilLaunchApp`).

## Résultat sur PS5 (13 octobre 2026, firmware 13.60) : pas de lancement depuis une app

- app-7c : `sceSystemServiceLaunchApp` et `sceLncUtilLaunchApp`, liées à la construction,
  valent 0 sur la console.
- app-8 : cherchées par nom pendant l'exécution (`sceKernelLoadStartModule` rend bien la
  poignée de `libSceSystemService`, puis `sceKernelDlsym`) : **les deux répondent
  `0x80020003`** (introuvable). La console ne les donne pas à une app ; les outils qui
  lancent un titre (ps5-homebrew-dev-protocol, shsrv/hbldr, ShadowMount+) sont des
  payloads, avec d'autres droits.
- Le fichier de passage n'est pas plus accessible : une app n'a pas `/data`.
- **Décision (Vivian)** : Régie5 garde le repli **« Chercher dans Jelly5 »** (Options) : une
  note « Ouvrez Jelly5 et cherchez « titre » » pendant 8 s. Le code de lancement reste en
  place et se réactive seul si une console fournit un jour l'une des deux fonctions.
- Conséquence pour cette proposition : sans lanceur, Régie5 ne peut pas s'en servir sur
  PS5. Elle reste valable pour un lanceur qui aurait ces droits (un payload) ; la proposer
  ou non à Jelly5 est une décision à part.
