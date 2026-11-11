# Personnage

Ce document décrit le personnage jouable : logique, contrôles et affichage.

## Architecture

| Cible                  | Dossier         | Dépendances | Rôle                                                                 |
|------------------------|-----------------|-------------|----------------------------------------------------------------------|
| `Bomberman::character` | `lib/character` | `grid`      | `Character`, `CharacterCommand`, `Direction` (aucune dépendance SFML) |
| `Bomberman::render`    | `lib/render`    | + `character` | `CharacterRenderer` (dessin animé), `KeyboardController` (entrées)  |

## Choix du pirate

Le pirate se choisit dans la rangée « pirate » du menu de lancement (voir [menu.md](menu.md)) : les six membres de l'équipage sont alignés sur le pont, le choisi s'avance, grandit et saute dans un cercle doré, les autres restent en retrait, assombris. Il n'y a pas de texte : le projet n'embarque aucune police. (L'ancien écran « Choix du pirate », `CharacterSelectRenderer`, n'est plus affiché : voir `TODO.md`.)

| Pirate | `CharacterModel` | Signes distinctifs |
|---|---|---|
| Le Boucanier | `Buccaneer` | bandana rouge à pois noué, un œil, cache-œil, cicatrice (le modèle d'origine) |
| Le Capitaine | `Captain` | tricorne noir galonné d'or avec une tête de mort, moustache en guidon |
| La Vigie | `Lookout` | bonnet rayé bleu et blanc à pompon rouge, taches de rousseur |
| Le Perroquet | `Parrot` | plumes vertes, ailes, bec jaune crochu, crête rouge, pattes orange |
| Le Revenant | `Revenant` | squelette aux orbites lumineuses, bandana violet sombre |
| Le Chien de bord | `Spaniel` | cocker chocolat et feu (maquette validée « Le chien du bord », proposition A) : longues oreilles ondulées plus foncées que la robe, mèche sur le crâne, museau, sourcils et pattes feu, langue qui dépasse, foulard rouge à pois noué dans le dos, queue qui remue (même à l'arrêt) |

Tous partagent le même gabarit (corps rond, bras, jambes, mêmes coordonnées), donc la marche, le saut et la pose de bombe fonctionnent pour chacun ; seuls la tête, le visage et les couleurs changent. Les oreilles du chien pendent hors de la tête (décalées de 5 unités vers l'extérieur de face et de dos) pour ne pas couvrir ses yeux ; de profil, l'oreille visible pend derrière l'œil. Le choix est purement visuel : tous les pirates jouent de la même façon.

| Touche (sélection) | Action |
|---|---|
| `Q` / `D` ou `←` / `→` | Pirate précédent / suivant |
| `Entrée` ou `Espace` | Lance la partie |
| `Échap` | Quitte le jeu |

## Contrôles

| Touche        | Action                                                        |
|---------------|---------------------------------------------------------------|
| `Z Q S D`     | Déplacement haut / gauche / bas / droite, immédiat (touche maintenue : avance en continu) |
| `Espace`      | Pose une bombe sur la case du personnage                      |
| `Entrée`      | Pousse la bombe située devant le personnage                   |
| `Tab`         | Revient au menu de lancement                                  |
| `1` à `4`     | Recommence sur une autre map, avec le même pirate             |

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
- Déplacement immédiat : une commande `move(direction)` oriente le personnage dans cette
  direction et le fait avancer aussitôt si la case est libre (`move()` renvoie `true`). Vers
  une case bloquée (mur, caisse, bombe), il pivote seulement (`false`) : c'est ainsi qu'on
  se tourne vers une bombe pour la pousser avec `Entrée`.
- Pose de bombe : la bombe tombe **sur la case du personnage**. Il peut en sortir (seule la
  case visée compte) mais pas y revenir tant qu'elle n'a pas explosé.
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
