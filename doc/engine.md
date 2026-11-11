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
| Se tourner vers une case bloquée (`turnTicks`) | 5 | ~0,08 s |
| Pose de bombe (`placeBombTicks`) | 15 | 0,25 s |
| Poussée de bombe (`pushBombTicks`) | 15 | 0,25 s |

Une action qui échoue (mur, pas de bombe à pousser, limite de bombes atteinte) ne coûte aucun délai. `GameRules` porte aussi les stats de départ des personnages (`startingStats` : 1 bombe, poussée d'1 case, rayon d'1 case), les règles des upgrades (`upgrades`) et la graine du hasard (`seed`) : voir [upgrade.md](upgrade.md).

## Composants

- `FixedTimestep` : patron « pas fixe + accumulateur ». Convertit le temps réel d'une frame en nombre de ticks à jouer, garde le reliquat pour la frame suivante, et après un gel (fenêtre déplacée, debugger) ne joue pas plus de 5 ticks d'un coup (le retard est abandonné plutôt que d'accélérer le jeu).
- `GameEngine` : possède la `Grid`, le `BombManager` et tous les personnages.
  - `addPlayer()` place un joueur sur le prochain point de spawn libre et renvoie son `PlayerId` (0, 1, 2…, aussi utilisé comme propriétaire de ses bombes).
  - `submitCommand(id, command)` : point d'entrée du clavier (et demain d'un pair réseau), une commande par frame.
  - `setController(id, controller)` : confie un joueur à un `PlayerController` (les bots, voir [bot.md](bot.md)). Le moteur lui demande sa commande **à chaque tick où le joueur est prêt**, ce qui garde la partie indépendante du framerate ; le joueur ignore alors `submitCommand`. Dans les deux cas la commande passe par la même exécution (`execute`) : un bot n'a aucun pouvoir de plus qu'un joueur au clavier.
  - `advance(realSeconds)` joue les ticks couverts par le temps réel ; `tick()` joue exactement un tick (tests).
  - `setCommandRecording(true)` / `takeExecutedCommands()` : enregistre chaque commande remise à un joueur prêt (tick, joueur, commande) ; les rejouer tick par tick sur un autre moteur identique reproduit la partie (multijoueur, voir [network.md](network.md)). `stateHash()` donne une empreinte de tout l'état pour vérifier que deux moteurs sont restés identiques.
  - `takeEvents()` renvoie les `GameEvent` survenus (`PlayerTurned`, `PlayerMoved`, `BombPlaced`, `BombPushed`, `Explosion`, `PlayerKilled`, `UpgradeSpawned`, `UpgradeCollected`, `UpgradeDestroyed`, horodatés par tick) : le rendu (animation de pose, reconstruction de la grille après une explosion) réagit à ces événements sans scruter l'état.

## Déroulement d'un tick

Ordre déterministe, joueurs par id croissant :

1. chaque joueur vivant et **prêt** (pas en déplacement, délai écoulé) exécute sa commande : celle que renvoie son contrôleur s'il en a un (qui voit déjà les actions des joueurs d'id inférieur), sinon sa commande en attente ;
2. chaque joueur vivant ramasse l'upgrade de sa case (dès le début d'un pas) ;
3. les personnages avancent dans leur déplacement, les délais décomptent ;
4. les mèches brûlent ; une explosion détruit les blocs et les upgrades touchées, les blocs détruits peuvent révéler une upgrade (épargnée par cette même explosion), puis les joueurs touchés meurent (événements `Explosion`, `UpgradeDestroyed`, `UpgradeSpawned`, `PlayerKilled`).

## Commandes : une intention « maintenant »

Une commande reçue pendant qu'un joueur est occupé est **abandonnée**, jamais mise en file : maintenir une touche la renvoie à chaque frame, et relâcher la touche pendant un déplacement ne provoque pas de pas supplémentaire. Une commande soumise avant `advance()` reste valable pour tous les ticks de cette frame (comme une touche maintenue toute la frame), ce qui évite qu'un framerate inférieur à 60 fps fasse rater le tick où le joueur redevient prêt.

## Intégration (`exe/main.cpp`)

Chaque frame : lecture du clavier → `submitCommand` → `advance(dt)` (les bots décident à l'intérieur, tick par tick) → traitement des événements → mise à jour des renderers à partir de l'état du moteur. Les déplacements sont immédiats (pas de demi-tour préalable) : maintenir une touche fait avancer en continu. Une bombe est posée sur la case du joueur, qui peut en sortir mais pas y revenir.

## Évolutions prévues

- Fin de partie : `MatchReferee` (même bibliothèque) dit quand la partie est finie, qui a gagné, et accélère la partie quand le joueur est mort ; voir [end_of_match.md](end_of_match.md).
- IA : faite, voir [bot.md](bot.md) (un `Bot` par adversaire via `setController`).
- Multijoueur local (même clavier) : `addPlayer()` pour chaque participant, une source de commandes par `PlayerId`.
- Réseau : fait, en lockstep piloté par l'hôte (commandes enregistrées puis rejouées par les clients, empreinte de l'état) : voir [network.md](network.md). Le projet est compilé avec `-ffp-contract=off` pour que les flottants de la simulation soient identiques sur toutes les machines.
- Bonus : fait, voir [upgrade.md](upgrade.md). Les délais d'action pourraient devenir eux aussi des stats par joueur (bonus de vitesse).
