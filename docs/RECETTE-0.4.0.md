# Régie5 — Recette de la 0.4.0 sur PS5 (10 minutes)

Avant la publication : le zip de la release (`build/ps5/release-0.4.0/upload/PPSA99520.zip`),
installé par-dessus la 0.3.2, sur la console, avec la manette et un téléphone sur le même
réseau. Pas de journal dans une release : tout se juge à l'écran. Seule écriture : le
`config.json` de la console, réenregistré tel quel depuis la page du téléphone.

Nouveau en 0.4.0 : **Emby (beta)** et l'**assistant de première configuration**. Déjà vus sur
la console avec des builds de test : Emby n'a rien changé pour Jellyfin (`emby-test-1`,
14 octobre) ; l'assistant, config absente et rouvert (`app-16`, 15 octobre) ; Emby de bout en
bout contre l'Emby de test 4.10.1.0, assistant avec le choix Emby compris (`app-17`, essai B,
15 octobre). Cette recette ne revoit donc que la mise à jour, avec ta config Jellyfin.

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu vois,
**ne publie rien** et note le numéro de l'étape.

## A. Mise à jour depuis la 0.3.2

1. Vérifie dans Réglages que la console a la 0.3.2 → « Version 0.3.2 ». Ferme Régie5.
2. Décompresse `PPSA99520.zip` sur le PC.
3. Copie le dossier `PPSA99520` par-dessus `/data/homebrew/PPSA99520/`.
4. Attends 30 secondes.
5. Regarde l'accueil de la PS5 → la tuile Régie5 est dans **Jeux**.
6. Lance Régie5 → **pas d'assistant** : l'Accueil s'affiche, avec tes données.

## B. Réglages

7. Appuie sur R1 jusqu'à l'engrenage → tous les services au vert.
8. Regarde la ligne du serveur multimédia → elle s'appelle « Jellyfin », **sans** pastille
   « bêta ».
9. Lis le bas de l'écran → « Version 0.4.0 ».
10. Lis la ligne « Affichage » → « 3840 × 2160, 120 Hz » (ce que la 0.3.2 donnait).
11. Regarde la rangée de boutons → cinq boutons, le dernier est « Assistant », tous entiers.

## C. Depuis le téléphone (à chaque version)

12. Va sur « Depuis le téléphone », ✕ → un QR code et un code à 6 chiffres.
13. Scanne le QR code → la page s'ouvre ; sous Jellyfin, une section « Emby » vide et
    décochée.
14. Sans rien changer, touche « Enregistrer sur la console » → « Enregistré » ; sur la télé,
    les services restent au vert.
15. Appuie sur ○ → la fenêtre se ferme.

## D. Jellyfin inchangé

16. L1 jusqu'à l'onglet Jellyfin → titre « Jellyfin », lectures en cours, bibliothèques,
    utilisateurs.
17. R1 jusqu'à Activité, → « Journal » → des lectures et connexions Jellyfin.
18. Bibliothèque › une fiche film › Options (M) → « Chercher dans Jelly5 ».

## E. Fin

19. Navigue quelques minutes entre les onglets → pas de fermeture.
20. Ferme Régie5 → l'accueil de la PS5 est normal.

Tout est conforme : la 0.4.0 peut être publiée (`docs/RELEASING.md` §3, étapes 3 à 6).
