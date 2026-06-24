from conan import ConanFile
from conan.tools.gnu import PkgConfigDeps
from conan.tools.meson import MesonToolchain, Meson


class PocMixr(ConanFile):
    name = "poc-miia"
    version = "0.0.1"

    license = "MIT"
    url = ""
    description = ""
    settings = "arch", "build_type", "compiler", "os"
    
    exports_sources = "meson*", "src/*", "conanfile.py"
    
    def requirements(self):
        self.requires("mixr/0.0.1")

    def generate(self):
        pc = PkgConfigDeps(self)
        pc.generate()
        
        tc = MesonToolchain(self)
        tc.generate()
        
    def layout(self):
        self.folders.build = "build"
        self.folders.generators = "build"
        
    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        meson = Meson(self)
        meson.install()

    # def package_info(self):
    #     self.cpp_info.libs = ["miia_client"]
    #     self.cpp_info.includedirs = ["include"]