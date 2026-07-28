# Utiliser SFML dans ce projet

Ce projet dépend de **SFML 3.0.2**, géré via Conan (voir [`conanfile.py`](../conanfile.py)). Ce document explique comment SFML est intégré et comment l'utiliser pour développer les fonctionnalités graphiques du jeu.

## Sommaire

- [Installation et configuration](#installation-et-configuration)
- [Modules de SFML](#modules-de-sfml)
- [Créer une fenêtre](#créer-une-fenêtre)
- [La boucle de jeu](#la-boucle-de-jeu)
- [Gérer les événements](#gérer-les-événements)
- [Dessiner des formes](#dessiner-des-formes)
- [Charger et afficher une texture / sprite](#charger-et-afficher-une-texture--sprite)
- [Afficher du texte](#afficher-du-texte)
- [Horloge et delta time](#horloge-et-delta-time)
- [Ressources utiles](#ressources-utiles)

## Installation et configuration

SFML est déclaré comme dépendance dans `conanfile.py` :

```python
def requirements(self):
    self.requires("sfml/3.0.2")
```

Conan télécharge et compile SFML, puis génère les fichiers CMake nécessaires (`CMakeToolchain`, `CMakeDeps`). Pour l'utiliser dans une cible CMake (par exemple dans `lib/` ou `exe/`), il faut :

```cmake
find_package(SFML COMPONENTS System Window Graphics Audio Network REQUIRED)

target_link_libraries(ma_cible PRIVATE SFML::Graphics SFML::Window SFML::System)
```

N'utilisez que les composants dont vous avez réellement besoin (`Graphics`, `Window`, `System` suffisent pour la plupart des besoins d'affichage).

## Modules de SFML

| Module     | Rôle                                                              |
|------------|--------------------------------------------------------------------|
| `System`   | Types de base (vecteurs, horloges, threads, gestion des erreurs)   |
| `Window`   | Création de fenêtre, gestion des événements clavier/souris          |
| `Graphics` | Rendu 2D : formes, sprites, textures, texte, vues                  |
| `Audio`    | Sons et musique                                                     |
| `Network`  | Communication réseau (sockets, HTTP, FTP)                          |

Pour Bomberman, les modules `System`, `Window` et `Graphics` sont les plus pertinents (affichage de la grille, des joueurs, des bombes, etc.).

## Créer une fenêtre

```cpp
#include <SFML/Graphics.hpp>

int main()
{
    sf::RenderWindow window(sf::VideoMode({800, 600}), "Bomberman");

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        window.clear(sf::Color::Black);
        // ... dessiner ici ...
        window.display();
    }
}
```

> **Note** : SFML 3 a changé l'API des événements par rapport à SFML 2. On utilise désormais `window.pollEvent()` qui retourne un `std::optional<sf::Event>`, et `event->is<T>()` / `event->getIf<T>()` pour tester/extraire le type d'événement.

## La boucle de jeu

Le schéma classique d'une boucle de jeu SFML :

1. Traiter les événements (`pollEvent`)
2. Mettre à jour la logique du jeu (déplacement, collisions, bombes...)
3. Effacer l'écran (`window.clear`)
4. Dessiner les éléments (`window.draw`)
5. Afficher le résultat (`window.display`)

```cpp
while (window.isOpen())
{
    while (const std::optional event = window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
            window.close();
    }

    update(deltaTime);

    window.clear();
    window.draw(gridSprite);
    window.draw(playerSprite);
    window.display();
}
```

## Gérer les événements

```cpp
while (const std::optional event = window.pollEvent())
{
    if (event->is<sf::Event::Closed>())
        window.close();

    if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
    {
        if (keyPressed->code == sf::Keyboard::Key::Escape)
            window.close();
        if (keyPressed->code == sf::Keyboard::Key::Space)
            placeBomb();
    }
}
```

Pour un jeu comme Bomberman, il est souvent préférable de vérifier l'état des touches en dehors de la boucle d'événements (mouvement continu tant qu'une touche est maintenue) :

```cpp
if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
    movePlayerUp();
```

## Dessiner des formes

Utile pour prototyper la grille avant d'avoir des sprites définitifs :

```cpp
sf::RectangleShape tile({32.f, 32.f});
tile.setPosition({x * 32.f, y * 32.f});
tile.setFillColor(sf::Color::Green);

window.draw(tile);
```

Autres formes disponibles : `sf::CircleShape`, `sf::ConvexShape`.

## Charger et afficher une texture / sprite

```cpp
sf::Texture texture;
if (!texture.loadFromFile("data/textures/wall.png"))
{
    // gérer l'erreur de chargement
}

sf::Sprite sprite(texture);
sprite.setPosition({x * 32.f, y * 32.f});

window.draw(sprite);
```

Dans ce projet, les chemins vers les ressources sont généralement construits à partir de la variable d'environnement `BOMBERMAN_ROOT` (voir [`exe/main.cpp`](../exe/main.cpp)), de la même façon que pour les fichiers de grille JSON.

## Afficher du texte

```cpp
sf::Font font;
if (!font.openFromFile("data/fonts/arial.ttf"))
{
    // gérer l'erreur
}

sf::Text text(font, "Score: 0", 24);
text.setFillColor(sf::Color::White);
text.setPosition({10.f, 10.f});

window.draw(text);
```

## Horloge et delta time

Pour des animations et déplacements indépendants du framerate :

```cpp
sf::Clock clock;

while (window.isOpen())
{
    sf::Time deltaTime = clock.restart();
    float dt = deltaTime.asSeconds();

    update(dt); // ex : position += vitesse * dt
    // ...
}
```

## Ressources utiles

- Documentation officielle : https://www.sfml-dev.org/documentation/3.0.2/
- Tutoriels officiels : https://www.sfml-dev.org/tutorials/3.0/
- Dépôt GitHub : https://github.com/SFML/SFML

> Rappel : ce projet utilise **SFML 3.0**, dont l'API des événements et des ressources (`sf::Font::openFromFile`, `sf::Texture` sans `create`, etc.) diffère de SFML 2.x. Toujours se référer à la documentation de la version 3.0 pour éviter les erreurs de compilation liées à une API obsolète.
