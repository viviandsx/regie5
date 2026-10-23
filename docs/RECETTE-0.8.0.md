# Régie5 — Recette de la 0.8.0 sur PS5 (12 minutes)

Avant la publication : le zip de la release (`build/ps5/release-0.8.0/upload/PPSA99520.zip`),
installé par-dessus le build de test `app-19`, avec **ta** config (ton Jellyfin, ton Scrutiny
déjà ajouté pendant l'essai de `app-19`), la manette et un téléphone sur le même réseau. Pas de
journal dans une release : tout se juge à l'écran. **Aucune écriture sur tes serveurs** (on ne
valide aucune action, on n'enregistre rien depuis le téléphone).

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu vois,
**ne publie rien** et note le numéro de l'étape.

## A. Mise à jour

1. Sur la PS5, appuie sur le bouton PS et ferme Régie5 depuis ce menu.
2. Sur le PC, décompresse `PPSA99520.zip`.
3. Avec ps5upload, copie le dossier `PPSA99520` par-dessus `/data/homebrew/PPSA99520/`.
4. Attends 30 secondes.
5. Regarde l'accueil de la PS5 → la tuile Régie5 est dans **Jeux**.
6. Lance Régie5 → l'Accueil, avec tes données et tes jaquettes (l'écran Nouveautés a déjà été
   vu avec `app-19` : il ne s'ouvre pas, c'est normal).

## B. Accueil

7. Regarde le bandeau santé → comme d'habitude (tes 3 disques sont sains : pas d'alerte de
   disque).

## C. L'onglet Jellyfin (le défaut corrigé)

8. Appuie sur R1 jusqu'à **Jellyfin** → le titre, puis les pilules « Serveur · Statistiques »,
   puis « En cours de lecture » **sous** les pilules, avec un espace comme dans Activité ; le
   panneau « Personne ne regarde en ce moment » (ou tes lectures) entier.
9. Descends avec ↓ jusqu'au dernier disque → la page défile ; en haut, sous les pilules, aucun
   panneau coupé à moitié : soit entier, soit plus du tout.
10. Remonte avec ↑ jusqu'aux pilules → la page revient tout en haut, « En cours de lecture » et
    son panneau entiers.
11. Redescends sur **Disque sdb**, appuie sur ✕ → la fiche du disque ; ○ la ferme.
12. Remonte aux pilules, → **Statistiques**, ✕ → « Aucun visionnage sur cette période » (ou tes
    premières heures, si tu as regardé quelque chose depuis l'installation du plugin), sous les
    pilules, avec le même espace.
13. Va sur « 12 mois » (→ puis ✕) → même écran, rien de coupé.
14. ← jusqu'à « Serveur », ✕ → la page du serveur revient.

## D. Réglages

15. Appuie sur R1 jusqu'à l'engrenage → tous tes services au vert ; Scrutiny « 3 disques » ;
    sur la ligne Jellyfin « Statistiques : Playback Reporting 19 » ; une adresse sous chaque
    nom.
16. Lis le bas de l'écran → « Version 0.8.0 ».
17. Lis la ligne « Affichage » → « 3840 × 2160, 120 Hz » (comme en 0.7.0).

## E. Depuis le téléphone

18. Va sur « Depuis le téléphone », ✕ → un QR code et un code à 6 chiffres.
19. Scanne le QR code → la page s'ouvre, en français, avec tes services ; en bas, la section
    **Scrutiny** avec ton adresse ; sous Jellyfin, la ligne sur Playback Reporting.
20. N'enregistre rien. Appuie sur ○ → la fenêtre se ferme.

## F. Fin

21. Appuie sur le bouton PS, ferme Régie5, puis relance-le → il démarre normalement, sans
    Nouveautés ni assistant.
22. Appuie sur le bouton PS et ferme Régie5 → l'accueil de la PS5 est normal.

Tout est conforme : la 0.8.0 peut être publiée (`docs/ROADMAP.md`, « Pour publier la 0.8.0 »).
