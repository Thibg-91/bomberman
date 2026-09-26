#include <iostream>
#include <filesystem>
#include "grid.h"

int main()
{
   std::cout << "Start Bomberman" << std::endl;
   const char* bomberman_root = std::getenv("BOMBERMAN_ROOT");
    if (bomberman_root)
      std::cout << "bomberman_root = " << bomberman_root << "\n";
    else
      std::cout << "BOMBERMAN_ROOT n'est pas défini\n";

   std::filesystem::path grid_path = std::filesystem::path(bomberman_root ? bomberman_root : ".") / "data" / "grid" / "test_grid.json";

   Bomberman::Grid grid = Bomberman::Grid(grid_path);
}