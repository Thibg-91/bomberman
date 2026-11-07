#include "MyClass.hpp"

namespace mynamespace
{

// =========================
// Static members
// =========================
int MyClass::s_instanceCount = 0;

// =========================
// Constructors / Destructor
// =========================
MyClass::MyClass()
    : m_name("default")
{
    ++s_instanceCount;
}

MyClass::MyClass(const std::string& name)
    : m_name(name)
{
    ++s_instanceCount;
}

// Copy constructor
MyClass::MyClass(const MyClass& other)
    : m_name(other.m_name),
      m_values(other.m_values)
{
    ++s_instanceCount;
}

// Move constructor
MyClass::MyClass(MyClass&& other) noexcept
    : m_name(std::move(other.m_name)),
      m_values(std::move(other.m_values))
{
    ++s_instanceCount;
}

// Copy assignment
MyClass& MyClass::operator=(const MyClass& other)
{
    if (this != &other)
    {
        m_name = other.m_name;
        m_values = other.m_values;
    }
    return *this;
}

// Move assignment
MyClass& MyClass::operator=(MyClass&& other) noexcept
{
    if (this != &other)
    {
        m_name = std::move(other.m_name);
        m_values = std::move(other.m_values);
    }
    return *this;
}

// =========================
// Public Methods
// =========================
void MyClass::doSomething() const
{
    // Exemple simple
    // (à remplacer par ta logique)
}

void MyClass::setName(const std::string& name)
{
    m_name = name;
}

const std::string& MyClass::getName() const noexcept
{
    return m_name;
}

// =========================
// Static Methods
// =========================
int MyClass::getInstanceCount()
{
    return s_instanceCount;
}

// =========================
// Private Methods
// =========================
void MyClass::helperFunction()
{
    // logique interne
}

} // namespace mynamespace