# Régie5 — L'app sur PS5 (phase 6)

Régie5 sur la console : titre « Régie5 », Title ID `PPSA99520`. Validé : vrais serveurs
(6.2), heure locale et horaires des serveurs, horloge à la seconde, redémarrage, mise à
jour par le zip, configuration depuis le téléphone (6.5), HTTPS avec certificat auto-signé.

Dossier de ce test : **`build/ps5/app-8/PPSA99520`** (version de développement, journal
UDP). `app-1` à `app-7c` sont d'anciens essais : ne pas les utiliser.

## Ce que ce test vérifie

| Quoi | Ce qui a changé |
|---|---|
| Ouvrir dans Jelly5 | app-7c ne trouvait aucune fonction de lancement : les outils de construction les rangeaient sous le mauvais nom de bibliothèque. app-8 les cherche par leur nom pendant l'exécution, comme les payloads qui lancent des titres. Le journal dit ce qu'il trouve. |
| Si la console ne lance rien | L'entrée devient « Chercher dans Jelly5 » : une note dit quel titre chercher dans Jelly5 (8 s). |
| Logo | Le « R5 » en verre (B) : icône, fond de la tuile sélectionnée, écran de lancement. |
| HTTPS | Envoi immédiat des petits paquets (`TCP_NODELAY`) ; le journal détaille le temps d'une requête `https` (connexion, poignée de main TLS). |

## Ce qu'il faut regarder dans le journal

- `open in Jelly5: module handle …; by name: … found / NOT found` : quelles fonctions la
  console fournit.
- `launch: PPSA99505 by … -> 0x…` après un essai : `0x00000000` = lancé ; autre chose = refusé
  par la console (le code dit pourquoi).

## Si quelque chose ne va pas

- **L'ancienne icône reste sur l'accueil** : la console a copié les visuels quand le titre
  a été enregistré la première fois. Dis-le-moi avant de toucher à quoi que ce soit.
- **« Ouvrir dans Jelly5 » ne fait rien, ou l'app se ferme** : note ce que tu vois ; le
  journal a la ligne `launch:`.
