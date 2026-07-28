#include "Grid.hpp"

namespace mynamespace
{


Grid::Grid()
{
}

void Grid::loadGrid(std::filesystem::path grid_file) 
{
   
}

void Grid::setName(const std::string& name)
{
    m_name = name;
}

const std::string& Grid::getName() const noexcept
{
    return m_name;
}

// =========================
// Static Methods
// =========================
int Grid::getInstanceCount()
{
    return s_instanceCount;
}

// =========================
// Private Methods
// =========================
void Grid::helperFunction()
{
    // logique interne
}

} 