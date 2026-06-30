from conan import ConanFile
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.gnu import PkgConfigDeps


class Recipe(ConanFile):
    name = "poc-mixr"
    version = "0.1.0"

    license = "LGPL-3.0"
    description = "Demonstracao funcional do MIXR com gravacao de trajetoria para o Tacview (ACMI 2.2)."

    settings = "arch", "build_type", "compiler", "os"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True}

    exports_sources = "meson.build*", "meson_options.txt", "include/*", "src/*", "config/*"

    def requirements(self):
        # mixr ja embute jsbsim, openrti e protobuf como dependencias transitivas
        self.requires("mixr/1.0.5")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        self.folders.build = "build"
        self.folders.generators = "build"

    def generate(self):
        tc = PkgConfigDeps(self)
        tc.generate()

        tc = MesonToolchain(self)
        tc.generate()

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        meson = Meson(self)
        meson.install()

    def package_info(self):
        self.cpp_info.includedirs = ["include"]
