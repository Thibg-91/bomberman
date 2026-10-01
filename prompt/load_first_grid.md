# Charger un premier terrain

## Objectif
Charger une première grille de taille conventionnelle Bomberman depuis un fichier et
l'afficher avec SFML (récupéré par Conan).

## Contexte / fichiers concernés
lib/grid (modèle et chargement), nouvelle lib/render (affichage SFML), exe/main.cpp,
data/grid/level_01.json.

## Contraintes
Code pensé pour accueillir plus tard personnages, bombes et objets.
Respecter les templates CMake et le style de lib/cpp_template, namespace Bomberman::.

## Critère de succès
Le niveau s'affiche dans une fenêtre ; des tests unitaires couvrent la feature et passent.

## Hors scope
Personnages, bombes, bonus, textures.
