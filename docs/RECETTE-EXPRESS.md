# Régie5 — Recette express sur PS5 (étape 6.7)

La recette de fin de V1, **sur la console**, avec la manette et la tuile Régie5. Le journal
(requêtes HTTP, mesures d'image) arrive sur le laptop par UDP : il remplace le terminal de
la recette sous Linux. Seulement ce qui demande ta présence : les actions réelles et ce qui
se juge à l'œil ; le reste de `docs/RECETTE.md` a été vérifié automatiquement (phase 5).

Build : **app-8**, déjà installée (version de développement, elle envoie le journal). Rien
à recopier sur la console.

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu vois,
note le numéro de l'étape et continue si tu peux.

## A. À préparer avant (sans la console)

**Seerr — un utilisateur de test qui n'est pas approuvé automatiquement**

1. Seerr › Paramètres › Utilisateurs : coche « Activer la connexion locale » si elle ne l'est pas.
2. Seerr › Utilisateurs › « Créer un utilisateur local » (adresse e-mail inventée, mot de passe à toi).
3. Ouvre ses permissions : coche seulement « Demander ».
4. Vérifie que **toutes** les cases « Approbation automatique » (films, séries, 4K) sont décochées, et qu'il n'a ni « Admin » ni « Gérer les demandes ».
5. Dans une fenêtre de navigation privée, connecte-toi à Seerr avec cet utilisateur.
6. Demande un **premier film** (il sera refusé : n'importe lequel absent de ta bibliothèque).
7. Demande un **second film** (il sera approuvé : Radarr l'ajoutera et le cherchera vraiment ; choisis-en un que tu acceptes d'avoir, ou prévois de le retirer).
8. Ferme la fenêtre privée. Dans Seerr (ton compte) › Demandes → les deux demandes sont « En attente ».

**Téléphone — une lecture qui transcode**

9. Dans l'app Jellyfin du téléphone, avec ton propre compte : Paramètres › Lecture › qualité / débit maximal sur une valeur basse (par exemple 720p, 2 Mbit/s).
10. Choisis un film à lancer au point E, sans le lancer encore.

**Ménage**

11. Pense à deux ou trois films petits et sans valeur, déjà vus, que tu acceptes de supprimer **avec leurs fichiers**. Le choix final se fait à l'étape 57, parmi ceux que Régie5 propose.

**Laptop**

12. Lance le journal et laisse-le ouvert jusqu'à la fin :

```bash
scripts/ps5-log.sh
```

## B. Tuile, écran, manette

13. Sur l'accueil de la PS5, pose-toi sur la tuile Régie5 → icône « R5 » en verre ; fond graphite avec « R5 » à droite.
14. Ouvre Régie5 → écran de lancement « R5 » et « Régie5 », puis l'Accueil.
15. Compare l'horloge en haut à droite avec ton téléphone → même minute.
16. Regarde l'image de près → nette, textes lisibles à 3 m.
17. Croix directionnelle : parcours les jaquettes de l'Accueil → la goutte glisse avec un rebond ; le fond flouté change en fondu.
18. Maintiens → (droite) → le déplacement se répète.
19. Appuie sur R1 plusieurs fois → les onglets défilent jusqu'à l'engrenage (Réglages).
20. Appuie sur L1 jusqu'à revenir à l'Accueil → mêmes onglets en sens inverse.
21. Appuie sur △ → l'Accueil se recharge (squelettes brefs, puis les données).
22. Va dans Bibliothèque, ← / → jusqu'à la pilule « Films », ✕ → la grille des films.
23. Maintiens ↓ → défilement sans à-coup ; jaquettes en fondu, jamais d'image cassée.
24. Appuie plusieurs fois sur R2 → saut à la lettre suivante (indicateur de lettre).
25. Appuie sur L2 → lettre précédente.
26. Appuie sur ○ → retour à la barre d'onglets.
27. Fais le tour de tous les onglets (R1) → aucun texte coupé de façon gênante (sauf titres longs finis par « … ») ; tout en français.

## C. Bibliothèque

28. Bibliothèque › Séries : ✕ sur une série que tu connais bien → saisons, épisodes présents / manquants, profil et qualité identiques à sa page Sonarr.
29. Appuie sur Options → menu « Chercher dans Jelly5 » / Annuler, focus sur Annuler.
30. Appuie sur ○ deux fois → retour à la grille.

## D. Jellyfin : scan

31. Onglet Jellyfin : descends jusqu'au bouton « Scanner les bibliothèques », ✕ → « Scanner toutes les bibliothèques ? », focus sur **Annuler**.
32. → puis ✕ sur « Scanner » → note « Scan des bibliothèques lancé ».
33. Descends à « Tâches planifiées » → « Analyser la médiathèque » passe en cours.

## E. Jellyfin : diagnostic et arrêt d'une lecture

34. Sur le téléphone, lance le film choisi à l'étape 10.
35. Dans Régie5, onglet Jellyfin, attends quelques secondes → ta session apparaît, avec « Transcodage ».
36. ✕ sur ta session → fiche diagnostic : raisons, source / envoyé au client, accélération, « le transcodage suit-il ? ».
37. Compare avec Jellyfin › Tableau de bord › Activité (navigateur) → mêmes méthode et raisons.
38. Dans la fiche, ✕ sur « Arrêter la lecture » → « Arrêter la lecture ? », focus sur **Annuler**.
39. → puis ✕ sur « Arrêter » → la lecture s'arrête sur le téléphone ; note « Lecture arrêtée » ; la session disparaît.
40. Sur le téléphone, remets la qualité de lecture d'origine.

## F. Demandes Seerr

41. Onglet Demandes, filtre « En attente » → les deux demandes de test, avec jaquette, demandeur, date.
42. ✕ sur la **première** → feuille Approuver / Refuser / Annuler, focus sur Annuler.
43. ← une fois, ✕ sur « Refuser » → « Refuser cette demande ? », focus sur **Annuler**.
44. → puis ✕ sur « Refuser » → note « Demande refusée » ; elle quitte « En attente ».
45. ✕ sur la **seconde** → même feuille, focus sur Annuler.
46. ← deux fois, ✕ sur « Approuver » → « Approuver cette demande ? », focus sur **Annuler**.
47. → puis ✕ sur « Approuver » → note « Demande approuvée » ; elle passe dans « Approuvées ».
48. Dans Seerr (navigateur) → la première refusée, la seconde approuvée.

## G. Ménage

49. Réglages : ✕ sur « Ménage : désactivé » → avertissement, focus sur **Annuler**.
50. ✕ sur Annuler → rien ne change, toujours « désactivé ».
51. ✕ à nouveau, → puis ✕ sur « Activer » → « Ménage : activé ».
52. Bibliothèque : → jusqu'à la nouvelle pilule « Ménage », ✕ → la liste des candidats et le total récupérable. Si elle est vide : pilules « Vu par : au moins un » et « Non vu : 3 mois ».
53. Pour 3 candidats, compare avec Jellyfin (vu par qui, dernière lecture, pas en favori) → les critères sont remplis.
54. Sur un candidat que tu veux garder : Options › « Garder » → note « « … » gardé » ; il quitte la liste.
55. Pilule « Gardés : 1 », ✕ → il y est.
56. Options › « Ne plus garder » → il revient dans les candidats.
57. Choisis **le** film à supprimer dans la liste. Dans Radarr (navigateur), note son titre, son année, sa taille et son instance.
58. Dans Régie5, sur ce film : Options › « Supprimer… » → feuille 1 : titre, année, taille, instance justes ; focus sur Annuler.
59. → puis ✕ sur « Continuer » → feuille 2 « Supprimer définitivement ? », focus sur Annuler.
60. → puis ✕ sur « Supprimer » → note « « … » supprimé : X libérés · Seerr le saura à sa prochaine synchronisation ».
61. Dans Radarr → le film et ses fichiers ont disparu ; il est dans Paramètres › Listes › Exclusions.
62. Dans Seerr, après sa tâche « Media Availability Sync » (la nuit, ou lancée à la main dans Paramètres › Tâches) → le film n'est plus « disponible » et peut être redemandé ; ses anciennes demandes sont gardées.
63. Réglages : désactive le Ménage si tu ne t'en sers pas.

## H. Fin

64. Ferme Régie5 (bouton PS › fermer l'application).
65. Sur le laptop, arrête le journal (Ctrl-C).
66. Envoie-moi la sortie des deux commandes ci-dessous.

Toutes les écritures de la recette, une ligne chacune : scan, arrêt de lecture, refus,
approbation (`POST`), puis **un seul** `DELETE` vers Radarr ou Sonarr (rien vers Seerr) :

```bash
grep -E "http: (DELETE|POST)" "$(ls -t build/ps5/logs/console-*.log | grep -v driver | head -1)"
```

La mesure d'image sur la console :

```bash
grep -E "^fps|slow frame" "$(ls -t build/ps5/logs/console-*.log | grep -v driver | head -1)" | tail -60
```

Attendu : `fps: 60 frames` en régime normal, `cpu worst` sous 16 ms ; quelques « slow
frame » à l'ouverture d'un écran sont connues (glyphes nouveaux). Le `gpu avg` voisin de
16,4 ms est l'attente de l'image suivante, pas la charge.

**Anglais** (facultatif) : l'app suit la langue de la console ; les textes anglais ont été
vérifiés sur les captures Linux.
