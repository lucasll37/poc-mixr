from conan import ConanFile
from conan.tools.scm import Git
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
    # os simbolos dela com '-Wl,--exclude-libs,ALL' (ver models/A4/
    # meson.build); o host nunca a linka, pra nao duplicar o contador estatico
    # de BT::getUID() entre host e plugin. O conanfile.py da raiz repete este
    # override do lado do consumidor.
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tools": False,
        "with_coroutines": True,
    }

    generators = "CMakeDeps"

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/ASA-Simulation/BehaviorTree.CPP", target=".")
        git.checkout("v3.5.6")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        # boost e cppzmq NAO sao decorativos: o CMakeLists da 3.5.6 faz
        # find_package sobre os dois e MUDA o conteudo do pacote conforme achar
        # ou nao (com boost, '-DBT_BOOST_COROUTINE2' e sem ele
        # '-DBT_NO_COROUTINES', que apaga a declaracao de CoroActionNode; com
        # zmq, o PublisherZMQ e compilado e '-DZMQ_FOUND' vaza como definicao
        # PUBLICA para o consumidor). Declarar as duas aqui e o que impede o
        # pacote de mudar conforme o que estiver instalado na maquina --
        # confirmado: a .a em cache tem simbolos de PublisherZMQ.
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
