# Régie5 — travailler et publier une version

Mode d'emploi depuis la publication (dépôt public https://github.com/viviandsx/regie5,
release `v0.2.0`, listée sur homebrew.page depuis la fusion de la PR #244). Règles du catalogue suivies :
`docs/submitting.md` et `docs/versioning.md` de `blackbearreloaded/ps5-homebrew-catalog`.

---

## 1. Branches et envois

### Où on travaille

Sur le dépôt GitHub, une seule branche : `main`. En local (renommage fait le 13 octobre 2026) :

| Branche locale | Contenu | Règle |
|---|---|---|
| `main` | l'historique public, suit `origin/main` | **la branche de travail** |
| `archive-privee` | l'ancien historique (avec des mentions retirées de la version publique) | archive, **jamais poussée** |

`push.default` vaut `upstream` : `git push` sans argument n'envoie que `main` vers
`origin/main`. Ne jamais lancer `git push --all`, ni `git push origin archive-privee`.

### Au quotidien

1. Le code est préparé dans l'arbre de travail ; une étape = un message de commit proposé.
2. Vivian teste (Linux, puis console si besoin), relit `git diff`, puis commit et push :

   ```bash
   git status
   ```

   ```bash
   git add -A && git commit
   ```

   ```bash
   git push
   ```

3. Pousser sur `main` ne publie **rien** pour les utilisateurs : le catalogue ne regarde que
   les **releases** (tag + zip). On peut donc pousser des étapes validées au fil de l'eau ;
   le README de GitHub, lui, change tout de suite.
4. Gros chantier sur plusieurs jours : une branche à part (`git switch -c guide-bazarr`),
   fusionnée dans `main` quand elle est validée. Facultatif.

Jamais : `git push --force` sur `main`, réécriture de l'historique publié, ni push d'un tag
qui n'est pas une release prête (§3).

---

## 2. Régie5 est au catalogue : ce qui publie quoi

La PR #244 sur `blackbearreloaded/ps5-homebrew-catalog` a été fusionnée le 13 octobre 2026 :
Régie5 0.2.0 est sur homebrew.page (fiche `apps/PPSA99520.json`).

**Le catalogue suit seul chaque nouvelle release GitHub du dépôt, pré-releases comprises.**
Chaque jour, sa tâche cherche la dernière release et propose la mise à jour de la fiche :
toute release (ou pré-release) publiée sur `viviandsx/regie5` finit chez les utilisateurs.
Donc :

- **aucune release ni pré-release** tant que la version suivante (0.3.0) n'est pas finie et
  testée sur la console (recette rapide, §3 étape 2) ;
- pas de « release de test » sur GitHub : les zips de test se copient sur la console à la
  main, sans passer par GitHub ;
- pousser des commits sur `main` reste sans effet pour les utilisateurs (§1).

Ne jamais toucher à ce qui est déjà publié :

| Ne pas toucher | Pourquoi |
|---|---|
| Les tags publiés (`v0.2.0`…) : ni supprimer, ni déplacer | `icon_url` de la fiche est lu **au tag**, avec le `param.json` voisin |
| Les releases publiées et leur `PPSA99520.zip` (ni remplacer, ni renommer, ni supprimer) | `artifact_url` et `sha256` de la fiche : un autre zip n'a pas la même empreinte, et les utilisateurs peuvent revenir à une ancienne version |
| Le dépôt : nom, visibilité (public), propriétaire | `source_repo`, et seul le propriétaire du dépôt peut mettre à jour la fiche |

Un défaut dans un zip publié : **pas de remplacement**. Soit il attend la 0.3.0, soit Vivian
décide de sortir un correctif (`0.2.1`, §3), testé sur la console comme une version.

## 3. Sortir une version (exemple : 0.3.0)

### Numéros

| Où | 0.2.0 | 0.3.0 | correctif 0.3.1 |
|---|---|---|---|
| `ps5/app/sce_sys/param.json` → `contentVersion` | `00.002.000` | `00.003.000` | `00.003.001` |
| `CMakeLists.txt` → `project(regie5 VERSION …)` | `0.2.0` | `0.3.0` | `0.3.1` |
| Tag git | `v0.2.0` | `v0.3.0` | `v0.3.1` |

Règles du catalogue : `contentVersion` **monte à chaque release** (sinon la console ne voit
pas la mise à jour), ne descend jamais et ne se réutilise jamais (`00.001.000` et
`00.001.001` sont déjà grillés par les zips de test). `release-ps5.sh` tire la version
affichée de `contentVersion` ; il refuse de reconstruire un dossier `release-<v>` existant.

### Étapes

