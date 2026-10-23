# Statistiques de visionnage (optionnel)

Régie5 montre les statistiques de visionnage de **Jellyfin** quand le plugin officiel
**Playback Reporting** y est installé : **Jellyfin › Statistiques** (la pilule n'apparaît
qu'avec lui), sur **7 jours, 30 jours ou 12 mois** :

- le temps de visionnage en tout et le nombre de lectures ;
- une barre par jour (par mois sur 12 mois) ;
- le temps par utilisateur ;
- les films et les séries les plus vus (leur jaquette vient de Radarr / Sonarr quand le titre
  y porte le même nom ; sinon, une jaquette générée).

Rien à saisir dans Régie5 : il utilise la clé d'API (administrateur) de Jellyfin déjà dans
`config.json`. Réglages dit, sur la ligne de Jellyfin, si le plugin est là
(« Statistiques : Playback Reporting 19 »). Avec un compte non administrateur (Quick Connect),
les statistiques ne sont pas disponibles.

## Installer le plugin

1. Jellyfin › Tableau de bord › **Extensions** › Catalogue : **Playback Reporting** (dépôt
   officiel de Jellyfin), Installer.
2. Redémarrer Jellyfin.
3. Pour un récapitulatif sur 12 mois : dans les réglages du plugin, « Keep data for »
   **Forever** (il garde 3 mois par défaut).

Le plugin note les lectures **à partir de son installation** : les premiers jours,
Statistiques dit « Aucun visionnage sur cette période ». Un utilisateur que le plugin ignore
(sa liste d'utilisateurs ignorés) n'apparaît pas.

## Ce que Régie5 lit, et ce qu'il ne lit pas

Seulement ces `GET` du plugin : `user_usage_stats/user_activity`, `PlayActivity`,
`MoviesReport`, `GetTvShowsReport`, et `/Plugins` (savoir s'il est là). Jamais ses appels
qui écrivent (même en `GET` : `user_manage`, `load_backup`, `save_backup`), ni sa requête SQL
libre, ni les répartitions par appareil ou par client : seuls les noms d'utilisateurs, les
titres et les durées sont montrés.

Pour cacher la pilule sans désinstaller le plugin : `"statistics": false` dans l'entrée
`jellyfin` de `config.json`.

**Emby, Plex** : pas de statistiques pour l'instant (Emby a un plugin équivalent, pas encore
essayé ; Plex : Tautulli, à étudier).
