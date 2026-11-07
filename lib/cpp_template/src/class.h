#ifndef MYCLASS_HPP
#define MYCLASS_HPP

#pragma once

#include <string>
#include <memory>
#include <vector>

namespace mynamespace
{

class MyClass
{
public:
    // =========================
    // Constructors / Destructor
    // =========================
    MyClass();
    explicit MyClass(const std::string& name);

    ~MyClass() = default;

    // Rule of 5
    MyClass(const MyClass& other);
    MyClass(MyClass&& other) noexcept;
    MyClass& operator=(const MyClass& other);
    MyClass& operator=(MyClass&& other) noexcept;

    // =========================
    // Public Methods
    // =========================
    void doSomething() const;
    void setName(const std::string& name);
    [[nodiscard]] const std::string& getName() const noexcept;

    // =========================
    // Static Methods
    // =========================
    static int getInstanceCount();

private:
    // =========================
    // Private Methods
    // =========================
    void helperFunction();

    // =========================
    // Members
    // =========================
    std::string m_name;
    std::vector<int> m_values;

    static int s_instanceCount;
};

} // namespace mynamespace

#endif // MYCLASS_HPP