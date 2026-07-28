from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout

class BombermanConan(ConanFile):
    name = "bomberman"
    version = "1.0.0"

    settings = "os", "compiler", "build_type", "arch"

    requires = "nlohmann_json/3.11.3"

    generators = "CMakeToolchain", "CMakeDeps"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("sfml/3.0.2")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
