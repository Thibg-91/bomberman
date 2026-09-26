import os

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import collect_libs, copy

class BombermanConan(ConanFile):
    name = "bomberman"
    version = "1.0.0"

    # Marks this package's requirement as "run" for consumers, which is what
    # makes Conan inject package_folder/bin into PATH (VirtualRunEnv) so the
    # grid.dll gets found at runtime.
    package_type = "shared-library"

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

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "*",
             src=os.path.join(self.source_folder, "data"),
             dst=os.path.join(self.package_folder, "data"))

    def package_info(self):
        self.cpp_info.libs = collect_libs(self)
        self.runenv_info.define_path("BOMBERMAN_ROOT", self.package_folder)
