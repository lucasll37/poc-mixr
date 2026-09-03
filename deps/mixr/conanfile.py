from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.gnu import PkgConfigDeps


class Recipe(ConanFile):
    # Mandatory metadata
    name = "mixr"
    version = "1.0.5"

    # Optional metadata
    license = "LGPL-3.0"
    url = "https://github.com/ASA-Simulation/mixr"
    description = "A fork of MIXR focused on meson/conan integration."

    # Binary configuration
    settings = "arch", "build_type", "compiler", "os"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True}

    # Commit fixado, e NAO a tag v1.0.5 -- ver o comentario de source().
    _commit = "4a4efbd5ef23f6edc60dbc3ddcc38c4fbe91ab77"

    def requirements(self):
        # As duas primeiras sao construidas por ./deps/jsbsim e ./deps/openrti
        # (mesmas receitas que o proprio repositorio do mixr carrega em deps/),
        # entao scripts/deps.sh tem de cria-las ANTES desta. protobuf vem do
        # conancenter e traz o protoc que o meson do mixr acha pelo prefix do
        # .pc ("protobuf_dep.get_variable('prefix')" no meson.build da raiz do
        # mixr) -- por isso nao ha tool_requires de protobuf aqui.
        self.requires("jsbsim/1.1.11", transitive_headers=True)
        self.requires("openrti/814a210978b7faafd65affbe70a2e25679921b23")
        self.requires("protobuf/3.21.12")

    def source(self):
        # O commit e o da branch 'development' -- deliberadamente NAO a tag
        # v1.0.5, apesar de a versao do pacote ser 1.0.5 (e a versao declarada
        # no project() do meson.build DESTE commit):
        #
        #   * a tag v1.0.5 e ANTERIOR e nao tem os acessores
        #     Player::getRFSignature()/getIRSignature() nem os
        #     dis::NetIO::sendData()/recvData()/createNewOutputNib() virtuais
        #     (na tag eles sao 'final'), que sao o motivo do fork;
        #   * a branch 'main' ja divergiu: acrescenta '-static-libstdc++
        #     -static-libgcc' ao link de cada lib do mixr, o que mudaria o que
        #     este projeto (host + plugins abertos com dlopen, trocando objetos
        #     C++ pela fronteira) linka em runtime.
        #
        # Este commit e exatamente a arvore de contexts/src/mixr e e o que
        # gerou o pacote mixr/1.0.5 hoje em cache.
        git = Git(self)
        git.clone(url="https://github.com/ASA-Simulation/mixr", target=".")
        git.checkout(self._commit)

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
        self.cpp_info.components["base"].libs = ["mixr_base"]
        self.cpp_info.components["interop_common"].libs = ["mixr_interop_common"]
        self.cpp_info.components["interop_dis"].libs = ["mixr_interop_dis"]
        self.cpp_info.components["linearsystem"].libs = ["mixr_linearsystem"]
        self.cpp_info.components["linkage"].libs = ["mixr_linkage"]
        self.cpp_info.components["models"].libs = ["mixr_models"]
        self.cpp_info.components["recorder"].libs = ["mixr_recorder"]
        self.cpp_info.components["simulation"].libs = ["mixr_simulation"]
        self.cpp_info.components["terrain"].libs = ["mixr_terrain"]
