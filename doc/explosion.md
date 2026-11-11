# Explosions et zone de danger

Le souffle d'une bombe s'affiche en croix sur exactement les cases touchées : sa longueur suit donc la puissance de la bombe, augmentée par l'upgrade « Force de la bombe ». Pendant la mèche, ces mêmes cases sont teintées en rouge pour avertir les joueurs. Le design a été validé sur la maquette « Souffle des bombes ».

## Une seule règle : `Blast`

`Blast::compute(centre, rayon, grille)` (`lib/bomb`) donne la forme d'une explosion : le centre, plus le nombre de cases de chaque branche (haut, bas, gauche, droite). Une branche s'arrête **avant** un mur ou le bord, et **inclut** la caisse qu'elle touche. Le calcul ne modifie pas la grille.

- `BombManager::update` l'applique (destruction des caisses) et renvoie les `Blast` des bombes qui ont explosé. Les bombes explosent l'une après l'autre : une caisse détruite par la première ne bloque plus la seconde.
- `GameEngine` émet un événement `Explosion` **par bombe**, avec sa `Blast`.
- `DangerZoneRenderer` la prévisualise pendant la mèche : la zone affichée est toujours exactement celle de l'explosion à venir (tant que la grille ne change pas d'ici là).

## Rendu

| Élément | Classe | Dessin |
|---|---|---|
| Explosion (en jeu) | `ExplosionRenderer`, style `Fireballs` | chaîne de boules de feu qui éclosent case après case depuis le centre (0,05 de la chronologie par case), puis tournent en fumée grise |
| Explosion (réserve) | `ExplosionRenderer`, style `FlameCross` | croix continue en quatre couches (rouge, orange, jaune, blanc) à bouts ronds, gardée pour une future fonctionnalité |
| Zone de danger | `DangerZoneRenderer` | carré arrondi rouge dans chaque case menacée, de plus en plus opaque, avec une pulsation qui accélère vers la fin de la mèche |

Une explosion dure 0,6 s. À partir de 60 % de sa durée, elle s'estompe **d'un bloc**, comme l'opacité de groupe de la maquette : elle est dessinée opaque dans une texture intermédiaire, puis cette texture est fondue dans la scène. Sans cela, les contours noirs des boules transparaîtraient à travers le feu.

Ordre de dessin dans `exe/main.cpp` : grille → zone de danger → upgrades → bombes → joueur → explosions.

## Tests

- `tests/bomb/blast_test.cpp` : portée complète, arrêt sur les murs et le bord, caisse incluse, ordre des cases, cohérence avec `BombManager`, explosions successives.
- `tests/engine` : un événement `Explosion` par bombe, avec sa `Blast`.
- `tests/render/explosion_renderer_test.cpp` : les deux styles ; durée, estompage, longueur de la croix selon la portée, départ du centre, éclosion case après case, passage à la fumée.
- `tests/render/danger_zone_renderer_test.cpp` : cases teintées identiques à l'explosion à venir, plus de cases avec un rayon plus grand, forme et couleur, intensité croissante, bombes explosées ignorées.
