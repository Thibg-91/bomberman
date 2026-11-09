# Personnage

Ce document décrit le personnage jouable : logique, contrôles et affichage.

## Architecture

| Cible                  | Dossier         | Dépendances | Rôle                                                                 |
|------------------------|-----------------|-------------|----------------------------------------------------------------------|
| `Bomberman::character` | `lib/character` | `grid`      | `Character`, `CharacterCommand`, `Direction` (aucune dépendance SFML) |
| `Bomberman::render`    | `lib/render`    | + `character` | `CharacterRenderer` (dessin animé), `KeyboardController` (entrées)  |

## Contrôles

| Touche        | Action                                                        |
|---------------|---------------------------------------------------------------|
| `Z Q S D`     | Déplacement haut / gauche / bas / droite (touche maintenue)   |
| `Espace`      | Pose une bombe sur la case devant le personnage               |
| `Entrée`      | Pousse la bombe située devant le personnage                   |

Les touches sont configurables via `KeyboardController::Bindings`.

## Fonctionnement

- Le personnage se déplace case par case. La position logique change dès qu'un
  déplacement est accepté ; le personnage reste ensuite « occupé » pendant
  `moveDuration` secondes (`Character::update(dt)`) et refuse tout nouvel ordre.
  Le renderer interpole entre `getPreviousPosition()` et `getPosition()` grâce à
  `getMoveProgress()` (glissement + petit saut).
- Les collisions passent par `Grid::isWalkable` : murs, blocs et hors-grille bloquent.
  Les bombes bloquent aussi : `Character` ne les connaît pas, l'appelant passe leurs cases
  en `obstacles` à `move()` / `execute()` (`BombManager::getBombPositions()` dans `exe/main.cpp`).
  Seule la case visée compte : on peut toujours quitter une case qui contient une bombe.
- Orientation d'abord : une commande `move(direction)` vers une direction que le personnage
  ne regarde pas encore le fait seulement pivoter (instantané, `move()` renvoie `false`) ;
  une fois orienté, les commandes suivantes dans cette direction le font avancer. On peut
  donc se tourner vers une case bloquée.
- Clavier (`exe/main.cpp`) : un pivot « consomme » l'appui ; tant que la touche reste
  enfoncée, le personnage ne bouge pas. Il faut la relâcher et la ré-appuyer pour avancer
  (la garder enfoncée ensuite le fait avancer en continu). `KeyboardController::isHeld()`
  sert à détecter le relâchement.
- Le personnage ne casse jamais de bloc lui-même : seules les explosions de bombes
  détruisent les blocs destructibles. `Character::execute` reçoit la grille en lecture
  seule. Les commandes de bombe (`placeBomb()`, `pushBomb()`) renvoient `false` : elles
  sont traitées par l'appelant, qui possède le `BombManager` (voir `exe/main.cpp`).
- Une seule bombe active par personnage : chaque bombe connaît son propriétaire
  (`Bomb::getOwnerId()`), et `BombManager::placeBomb(position, grid, ownerId, maxActiveBombs)`
  échoue tant que ce propriétaire a déjà `maxActiveBombs` bombes non explosées
  (`BombManager::DefaultMaxActiveBombs` = 1). Le joueur peut en reposer une dès que la
  précédente a explosé. La limite est un paramètre pour pouvoir l'augmenter plus tard
  (bonus).

## Préparé pour la suite (IA, en ligne)

`Character` ne connaît ni le clavier ni SFML : il est piloté uniquement par des
`CharacterCommand` (`move(direction)` / `pushBomb()` / `placeBomb()`) via `Character::execute`.
Une IA ou un message réseau n'ont qu'à produire ces commandes, comme le fait
`KeyboardController`. Le temps est injecté (`update(dt)`), ce qui rend la logique
déterministe et testable.

## Tests

- `tests/character` : déplacements, collisions, aucune commande ne casse de bloc, durée de mouvement, niveau 1 ;
- `tests/render` : interpolation du `CharacterRenderer`, mapping des touches.
