# Editor/monitor visual do BehaviorTree.CPP v3 (github.com/BehaviorTree/Groot).
# E' um app Qt separado, nao parte do fonte de deps/behaviortree/ -- ver o
# comentario no topo de deps/behaviortree/conanfile.py.
#
# Pre-requisito de SISTEMA (nao e' requires() do Conan -- buildar Qt5 do fonte
# levaria horas e nao ha precedente disso aqui): em Ubuntu/Debian,
#   sudo apt install qtbase5-dev libqt5svg5-dev libzmq3-dev libdw-dev
# mais CMake >= 3.2.
import os
import shutil

from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.files import replace_in_file
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout


class Recipe(ConanFile):
    name = "groot"
    version = "1.0.0"

    # E' um binario para RODAR, nao uma lib para linkar -- ver package_info().
    package_type = "application"

    license = "BSD-3-Clause"
    url = "https://github.com/BehaviorTree/Groot"
    description = "Editor e monitor visual para BehaviorTree.CPP v3."

    settings = "os", "compiler", "build_type", "arch"

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/BehaviorTree/Groot", target=".")
        git.checkout("1.0.0")

        # FIX 1: QtNodeEditor (submodulo desta tag) declara a propria
        # especializacao de std::hash<QString> -- Qt >= 5.14 (Ubuntu 24.04
        # traz 5.15.13) ja fornece a MESMA especializacao em
        # QtCore/qhashfunctions.h, sem guarda de versao nenhuma la, entao as
        # duas colidem com "redefinition of struct std::hash<QString>".
        # Groot 1.0.0 e' de ~2020, anterior a essa mudanca do Qt; restringe a
        # especializacao propria so' a Qt < 5.14, onde ela ainda faz falta.
        # Confirmado rodando: sem isto, o build morre em ~1% (QtNodeEditor).
        qstring_hash = os.path.join(
            "QtNodeEditor", "include", "nodes", "internal", "QStringStdHash.hpp")
        replace_in_file(
            self, qstring_hash,
            "namespace std\n{\ntemplate<>\nstruct hash<QString>\n{\n"
            "  inline std::size_t\n  operator()(QString const &s) const\n"
            "  {\n    return qHash(s);\n  }\n};\n}",
            "#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)\n"
            "namespace std\n{\ntemplate<>\nstruct hash<QString>\n{\n"
            "  inline std::size_t\n  operator()(QString const &s) const\n"
            "  {\n    return qHash(s);\n  }\n};\n}\n#endif",
        )

        # FIX 2: o .gitmodules desta tag aponta o submodulo depend/BehaviorTree.CPP
        # pra uma branch que nao existe mais no remoto oficial (`ver_3`,
        # renomeada para `v3.8` faz tempo) -- git resolve entao para o ULTIMO
        # commit gravado no arvore de git do Groot (73e10fb8, "3.8.0-1-g...")
        # em vez de acompanhar a branch de verdade, e esse commit tem DUAS
        # quebras de API reais contra o proprio codigo do Groot (nao e' so o
        # layout de include -- ver FIX 3): 'behaviortree_cpp_v3/' continua
        # sendo o nome do diretorio (o codigo do Groot usa 'behaviortree_cpp/'
        # sem sufixo) e 'BT::VerifyXML()' ja mudou de assinatura
        # ('std::unordered_map<string,NodeType>' em vez do
        # 'std::set<string>' que XML_utilities.cpp ainda espera).
        #
        # Em vez de perseguir um commit de BT.CPP que bata com tudo que o
        # Groot 1.0.0 espera (o proprio submodulo fica claramente sem manter
        # havia anos), usa-se aqui a MESMA v3.5.6 (fork ASA) que
        # deps/behaviortree/conanfile.py ja clona para o resto deste
        # repositorio -- e ela bate com a API que o Groot 1.0.0 quer (mesma
        # assinatura de VerifyXML, testado). Bonus: o Groot construido fica
        # falando o mesmo dialeto de XML que o host deste projeto de fato usa.
        # ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): fixava a TAG
        # 'v3.5.6' -- mesma razao do mesmo fix em
        # deps/behaviortree/conanfile.py: fork mantido pela PROPRIA equipe
        # (ASA-Simulation), tag mais suscetivel a mover por engano do que a
        # de projeto de terceiro estabelecido. MESMO commit usado la
        # (confirmado apontar pro mesmo lugar que 'v3.5.6' hoje).
        shutil.rmtree(os.path.join("depend", "BehaviorTree.CPP"), ignore_errors=True)
        bt_git = Git(self, folder=os.path.join("depend", "BehaviorTree.CPP"))
        bt_git.clone(url="https://github.com/ASA-Simulation/BehaviorTree.CPP", target=".")
        bt_git.checkout("abd9d75aa2f7f2fa21cab60dbd20cfc7503e4c43")

        # v3.5.6 tambem usa 'behaviortree_cpp_v3/' como nome de diretorio
        # (nao mudou disso pra ca) -- o mesmo symlink de compatibilidade da
        # FIX 2 continua necessario.
        bt_include = os.path.join("depend", "BehaviorTree.CPP", "include")
        alias = os.path.join(bt_include, "behaviortree_cpp")
        if not os.path.exists(alias):
            os.symlink("behaviortree_cpp_v3", alias)

        # FIX 3: 'BT::DecoratorSubtreeNode' e' um nome de classe de uma fase
        # bem mais antiga da BehaviorTree.CPP (anterior a remocao geral do
        # prefixo 'Decorator' das subclasses) -- nem a v3.5.6 mais recem
        # clonada acima tem essa classe, so' 'BT::SubtreeNode' (mesmo papel,
        # so' o nome mudou). Unico call site no Groot inteiro
        # (bt_editor_base.cpp) fora do submodulo.
        replace_in_file(
            self, os.path.join("bt_editor", "bt_editor_base.cpp"),
            "BT::DecoratorSubtreeNode", "BT::SubtreeNode",
        )

        # FIX 4: mesma raiz da FIX 2 (nome do diretorio de include mudou,
        # nome do TARGET cmake nao) -- so' que agora no lado do LINKER.
        # 'CMakeLists.txt'/'test/CMakeLists.txt' do proprio Groot linkam
        # contra o alvo cmake 'behaviortree_cpp' (sem sufixo), mas o
        # 'add_subdirectory(depend/BehaviorTree.CPP)' acima produz um alvo
        # chamado 'behaviortree_cpp_v3' (o nome do project() dela, inalterado
        # entre 3.5.6 e a HEAD do submodulo original) -- sem isto, o
        # 'Groot'/'editor_test'/'replay_test' falham no ultimo passo (link)
        # com "cannot find -lbehaviortree_cpp". Um ALIAS resolve os DOIS
        # call sites de uma vez, sem precisar editar cada um.
        replace_in_file(
            self, "CMakeLists.txt",
            "add_subdirectory( depend/BehaviorTree.CPP )",
            "add_subdirectory( depend/BehaviorTree.CPP )\n"
            "    add_library(behaviortree_cpp ALIAS behaviortree_cpp_v3)",
        )

        # FIX 5: 'behavior_tree_editor' (a .so intermediaria entre QtNodeEditor
        # e o executavel 'Groot' -- ver 'add_library(behavior_tree_editor
        # SHARED ...)' logo depois do bloco de include_directories acima) NAO
        # TEM 'install()' NENHUM neste CMakeLists.txt -- so' 'Groot' e' listado
        # em INSTALL(TARGETS ...). E' um buraco real do proprio empacotamento
        # do Groot 1.0.0 (GROOT_LIB_DESTINATION e' definida e nunca usada, ver
        # linhas 132-138), nao algo desta receita ter introduzido. Sem isto o
        # binario empacotado falha ao ABRIR, mesmo com todo o resto certo,
        # com "libbehavior_tree_editor.so: cannot open shared object file".
        replace_in_file(
            self, "CMakeLists.txt",
            "INSTALL(TARGETS Groot RUNTIME DESTINATION ${GROOT_BIN_DESTINATION} )",
            "INSTALL(TARGETS Groot RUNTIME DESTINATION ${GROOT_BIN_DESTINATION} )\n"
            "INSTALL(TARGETS behavior_tree_editor LIBRARY DESTINATION ${GROOT_LIB_DESTINATION} )",
        )

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        # Sem isto, 'bin/Groot' empacotado nao acha 'lib/libbehaviortree_cpp_v3.so'
        # nem 'lib/libbehavior_tree_editor.so' em runtime -- Groot 1.0.0 nunca
        # setou RPATH nenhum (roda sempre de dentro da propria build tree,
        # onde os .so ja estao ao lado). Mesma classe de gotcha ja documentada
        # em CLAUDE.md para o host deste projeto (rpath dist/ vs build/).
        tc.variables["CMAKE_INSTALL_RPATH"] = "$ORIGIN/../lib"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()

    def package_info(self):
        self.cpp_info.bindirs = ["bin"]
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
