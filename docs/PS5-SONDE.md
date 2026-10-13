# Régie5 — Sonde PS5 (phase 6.1)

La sonde est Régie5 lui-même, en **mode démo** : données d'exemple embarquées, **aucun
réseau, aucune config, aucune action réelle**. Elle répond à une question : l'interface
tourne-t-elle sur la console avec ps5-opengl (OpenGL 4.6, contexte EGL), à 60 images/s en
4K, flou compris ? Si oui, on garde cette voie ; sinon, voie A (backend de Jelly5).

**Sonde validée le 12 octobre 2026 avec probe-6** (60 images/s en 4K) : la suite est
l'app elle-même, `docs/PS5-APP.md`. probe-8 (même sonde, glyphes préchauffés et atlas de
48 Mio) n'est utile que pour comparer les pics d'ouverture d'écran.

Dossier de la sonde : **`build/ps5/probe-8/PPSA99521`** (titre « Régie5 Probe », Title ID
`PPSA99521`). Il **remplace** le dossier `PPSA99521` déjà sur la console. Les dossiers
`probe-1` à `probe-7` et `probe-scan` sont d'anciennes versions : ne pas les utiliser (tu
peux les supprimer).

**Historique** : probe-2 (firmware 13.60) plantait juste après sa première ligne de journal
(« Une erreur s'est produite… »). Le journal n'était relayé que toutes les 200 ms : la
suite était perdue. probe-3 envoie chaque étape du démarrage **avant** de l'exécuter,
signale un plantage (signal, adresse fautive) et n'utilise plus les fonctions de date de
la libc de la console (`localtime`, `mktime`, `time`, `strftime`), principales suspectes.

probe-3 démarrait jusqu'à OpenGL 4.6 en 4K, puis s'arrêtait sans rapport juste après
« step: screens ». probe-4 : plus aucun gros tampon sur la pile des threads de travail (un
de 64 Kio, appelé à chaque requête de démo, suspect principal : un débordement de pile ne
laisse aucun rapport) ; la mémoire du tas journalisée à chaque étape, autour de chaque
écran et toutes les 10 s ; un rapport pour une allocation qui échoue, `abort`, `exit`,
`_Exit` et les arrêts de libc++ ; tas de 512 Mio ; budgets console (64 Mio de jaquettes,
24 Mio d'atlas) ; Réglages ne teste plus les services avant d'être ouvert.

probe-4 a planté pendant « step: fonts » (« Une erreur s'est produite »). Le journal dit
pourquoi : la console a **refusé le tas de 512 Mio** (« heap NOT READY »), l'app a continué
sur le malloc du système, beaucoup trop petit, et la première grosse allocation (le chargement
des polices) a échoué. Dans cette chaîne, un `new` qui échoue exécute une instruction
invalide (`ud2`) : c'est le signal 4 du journal, à l'adresse `0x4001a9`, qui est
`operator new` dans `llvm-pie.elf`. Ce n'est pas un mélange d'allocateurs : `free` sait à
qui appartient chaque bloc. L'adresse « rip » du rapport était fausse (la structure du
contexte de la console semble différer de celle du SDK).

probe-5 :
- **Tas** : essaie 256 Mio, puis 192, puis 128 (128 passait en probe-3), garde le premier
  accordé et le dit (« heap: 256 MiB granted » ou « refused (mmap errno …) »). **Si aucun
  n'est accordé, la sonde s'arrête aussitôt** avec une ligne « STOP: the console refused
  the title's memory » (rien à l'écran, retour à l'accueil), au lieu de planter plus loin.
  Toute allocation qui échoue est signalée avec sa taille, y compris sur le malloc du système.
- **Budgets** selon le tas obtenu : jaquettes 64 / 48 / 32 Mio, pages de glyphes 24 / 24 /
  16 Mio (256 / 192 / 128 Mio de tas).
- **Piles** : les threads de travail (lecture des données, JSON, décodage des jaquettes)
  demandent 512 Kio au lieu des 64 Kio par défaut de la console. Sous Linux, le tour
  complet passe même avec 16 Kio par thread (`REGIE5_WORKER_STACK_KIB=16`) : le travail de
  ces threads tient largement dans 64 Kio, c'est une marge.
- **Rapport de plantage** : le rip est pris à l'adresse fautive pour une instruction
  invalide, et la pile est lue depuis le gestionnaire lui-même (plus de dépendance à la
  structure du contexte).

probe-5 **tourne sur la console** (13.60) : tour complet, fermeture propre, tas de 256 Mio
accordé, ~20 Mio utilisés et stable. Mais **≈ 3 images/s** : ~470 ms par image dans
« submit », avec à chaque image `[ps5-gallium] software-blit color=3840x2160->3840x2160
filter=0`. C'est le `glBlitFramebuffer` de fin d'image (scène → écran, même taille,
`GL_NEAREST`) : ps5-gallium le fait **au processeur**. Le flou (~2 ms) n'y est pour rien.

probe-6 : plus aucun `glBlitFramebuffer`. La scène est posée à l'écran par un **quad
texturé** (un petit shader de copie), comme le fait ps5-homebrew-ui avec son canevas ; la
réduction de la zone à flouter passe aussi par ce quad. Rendu identique sous Linux (captures
comparées en 1080p et 4K). Option de mesure : un fichier vide nommé **`output-1080p`** dans
`/data/homebrew/PPSA99521/` fait démarrer la sonde en sortie 1080p (le journal dit
« output asked: 1080p ») ; sans lui, 4K.

À regarder dans le journal : plus aucune ligne `software-blit`, et la ligne de fin de tour
(`probe: tour finished … avg_ms … worst_ms`) : objectif 16,7 ms.

Point à surveiller : si 256 Mio est accordé mais que l'affichage ne démarre plus
(« the display did not open » ou un plantage dans « step: display »), c'est que le tas prend
la mémoire dont OpenGL a besoin : dis-le-moi, on reviendra à 128 Mio.

## Ce qu'elle fait

1. Démarrage : quelques secondes sur l'image de démarrage (compilation des shaders en 4K).
2. **≈ 75 s de tour automatique** : tous les onglets défilent seuls, des menus et feuilles
   s'ouvrent et se ferment, défilement rapide des films. Ne touche à rien.
3. Ensuite **la manette est à toi** pendant 4 minutes : croix ou stick gauche, ✕, ○,
   L1/R1, L2/R2, △, Options.
4. Elle **se ferme toute seule** 4 minutes après le tour, proprement. Pour la fermer avant :
   **maintiens Options + ○ pendant 2 secondes**.

Elle mesure chaque image et envoie son journal au laptop.

## Avant de commencer (laptop)

Le pare-feu `ufw` du laptop est actif : il bloquera probablement le journal envoyé par la
console. Pour l'autoriser, **depuis ton réseau local seulement** :

```bash
sudo ufw allow from 192.168.1.0/24 to any port 5555 proto udp
```

Puis, dans un terminal laissé ouvert pendant tout le test :

```bash
scripts/ps5-log.sh
```

Chaque lancement écrit un nouveau fichier `build/ps5/logs/console-<date>.log`. Après le
test, tu peux retirer l'autorisation :

```bash
sudo ufw delete allow from 192.168.1.0/24 to any port 5555 proto udp
```

## Le test (console)

1. **Lance le jailbreak** comme d'habitude, avec ShadowMount+ et ton serveur ps5upload ou FTP.
2. **Envoie le dossier** `build/ps5/probe-6/PPSA99521` dans **`/data/homebrew/`** (comme
   pour Jelly5), pour que `/data/homebrew/PPSA99521/eboot.bin` existe (il remplace
   l'ancien).
3. **Attends ~10 secondes** : ShadowMount+ ajoute la tuile « Régie5 Probe ».
4. **Ouvre-la**, et ne touche à rien jusqu'à la fin du tour (≈ 75 s, plus le démarrage).
5. **Essaie la manette** une minute.
6. **Ferme** avec Options + ○ (2 s), ou laisse-la se fermer seule.
7. Dis-moi « fini » et ce que tu as vu (fluidité, texte net ou flou, couleurs, verre,
   quelque chose de bizarre). Je lis le journal sur le laptop.

## Si la sonde se ferme tout de suite

Retour à l'accueil sans rien afficher, sans message d'erreur : regarde le journal. La ligne
« STOP: the console refused the title's memory » veut dire qu'aucune taille de tas n'a été
accordée ; ferme les autres applications (jeux compris) et relance une fois. Dis-moi ce que
disent les lignes « heap: … ».

## Si la console fige

1. **Attends une minute** : un écran noir ou l'image de démarrage quelques secondes au
   lancement est normal.
2. **Bouton PS** : si le menu s'affiche, ferme « Régie5 Probe » depuis le sélecteur
   d'applications. C'est la fermeture normale.
3. **Si rien ne répond après une minute** : maintiens le **bouton d'alimentation de la
   console** jusqu'au deuxième bip et à l'extinction (≈ 10 s). Au redémarrage, la PS5 peut
   vérifier son stockage : laisse-la finir.
4. **Récupérer le journal** (sans FTP, le journal UDP suffit) :
   - Si `scripts/ps5-log.sh` tournait et recevait des lignes, **le journal jusqu'au blocage
     est déjà sur le laptop** (`build/ps5/logs/`). Rien d'autre à faire : dis-moi « figé ».
   - Sinon, et seulement avec un serveur FTP sur la console : le **mode secours** est
     désactivé par défaut. Pour l'activer, ajoute un fichier vide nommé `safe-mode` dans
     `/data/homebrew/PPSA99521/`. Rouvre alors **une fois** « Régie5 Probe » : elle voit que
     la fois précédente n'est pas allée au bout, **ne dessine rien** (l'image de démarrage
     reste) et reste ouverte 5 minutes. Pendant ce temps, sur le laptop :

     ```bash
     scripts/ps5-fetch-log.py <adresse IP de la PS5> 2121
     ```

     (`1337` à la place de `2121` si ton FTP est celui d'etaHEN.) Le script copie, en
     lecture seule, `app.prev.log` (le lancement figé) dans `build/ps5/logs/`. Avec un
     client FTP (FileZilla…), le fichier est
     `/mnt/sandbox/PPSA99521_000/download0/regie5/dev/app.prev.log`.
   - Elle se ferme seule après 5 minutes. Ne la relance pas : dis-moi « figé ».

Avec le fichier `safe-mode`, si tu fermes la sonde toi-même par le bouton PS **avant la fin
du tour**, le lancement suivant part aussi dans ce mode « rien à l'écran pendant 5
minutes » : c'est voulu. Sans ce fichier, chaque lancement est normal.

## Après

Supprime le dossier `/data/homebrew/PPSA99521` de la console quand on n'en a plus besoin
(c'est toi qui supprimes sur la console).

## Limites connues

- La chaîne d'empaquetage (ps5-native-app-boilerplate) est vérifiée sur les firmwares
  6.02 et 12.70 ; Jelly5 l'est sur 11.60 et 13.60 avec une autre chaîne. Un autre firmware
  est possible mais non vérifié.
- La catégorie du titre est « jeu » (comme Homebrew UI Lab) : la tuile peut apparaître avec
  les jeux plutôt que dans Média.
