# add game engine

## Objectif

L'objectif de la feature est d'ajouter des items d'upgrade dans les cases destructibles.

# Contexte

A chaque fois qu'une case est détruite, il y a une probabilité de 25% qu'il y ai une upgrade dedans, un personnage doit marcher dessus pour la consommer.
Il y a trois type d'upgrades : 
- bombe supplémentaire : augmente le nombre de bombe pouvant être posé en simultanée de +1.
- Force du personnage : augmente la force de poussé d'un personnage de +1.
- Force de la bombe, augmente le rayon d'explosion d'une bombe de +1.

Je souhaite que tu me propose des designs pour chacune de ces augmentation puis que tu les implementes.

## Contraintes

pars de la branche master, Tu dois implementer ce nouvel ajout, je veux un jeu de test permettant de prouver son bon fonctionnement et son maintien. 

## Critère de succès
Le projet actuel compile, l'ensemble des tests passent.
