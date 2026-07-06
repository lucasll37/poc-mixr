from conan import ConanFile
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.gnu import PkgConfigDeps
from conan.tools.layout import basic_layout


class MixrHelloConan(ConanFile):
    name = "mixr-hello"
    version = "1.0.0"

    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("mixr/1.0.5", transitive_headers=True)
        self.requires("behaviortree.cpp.asa/3.5.6")

    def layout(self):
        self.folders.build = "build"
        self.folders.generators = "build"

    def generate(self):
        tc = MesonToolchain(self)
        tc.generate()

        pc = PkgConfigDeps(self)
        pc.generate()
        
    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()
