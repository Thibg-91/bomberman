# Roadmap — Découpage des tâches

Ce document découpe la réalisation du projet en tâches ordonnées, en s'appuyant sur [architecture.md](architecture.md). Les phases sont séquentielles ; à l'intérieur d'une phase, les tâches sont listées dans l'ordre recommandé mais certaines peuvent se paralléliser si plusieurs personnes travaillent dessus.

Chaque tâche a un statut `[ ]` à cocher une fois terminée.

---

## Phase 0 — Fondations du build

Objectif : un exécutable qui compile et se lance, prêt à accueillir le code métier.

- [ ] Ajouter `entt` aux dépendances Conan (`conanfile.py`)
- [ ] Vérifier/nettoyer `doc/include_dependencies.cmake` et l'intégrer proprement dans `CMakeLists.txt` racine (actuellement non référencé)
- [ ] Mettre en place un `CMakeLists.txt` par sous-module de `lib/` (structure décrite en §3 de `architecture.md`)
- [ ] Définir la variable d'environnement/mécanisme `BOMBERMAN_ROOT` de façon robuste (actuellement non vérifié si absent dans `main.cpp`)
- [ ] Vérifier que le build complet (Conan + CMake) fonctionne de bout en bout sur un poste propre

---

## Phase 1 — Grille et niveaux (base statique)

Objectif : charger et afficher un niveau statique, sans logique de jeu.

- [ ] Corriger `lib/grid/grid.h` (constructeur dupliqué, `#include "Grid.hpp"` incorrect dans `grid.cpp`, namespace incohérent `mynamespace` vs `Bomberman`)
- [ ] Implémenter `Tile` avec position et accès au type
- [ ] Implémenter `Grid::loadFromJson` (parsing via `nlohmann_json`, format décrit en §7.2 de `architecture.md`)
- [ ] Formaliser et documenter le format JSON de niveau (légende des symboles, spawns joueurs/ennemis)
- [ ] Écrire un test unitaire de chargement de `data/test_grid.json`
- [ ] Créer `lib/render/` : `RenderSystem` minimal capable d'afficher une grille statique (sprites de tuiles)
- [ ] Créer `lib/resources/ResourceManager` pour charger textures depuis `${BOMBERMAN_ROOT}/data/textures`
- [ ] Afficher le niveau `test_grid.json` dans une fenêtre SFML depuis `main.cpp` (validation visuelle manuelle)

---

## Phase 2 — Boucle de jeu et états

Objectif : une architecture d'application capable de gérer plusieurs écrans.

- [ ] Implémenter `lib/core/application.h/.cpp` avec boucle à pas de temps fixe (accumulator pattern, §5.1)
- [ ] Implémenter `GameState` (interface abstraite) et `StateStack` (§5.2)
- [ ] Implémenter `MenuState` minimal (titre + "Start Game" au clavier)
- [ ] Implémenter `PlayState` qui charge un niveau via le code de la Phase 1 et l'affiche
- [ ] Brancher `Application`/`StateStack` dans `exe/main.cpp` à la place du code actuel
- [ ] Validation manuelle : lancement → menu → transition vers l'affichage du niveau

---

## Phase 3 — ECS et déplacement joueur

Objectif : un joueur contrôlable se déplaçant sur la grille.

