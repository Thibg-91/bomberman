# TODO

## `CharacterSelectRenderer` : le réutiliser ou le supprimer

L'ancien écran de lancement (choix du pirate seul, maquette « Choix du pirate », mise en page « Sur le pont ») n'est plus affiché : depuis l'ajout des bots, le jeu s'ouvre sur le menu « Préparer l'abordage » (`MatchSetupRenderer`, voir [doc/menu.md](doc/menu.md)), qui contient lui-même la rangée de choix du pirate.

Le code est conservé, mais rien ne l'utilise :

- `lib/render/src/character_select_renderer.h` / `.cpp`
- `tests/render/character_select_renderer_test.cpp`

À décider :

- **le réutiliser**, par exemple comme écran de choix du pirate pour chaque joueur du futur mode multijoueur local (chaque joueur choisit son pirate à tour de rôle), ou comme écran dédié ouvert depuis la rangée « pirate » du menu ;
- **ou le supprimer** (les trois fichiers ci-dessus), puis retirer sa mention dans `CLAUDE.md` (section « Architecture » et « Rendering notes »).
