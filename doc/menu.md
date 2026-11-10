# Menu de lancement (« Préparer l'abordage »)

Le jeu s'ouvre sur ce menu (maquette validée « Préparer l'abordage », mise en page « Le tableau de bord »). Il n'a pas de texte, le projet n'embarquant aucune police : chaque réglage se lit par l'image.

## Les rangées

De haut en bas, une rangée par réglage, chacune précédée d'un badge doré qui dit ce qu'elle règle :

| Rangée | Badge | Valeurs |
|---|---|---|
| Mode | sabres croisés | solo contre l'ordinateur (le boucanier face à un squelette) ; multijoueur local (deux boucaniers et un cadenas : pas encore jouable) |
| Pirate | bandana | les 5 modèles de l'équipage ; le choisi est plus grand, saute dans un cercle doré, les autres sont assombris |
| Carte | rose des vents | une vraie miniature de chaque carte (`GridRenderer` dessiné petit) ; le niveau passé en ligne de commande est proposé en premier |
| Difficulté | tête de mort | 1, 2 ou 3 têtes de mort sur un fanion vert, orange ou rouge (facile, normal, difficile) |
| Adversaires | deux squelettes | 1, 2 ou 3 adversaires, chaque nombre dans sa propre case de bois avec autant de points dessous |

La rangée active a un cadre doré ; le bouton de lancement (cercle doré avec un triangle) pulse, et devient gris quand la partie ne peut pas démarrer (mode multijoueur).

## Commandes

- `Z`/`S` ou `↑`/`↓` : changer de rangée ; `Q`/`D` ou `←`/`→` : changer la valeur (en boucle).
- `Entrée`/`Espace` : lancer la partie. `Échap` : quitter.
- En jeu : `Tab` revient au menu, `1`–`4` relancent sur la carte correspondante avec les mêmes réglages.
- Fin de partie : voir [end_of_match.md](end_of_match.md) (si tu meurs, la partie continue en accéléré entre les adversaires ; écran GAME OVER de 3 s avec le gagnant, puis retour au menu).

## Dans le code

- `MatchSetup` (`lib/render/src/match_setup.h`) : la logique pure (rangée active, valeurs, `canStart()`), testée sans fenêtre ; `opponentModels()` donne l'apparence des bots : le reste de l'équipage, dans l'ordre, sans le modèle du joueur (un boucanier adverse reçoit la couleur de bandana de son `PlayerId`).
- `MatchSetupRenderer` : le dessin, dans un espace de 1600x900 (`viewFor()` l'adapte à l'écran avec des bandes noires), avec les coordonnées exactes de la maquette.
- `exe/main.cpp` : convertit le réglage de difficulté en `BotDifficulty` et crée un `Bot` par adversaire (voir [bot.md](bot.md)).