1. **Préparer** (dans l'arbre de travail) : les deux numéros ci-dessus, les notes de version
   `build/publish/release-notes-v0.3.0.md` (anglais, comme celles de 0.2.0), le README si des
   écrans ont changé, `docs/ROADMAP.md`.
2. **Tester** :

   ```bash
   cmake --build build/linux && ctest --test-dir build/linux --output-on-failure
   ```

   ```bash
   scripts/release-ps5.sh
   ```

   Le zip sort dans `build/ps5/release-0.3.0/upload/PPSA99520.zip`. Vivian le copie sur la
   console par-dessus l'installation (le `config.json` est gardé) et fait la recette rapide
   (`docs/RECETTE-EXPRESS.md`, au moins Réglages, un écran par onglet, ce qui a changé).

   **À chaque version, même si rien n'a changé à ces endroits** (la 0.3.0 a cassé la page du
   téléphone par son seul `param.json`, et sa recette ne passait pas par là) :

   - la tuile est dans la bonne rangée (Multimédia ou Jeux) et l'app démarre ;
   - **Réglages › Depuis le téléphone** : le QR code s'affiche, la page s'ouvre sur le
     téléphone (sans rien enregistrer, ○ pour fermer) ;
   - Réglages › ligne « Affichage » : la fréquence obtenue est celle attendue.

   `release-ps5.sh` refuse un `param.json` dont le profil (catégorie, insigne, `attribute`,
   `attribute2`, `attribute3`, blocs `amm` / `kernel`, `gameIntent`) n'a pas été vu sur la
   console démarrer **et** écouter : la liste est `ps5/app/param-profiles.json`
   (`scripts/check-param.py`). Un nouveau profil y entre après un essai console des trois
   points ci-dessus, jamais avant.
3. **Commit et push** de la version (message proposé : « Régie5 0.3.0 »).
4. **Tag** sur ce commit, puis push du tag seul :

   ```bash
   git tag -a v0.3.0 -m "Régie5 0.3.0"
   ```

   ```bash
   git push origin v0.3.0
   ```

5. **Release GitHub** sur le tag `v0.3.0` : titre « Régie5 0.3.0 », texte = les notes de
   version, pièce jointe **`PPSA99520.zip`** (toujours ce nom : la tâche de mise à jour du
   catalogue s'y attend). Release normale, pas « pre-release » (une pré-release serait
   proposée aux utilisateurs comme une vraie).
6. **Empreinte** du zip, à garder pour la fiche :

   ```bash
   sha256sum build/ps5/release-0.3.0/upload/PPSA99520.zip
   ```

### Mise à jour de la fiche du catalogue

Régie5 est listé (PR #244 fusionnée) ; deux chemins :

- **Automatique** : chaque jour, le catalogue cherche les nouvelles releases et ouvre lui-même
  une PR qui fait pointer la fiche vers la dernière. Vérifier dans cette PR que `version`,
  `artifact_url`, `sha256` (= celui de l'étape 6) et `icon_url` (doit finir par
  `/v0.3.0/ps5/app/sce_sys/icon0.png`) sont justes. Si `icon_url` reste sur `v0.2.0`, le
  catalogue lirait l'ancien `contentVersion` : le signaler dans la PR ou passer au chemin
  manuel.
- **Manuel** (si rien n'arrive sous 2 jours, ou si la PR automatique est fausse) :
  1. sur GitHub, fork `viviandsx/ps5-homebrew-catalog` → « Sync fork » ;
  2. nouvelle branche (`regie5-0.3.0`), modifier **seulement** `apps/PPSA99520.json` :

     ```json
     "version": "0.3.0",
     "artifact_url": "https://github.com/viviandsx/regie5/releases/download/v0.3.0/PPSA99520.zip",
     "sha256": "<empreinte de l'étape 6>",
     "icon_url": "https://raw.githubusercontent.com/viviandsx/regie5/v0.3.0/ps5/app/sce_sys/icon0.png"
     ```

  3. ouvrir la PR depuis le compte `viviandsx` (seul le propriétaire du dépôt listé peut
     mettre à jour sa fiche) ; attendre « Validate submission » au vert.

  Une seule PR ouverte à la fois pour Régie5 : si la PR automatique existe, la commenter
  plutôt que d'en ouvrir une deuxième.

Vérification locale possible avant la PR, dans un clone du catalogue :

```bash
python3 -m catalog verify PPSA99520
```

### Essais console : où sont les fichiers de l'app

À recopier tel quel dans toute liste d'étapes d'essai :

| Fichier | Vu depuis FTP / gestionnaire de fichiers | Quand |
|---|---|---|
| `config.json`, `test-config`, `config-test.json` | `/data/homebrew/PPSA99520/` | toujours |
| `settings.json`, `settings-test.json`, `launches`, copie de `config.json`, `dev/app.log` | `/mnt/sandbox/PPSA99520_000/download0/regie5/` | **seulement pendant que Régie5 est ouvert** |

`/download0` est le stockage propre au titre : la console ne le monte que pendant que le titre
tourne (à `/mnt/sandbox/<TITLE_ID>_000/download0/`). Régie5 fermé, ce dossier n'existe pas
dans un gestionnaire de fichiers. Pour y toucher : lancer Régie5, puis y aller par FTP
(ftpsrv sur 2121, etaHEN sur 1337) ; un gestionnaire de fichiers web peut ne pas montrer
`/mnt/sandbox`. `settings-test.json` (réglages de démonstration, identifiant d'appareil de
l'essai) peut rester : une release ne le lit jamais, et l'appareil a été retiré de Jellyfin.

### Après

- Les anciennes releases restent sur GitHub (le catalogue ne liste que la dernière) : ne pas
  les supprimer, des utilisateurs peuvent y revenir.
- `docs/ROADMAP.md` : version publiée, date, numéro de la PR du catalogue.
