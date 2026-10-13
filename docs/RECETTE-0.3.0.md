# Régie5 — Recette de la 0.3.0 sur PS5

Avant la publication : le zip de la release (`build/ps5/release-0.3.0/upload/PPSA99520.zip`),
installé par-dessus la 0.2.0, sur la console, avec la manette. Pas de journal UDP dans
une release : tout se juge à l'écran. **Lecture seule** : aucune étape n'envoie
d'écriture aux serveurs (chaque fenêtre est fermée avec ○).

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu
vois, note le numéro de l'étape et continue si tu peux.

## A. Installation

1. Ferme Régie5 sur la console.
2. Décompresse `PPSA99520.zip` sur le PC.
3. Copie le dossier `PPSA99520` par-dessus `/data/homebrew/PPSA99520/`.
4. Attends 30 secondes.
5. Regarde l'accueil → la tuile Régie5 est dans **Multimédia** (plus dans Jeux).
6. Lance Régie5 → l'Accueil s'affiche, avec tes données.

## B. Réglages

7. Appuie sur R1 jusqu'à l'engrenage → tous les services au vert, chacun avec son adresse.
8. Lis le bas de l'écran → « Version 0.3.0 », puis la ligne « Affichage : … » en entier.
   Si ton réglage était resté sur « 120 Hz » (essais d'app-10d) : « 3840 × 2160, 120 Hz ».
   Sinon : « 3840 × 2160, 60 Hz ».
9. Descends sur les boutons et va sur « Fréquence d'affichage : … ».
10. Appuie sur ✕ jusqu'à « Fréquence d'affichage : Auto (60 Hz) » → si l'affichage était
    à 120 Hz, la ligne dit « nouveau réglage au prochain lancement ».
11. Ferme Régie5 et relance-le.
12. Retourne dans Réglages → « Affichage : 3840 × 2160, 60 Hz », sans autre mention.

## C. Pastilles

13. Regarde la barre d'onglets → une pastille sur Jellyfin si quelqu'un regarde, sur
    Activité s'il y a des téléchargements, sur Demandes s'il y a des demandes en attente
    (rien quand il n'y a rien).

## D. Demandes › Signalements

14. Appuie sur L1 jusqu'à Demandes.
15. Appuie sur → puis ✕ → la pilule « Signalements » est choisie.
16. Appuie sur → deux fois, puis ✕ → le filtre « Résolus », ton signalement résolu s'affiche.
17. Appuie sur ↓ puis ✕ → le fil s'ouvre, chaque commentaire avec son auteur.
18. Appuie sur ○ → le fil se ferme.

## E. Jellyfin

19. Appuie sur R1 → l'onglet Jellyfin.
20. Descends jusqu'à « Bibliothèques » → Films, Séries (avec leurs nombres et leur taille)
    et Collections (« 46 collections »).
21. Descends encore → « Utilisateurs », chacun avec « Actif il y a … ».

## F. Fin

22. Ferme Régie5 → l'accueil de la PS5 est normal.
23. Note les numéros des étapes qui n'ont pas donné ce qui est écrit.
