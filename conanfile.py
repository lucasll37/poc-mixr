from conan import ConanFile
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.gnu import PkgConfigDeps
from conan.tools.layout import basic_layout


class MixrHelloConan(ConanFile):
    name = "poc-mixr"
    version = "1.0.0"

    settings = "os", "compiler", "build_type", "arch"

    # A BehaviorTree.CPP tem de ser ESTATICA: o plugin do modelo a linka com
    # -Wl,--exclude-libs,ALL para esconder os simbolos dela do .dynsym (ver
    # models/players/A-4/meson.build); o host nunca a linka, pra nao
    # duplicar o contador estatico BT::getUID() entre host e plugin.
    #
    # ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): este comentario
    # afirmava que a receita da behaviortree.cpp.asa tem
    # default_options={"shared": True} -- a receita LOCAL (deps/behaviortree/
    # conanfile.py, usada por scripts/deps.sh) ja declara default_options=
    # {"shared": False}, o oposto. Nao verificavel daqui se o pacote do
    # remote privado da ASA (o caminho que 'make configure' de fato usa,
    # ver README.md SS2.4) foi construido com opcoes diferentes da receita
    # local -- mas o override abaixo e' seguro/idempotente nos dois casos
    # (redundante se o default ja for False, essencial se nao for), entao
    # fica mantido como defesa em profundidade independente da resposta.
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

        # So src/rl/bindings linka isto -- o modulo de extensao Python que expoe
        # a Station a um gymnasium.Env (ver src/rl/README.md). Header-only, sem
        # dependencia de sistema alem do proprio Python de desenvolvimento
        # (Python.h/libpython, resolvidos pelo modulo 'python' do Meson, nao
        # pelo Conan).
        self.requires("pybind11/2.13.6")

        # So libs/xinfer linka isto -- o motor de inferencia que roda a
        # politica .onnx de dentro do frame (ver libs/xinfer/README.md).
        # Estatico (a receita ja tem shared=False por default), e contido
        # inteiro dentro de libxinfer.so por '-Wl,--exclude-libs,ALL': nenhum
        # outro alvo, e nenhum modelo, ve header ou biblioteca do ORT.
        self.requires("onnxruntime/1.17.3")

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
