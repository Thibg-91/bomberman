# Ajout du systeme de bombe

## Objectif

L'objectif de cette feature est d'ajouter un nouvel objet : les bombes. Ajoute aux personnage la capacité de poser une bombe, son comportement est décris dans la section "Contraites". 
Dans un premier temps, j'aimerais que tu me propose différents design de bombe que tu auras généré,  afin que je choisisse mon préféré. Ca sera le design à chargé par la suite sur la grille.

## Contexte / fichiers concernés

Les bombes doivent être dans une nouvelle classe.

## Contraintes

Elle doit être placée sur la case devant lui, si cette case est occupé alors l'action échoue. Lorsque l'un des personnages tape dans une bombe, elle est repoussé de X cases.
le X par défaut vaut 1 mais il pourra augmenter dans une prochaine feature.
Une bombe une fois posé explose au bout de 2 secondes. elle détruit les cases destructibles autour d'elle en forme de croix (le centre étant la bombe), les rayons de la croix font X cases, avec x valant 1 au départ, X pourra être modifiée dans une future feature. Lorsqu'un personnage est touché par le rayon d'explosion d'une bombe (même la sienne), alors il meurt.
Ajoute des tests couvrant le comportemet décris.

## Critère de succès

Le projet doit compiler, l'ensemble des tests doivent passer.