# Titre court de la tâche

## Objectif
Implementer la buildchain de ce projet Bomberman en C++ basé sur Conan et la tool chain Conan.

## Contexte / fichiers concernés
Pour le moment, il n'y a qu'un CMakeLists à la racine qui peut évoluer. Ajoutes les fichiers .cmake afin de respecter les contraintes et les fichiers CMakeLists.txt aux endroits necessaires afin de génerer les libs souhaités. 

## Contraintes
Tu dois travailler avec des templates CMake que tu dois également générer, je veux avoir à minima un template CMake permettant de compiler une librairie, une librairie header-only et un executable. Je t'encourage à génerer également un fichier helper dans lequel tu mettras l'ensemble des macros CMake communes.
L'ensemble des fichiers ayant l'extension .cmake devront être générés dans un dossier cmake.
Je veux des libraries dynamiques
Je veux que ce code soit portable vers Linux/MacOS.

## Critère de succès
Le projet actuel compile, je veux une librairie pour grid et un exe pour le main.

## Hors scope
Aucun test ne sont attendus.