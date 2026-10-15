# Régie5 — Recette de la 0.5.0 sur PS5 (10 minutes)

Avant la publication : le zip de la release (`build/ps5/release-0.5.0/upload/PPSA99520.zip`),
installé par-dessus la 0.4.0, sur la console, avec la manette et un téléphone sur le même
réseau. Pas de journal dans une release : tout se juge à l'écran. Seule écriture : le
`config.json` de la console, réenregistré tel quel depuis la page du téléphone.

Nouveau en 0.5.0 : **Demandes › Découvrir** (demandes Seerr), **Quick Connect** pour Jellyfin,
**espagnol et allemand**, écran **Nouveautés**. Déjà vus sur la console avec le build de test
`app-18` (essai du 16 octobre, parties A à F) : Découvrir jusqu'à la confirmation, Quick Connect
sur le Jellyfin 12, session révoquée, espagnol et allemand. Cette recette ne revoit donc que la
mise à jour, avec ta config.

**Attention** : cette release envoie vraiment les demandes Seerr (au nom du compte de la clé
d'API, donc approuvées tout de suite). Dans cette recette, tu n'ouvres jamais la feuille de
demande.

Une étape = une action. Après « → » : ce que tu dois voir. Si ce n'est pas ce que tu vois,
**ne publie rien** et note le numéro de l'étape.

## A. Mise à jour depuis la 0.4.0

1. Vérifie dans Réglages que la console a la 0.4.0 → « Version 0.4.0 ». Ferme Régie5.
2. Décompresse `PPSA99520.zip` sur le PC.
3. Copie le dossier `PPSA99520` par-dessus `/data/homebrew/PPSA99520/`.
4. Attends 30 secondes.
5. Regarde l'accueil de la PS5 → la tuile Régie5 est dans **Jeux**.
6. Lance Régie5 → **pas d'assistant** ; l'écran « Nouveautés de Régie5 0.5 » avec 4 lignes
   (Seerr, Quick Connect, Español et Deutsch, Réglages › Nouveautés).
7. ✕ sur OK → l'écran se ferme ; l'Accueil, avec tes données.

## B. Réglages

8. Appuie sur R1 jusqu'à l'engrenage → tous les services au vert.
9. Lis le bas de l'écran → « Version 0.5.0 » (pas « 0.5.0-test »).
10. Lis la ligne « Affichage » → « 3840 × 2160, 120 Hz » (ce que la 0.4.0 donnait).
11. Regarde la rangée de boutons → six boutons, le dernier est « Nouveautés », tous entiers.

## C. Depuis le téléphone (à chaque version)

12. Va sur « Depuis le téléphone », ✕ → un QR code et un code à 6 chiffres.
13. Scanne le QR code → la page s'ouvre, en français, avec tes services.
14. Sans rien changer, touche « Enregistrer sur la console » → « Enregistré » ; sur la télé,
    les services restent au vert.
15. Appuie sur ○ → la fenêtre se ferme.

## D. Découvrir (lecture seule)

16. R1 jusqu'à Demandes, → jusqu'à « Découvrir », ✕ → le champ de recherche, puis les rangées
    Tendances, Films populaires… avec les affiches de ton Seerr.
17. ↓ ↓ jusqu'à une rangée, ✕ sur une jaquette → sa fiche.
18. ○ → la fiche se ferme. **Ne choisis pas « Demander… ».**

## E. Fin

19. Ferme Régie5 et relance-le → **pas** d'écran Nouveautés cette fois.
20. Navigue quelques minutes entre les onglets → pas de fermeture.
21. Ferme Régie5 → l'accueil de la PS5 est normal.

Tout est conforme : la 0.5.0 peut être publiée (`docs/RELEASING.md` §3, étapes 3 à 6).

Si l'écran Nouveautés ne s'ouvre pas à l'étape 6 (données de l'app effacées par un
redémarrage : Régie5 se croit alors fraîchement installé), ce n'est pas bloquant : ouvre-le par
Réglages › Nouveautés et note-le.
