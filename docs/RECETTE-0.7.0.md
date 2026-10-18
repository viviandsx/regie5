# Régie5 — Recette de la 0.7.0 sur PS5 (12 minutes)

Avant la publication : le zip de la release (`build/ps5/release-0.7.0/upload/PPSA99520.zip`),
installé par-dessus la 0.6.0, avec **ta** config (ton Jellyfin), la manette et un téléphone sur
le même réseau. Pas de journal dans une release : tout se juge à l'écran. **Aucune écriture
sur tes serveurs** (on ne valide aucune action). La 0.7.0 change la façon dont Régie5
travaille (mémoire, réseau, images, textes) et ajoute une police : cette recette vérifie que
**tout s'affiche comme avant, plus fluide**.

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu vois,
**ne publie rien** et note le numéro de l'étape.

## A. Mise à jour

1. Sur la PS5, appuie sur le bouton PS et ferme Régie5 depuis ce menu.
2. Sur le PC, décompresse `PPSA99520.zip`.
3. Avec ps5upload (ou PS5 Web File Manager), copie le dossier `PPSA99520` par-dessus
   `/data/homebrew/PPSA99520/`.
4. Attends 30 secondes.
5. Regarde l'accueil de la PS5 → la tuile Régie5 est dans **Jeux**.
6. Lance Régie5 → l'écran **Nouveautés de Régie5 0.7** s'ouvre, avec cinq lignes (plus fluide
   à 120 Hz, jaquettes déjà reçues, grandes bibliothèques, titres japonais/chinois/coréens,
   Réglages › Nouveautés) ; pas d'assistant.
7. Appuie sur ✕ → l'écran se ferme ; l'Accueil avec tes données et tes jaquettes.

## B. Accueil et Bibliothèque

8. Regarde « À venir » et « Ajouts récents » → tes épisodes, films et ajouts, avec leurs
   jaquettes et le fond flouté, comme en 0.6.0.
9. Appuie sur R1 → **Bibliothèque › Séries** : tes séries avec leurs jaquettes.
10. Va sur **Films**, descends vite avec R2 jusqu'en bas, puis remonte avec L2 → ça défile
    sans à-coup, les jaquettes reviennent tout de suite en remontant.
11. Ouvre une fiche (✕) puis ferme-la (○) → la fiche s'affiche comme avant.

## C. Activité, Demandes, Jellyfin

12. Appuie sur R1 → **Activité** : la file d'attente, puis Journal (→) : comme avant.
13. Appuie sur R1 → **Demandes** : tes demandes ; va sur **Découvrir** → les rangées
    Tendances, Films et séries populaires, avec leurs affiches.
14. Appuie sur R1 → **Jellyfin** : tes bibliothèques (avec leur taille), tes utilisateurs,
    les tâches planifiées.

## D. Réglages

15. Appuie sur R1 jusqu'à l'engrenage → tous tes services au vert.
16. Lis le bas de l'écran → « Version 0.7.0 ».
17. Lis la ligne « Affichage » → « 3840 × 2160, 120 Hz » (comme en 0.6.0).

## E. Depuis le téléphone

18. Va sur « Depuis le téléphone », ✕ → un QR code et un code à 6 chiffres.
19. Scanne le QR code → la page s'ouvre, en français, avec tes services.
20. N'enregistre rien. Appuie sur ○ → la fenêtre se ferme.

## F. Fin

21. Reste au moins 30 secondes dans Régie5, en passant d'un onglet à l'autre → pas de
    fermeture, pas de ligne « En attente de … » en bas à gauche (tes serveurs répondent vite).
22. Appuie sur le bouton PS, ferme Régie5, puis relance-le → il démarre normalement, sans
    Nouveautés ni assistant.
23. Appuie sur le bouton PS et ferme Régie5 → l'accueil de la PS5 est normal.

Tout est conforme : la 0.7.0 peut être publiée (`docs/ROADMAP.md`, « Pour publier la 0.7.0 »).
