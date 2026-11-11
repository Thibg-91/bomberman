# Upgrades (`lib/upgrade`)

Les caisses détruites peuvent cacher une upgrade. Un personnage la ramasse en marchant dessus, et l'une de ses capacités augmente jusqu'à sa mort.

| Upgrade | Effet | Plafond | Dessin |
|---|---|---|---|
| Bombe supplémentaire (`ExtraBomb`) | +1 bombe posée en même temps | 8 | petite bombe allumée, badge doré « + » |
| Force du personnage (`PushStrength`) | +1 case de poussée | 5 | bras contracté, ancre tatouée, brassard rouge |
| Force de la bombe (`BlastRadius`) | +1 case de rayon d'explosion | 8 | éclat d'explosion façon BD |

Les trois flottent au-dessus de leur ombre, montent et descendent doucement (période de 1,6 s), avec une petite étoile qui scintille. Le design a été validé sur la maquette « Butin des caisses » ; `UpgradeRenderer` en reprend les coordonnées exactes.

## Règles

- **Apparition** : chaque caisse détruite a 25 % de chance (`UpgradeRules::dropChancePercent`) de cacher une upgrade, dont le type est tiré avec des poids égaux (`weights`, 1 chance sur 3 chacun).
- **Ramassage** : dès que le personnage entre sur la case (la position logique change au début du pas). Au plafond, l'upgrade est consommée sans effet.
- **Poussée** : une bombe poussée glisse jusqu'à `pushDistance` cases et s'arrête devant le premier obstacle (mur, caisse, autre bombe, autre joueur). Elle échoue seulement si elle ne peut pas bouger du tout. À 1 case, c'est le comportement d'avant.
- **Rayon** : une bombe garde le rayon qu'avait son poseur au moment de la pose.
- **Explosions** : une upgrade touchée par une explosion est détruite, sauf celle que cette explosion vient de révéler.
- **Obstacle** : une upgrade ne bloque ni les déplacements, ni les bombes, ni le souffle.

## Hasard déterministe

Tous les tirages passent par `Random` (SplitMix64), initialisé avec `GameRules::seed`. Contrairement aux distributions de `<random>`, dont le résultat dépend de la bibliothèque standard, sa sortie est la même sur tous les compilateurs : la même graine et les mêmes commandes donnent la même partie, ce que le futur mode réseau (lockstep) et d'éventuels replays exigent. `UpgradeManager::spawnInDestroyedBlocks` tire une fois par caisse détruite, même quand l'upgrade ne peut pas être posée, pour que la suite des tirages ne dépende que des caisses.

## Composants

- `UpgradeType`, `Upgrade` : le type et une upgrade posée sur la grille (entité, pas une tuile).
- `UpgradeRules` : probabilité, poids des types, plafonds. Elles font partie de `GameRules::upgrades`.
- `PlayerStats` : `maxBombs`, `pushDistance`, `blastRadius` d'un personnage ; `apply(type, rules)` applique une upgrade. Valeurs de départ : `GameRules::startingStats`.
- `UpgradeManager` : les upgrades posées ; `add`, `takeAt`, `destroyAt`, `spawnInDestroyedBlocks`, `roll`.
- `GameEngine` orchestre le tout (voir [engine.md](engine.md)) : `getUpgrades()`, `addUpgrade()`, `getStats(id)` et les événements `UpgradeSpawned`, `UpgradeCollected`, `UpgradeDestroyed`.
- `UpgradeRenderer` (`lib/render`) : dessine toutes les upgrades dans un seul tableau de sommets.

## Tests

- `tests/upgrade` : sortie figée de `Random`, probabilités mesurées sur 60 000 tirages (25 % puis un tiers par type), plafonds, ajout, ramassage et destruction.
- `tests/bomb` : rayon par bombe, glissade et ses arrêts, caisses détruites signalées.
- `tests/engine` : apparition, ramassage au premier tick du pas, effet de chaque upgrade, poussée arrêtée par un joueur, destruction par une explosion, joueurs morts, déterminisme selon la graine.
- `tests/render` : placement conforme à la maquette, reste dans sa case pendant l'animation, couleurs propres à chaque type, ombre fixe, boucle d'animation.
