# Régie5 — Recette de la 0.5.1 sur PS5 (5 minutes)

Avant la publication : le zip de la release (`build/ps5/release-0.5.1/upload/PPSA99520.zip`),
installé par-dessus ce que la console a (0.5.0 ou le zip de test `emby-fix-1`), avec ton
Jellyfin, la manette et un téléphone sur le même réseau. Pas de journal dans une release :
tout se juge à l'écran. Aucune écriture sur tes serveurs.

Nouveau en 0.5.1 : images d'Emby à la bonne taille, rapport de plantage corrigé, garde-fou de
`config.json`. Le code est celui d'`emby-fix-1`, déjà essayé sur la console avec ton Jellyfin
(16 octobre) ; seuls changent les numéros, le README et `INSTALL.txt`. Cette recette ne revoit
donc que la mise à jour et ce qui se vérifie à chaque version.

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu vois,
**ne publie rien** et note le numéro de l'étape.

## A. Mise à jour

1. Ferme Régie5.
2. Décompresse `PPSA99520.zip` sur le PC.
3. Copie le dossier `PPSA99520` par-dessus `/data/homebrew/PPSA99520/`.
4. Attends 30 secondes.
5. Regarde l'accueil de la PS5 → la tuile Régie5 est dans **Jeux**.
6. Lance Régie5 → **ni assistant, ni écran Nouveautés** (même série 0.5) ; l'Accueil avec
   tes données, jaquettes et fond flouté.

## B. Réglages

7. Appuie sur R1 jusqu'à l'engrenage → tous les services au vert.
8. Lis le bas de l'écran → « Version 0.5.1 » (pas « 0.5.1-test1 »).
9. Lis la ligne « Affichage » → « 3840 × 2160, 120 Hz » (comme en 0.5.0).

## C. Depuis le téléphone (à chaque version)

10. Va sur « Depuis le téléphone », ✕ → un QR code et un code à 6 chiffres.
11. Scanne le QR code → la page s'ouvre, en français, avec tes services.
12. N'enregistre rien. Appuie sur ○ → la fenêtre se ferme.

## D. Fin

13. Reste au moins 30 secondes dans Régie5, en passant d'un onglet à l'autre → pas de
    fermeture.
14. Ferme Régie5 et relance-le → il démarre normalement, sans assistant.
15. Ferme Régie5 → l'accueil de la PS5 est normal.

Tout est conforme : la 0.5.1 peut être publiée (`docs/ROADMAP.md`, « Pour publier la 0.5.1 »),
dès que le testeur Emby confirme ou au plus tard le 18 octobre 2026.
