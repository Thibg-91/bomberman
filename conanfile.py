import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
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

    options = {"coverage": [True, False]}
    # SFML must be shared: render.dll exports all its symbols, so a static
    # SFML would be defined both in render.dll and in each consumer.
    default_options = {"sfml/*:shared": True, "coverage": False}

    generators = "CMakeDeps"

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BOMBERMAN_COVERAGE"] = bool(self.options.coverage)
        tc.generate()

    def package_id(self):
        # Instrumentation does not change what consumers get.
        del self.info.options.coverage

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("sfml/3.0.2")

    def build_requirements(self):
        self.test_requires("gtest/1.17.0")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        # Skipped with -c tools.build:skip_test=True
        cmake.test()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "*",
             src=os.path.join(self.source_folder, "data"),
             dst=os.path.join(self.package_folder, "data"))

    def package_info(self):
        self.cpp_info.libs = collect_libs(self)
        self.runenv_info.define_path("BOMBERMAN_ROOT", self.package_folder)
