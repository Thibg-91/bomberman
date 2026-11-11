# Fin de partie (mort, arbitre, écran « GAME OVER »)

Maquettes validées : « Fin de l'abordage » (animation de mort « Carbonisé », écran « Le pavillon noir », coffre « Le coffre débordant »).

## Animation de mort « Carbonisé » (`CharacterRenderer`)

`exe/main.cpp` appelle `triggerDeathAnimation()` sur l'événement `PlayerKilled`, puis continue d'appeler `update()` et de dessiner le personnage tant que `isDying()` est vrai. Elle dure `DeathDuration` = 1,3 s, sur la dernière case du personnage :

| Temps | Effet |
|---|---|
| 0 – 0,12 s | éclair blanc (couleurs × 2,2, plafonnées) |
| 0,12 – 0,35 s | carbonisation : couleurs ramenées à 18 % |
| 0,2 – 0,6 s | tremblement horizontal (± 3 % de case) |
| 0,6 – 1,1 s | effondrement : écrasé vers ses semelles (hauteur → 0, largeur × 1,15) et fondu partiel |
| 0,6 – 1,3 s | 10 cendres projetées puis tombant, qui s'effacent de 1,0 à 1,3 s |
| 0,55 – 1,3 s | nuage de fumée grise (3 disques) qui monte, grossit et s'efface |

Les positions des cendres viennent du même pseudo-hasard que la maquette (`hash(i, salt)`), donc l'animation est identique à chaque fois.

## L'arbitre (`MatchReferee`, `lib/engine`)

Purement logique, testé sans fenêtre. À chaque frame, `exe/main.cpp` lui demande combien de temps de jeu faire avancer (`gameSeconds()`), puis lui montre l'état du moteur (`update()`) :

- **Playing** : temps normal ;
- **Spectating** : tu es mort mais il reste au moins deux adversaires : la partie continue **4 fois plus vite** (`SpectatorSpeed`) jusqu'au dernier survivant, au plus 30 s réelles (`SpectatorTimeLimit`), sinon égalité ;
- **Over** : il reste un pirate (le gagnant) ou aucun (égalité) ; le moteur est figé (`gameSeconds()` renvoie 0), les animations (feu, mort) continuent.

## Écran « GAME OVER » (`GameOverRenderer`)

Une fois la partie terminée, la dernière animation de mort se joue (1,3 s), puis l'écran s'affiche `Duration` = 3 s, puis le menu revient. Il est dessiné par-dessus le plateau figé, dans un espace de 1600x900 (`viewFor()`) :

- le plateau s'assombrit (0 – 0,4 s) ;
- un pavillon noir à tibias croisés tombe du haut (0,05 – 0,5 s) et ondule ;
- les lettres de GAME OVER tombent une à une sur le pavillon (à partir de 0,35 s, une toutes les 0,06 s) : **dorées pour une victoire**, blanc os sinon. Le projet n'ayant pas de police, elles sont dessinées en traits arrondis (glyphes G, A, M, E, O, V, R dans une boîte 70x100) ;
- sous un projecteur, le coffre ouvert débordant d'or surgit en rebondissant (0,55 s), le gagnant debout dans les pièces, couronné, qui sautille ; une tête de mort le remplace en cas d'égalité ;
- pour une victoire, une pluie de pièces d'or.

`MatchOutcome` : `Victory` (tu es le dernier), `Defeat` (un adversaire l'est), `Draw` (personne).
