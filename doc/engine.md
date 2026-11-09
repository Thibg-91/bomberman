# Moteur de jeu (`lib/engine`)

Le moteur fait avancer une partie à **cadence fixe**, indépendamment du framerate, et impose un **léger délai après chaque action** d'un personnage. Il est purement logique (aucune dépendance SFML) : tout est testable sans fenêtre (`tests/engine`).

## Fréquence : 60 ticks par seconde

- Elle correspond au framerate de la fenêtre (plafonné à 60 fps) : en général un tick par frame, donc un mouvement fluide sans interpolation supplémentaire.
- La latence d'entrée reste sous 17 ms et un tick (16,7 ms) est un grain assez fin pour régler les délais.
- La logique est légère, 60 Hz reste peu coûteux même à 4 personnages ; un futur mode réseau pourra échanger les entrées tous les N ticks sans changer la fréquence de simulation.

Tous les délais sont exprimés en **nombre entier de ticks** (`GameRules`) : la partie se déroule exactement de la même manière à 30, 60 ou 144 fps (base nécessaire pour des replays ou du réseau en lockstep).

## Délais par défaut (`GameRules`)

| Action | Ticks | Durée |
|---|---|---|
| Déplacement d'une case (`moveTicks`) | 10 | ~0,17 s |
| Demi-tour / changement d'orientation (`turnTicks`) | 5 | ~0,08 s |
| Pose de bombe (`placeBombTicks`) | 15 | 0,25 s |
| Poussée de bombe (`pushBombTicks`) | 15 | 0,25 s |

Une action qui échoue (mur, pas de bombe à pousser, limite de bombes atteinte) ne coûte aucun délai. `GameRules` porte aussi `maxActiveBombs` et `pushDistance`, prévus pour devenir des bonus par personnage.

## Composants

- `FixedTimestep` : patron « pas fixe + accumulateur ». Convertit le temps réel d'une frame en nombre de ticks à jouer, garde le reliquat pour la frame suivante, et après un gel (fenêtre déplacée, debugger) ne joue pas plus de 5 ticks d'un coup (le retard est abandonné plutôt que d'accélérer le jeu).
- `GameEngine` : possède la `Grid`, le `BombManager` et tous les personnages.
  - `addPlayer()` place un joueur sur le prochain point de spawn libre et renvoie son `PlayerId` (0, 1, 2…, aussi utilisé comme propriétaire de ses bombes).
  - `submitCommand(id, command)` : **seul** point d'entrée pour piloter un personnage. Le clavier aujourd'hui, une IA ou un pair réseau demain appellent la même méthode avec leur propre id.
  - `advance(realSeconds)` joue les ticks couverts par le temps réel ; `tick()` joue exactement un tick (tests).
  - `takeEvents()` renvoie les `GameEvent` survenus (`PlayerTurned`, `PlayerMoved`, `BombPlaced`, `BombPushed`, `Explosion`, `PlayerKilled`, horodatés par tick) : le rendu (animation de pose, reconstruction de la grille après une explosion) réagit à ces événements sans scruter l'état.

## Déroulement d'un tick

Ordre déterministe, joueurs par id croissant :

1. chaque joueur vivant, **prêt** (pas en déplacement, délai écoulé) et ayant une commande en attente l'exécute ;
2. les personnages avancent dans leur déplacement, les délais décomptent ;
3. les mèches brûlent ; une explosion détruit les blocs et tue les joueurs touchés (événement `Explosion` puis `PlayerKilled`).

## Commandes : une intention « maintenant »

Une commande reçue pendant qu'un joueur est occupé est **abandonnée**, jamais mise en file : maintenir une touche la renvoie à chaque frame, et relâcher la touche pendant un déplacement ne provoque pas de pas supplémentaire. Une commande soumise avant `advance()` reste valable pour tous les ticks de cette frame (comme une touche maintenue toute la frame), ce qui évite qu'un framerate inférieur à 60 fps fasse rater le tick où le joueur redevient prêt.

## Intégration (`exe/main.cpp`)

Chaque frame : lecture du clavier → `submitCommand` → `advance(dt)` → traitement des événements → mise à jour des renderers à partir de l'état du moteur. La règle « un demi-tour consomme l'appui » reste côté clavier (`turnKeyHeld`, alimenté par l'événement `PlayerTurned`), car c'est une question d'ergonomie de l'entrée, pas une règle du jeu.

## Évolutions prévues

- Multijoueur local / IA : `addPlayer()` pour chaque participant, une source de commandes par `PlayerId`.
- Réseau : les commandes horodatées par tick et la simulation déterministe permettent un lockstep ; les événements peuvent servir à la synchronisation de l'état.
- Bonus : rendre `GameRules` (délais, nombre de bombes, portée) modifiable par joueur.
