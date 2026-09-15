from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.files import replace_in_file
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout


class Recipe(ConanFile):
    # Mandatory metadata
    name = "behaviortree.cpp.asa"
    version = "3.5.6"

    # Optional metadata
    license = "MIT"
    url = "https://github.com/ASA-Simulation/BehaviorTree.CPP"
    description = "The C++ library to build Behavior Trees (fork ASA da v3.5.6)."

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tools": [True, False],
        "with_coroutines": [True, False],
    }
    # shared=False porque o plugin do modelo linka a BT.CPP ESTATICA e esconde
    # os simbolos dela com '-Wl,--exclude-libs,ALL' (ver models/players/A-4/
    # meson.build); o core nunca a linka, pra nao duplicar o contador estatico
    # de BT::getUID() entre core e plugin. O conanfile.py da raiz repete este
    # override do lado do consumidor.
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tools": False,
        "with_coroutines": True,
    }

    generators = "CMakeDeps"

    # Fixa o commit, nao a tag 'v3.5.6' -- mesma pratica de deps/mixr e
    # deps/openrti. Uma tag de fork mantido pela propria equipe e' mais
    # suscetivel a ser movida por engano do que a de um projeto de terceiro
    # estabelecido; se isso acontecesse, scripts/deps.sh construiria
    # silenciosamente uma arvore diferente da que gerou o pacote em cache,
    # sem erro ate algo quebrar em runtime.
    _commit = "abd9d75aa2f7f2fa21cab60dbd20cfc7503e4c43"

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/ASA-Simulation/BehaviorTree.CPP", target=".")
        git.checkout(self._commit)

        # FIX: use-after-free em ~PublisherZMQ() (bug do simulador, distinto
        # do crash do Groot documentado na secao "Groot" do CLAUDE.md, mas no
        # mesmo caminho de codigo). PublisherZMQ::callback() agenda o envio
        # via std::async, mas o destrutor faz 'delete zmq_' no corpo antes de
        # o destrutor do membro send_future_ esperar essa tarefa -- a tarefa
        # pendente pode chamar flush()->zmq_->publisher.send(...) sobre
        # memoria ja liberada. A janela abre em todo treePublisher_.reset() do
        # modelo (reset()/copyData()/shutdownNotification()), ou seja, em
        # qualquer encerramento com MIXR_GROOT_MONITOR ligado. O patch espera
        # o envio pendente antes do delete (seguro: nenhum lock esta em maos
        # nesse ponto). Aplicado so' em source(), portanto so' existe em
        # pacotes construidos do fonte via scripts/deps.sh -- o caminho
        # corrente (README.md, INSTALL.md SS4, CI). Um pacote consumido pronto
        # de um remoto, ou em cache anterior a este patch, nao o recebe.
        replace_in_file(
            self, "src/loggers/bt_zmq_publisher.cpp",
            "    flush();\n"
            "    delete zmq_;",
            "    // POC-MIXR: espera o envio agendado por callback() antes de\n"
            "    // liberar o Pimpl -- ver deps/behaviortree/conanfile.py.\n"
            "    if (send_future_.valid())\n"
            "    {\n"
            "        send_future_.wait();\n"
            "    }\n"
            "    flush();\n"
            "    delete zmq_;",
        )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        # boost e cppzmq nao sao decorativos: o CMakeLists da 3.5.6 faz
        # find_package sobre os dois e muda o conteudo do pacote conforme
        # achar ou nao (sem boost, '-DBT_NO_COROUTINES' apaga
        # CoroActionNode; sem zmq, PublisherZMQ nao e compilado e
        # '-DZMQ_FOUND' nao vaza como definicao publica). Declara-las aqui
        # impede o pacote de variar conforme o que estiver instalado na
        # maquina de build.
        if self.options.with_coroutines:
            self.requires("boost/1.83.0")
        self.requires("cppzmq/4.10.0")
        # sqlite3 nao e referenciada em lugar nenhum do fonte da 3.5.6 (nem no
        # CMakeLists, nem em src/ ou include/) -- fica so porque e o que o
        # pacote hoje em cache declara; e seguro remover.
        self.requires("sqlite3/3.43.0")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["BUILD_EXAMPLES"] = False
        tc.variables["BUILD_UNIT_TESTS"] = False
        tc.variables["BUILD_TOOLS"] = self.options.with_tools
        tc.variables["BT_COROUTINES"] = self.options.with_coroutines
        # Mesmo motivo do bloco de requirements(): sem fixar isto o CMakeLists
        # cai no proprio default ('option(BUILD_WITH_CURSES ... ON)') e faz
        # find_package(Curses) -- numa maquina COM ncurses o pacote sai com
        # src/controls/manual_node.cpp dentro e '-DNCURSES_FOUND', sem ela nao.
        tc.variables["BUILD_WITH_CURSES"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.verbose = True
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        libname = "behaviortree_cpp_v3"

        self.cpp_info.set_property("cmake_file_name", "BehaviorTree")
        self.cpp_info.set_property("cmake_target_name", f"BT::{libname}")

        postfix = "d" if self.settings.os == "Windows" and self.settings.build_type == "Debug" else ""
        self.cpp_info.components[libname].libs = [f"{libname}{postfix}"]
        self.cpp_info.components[libname].requires = ["cppzmq::cppzmq", "sqlite3::sqlite3"]

        if self.options.with_coroutines:
            self.cpp_info.components[libname].requires.append("boost::coroutine")

        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.components[libname].system_libs.append("pthread")

        self.cpp_info.components[libname].names["cmake_find_package"] = libname
        self.cpp_info.components[libname].names["cmake_find_package_multi"] = libname
        self.cpp_info.components[libname].set_property("cmake_target_name", f"BT::{libname}")
