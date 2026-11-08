# Ajout d'un personnage

## Objectif
Implementer un personnage dans le jeu, ses déplacements, en respectant les contraintes du jeu déjà existantes. Il doit également pouvoir casser les cases destructibles L'animation du personnage sur la grille de jeu chargée est également attendue.

## Contexte / fichiers concernés
Ajoute une nouvelle librairie implementant la creation et le controle d'un personnage.

## Contraintes
Le personnage créé doit pouvoir se deplacer via des inputs clavier (par défaut z/q/s/d) et casser les cases de grille destructibles via appui sur la touché "enter", il doit respecter les régles de colision de la grille. 
Lorsqu'une case de grille est cassée, l'état doit être mis à jour aussi bien en back que sur le front.
Prends en compte le fait que je compte dans le futur implementé un/des personnages Ordinateur (controlé par une IA générée) mais également un mode en ligne.
Ajoute une série de test permettant de couvrir le scope défini ici.

## Critère de succès
Le projet doit compiler, l'ensemble des tests doivent passer.

## Hors scope
Le mode multijoueur et l'IA.