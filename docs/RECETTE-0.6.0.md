# Régie5 — Recette de la 0.6.0 sur PS5 (10 minutes)

Avant la publication : le zip de la release (`build/ps5/release-0.6.0/upload/PPSA99520.zip`),
installé par-dessus la 0.5.1, avec **ta** config (ton Jellyfin), la manette et un téléphone sur
le même réseau. Pas de journal dans une release : tout se juge à l'écran. **Aucune écriture
sur tes serveurs** (on ne valide aucune action). Pas d'essai Plex sur la console : la 0.6.0
ajoute Plex, cette recette vérifie que **Jellyfin n'a pas changé**.

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu vois,
**ne publie rien** et note le numéro de l'étape.

## A. Mise à jour

1. Sur la PS5, appuie sur le bouton PS et ferme Régie5 depuis ce menu.
2. Sur le PC, décompresse `PPSA99520.zip`.
3. Avec ps5upload (ou PS5 Web File Manager), copie le dossier `PPSA99520` par-dessus
   `/data/homebrew/PPSA99520/`.
4. Attends 30 secondes.
5. Regarde l'accueil de la PS5 → la tuile Régie5 est dans **Jeux**.
6. Lance Régie5 → l'écran **Nouveautés de Régie5 0.6** s'ouvre, avec trois lignes (Plex
   bêta, retours des utilisateurs de Plex, Réglages › Nouveautés) ; pas d'assistant.
7. Appuie sur ✕ → l'écran se ferme ; l'Accueil avec tes données.

## B. Accueil

8. Regarde « À venir » → tes épisodes et films à venir, avec leurs jaquettes.
9. Regarde « Ajouts récents » → tes derniers ajouts Jellyfin, avec jaquettes et fond flouté.
10. Descends sur un ajout récent et appuie sur Options → le menu propose « Chercher dans
    Jelly5 » (pas « Plex »).
11. Appuie sur ○ → le menu se ferme.

## C. Onglet Jellyfin

12. Appuie sur R1 jusqu'à l'onglet **Jellyfin** → l'onglet s'appelle « Jellyfin » ; tes
    bibliothèques, tes utilisateurs et les tâches planifiées s'affichent.
13. (Si quelqu'un regarde quelque chose) Appuie sur ✕ sur la lecture → la fiche diagnostic
    s'ouvre ; appuie sur ○ pour la fermer, **sans** arrêter la lecture.

## D. Réglages

14. Appuie sur R1 jusqu'à l'engrenage → tous tes services au vert ; la ligne Jellyfin sans
    pastille « bêta ».
15. Lis le bas de l'écran → « Version 0.6.0 ».
16. Lis la ligne « Affichage » → « 3840 × 2160, 120 Hz » (comme en 0.5.1).

## E. Depuis le téléphone

17. Va sur « Depuis le téléphone », ✕ → un QR code et un code à 6 chiffres.
18. Scanne le QR code → la page s'ouvre, en français, avec tes services (et une section Plex
    vide, en bas, avec le bouton « Se connecter à Plex »).
19. N'enregistre rien. Appuie sur ○ → la fenêtre se ferme.

## F. Fin

20. Reste au moins 30 secondes dans Régie5, en passant d'un onglet à l'autre → pas de
    fermeture.
21. Appuie sur le bouton PS, ferme Régie5, puis relance-le → il démarre normalement, sans
    Nouveautés ni assistant.
22. Appuie sur le bouton PS et ferme Régie5 → l'accueil de la PS5 est normal.

Tout est conforme : la 0.6.0 peut être publiée (`docs/ROADMAP.md`, « Pour publier la 0.6.0 »).
