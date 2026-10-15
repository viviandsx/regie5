# Régie5 — Recette de la 0.4.0 sur PS5 (25 minutes)

Avant la publication : le zip de la release (`build/ps5/release-0.4.0/upload/PPSA99520.zip`),
installé par-dessus la 0.3.2, sur la console, avec la manette et un téléphone sur le même
réseau. Pas de journal dans une release : tout se juge à l'écran. Seule écriture : le
`config.json` de la console, réenregistré depuis la page du téléphone.

Nouveau en 0.4.0 : **Emby** (clé `"emby"` à la place de `"jellyfin"`) et l'**assistant de
première configuration**. Déjà vus sur la console avec des builds de test : Emby n'a rien
changé pour Jellyfin (`emby-test-1`, 14 octobre) ; l'assistant avec une config absente,
« Plus tard », la page du téléphone et la vérification (`app-16`, 15 octobre). Une release
n'a pas le mode d'essai `test-config` : le cas « config absente » n'est **pas** refait ici
(il ferait perdre la copie de secours de ta config) ; l'assistant se rouvre depuis Réglages.

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu vois,
**ne publie rien** et note le numéro de l'étape.

## A. Installation et mise à jour depuis la 0.3.2

1. Ferme Régie5 sur la console.
2. Décompresse `PPSA99520.zip` sur le PC.
3. Copie le dossier `PPSA99520` par-dessus `/data/homebrew/PPSA99520/`.
4. Attends 30 secondes.
5. Regarde l'accueil → la tuile Régie5 est dans **Jeux**.
6. Lance Régie5 → **pas d'assistant** (ta config est remplie) : l'Accueil s'affiche, avec
   tes données.

## B. Réglages

7. Appuie sur R1 jusqu'à l'engrenage → tous les services au vert, la ligne s'appelle
   « Jellyfin ».
8. Lis le bas de l'écran → « Version 0.4.0 », « Affichage : 3840 × 2160, 120 Hz ».
9. Regarde la rangée de boutons → cinq boutons, le dernier est « Assistant », tous
   entiers (aucun coupé ni qui déborde).

## C. Depuis le téléphone (à chaque version)

10. Va sur « Depuis le téléphone », ✕ → un QR code et un code à 6 chiffres.
11. Scanne le QR code → la page s'ouvre ; elle a une section « Emby » (vide, décochée)
    sous Jellyfin, avec la note « À remplir seulement si votre serveur multimédia est Emby ».
12. Sans rien changer, appuie sur « Enregistrer sur la console » → « Enregistré » ; sur la
    télé, les services restent au vert.
13. ○ pour fermer la fenêtre.

## D. L'assistant, rouvert depuis Réglages

14. Va sur « Assistant », ✕ → « Régie5 », « Bienvenue ».
15. ✕ « Commencer » → « Votre serveur multimédia », focus sur « Jellyfin ».
16. ✕ « Jellyfin » → le QR code et le code. Scanne-le → la page ne montre **pas** de
    section Emby ; tes services y sont, clés gardées (« Clé enregistrée — vide pour la
    garder »).
17. Sans rien changer, « Enregistrer sur la console » → sur la télé, l'écran
    « Vérification » s'ouvre tout seul, tous tes services au vert.
18. ✕ « Commencer » → l'Accueil, avec tes données.
19. Ferme Régie5, relance-le → pas d'assistant.

## E. Écrans Jellyfin (non-régression)

20. Onglet Jellyfin → titre « Jellyfin », lectures en cours, bibliothèques, utilisateurs.
21. Activité › Journal → des lectures et connexions Jellyfin.
22. Bibliothèque › une fiche film › Options (M) → « Chercher dans Jelly5 ».

## F. Facultatif : Emby depuis la console (15 minutes de plus)

Seulement si le testeur de l'issue n'a pas pu vérifier un point, ou si tu veux le voir
toi-même : l'Emby de test du laptop sur le réseau local (port 18096 ouvert dans ufw le
temps de l'essai). Les commandes et les étapes sont celles de
`build/ps5/emby-test-1/ESSAIS.txt`, partie B (adresse `http://192.168.1.15:18096`,
Jellyfin décoché, Emby coché) ; à la fin, Jellyfin recoché, Emby décoché, l'Emby de test
remis en local et le port refermé.

## G. Fin

23. Laisse Régie5 ouvert quelques minutes en naviguant → pas de fermeture.
24. Ferme Régie5 → l'accueil de la PS5 est normal.
