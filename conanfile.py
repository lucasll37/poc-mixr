from conan import ConanFile
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.gnu import PkgConfigDeps
from conan.tools.layout import basic_layout


class MixrHelloConan(ConanFile):
    name = "mixr-hello"
    version = "1.0.0"

    settings = "os", "compiler", "build_type", "arch"

    # A BehaviorTree.CPP tem de ser ESTATICA: o plugin do modelo a linka com
    # -Wl,--exclude-libs,ALL para esconder os simbolos dela do .dynsym (ver
    # models/flight/meson.build); o host nunca a linka, pra nao
    # duplicar o contador estatico BT::getUID() entre host e plugin. A
    # receita da behaviortree.cpp.asa tem default_options={"shared": True},
    # entao sem este override o Conan resolve/constroi a variante .so e o
    # link do modelo quebra com "dependencia nao resolvida" em runtime.
    default_options = {
        "behaviortree.cpp.asa/*:shared": False,
        "behaviortree.cpp.asa/*:fPIC": True,
    }

    def requirements(self):
        self.requires("mixr/1.0.5", transitive_headers=True)
        self.requires("behaviortree.cpp.asa/3.5.6")
        # So o ./app/ linka isto (TUI do laco de tempo real) -- nenhum outro
        # poc nem o modelo. MIT, sem dependencia de sistema alem de um
        # compilador C++17.
        self.requires("ftxui/7.0.3")

    def build_requirements(self):
        # Framework da suite de testes. Fica em test_requires porque nenhum
        # binario da aplicacao linka gtest -- so os alvos de tests/, e so
        # quando o build e configurado com -Dtests=true.
        self.test_requires("gtest/1.14.0")

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
