# Editor/monitor visual do BehaviorTree.CPP v3 (github.com/BehaviorTree/Groot).
# E' um app Qt separado, nao parte do fonte de deps/behaviortree/ -- ver o
# comentario no topo de deps/behaviortree/conanfile.py.
#
# Pre-requisito de SISTEMA (nao e' requires() do Conan -- buildar Qt5 do fonte
# levaria horas e nao ha precedente disso aqui): em Ubuntu/Debian,
#   sudo apt install qtbase5-dev libqt5svg5-dev libzmq3-dev cppzmq-dev libdw-dev
# mais CMake >= 3.2. ('cppzmq-dev' e' o header C++ 'zmq.hpp', que ate o
# Ubuntu 22.04 vinha DENTRO do 'libzmq3-dev' e no 24.04 virou pacote a
# parte -- sem ele o build quebra em 'sidepanel_monitor.cpp'; INSTALL.md SS5.)
import os
import shutil

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.scm import Git
from conan.tools.files import load, replace_in_file
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

        # FIX 6: o modo MONITOR do Groot FECHA SOZINHO, sem dialogo e sem
        # mensagem, poucos milissegundos depois de conectar. Causa raiz, lida no
        # fonte dos dois lados (ver CLAUDE.md, secao "Groot", armadilha no 3):
        #
        #   - 'PublisherZMQ::createStatusBuffer()' publica, 3 bytes por no, o
        #     UID do TreeNode (bt_zmq_publisher.cpp: WriteScalar(..., node->UID())).
        #   - 'SidepanelMonitor::on_timer()' le esse campo e o passa DIRETO como
        #     INDICE para '_loaded_tree.node(index)' -- que e' '&_nodes.at(index)'
        #     sobre um std::deque (bt_editor_base.h) e portanto LANCA
        #     std::out_of_range. O mapa certo (_uid_to_index) existe, e' montado
        #     corretamente no Connect e ate' usado duas linhas abaixo, no laco de
        #     TRANSICOES -- so' o laco de STATUS o ignora.
        #   - o unico 'catch' em escopo e' 'catch(zmq::error_t&)', que nao pega
        #     std::out_of_range. Como 'on_timer' e' slot de um QTimer de 20 ms e o
        #     main.cpp do Groot e' um 'return app.exec();' puro (sem try/catch,
        #     sem override de QApplication::notify(), sem set_terminate), a
        #     excecao escapa do laco de eventos -> std::terminate() -> SIGABRT.
        #
        # Por que isso e' intermitente ("por vezes"): o contador de UID do BT.CPP
        # e' um 'static uint16_t uid = 1' que NUNCA zera (tree_node.cpp), com
        # escopo por .so de plugin (medido: 'nm -C libflight.so' mostra
        # 'BT::getUID()::uid' como simbolo LOCAL). Logo, so' a PRIMEIRA arvore
        # construida naquele .so tem UIDs 1..N -- que e' o unico caso em que a
        # confusao UID/indice passa despercebida. Com 8 aeronaves construindo a
        # arvore preguicosamente, em paralelo, sob 'g_treeBuildMutex', quem fica
        # em primeiro e' corrida de thread.
        #
        # A prova de que e' descuido pontual, e nao invariante de desenho: o
        # MESMO campo do fio e' lido corretamente em sidepanel_replay.cpp, via
        # 'uid_to_index.at(uid)'.
        #
        # O marcador 'POC-MIXR-FIX6' aparece nas mensagens de qDebug de 6c/6d --
        # ou seja, sobrevive como literal no binario. E' isso que
        # scripts/find_groot.sh usa para avisar quando o Groot em cache e'
        # anterior a esta correcao (um pacote velho reintroduz o bug em
        # silencio). Todos os replace_in_file abaixo sao strict (default): se a
        # tag 1.0.0 mudar, o build FALHA em vez de aplicar meio patch.
        monitor = os.path.join("bt_editor", "sidepanel_monitor.cpp")

        # 6a -- o laco de STATUS, a causa direta.
        replace_in_file(
            self, monitor,
            "                uint16_t index = flatbuffers::ReadScalar<uint16_t>(&buffer[offset]);\n"
            "                AbstractTreeNode* node = _loaded_tree.node( index );",
            "                // POC-MIXR-FIX6: o campo do fio e' o UID do TreeNode, NAO o\n"
            "                // indice no deque de nos do Groot. UID desconhecido e' PULADO.\n"
            "                const uint16_t uid_st = flatbuffers::ReadScalar<uint16_t>(&buffer[offset]);\n"
            "                const auto it_st = _uid_to_index.find( uid_st );\n"
            "                if( it_st == _uid_to_index.end() ) continue;\n"
            "                const int index = it_st->second;\n"
            "                if( index < 0 || static_cast<size_t>(index) >= _loaded_tree.nodesCount() ) continue;\n"
            "                AbstractTreeNode* node = _loaded_tree.node( index );",
        )

        # 6b -- o laco de TRANSICOES: ja usa o mapa, mas com '.at()', que lanca o
        # MESMO std::out_of_range quando o publicador reconstroi a arvore com o
        # Groot ja conectado (reset()/copyData()/shutdown, ou o execv de
        # "reiniciar" do ./app).
        replace_in_file(
            self, monitor,
            "                const uint16_t index = _uid_to_index.at(uid);",
            "                // POC-MIXR-FIX6: UID desconhecido e' PULADO, nunca '.at()'.\n"
            "                const auto it_tr = _uid_to_index.find( uid );\n"
            "                if( it_tr == _uid_to_index.end() ) continue;\n"
            "                const uint16_t index = static_cast<uint16_t>(it_tr->second);\n"
            "                if( static_cast<size_t>(index) >= _loaded_tree.nodesCount() ) continue;",
        )

        # 6c -- rede de seguranca no laco de status. O catch novo vem DEPOIS do
        # 'zmq::error_t&' (nunca antes: error_t deriva de std::exception, e um
        # catch mais generico primeiro engoliria o especifico).
        replace_in_file(
            self, monitor,
            "    catch( zmq::error_t& err)\n"
            "    {\n"
            "        qDebug() << \"ZMQ receive failed: \" << err.what();\n"
            "    }",
            "    catch( zmq::error_t& err)\n"
            "    {\n"
            "        qDebug() << \"ZMQ receive failed: \" << err.what();\n"
            "    }\n"
            "    catch( const std::exception& err)\n"
            "    {\n"
            "        qDebug() << \"POC-MIXR-FIX6: excecao ignorada no laco de status: \" << err.what();\n"
            "    }",
        )

        # 6d -- o mesmo no caminho de CONNECT: 'getTreeFromServer()' chama
        # 'models.at(registration_ID)' (utils.cpp) dentro de um try que tambem so'
        # pega zmq::error_t. O 'return false' faz on_Connect() cair no QMessageBox
        # "Was not able to connect" -- a GUI AVISA em vez de sumir.
        replace_in_file(
            self, monitor,
            "    catch( zmq::error_t& err)\n"
            "    {\n"
            "        qDebug() << \"ZMQ client receive failed: \" << err.what();\n"
            "        return false;\n"
            "    }",
            "    catch( zmq::error_t& err)\n"
            "    {\n"
            "        qDebug() << \"ZMQ client receive failed: \" << err.what();\n"
            "        return false;\n"
            "    }\n"
            "    catch( const std::exception& err)\n"
            "    {\n"
            "        qDebug() << \"POC-MIXR-FIX6: falha ao montar a arvore recebida: \" << err.what();\n"
            "        return false;\n"
            "    }",
        )

        # 6e -- pos-condicao. Os replace_in_file acima ja sao strict, mas isto
        # cobre o caso oposto: um upstream que mude o TEXTO sem mudar o defeito
        # (ex.: um '.at()' novo em outro ponto do mesmo arquivo).
        patched = load(self, monitor)
        if "_uid_to_index.at(" in patched:
            raise ConanException(
                "FIX 6: ainda ha '_uid_to_index.at(' em sidepanel_monitor.cpp -- "
                "um '.at()' nao mapeado mata a janela do Groot no primeiro UID "
                "desconhecido. Ver deps/groot/conanfile.py.")
        if patched.count("POC-MIXR-FIX6") < 4:
            raise ConanException(
                "FIX 6: marcadores POC-MIXR-FIX6 de menos em sidepanel_monitor.cpp "
                "-- algum dos quatro passos nao aplicou. Ver deps/groot/conanfile.py.")

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
