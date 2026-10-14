# Régie5 — Recette de la 0.3.2 sur PS5 (15 minutes)

Avant la publication : le zip de la release (`build/ps5/release-0.3.2/upload/PPSA99520.zip`),
installé par-dessus la 0.3.1, sur la console, avec la manette et un téléphone sur le même
réseau. Pas de journal dans une release : tout se juge à l'écran. Seule écriture : le
`config.json` de la console, réenregistré tel quel depuis la page du téléphone.

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu vois,
**ne publie rien** et note le numéro de l'étape.

## A. Installation

1. Ferme Régie5 sur la console.
2. Décompresse `PPSA99520.zip` sur le PC.
3. Copie le dossier `PPSA99520` par-dessus `/data/homebrew/PPSA99520/`.
4. Attends 30 secondes.
5. Regarde l'accueil → la tuile Régie5 est dans **Jeux**.
6. Lance Régie5 → l'Accueil s'affiche, avec tes données.

## B. Réglages et 120 Hz

7. Appuie sur R1 jusqu'à l'engrenage → tous les services au vert.
8. Lis le bas de l'écran → « Version 0.3.2 ».
9. Va sur « Fréquence d'affichage : … » et appuie sur ✕ jusqu'à « Fréquence d'affichage :
   Auto » (sans « (60 Hz) ») → plus aucune mention « expérimental ».
10. Ferme Régie5, relance-le, retourne dans Réglages → « Affichage : 3840 × 2160, 120 Hz »,
    en gris (pas en jaune), et le menu de ton écran (OSD) annonce 120 Hz.

## C. Depuis le téléphone (à chaque version)

11. Va sur « Depuis le téléphone », ✕ → un QR code et un code à 6 chiffres.
12. Scanne le QR code → la page s'ouvre sur le téléphone.
13. Sans rien changer, appuie sur « Enregistrer sur la console » → la page dit
    « Enregistré. Régie5 teste les services : regardez la TV. » ; sur la télé, les
    services restent au vert.
14. ○ pour fermer la fenêtre.

## D. Fiche d'un film (le défaut corrigé)

15. L1 jusqu'à Bibliothèque, → jusqu'à la pilule « Films », ✕, ↓ puis ✕ sur un film sorti
    → sous « Fichier », une ligne par date : « Au cinéma », « Sortie numérique »,
    « Sortie physique » (celles que Radarr connaît), chacune entière, sans « … ».
16. ○, ← jusqu'à « Séries », ✕, ↓ puis ✕ sur une série → les lignes sont entières, et la
    saison sélectionnée est visible dans la liste « Saisons » ; ↓ la fait descendre.
17. ○ pour revenir.

## E. Fin

18. Laisse Régie5 ouvert quelques minutes en naviguant normalement → pas de fermeture.
19. Ferme Régie5 → l'accueil de la PS5 est normal.
