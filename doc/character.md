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
| `Entrée`      | Casse le bloc destructible situé devant le personnage         |

Les touches sont configurables via `KeyboardController::Bindings`.

## Fonctionnement

- Le personnage se déplace case par case. La position logique change dès qu'un
  déplacement est accepté ; le personnage reste ensuite « occupé » pendant
  `moveDuration` secondes (`Character::update(dt)`) et refuse tout nouvel ordre.
  Le renderer interpole entre `getPreviousPosition()` et `getPosition()` grâce à
  `getMoveProgress()` (glissement + petit saut).
- Les collisions passent par `Grid::isWalkable` : murs, blocs et hors-grille bloquent.
  Un déplacement bloqué fait quand même pivoter le personnage.
- `breakBlock` détruit la case devant lui (`Tile::destroy`) et renvoie `true` si la
  grille a changé : l'appelant doit alors appeler `GridRenderer::update(grid)`
  (back et front restent synchronisés).

## Préparé pour la suite (IA, en ligne)

`Character` ne connaît ni le clavier ni SFML : il est piloté uniquement par des
`CharacterCommand` (`move(direction)` / `breakBlock()`) via `Character::execute`.
Une IA ou un message réseau n'ont qu'à produire ces commandes, comme le fait
`KeyboardController`. Le temps est injecté (`update(dt)`), ce qui rend la logique
déterministe et testable.

## Tests

- `tests/character` : déplacements, collisions, destruction de blocs, durée de mouvement, niveau 1 ;
- `tests/render` : interpolation du `CharacterRenderer`, mapping des touches.
