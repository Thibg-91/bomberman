# Architecture — Bomberman 2D (C++/SFML)

## 1. Contexte et objectifs

Ce document décrit l'architecture logicielle cible pour le projet Bomberman, un jeu 2D en C++ utilisant SFML 3 pour le rendu, sans moteur de jeu externe.

**Périmètre fonctionnel visé :**
- Mode solo avec ennemis pilotés par IA
- Multijoueur local (écran partagé / claviers multiples)
- Éditeur de niveau
- Chargement de niveaux au format JSON (voir `data/test_grid.json`)

**Décisions structurantes retenues :**
| Sujet | Choix |
|---|---|
| Modèle de données/logique | ECS via [EnTT](https://github.com/skypjack/entt) |
| Build system | CMake modulaire, dépendances via Conan (déjà en place) |
| Cycle de vie du jeu | State machine à pile (stack de `GameState`) |
| Sérialisation niveaux | JSON via `nlohmann_json` (déjà en dépendance) |
| Rendu | SFML 3.0.2 |

---

## 2. Vue d'ensemble

```
┌─────────────────────────────────────────────────────────┐
│                         exe/main.cpp                     │
│                    (point d'entrée, Application)         │
└───────────────────────────┬───────────────────────────────┘
                             │
                    ┌────────▼─────────┐
                    │   StateStack      │  (menu, jeu, pause, éditeur)
                    └────────┬─────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
┌───────▼───────┐   ┌────────▼────────┐  ┌─────────▼────────┐
│  MenuState    │   │  PlayState       │  │  EditorState      │
└───────────────┘   └────────┬────────┘  └───────────────────┘
                             │
                    ┌────────▼─────────┐
                    │  entt::registry   │  (monde ECS de la partie)
                    └────────┬─────────┘
                             │
        ┌───────────┬────────┴────────┬───────────────┐
        │           │                 │               │
  InputSystem  MovementSystem   BombSystem      RenderSystem
                    │                 │
              CollisionSystem   ExplosionSystem
                                      │
                                  AISystem
```

Chaque `GameState` possède son propre `entt::registry` (ou en partage un pour l'éditeur ↔ jeu, à trancher en section 8). Les systèmes ECS opèrent uniquement sur des composants ; la logique n'est jamais portée par des méthodes de classes d'entités.

---

## 3. Organisation des dossiers

Alignée sur la structure déjà présente (`lib/`, `exe/`, `data/`) :

```
bomberman/
├── CMakeLists.txt              # racine, add_subdirectory(lib) + add_subdirectory(exe)
├── conanfile.py                # sfml/3.0.2, nlohmann_json/3.11.3, entt à ajouter
├── cmake/                      # modules CMake partagés (find_package, helpers)
├── data/                       # assets et niveaux
│   ├── grid/                   # fichiers JSON de niveaux
│   ├── textures/
│   ├── sounds/
│   └── fonts/
├── exe/
│   └── main.cpp                # point d'entrée, construit l'Application
└── lib/
    ├── core/                   # Application, boucle de jeu, gestion du temps
    │   ├── application.h/.cpp
    │   └── clock.h/.cpp
    ├── states/                 # StateStack + implémentations d'états
    │   ├── state_stack.h/.cpp
    │   ├── game_state.h        # interface abstraite
    │   ├── menu_state.h/.cpp
    │   ├── play_state.h/.cpp
    │   ├── pause_state.h/.cpp
    │   └── editor_state.h/.cpp
    ├── ecs/
    │   ├── components/          # structs de données pures (Position, Velocity, Bomb, ...)
    │   ├── systems/              # fonctions/classes opérant sur le registry
    │   └── factories/            # création d'entités (player_factory, bomb_factory, enemy_factory)
    ├── grid/                    # Grid, Tile (existant, à faire évoluer, cf. §7)
    ├── input/                   # abstraction des entrées (clavier -> actions logiques)
    ├── render/                  # RenderSystem, gestion des textures/sprites, caméra
    ├── audio/                   # gestion des sons/musiques
    ├── resources/                # ResourceManager générique (textures, fonts, sounds)
    ├── ai/                      # comportements des ennemis (pathfinding, FSM d'IA)
    ├── level/                    # chargement/sauvegarde JSON des niveaux
    └── network/                  # (optionnel/futur) couche réseau si multijoueur distant envisagé
```

Chaque sous-dossier de `lib/` est un module CMake indépendant avec son propre `CMakeLists.txt` exposant une bibliothèque statique/objet, liée depuis `exe/`.

---

## 4. Le moteur ECS (EnTT)

### 4.1 Principe

- **Entités** : simples identifiants (`entt::entity`), pas de classe `Player`/`Enemy`/`Bomb`.
- **Composants** : structs de données pures, sans logique. Exemples :
  - `Position { float x, y; }`
  - `Velocity { float dx, dy; }`
  - `Sprite { sf::Sprite sprite; }`
  - `GridPosition { int col, row; }`
  - `PlayerControlled { int playerIndex; }`
  - `Bomb { float fuseTime; int range; entt::entity owner; }`
  - `Explosion { float remainingTime; }`
  - `Destructible {}`, `Solid {}` (tags de collision)
  - `Health { int lives; }`
  - `AIController { AIBehaviorType type; }`
- **Systèmes** : fonctions libres ou classes stateless prenant `entt::registry&` et `dt` en paramètre, exécutées dans un ordre défini chaque frame.

### 4.2 Ordre d'exécution des systèmes (par frame)

1. `InputSystem` — traduit les touches en `Velocity`/intentions sur les entités `PlayerControlled`
2. `AISystem` — calcule les intentions de déplacement/pose de bombe des ennemis
3. `MovementSystem` — applique les vélocités aux positions, résout le déplacement sur la grille
4. `CollisionSystem` — bloque les déplacements invalides (murs, bombes, autres joueurs)
5. `BombSystem` — décrémente les fuses, déclenche les explosions à expiration
6. `ExplosionSystem` — propage les dégâts, détruit les tuiles `Destructible`, applique les dégâts aux `Health`
7. `LifecycleSystem` — supprime les entités mortes/expirées, gère fin de partie
8. `RenderSystem` — synchronise `Position` → `sf::Sprite`, dessine dans l'ordre de profondeur

### 4.3 Pourquoi ECS ici

- Les entités du jeu (joueurs, bombes, explosions, bonus, ennemis) partagent beaucoup de composants transverses (position, collision, rendu) mais des comportements très différents → l'ECS évite une hiérarchie d'héritage fragile.
- Le multijoueur local et le mode IA nécessitent d'itérer efficacement sur des sous-ensembles d'entités (ex: tous les `PlayerControlled`, tous les `AIController`) — EnTT est optimisé pour ça (vues/groupes).
- L'éditeur de niveau peut réutiliser le même registry pour prévisualiser des entités sans dupliquer de logique.

---

## 5. Boucle de jeu et gestion des états

### 5.1 `Application` (`lib/core/application.h`)

Responsable de :
- la fenêtre SFML (`sf::RenderWindow`)
- la boucle principale à pas de temps fixe pour la logique (ex: 60 Hz) et rendu interpolé
- la possession du `StateStack`

```cpp
class Application {
public:
    void run();
private:
    sf::RenderWindow window_;
    StateStack stateStack_;
};
```

Boucle typique (pattern "fixed timestep + accumulator") :
```cpp
while (window.isOpen()) {
    processEvents();
    accumulator += clock.restart();
    while (accumulator >= dt) {
        stateStack.update(dt);
        accumulator -= dt;
    }
    window.clear();
    stateStack.render(window);
    window.display();
}
```

### 5.2 `StateStack` et `GameState`

Pile de `GameState` (interface abstraite) permettant d'empiler des états sans perdre celui du dessous — utile pour la `Pause` (affichée par-dessus `Play` sans le détruire).

```cpp
class GameState {
public:
    virtual ~GameState() = default;
    virtual void handleEvent(const sf::Event&) = 0;
    virtual void update(sf::Time dt) = 0;
    virtual void render(sf::RenderWindow&) = 0;
    virtual bool isTransparent() const { return false; } // pour Pause au-dessus de Play
};
```

États prévus : `MenuState`, `PlayState`, `PauseState`, `EditorState`, `GameOverState`.

`PlayState` possède le `entt::registry` de la partie en cours, instancie les systèmes ECS et orchestre leur exécution à chaque `update()`.

---

## 6. Rendu (SFML)

- `RenderSystem` lit les composants `Position` + `Sprite`/`AnimatedSprite` et dessine dans un ordre de calque (fond de grille → tuiles destructibles → bombes/explosions → entités mobiles → HUD).
- Le `ResourceManager` (dans `lib/resources/`) centralise le chargement des `sf::Texture`, `sf::Font`, `sf::SoundBuffer` avec cache par chemin, pour éviter les rechargements et les fuites.
- La résolution logique du jeu est découplée de la taille de fenêtre via une `sf::View` fixe, avec letterboxing si nécessaire — permet le redimensionnement de fenêtre sans casser le rendu.
- Les assets sont chargés depuis `${BOMBERMAN_ROOT}/data/...`, cohérent avec l'usage déjà présent dans `exe/main.cpp`.

---

## 7. Grille et niveaux

### 7.1 État actuel à faire évoluer

Le module `lib/grid/` existant (`Grid`, `Tile`) est un point de départ mais présente des incohérences à corriger avant de bâtir dessus (déclaration dupliquée du constructeur `Grid`, absence de stockage 2D réel, `Tile` sans position). Proposition cible :

```cpp
namespace Bomberman {

class Grid {
public:
    explicit Grid(std::filesystem::path levelFile);

    const Tile& at(int col, int row) const;
    int width() const noexcept;
    int height() const noexcept;

    void loadFromJson(const std::filesystem::path& path);

private:
    int width_, height_;
    std::vector<Tile> tiles_; // stockage linéaire, indexé par row * width_ + col
};

}
```

### 7.2 Format de niveau (JSON)

Le format existant dans `data/test_grid.json` sert de base : une grille de caractères mappés à des types de tuiles (`#` mur, `.` sol, `B` spawn bombe/joueur, etc.). À formaliser :

```json
{
  "width": 13,
  "height": 11,
  "legend": { "#": "wall", ".": "empty", "?": "destructible", "P1": "player_spawn_1" },
  "grid": ["...", "..."]
}
```

Le module `lib/level/` fait le pont entre ce JSON et la création d'entités ECS (une tuile `#` → entité avec composants `Position`, `Solid`, `Sprite` ; une tuile `P1` → position de spawn enregistrée pour `PlayerFactory`).

### 7.3 Éditeur de niveau

L'`EditorState` réutilise `Grid` en écriture, affiche la même `RenderSystem`, et sérialise vers le même format JSON via `lib/level/level_writer`. Il n'a pas besoin d'un ECS complet actif (pas de bombes/IA) — il peut manipuler directement des `Tile` sans passer par le registry, ou utiliser un registry "éditeur" allégé.

---

## 8. Points ouverts / décisions à prendre

Ces points n'ont pas encore été tranchés et méritent une discussion dédiée le moment venu :

1. **Pathfinding IA** : A* simple sur la grille suffira probablement (pas de navmesh nécessaire vu la structure en tuiles). À confirmer selon le comportement d'IA voulu (agressif, aléatoire, évitant les bombes...).
2. **Multijoueur local — gestion des inputs** : mapping clavier fixe par joueur (ex: WASD+Espace vs flèches+Entrée) à définir dans `lib/input/`, découplé des actions logiques (`InputAction::MoveUp`, `PlaceBomb`, etc.) pour permettre un futur remapping ou l'ajout de manettes.
3. **Écran partagé vs caméra unique** : Bomberman classique tient sur un seul écran fixe (pas de scrolling), donc probablement pas de split-screen nécessaire — à confirmer selon la taille de grille visée.
4. **Éditeur intégré vs outil séparé** : l'éditeur peut être un `GameState` de l'exécutable principal (proposé ci-dessus) ou un exécutable séparé partageant `lib/grid` et `lib/level`. Le choix state-in-app est recommandé pour limiter la duplication de rendu.
5. **Réseau** : non demandé dans le scope actuel (multijoueur local uniquement) ; le dossier `lib/network/` est laissé en placeholder si le besoin apparaît plus tard, mais ne doit pas être développé prématurément.

---

## 9. Dépendances (Conan)

À ajouter dans `conanfile.py` en complément de l'existant :

```python
requires = "nlohmann_json/3.11.3", "entt/3.13.2"
```

(`sfml/3.0.2` déjà présent dans `requirements()`.)

---

## 10. Prochaines étapes suggérées

1. Corriger et compléter `lib/grid/` (constructeur unique, stockage réel, chargement JSON).
2. Mettre en place `lib/core/application.h/.cpp` avec la boucle à pas fixe.
3. Ajouter EnTT en dépendance et créer `lib/ecs/` avec les premiers composants (`Position`, `Sprite`, `Velocity`).
4. Implémenter `StateStack` + `MenuState`/`PlayState` minimal (affichage de la grille statique).
5. Ajouter `InputSystem` et `MovementSystem` pour un joueur déplaçable.
6. Itérer : bombes → explosions → destructions → IA → éditeur → multijoueur local.