- [ ] Créer `lib/ecs/components/` : `Position`, `GridPosition`, `Velocity`, `Sprite`, `PlayerControlled`, `Solid`
- [ ] Créer `lib/input/` : mapping clavier → actions logiques (`InputAction::MoveUp/Down/Left/Right/PlaceBomb`) pour le joueur 1
- [ ] Implémenter `InputSystem` (lecture clavier → composant `Velocity`/intention)
- [ ] Implémenter `MovementSystem` (application de la vélocité sur la grille, pas de collision pour l'instant)
- [ ] Implémenter `CollisionSystem` basique (bloque le déplacement contre les tuiles `Solid`)
- [ ] Créer `lib/ecs/factories/player_factory` (instancie une entité joueur avec sprite + position de spawn issue du niveau)
- [ ] Intégrer les systèmes dans la boucle `PlayState::update()` (ordre défini en §4.2)
- [ ] Mettre à jour `RenderSystem` pour dessiner les entités ECS (et non plus seulement la grille statique)
- [ ] Validation manuelle : un joueur se déplace sur la grille et est bloqué par les murs

---

## Phase 4 — Bombes et explosions

Objectif : le cœur du gameplay Bomberman.

- [ ] Composants : `Bomb`, `Explosion`, `Destructible`, `Health`
- [ ] `BombSystem` : pose de bombe (touche dédiée), décompte de la mèche, déclenchement
- [ ] `ExplosionSystem` : propagation en croix selon la portée, destruction des tuiles `Destructible`, dégâts aux entités touchées
- [ ] `LifecycleSystem` : suppression des entités expirées (bombes explosées, explosions terminées, joueurs morts)
- [ ] Gestion de la mort du joueur / fin de partie (état `GameOverState`)
- [ ] Effets visuels/sonores de l'explosion (`lib/audio/`, sprites d'animation)
- [ ] Validation manuelle : poser une bombe, elle explose, détruit un mur destructible, tue un joueur en zone d'effet

---

## Phase 5 — Bonus et règles de jeu

- [ ] Composants et logique de bonus (augmentation de portée, nombre de bombes, vitesse, etc.)
- [ ] Apparition de bonus aléatoire sous les tuiles destructibles détruites
- [ ] Ramassage de bonus par collision joueur/bonus
- [ ] Règles de fin de partie / victoire (dernier joueur en vie)

---

## Phase 6 — Multijoueur local

- [ ] Étendre `lib/input/` pour plusieurs joueurs (mapping clavier distinct par joueur, cf. point ouvert §8.2 de `architecture.md`)
- [ ] Généraliser `player_factory` pour instancier N joueurs à des spawns distincts du niveau
- [ ] Écran de sélection du nombre de joueurs (`MenuState` étendu ou nouvel état)
- [ ] Validation manuelle : 2 à 4 joueurs jouables simultanément au clavier sur la même fenêtre

---

## Phase 7 — IA des ennemis

- [ ] Composant `AIController` + types de comportement (statique, aléatoire, agressif)
- [ ] Implémenter un pathfinding A* simple sur la grille (`lib/ai/`)
- [ ] `AISystem` : décision de déplacement/pose de bombe selon le comportement
- [ ] `enemy_factory` : instanciation des ennemis à partir des spawns du niveau
- [ ] Équilibrage/tests de comportement (l'ennemi évite les explosions, poursuit le joueur selon le profil choisi)
- [ ] Validation manuelle : un ennemi se déplace, évite les bombes, peut tuer/être tué

---

## Phase 8 — Éditeur de niveau

- [ ] `EditorState` : affichage de la grille en mode édition (réutilise `RenderSystem`)
- [ ] Interactions souris/clavier : placer/effacer des tuiles, placer des spawns joueurs/ennemis
- [ ] `lib/level/level_writer` : sérialisation de la grille éditée vers le format JSON (§7.2)
- [ ] Chargement d'un niveau existant en mode édition
- [ ] Sélecteur de niveau dans `MenuState` (choisir un fichier `.json` dans `data/grid/`)
- [ ] Validation manuelle : créer un niveau depuis zéro, le sauvegarder, le recharger et y jouer

---

## Phase 9 — Polish et robustesse

- [ ] Gestion des erreurs de chargement (fichier de niveau invalide/manquant)
- [ ] Menu pause complet (`PauseState` transparent au-dessus de `PlayState`, §5.2)
- [ ] Musiques et sons d'ambiance (`lib/audio/`)
- [ ] Écran de fin de partie / retour au menu
- [ ] Nettoyage des TODO existants dans le code (`grid.h`, `tile.h`)
- [ ] Revue de performance (profilage si baisse de FPS sur grandes grilles / nombreux ennemis)

---

## Hors scope actuel (backlog, non planifié)

- [ ] Multijoueur réseau (`lib/network/`) — non demandé, à ne pas développer avant décision explicite
- [ ] Manettes/gamepad
- [ ] Remapping des touches par le joueur en jeu
