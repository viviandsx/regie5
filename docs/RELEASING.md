# Régie5 — travailler et publier une version

Mode d'emploi depuis la publication (dépôt public https://github.com/viviandsx/regie5,
release `v0.2.0`, fiche au catalogue homebrew.page). Règles du catalogue suivies :
`docs/submitting.md` et `docs/versioning.md` de `blackbearreloaded/ps5-homebrew-catalog`.

---

## 1. Branches et envois

### Où on travaille

Sur le dépôt GitHub, une seule branche : `main`. En local, deux branches aujourd'hui :

| Branche locale | Contenu | Règle |
|---|---|---|
| `public` | l'historique neuf, suit `origin/main` | **la branche de travail** |
| `main` | l'ancien historique (avec des mentions retirées de la version publique) | archive, **jamais poussée** |

Pour ne plus se tromper, renommer une fois les deux branches (rien n'est perdu, rien n'est
envoyé) :

```bash
git branch -m main archive-privee
```

```bash
git branch -m public main
```

```bash
git branch -u origin/main main
```

```bash
git config push.default upstream
```

Après ça, `git push` sans argument n'envoie que `main` vers `origin/main`. Ne jamais lancer
`git push --all`, ni `git push origin archive-privee`.

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

## 2. Tant que la PR #244 n'est pas fusionnée

La fiche proposée (`apps/PPSA99520.json`) pointe vers des choses précises. Si l'une change,
les vérifications du catalogue échouent ou la relecture recommence.

| Ne pas toucher | Pourquoi |
|---|---|
| Le tag `v0.2.0` (ni supprimer, ni déplacer) | `version` de la fiche = le tag ; `icon_url` est lu **à ce tag** |
| La release « Régie5 0.2.0 » et son `PPSA99520.zip` (ni remplacer, ni renommer, ni supprimer) | `artifact_url` et `sha256` de la fiche : un autre zip, même identique en apparence, n'a pas la même empreinte |
| `ps5/app/sce_sys/icon0.png` et `param.json` **au tag** | le catalogue lit `contentVersion` dans le `param.json` voisin de l'icône. Les modifier sur `main` ne gêne pas (le tag reste figé) |
| Le dépôt : nom, visibilité (public), propriétaire | `source_repo`, et « le compte qui ouvre la PR possède le dépôt » |
| La branche `viviandsx-patch-1` du fork | seulement pour répondre au mainteneur, et seulement `apps/PPSA99520.json` (tout autre fichier fait échouer la PR) |

À éviter aussi pendant la relecture : **publier une autre release ou une pré-release** du
dépôt. La tâche quotidienne du catalogue suit les nouvelles releases, pré-releases comprises ;
une 0.3.0 qui sort pendant la relecture obligerait à refaire la fiche.

Si le mainteneur demande un changement :

- sur la fiche (description, licence…) : modifier `apps/PPSA99520.json` sur
  `viviandsx-patch-1` ; les contrôles se relancent seuls ;
- sur le zip lui-même : **pas de remplacement**. On sort une `0.2.1` (§3) et la fiche est
  mise à jour vers elle (`version`, `artifact_url`, `sha256`, `icon_url`).

Le travail sur `main` (code, doc, README) continue normalement pendant ce temps.

---

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

Une fois Régie5 listé (PR #244 fusionnée), deux chemins :

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

### Après

- Les anciennes releases restent sur GitHub (le catalogue ne liste que la dernière) : ne pas
  les supprimer, des utilisateurs peuvent y revenir.
- `docs/ROADMAP.md` : version publiée, date, numéro de la PR du catalogue.
