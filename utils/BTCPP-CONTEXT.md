---
titulo: "BehaviorTree.CPP — Base de Conhecimento Técnica (contexto para RAG)"
projeto: "BehaviorTree.CPP — biblioteca C++ para construir e executar árvores de comportamento"
upstream: "github.com/BehaviorTree/BehaviorTree.CPP"
fork_documentado: "empacotamento Conan 2 interno, pacote behaviortree.cpp.asa/3.5.6"
versao: "3.5.6 (CHANGELOG.rst, 2021-02-03); projeto CMake behaviortree_cpp_v3"
namespace: "BT"
biblioteca_gerada: "libbehaviortree_cpp_v3"
diretorio_de_cabecalhos: "behaviortree_cpp_v3/"
padrao_cpp: "C++14 (CMAKE_CXX_STANDARD 14, CMAKE_CXX_STANDARD_REQUIRED ON)"
licenca: "MIT (mais licenças de terceiros embutidos: Boost/BSL, zlib, Apache-2.0)"
fontes: "Manual técnico BehaviorTree.CPP (report LaTeX, 13 capítulos) + árvore de fontes C++ da versão 3.5.6 + os 12 exemplos de examples/"
idioma: "português do Brasil; identificadores, IDs de registro e nomes de porta em inglês (originais)"
convencao_de_verdade: "toda afirmação foi conferida contra o código-fonte da v3.5.6; divergências entre comentário de cabeçalho e código são registradas explicitamente"
aviso_de_versao: "NADA neste documento vale para a BehaviorTree.CPP v4, que tem API e XML incompatíveis"
---

# COMO USAR ESTE DOCUMENTO

Este é um documento de contexto técnico sobre a **BehaviorTree.CPP versão 3.5.6**,
destinado a alimentar sistemas de recuperação (RAG) que respondem perguntas sobre a
biblioteca. Cada seção é autocontida: repete o nome da classe, do arquivo e do mecanismo
de que trata, de modo que um trecho recuperado isoladamente continue interpretável fora
do seu contexto original.

Convenções deste documento:

- **REGRA** — norma da biblioteca: algo que o desenvolvedor precisa fazer (ou não fazer)
  para que o mecanismo funcione.
- **ARMADILHA** — comportamento real do código que contraria a expectativa razoável:
  valor padrão surpreendente, nome que não é o que parece, comentário de cabeçalho
  desatualizado, funcionalidade existente pela metade, defeito do fonte. São resultado da
  conferência do texto contra o código, e esta biblioteca tem um número incomum delas.
- **POR QUÊ** — motivação de uma decisão de projeto.
- Caminhos como `src/controls/sequence_node.cpp` referem-se à árvore de fontes da
  BehaviorTree.CPP 3.5.6.
- Blocos de código marcados com caminho de arquivo são transcrição do fonte
  (eventualmente condensada, com omissões marcadas por `// ...`), nunca reescrita.
- Blocos sem caminho são ilustrativos: mostram como usar um mecanismo, não como ele está
  escrito no fonte.

**Aviso de versão, repetido porque é a maior fonte de resposta errada:** existe uma
versão 4 da biblioteca, com mudanças incompatíveis de API e de XML. `NodeConfiguration`
virou `NodeConfig`; `SubTreePlus` virou o `SubTree` padrão; `tickRoot()` deu lugar a
`tickOnce()`/`tickWhileRunning()`; o tratamento de `IDLE` entre *ticks* mudou; os IDs de
alguns nós mudaram. Praticamente todo material encontrado na Internet hoje pressupõe a
v4. **Este documento descreve exclusivamente a v3.5.6.**

---

# SUMÁRIO

Os números de seção são estáveis e usados nas remissões internas do documento
("ver 12.6", "Ver seção 21.8").

- **1. IDENTIDADE DO PROJETO**
  - 1.1 O que é a BehaviorTree.CPP
  - 1.2 Por que árvores de comportamento e não máquinas de estados
  - 1.3 Público-alvo e escopo
  - 1.4 Características do empacotamento documentado
  - 1.5 Números de referência da versão 3.5.6
  - 1.6 Autoria e licença
- **2. PRINCÍPIOS DE ARQUITETURA**
  - 2.1 Composição dirigida por dados
  - 2.2 Tudo é um TreeNode
  - 2.3 Um único protocolo de retorno
  - 2.4 Execução sem threads por padrão
- **3. MAPA DO REPOSITÓRIO E ORGANIZAÇÃO DO CÓDIGO**
  - 3.1 Camadas do código-fonte
  - 3.2 Árvore de diretórios comentada
  - 3.3 O que a biblioteca deliberadamente NÃO oferece
- **4. O MODELO DE EXECUÇÃO: O TICK E O NodeStatus**
  - 4.1 O que é um tick
  - 4.2 Os dois enumerados fundamentais
  - 4.3 A leitura correta dos quatro valores: resultado versus estado
  - 4.4 Conversão de e para texto
  - 4.5 A propagação de um tick
  - 4.6 O RUNNING e a duração
  - 4.7 Memória e reatividade
  - 4.8 O ciclo de vida de um nó
  - 4.9 O que é halt()
  - 4.10 Os quatro passos de toda aplicação
- **5. BT::TreeNode — A CLASSE BASE DE TODOS OS NÓS**
  - 5.1 Interface pública essencial
  - 5.2 tick() versus executeTick()
  - 5.3 name() versus registrationName()
  - 5.4 status() versus isHalted()
  - 5.5 setStatus(): estado, notificação e concorrência
  - 5.6 O Signal (padrão observador)
  - 5.7 O identificador único (UID)
  - 5.8 NodeConfiguration e TreeNodeManifest
  - 5.9 getRawPortValue()
- **6. AS TRÊS ARIDADES: LeafNode, ControlNode, DecoratorNode**
  - 6.1 LeafNode — nenhum filho
  - 6.2 ControlNode — N filhos
  - 6.3 DecoratorNode — exatamente um filho
  - 6.4 SimpleDecoratorNode
  - 6.5 O tipo de um nó em tempo de compilação: getType<T>()
  - 6.6 Percorrer a árvore: as três funções livres
  - 6.7 Erros e exceções
- **7. PORTAS (PORTS)**
  - 7.1 Declarando portas: providedPorts()
  - 7.2 PortInfo e PortDirection
  - 7.3 As funções de construção de porta
  - 7.4 A detecção por SFINAE
  - 7.5 Lendo e escrevendo dentro do tick()
  - 7.6 Optional<T> e Result
  - 7.7 A resolução de um nome de porta (mecanismo central)
  - 7.8 setOutput()
  - 7.9 Valores padrão
  - 7.10 Portas definidas em tempo de execução
- **8. O BLACKBOARD**
  - 8.1 Estado interno
  - 8.2 API pública completa
  - 8.3 get() e set() fora da árvore
  - 8.4 Encadeamento entre pai e filho
  - 8.5 O travamento de tipo
  - 8.6 Inspeção: debugMessage() e getKeys()
  - 8.7 Como uma entrada do blackboard ganha tipo
- **9. convertFromString E TIPOS PRÓPRIOS**
  - 9.1 O caso geral
  - 9.2 As especializações fornecidas
  - 9.3 splitString
  - 9.4 Especializando para um tipo próprio
  - 9.5 toStr() — o caminho inverso
- **10. NÓS DE CONTROLE**
  - 10.1 Visão de conjunto
  - 10.2 Sequence — o E lógico
  - 10.3 SequenceStar — o E que não esquece
  - 10.4 ReactiveSequence — o E que reavalia tudo
  - 10.5 Fallback — o OU lógico
  - 10.6 ReactiveFallback — o OU que reavalia
  - 10.7 Os nomes da v2 significam o oposto na v3
  - 10.8 Parallel — $M$ de $N$
  - 10.9 IfThenElse
  - 10.10 WhileDoElse
  - 10.11 Switch2 … Switch6
  - 10.12 ManualSelector
  - 10.13 Escolhendo o nó de controle
- **11. DECORADORES**
  - 11.1 Visão de conjunto
  - 11.2 Inverter — o NÃO lógico
  - 11.3 ForceSuccess e ForceFailure
  - 11.4 KeepRunningUntilFailure
  - 11.5 Repeat
  - 11.6 RetryUntilSuccesful
  - 11.7 Timeout
  - 11.8 Delay
  - 11.9 O custo de Timeout e Delay: uma thread cada
  - 11.10 BlackboardCheckInt / BlackboardCheckDouble / BlackboardCheckString
  - 11.11 SubTree e SubTreePlus
  - 11.12 Escrevendo um decorador próprio
- **12. NÓS DE AÇÃO E DE CONDIÇÃO**
  - 12.1 Visão de conjunto
  - 12.2 A hierarquia de ação
  - 12.3 ConditionNode — perguntas, não comandos
  - 12.4 SyncActionNode — a ação que termina no tick
  - 12.5 SimpleActionNode
  - 12.6 StatefulActionNode — pedido e resposta
  - 12.7 AsyncActionNode — uma thread por execução
  - 12.8 CoroActionNode — suspender em vez de bloquear
  - 12.9 Ações auxiliares embutidas
  - 12.10 Argumentos que não são portas
- **13. O FORMATO XML**
  - 13.1 Estrutura mínima
  - 13.2 Forma compacta e forma explícita
  - 13.3 A validação: VerifyXML()
  - 13.4 O modelo para o Groot: <TreeNodesModel>
  - 13.5 <include>: dividir em arquivos
  - 13.6 Comentários e entidades XML
  - 13.7 Onde escrever o XML: arquivo ou string
- **14. BehaviorTreeFactory — REFERÊNCIA**
  - 14.1 Estado interno e tipos
  - 14.2 API pública completa
  - 14.3 O construtor: os nós embutidos
  - 14.4 registerNodeType<T>() e os cinco static_assert
  - 14.5 CreateBuilder<T>() e o construtor escolhido
  - 14.6 Registro e desregistro
  - 14.7 instantiateTreeNode()
  - 14.8 Os registros "simples"
  - 14.9 Consultando a fábrica
- **15. O PARSER: DA CARGA À ÁRVORE**
  - 15.1 A interface Parser e o XMLParser
  - 15.2 O Pimpl do parser
  - 15.3 As seis etapas da carga
  - 15.4 createNodeFromXML() — os quatro passos
  - 15.5 recursivelyCreateTree()
  - 15.6 Coleta das árvores e nomes gerados
- **16. SUBÁRVORES**
  - 16.1 <SubTree>: isolamento com remapeamento
  - 16.2 <SubTreePlus>: três formas de ligar
  - 16.3 A pilha de blackboards
  - 16.4 Quando usar cada forma
- **17. PLUGINS**
  - 17.1 A macro e o símbolo
  - 17.2 Do lado do código
  - 17.3 Do lado do build
  - 17.4 Do lado da aplicação
  - 17.5 Plugins ROS
- **18. O OBJETO Tree**
  - 18.1 Usos comuns de Tree
- **19. LOGGERS E OBSERVABILIDADE**
  - 19.1 StatusChangeLogger: a base comum
  - 19.2 StdCoutLogger — o traço no terminal
  - 19.3 FileLogger — o traço binário
  - 19.4 A serialização das transições
  - 19.5 A serialização da árvore
  - 19.6 MinitraceLogger — a linha do tempo
  - 19.7 PublisherZMQ e o Groot
  - 19.8 Groot
  - 19.9 As ferramentas de linha de comando
  - 19.10 Depuração sem logger
  - 19.11 Estratégia de depuração por sintoma
- **20. A INFRAESTRUTURA DE SUPORTE (utils/)**
  - 20.1 BT::Any — o valor de tipo apagado
  - 20.2 convertNumber() — conversões numéricas verificadas
  - 20.3 SimpleString — small object optimization
  - 20.4 TimerQueue — temporizadores
  - 20.5 SharedLibrary — carga dinâmica
  - 20.6 StrCat e demangle
  - 20.7 Código de terceiros embutido
- **21. SISTEMA DE COMPILAÇÃO**
  - 21.1 O topo do CMakeLists.txt
  - 21.2 As opções
  - 21.3 As quatro sondagens que mudam o binário
  - 21.4 A lista de fontes
  - 21.5 Plataforma e tipo de biblioteca
  - 21.6 Avisos, ligação e inclusão
  - 21.7 Instalação e exportação
  - 21.8 ROS 1 e ROS 2
  - 21.9 O empacotamento Conan 2 (o fork)
  - 21.10 O Makefile de conveniência do fork
  - 21.11 Consumindo o pacote
  - 21.12 Compilando o repositório diretamente
  - 21.13 Resumo das dependências
  - 21.14 Os testes
- **22. OS DOZE TUTORIAIS DO REPOSITÓRIO**
  - 22.1 t01_build_your_first_tree — a primeira árvore
  - 22.2 t02_basic_ports — portas de entrada e saída
  - 22.3 t03_generic_ports — tipos próprios
  - 22.4 t04_reactive_sequence — reatividade e ação assíncrona
  - 22.5 t05_crossdoor — subárvores, decoradores e loggers
  - 22.6 t06_subtree_port_remapping — a fronteira do blackboard
  - 22.7 t07_wrap_legacy — embrulhar código existente
  - 22.8 t08_additional_node_args — argumentos que não são portas
  - 22.9 t09_async_actions_coroutines — co-rotinas
  - 22.10 t10_include_trees — carregar de arquivo
  - 22.11 t11_runtime_ports — portas em tempo de execução
  - 22.12 t12_ncurses_manual_selector — o operador no laço
  - 22.13 broken_sequence.cpp
  - 22.14 O que os exemplos ensinam (e o que não ensinam)
- **23. RECEITAS E PADRÕES DE PROJETO**
  - 23.1 O esqueleto mínimo de uma aplicação
  - 23.2 Laço com frequência fixa
  - 23.3 Laço que reinicia a árvore ao final
  - 23.4 Encerrar de fora (sinal, botão, temporizador)
  - 23.5 Guarda contínua (o padrão mais útil de todos)
  - 23.6 Prioridade com alternativa (fallback)
  - 23.7 Tentativas com desistência
  - 23.8 Prazo (timeout) que realmente interrompe
  - 23.9 Passo opcional
  - 23.10 Executar até que algo dê errado
  - 23.11 Sequência que não desfaz o trabalho feito
  - 23.12 Máquina de estados dentro da árvore
  - 23.13 Condição explícita, com "senão"
  - 23.14 Várias coisas ao mesmo tempo
  - 23.15 Subárvore reutilizável com interface explícita
  - 23.16 Injetar o "mundo" nos nós
  - 23.17 Duas árvores no mesmo processo
  - 23.18 Trocar de árvore em tempo de execução
  - 23.19 Testar um nó isoladamente, sem XML
  - 23.20 Montar uma árvore em C++, sem XML
  - 23.21 Ler o resultado da árvore no C++
  - 23.22 Escrever no blackboard antes de ticar
  - 23.23 Uma condição que consulta um serviço lento
  - 23.24 Contador / limitador de taxa
  - 23.25 Emitir log de dentro de um nó
  - 23.26 Descobrir, em tempo de execução, o que está registrado
  - 23.27 Gerar o <TreeNodesModel> para o Groot
  - 23.28 Reagir a uma mudança de estado sem logger
- **24. BOAS PRÁTICAS**
  - 24.1 Sobre a estrutura da árvore
  - 24.2 Sobre os nós
  - 24.3 Sobre dados
  - 24.4 Sobre a aplicação
  - 24.5 Sobre o build
  - 24.6 Sobre o XML
- **25. ÍNDICE DE MENSAGENS DE ERRO**
  - 25.1 Erros de carga do XML (RuntimeError)
  - 25.2 Erros da fábrica
  - 25.3 Erros de execução (durante o tick)
  - 25.4 Erros de dados (blackboard e conversão)
  - 25.5 Erros dos loggers
- **26. CATÁLOGO CONSOLIDADO DE ARMADILHAS**
  - 26.1 Nomes e grafias
  - 26.2 Comentários desatualizados
  - 26.3 Defeitos de código
  - 26.4 Comportamentos surpreendentes
  - 26.5 Concorrência e recursos
  - 26.6 Build e empacotamento
- **27. CATÁLOGO DOS NÓS EMBUTIDOS**
  - 27.1 Nós de controle
  - 27.2 Decoradores
  - 27.3 Subárvores
  - 27.4 Ações
  - 27.5 Resumo por tipo
  - 27.6 IDs que não são o nome da classe
  - 27.7 O programa que extrai o catálogo
- **28. MIGRAÇÃO E COMPATIBILIDADE**
  - 28.1 v2 → v3: a renomeação dos nós de controle
  - 28.2 v3 → v4: o que muda (para reconhecer material da v4)
  - 28.3 Compatibilidade de plataforma
- **29. APLICAÇÃO COMPLETA DE REFERÊNCIA**
  - 29.1 A árvore (XML)
  - 29.2 O tipo próprio e a sua conversão
  - 29.3 Uma ação assíncrona sem thread
  - 29.4 Nós que precisam do "mundo"
  - 29.5 Uma ação com entrada e saída
  - 29.6 O main()
  - 29.7 Compilando
  - 29.8 A saída: a árvore construída
  - 29.9 A saída: o traço de execução
  - 29.10 Sete coisas para reparar nesse traço
  - 29.11 Três variações, com o resultado observado
- **30. TESTANDO NÓS E ÁRVORES**
  - 30.1 Testar um nó isoladamente, sem XML
  - 30.2 Testar uma máquina de estados (StatefulActionNode)
  - 30.3 Testar uma árvore inteira
  - 30.4 Nós de mentira (mocks) para testes de árvore
  - 30.5 Testar o comportamento reativo
  - 30.6 Testes que a própria biblioteca traz
  - 30.7 Verificação estática do XML antes de rodar
- **31. PERGUNTAS FREQUENTES**
  - 31.1 Começando
  - 31.2 Nós e classes base
  - 31.3 Portas e dados
  - 31.4 Nós de controle
  - 31.5 Decoradores
  - 31.6 XML
  - 31.7 Subárvores
  - 31.8 Plugins
  - 31.9 Depuração
  - 31.10 Build e integração
  - 31.11 Desempenho e recursos
  - 31.12 Erros conceituais comuns
- **32. GLOSSÁRIO**
- **33. MAPA DE CONSULTA RÁPIDA**
  - 33.1 Cabeçalho por assunto
  - 33.2 Símbolos mais usados
  - 33.3 Fluxo de decisão para escrever um nó
  - 33.4 Comandos essenciais
- **34. RESUMO EXECUTIVO — OS DOZE PONTOS QUE MAIS IMPORTAM**

---

# 1. IDENTIDADE DO PROJETO

## 1.1 O que é a BehaviorTree.CPP

A **BehaviorTree.CPP** é uma **biblioteca em C++** para construir e executar *árvores de
comportamento* (*behavior trees*, BT) — a estrutura de decisão que substituiu a máquina
de estados finitos em boa parte da robótica moderna e dos jogos.

**Não é um planejador nem um controlador.** É a máquina que decide, a cada instante,
*qual* das ações que o desenvolvedor escreveu deve ser executada agora. O comportamento
concreto (mover o robô, ler um sensor, chamar um serviço) é sempre código do usuário.

A decisão de projeto que a distingue de uma biblioteca convencional, e que governa
praticamente todo o resto, é a **separação entre estrutura e comportamento**:

- o **comportamento** de cada folha da árvore é implementado em C++;
- a **estrutura** da árvore — quais nós existem, como se aninham, com que parâmetros e
  ligados a quais dados — é descrita em um arquivo **XML**.

Em tempo de carga, um *parser* lê o arquivo XML e constrói a árvore de objetos
correspondente. Consequência prática direta: **reconfigurar um comportamento não exige
recompilar a aplicação** — basta editar o arquivo XML.

Essa escolha só funciona porque três mecanismos do núcleo a sustentam, e os três
reaparecem em toda a biblioteca:

1. **Fábrica por nome** (`BT::BehaviorTreeFactory`) — permite criar um nó a partir de uma
   *string* de identificação lida do arquivo, o **ID de registro**.
2. **Sistema de portas** (*ports*) — declara estaticamente quais entradas e saídas cada
   nó tem, e conecta os atributos do XML às entradas do *blackboard* que os alimentam.
3. **O *tick* e o `NodeStatus`** — um único protocolo de execução, com quatro valores
   possíveis, ao qual absolutamente todo nó obedece, dos embutidos aos do usuário.

Toda aplicação da biblioteca é uma **árvore de objetos `BT::TreeNode`** cuja raiz é
devolvida dentro de um `BT::Tree` — a estrutura que detém a posse dos nós, a pilha de
*blackboards* e os manifestos. A topologia completa dessa árvore é declarada em um
arquivo XML; o *parser* a constrói em tempo de carga e a mantém viva enquanto o objeto
`Tree` existir.

## 1.2 Por que árvores de comportamento e não máquinas de estados

Uma máquina de estados finitos codifica a lógica nas **transições**: para $N$ estados há
até $N^2$ arestas, e acrescentar um estado obriga a rever as arestas que chegam nele. A
árvore de comportamento move a lógica para os **nós de controle** internos, e o resultado
é composicional: uma subárvore que funciona continua funcionando ao ser pendurada em
outro lugar, porque tudo o que ela promete ao pai é um dos três resultados de um *tick*.

É essa propriedade — e não o desenho em forma de árvore — que a torna escalável. Duas
consequências práticas:

- **Reuso real de subárvore.** Uma subárvore "abrir porta" pode ser usada em dez lugares
  diferentes, porque a sua interface com o mundo é apenas `SUCCESS`/`FAILURE`/`RUNNING`.
- **Edição sem regressão global.** Trocar um filho de um `Fallback` não pode quebrar
  ramos que não foram tocados, porque não existem transições que apontem para dentro
  daquele ramo.

## 1.3 Público-alvo e escopo

Este material é referência para desenvolvedores que pretendem **usar** a
BehaviorTree.CPP para coordenar um sistema, ou **estender** a biblioteca com nós
próprios. Pressupõe familiaridade com C++ moderno. Não é tutorial de usuário final nem
introdução teórica a árvores de comportamento.

## 1.4 Características do empacotamento documentado

O *fork* documentado embrulha o `CMakeLists.txt` original numa receita **Conan 2**
(`behaviortree.cpp.asa/3.5.6`). Quatro características desse empacotamento decorrem
diretamente das opções escolhidas e afetam o planejamento de uma integração:

1. **As co-rotinas estão desligadas.** A receita desabilita a busca por Boost
   (`CMAKE_DISABLE_FIND_PACKAGE_Boost=True`), o que faz o `CMakeLists.txt` definir
   `BT_NO_COROUTINES`. **`BT::CoroActionNode` não existe no binário.**
2. **O `PublisherZMQ` não é compilado.** Sem ZeroMQ
   (`CMAKE_DISABLE_FIND_PACKAGE_ZMQ=True`), o *logger* que alimenta o Groot ao vivo fica
   de fora, assim como a ferramenta `bt3_recorder`.
3. **O `ManualSelectorNode` não é compilado.** Depende de *ncurses*, desabilitado com
   `BUILD_WITH_CURSES=False`. Um XML que use `<ManualSelector>` falha na carga com
   `"Node not recognized"`.
4. **O padrão de linguagem é C++14**, fixado em `CMAKE_CXX_STANDARD`. É essa fixação que
   obriga às implementações embutidas de `any`, `string_view` e `expected`.

A receita também não empacota exemplos, testes nem ferramentas
(`BUILD_EXAMPLES=False`, `BUILD_UNIT_TESTS=False`, `BUILD_TOOLS=False`).

## 1.5 Números de referência da versão 3.5.6

| Métrica | Valor |
|---|---|
| Nós registrados de fábrica | **29** (30 se *ncurses* estiver disponível) |
| Arquivos `.cpp` do projeto compilados na biblioteca | 29 (mais tinyxml2 e minitrace) |
| Nós de controle registrados | 13 IDs (`Sequence`, `SequenceStar`, `ReactiveSequence`, `Fallback`, `ReactiveFallback`, `Parallel`, `IfThenElse`, `WhileDoElse`, `Switch2`–`Switch6`) |
| Decoradores registrados (`NodeType::DECORATOR`) | 11 |
| Nós de subárvore (`NodeType::SUBTREE`) | 2 (`SubTree`, `SubTreePlus`) |
| Ações registradas | 3 (`AlwaysSuccess`, `AlwaysFailure`, `SetBlackboard`) |
| Condições registradas de fábrica | **0** — nenhuma condição vem pronta |
| Exemplos em `examples/` | 12 (`t01`–`t12`) mais `broken_sequence.cpp` |
| Ferramentas em `tools/` | 3 (`bt3_log_cat`, `bt3_recorder`, `bt3_plugin_manifest`) |
| Linhas de terceiros embutidos (4 maiores) | > 9 000 (tinyxml2 2 837, FlatBuffers 2 747, expected 1 957, string_view 1 532) |

## 1.6 Autoria e licença

- Copyright 2015–2018 Michele Colledanchise; 2018–2020 Davide Faconti, Eurecat.
- Licença **MIT** (arquivo `LICENSE` na raiz).
- Parte do projeto **MOOD2Be**, financiado pelo RobMoSys; o relatório final acompanha o
  repositório como `MOOD2Be_final_report.pdf`.
- **ARMADILHA (licenças):** a biblioteca é MIT, mas o pacote entregue contém código sob
  outras licenças — POCO (Boost Software License, em `utils/shared_library.h`), tinyxml2
  (zlib), FlatBuffers (Apache 2.0), *expected* e *string_view* (Boost), Minitrace (MIT).
  A receita Conan copia apenas o `LICENSE` da raiz para `licenses/`, o que subdocumenta a
  distribuição. Numa entrega com exigência de conformidade, os avisos das bibliotecas
  embutidas precisam acompanhar o binário.

---

# 2. PRINCÍPIOS DE ARQUITETURA

Quatro decisões arquiteturais permeiam a BehaviorTree.CPP. Mantê-las em mente torna todo
o resto previsível.

## 2.1 Composição dirigida por dados

Estrutura em XML, comportamento em C++. Favorece experimentação rápida e reaproveitamento
de subárvores, ao custo de um núcleo de *runtime* mais elaborado — o preço que a
biblioteca paga uma vez para que toda aplicação se beneficie.

O corolário operacional é que existem **dois momentos distintos** na vida de uma
aplicação:

- **tempo de carga** (*deployment time*): registrar nós, ler XML, validar, instanciar. É
  caro: lê arquivo, resolve tipos, aloca. Acontece uma vez.
- **tempo de execução** (*runtime*): *ticar* a árvore em laço. É barato: percorre
  ponteiros.

Quase toda verificação útil da biblioteca acontece no primeiro momento. Quase todo custo
de desempenho está no segundo. Confundir os dois é a origem de várias perguntas comuns
("por que meu nó só falha depois de dez minutos rodando?" — porque a validação daquele
ramo é feita no primeiro *tick* dele, não na carga).

## 2.2 Tudo é um `TreeNode`

Da raiz à menor condição, todos os nós compartilham a mesma maquinaria de execução,
estado e notificação de mudança. Essa uniformidade significa que:

- um mecanismo aprendido uma vez se aplica em toda a árvore;
- um *logger* escrito uma vez observa qualquer nó, inclusive os do usuário;
- o *parser* não precisa saber nada sobre nós específicos — só sobre o manifesto deles.

## 2.3 Um único protocolo de retorno

Todo *tick* devolve um `BT::NodeStatus`. É a **estreiteza** dessa interface que torna as
subárvores compostáveis: o pai não sabe nem precisa saber se o filho é uma condição
trivial ou uma subárvore de duzentos nós.

## 2.4 Execução sem *threads* por padrão

A árvore é percorrida na *thread* de quem chamou `tickRoot()`. Concorrência é opcional,
localizada e explícita — só aparece em três lugares:

- dentro de um `BT::AsyncActionNode` (uma *thread* por execução da ação);
- dentro de um `BT::TimerQueue` (uma *thread* por instância, usada por `Timeout` e
  `Delay`);
- dentro de um *logger* de rede (`BT::PublisherZMQ` mantém uma *thread* servidora).

**REGRA — o laço de execução é seu.** A biblioteca não tem *main loop*, relógio nem
agendador. Quem decide a frequência dos *ticks* é a aplicação. Nunca chame `tickRoot()`
em laço apertado sem pausa: a árvore não é autolimitada, e uma condição barata será
reavaliada milhões de vezes por segundo.

```cpp
// A forma canonica, presente em todos os exemplos do repositorio.
BT::NodeStatus status = BT::NodeStatus::RUNNING;
while (status == BT::NodeStatus::RUNNING)
{
    status = tree.tickRoot();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));   // nao remova
}
```

---

# 3. MAPA DO REPOSITÓRIO E ORGANIZAÇÃO DO CÓDIGO

## 3.1 Camadas do código-fonte

O repositório separa cabeçalhos públicos (`include/behaviortree_cpp_v3/`) de
implementação (`src/`), com dependência fluindo de cima para baixo:

```
┌───────────────────────────────────────────────────────────────────────┐
│ fábrica e parser  ·  loggers                                          │
│ bt_factory.h · xml_parsing.h · bt_parser.h · loggers/ · flatbuffers/   │
├───────────────────────────────────────────────────────────────────────┤
│ famílias de nós concretos                                             │
│ controls/ · decorators/ · actions/ · action_node.h · condition_node.h  │
├───────────────────────────────────────────────────────────────────────┤
│ núcleo                                                                │
│ tree_node.h · basic_types.h · blackboard.h · exceptions.h             │
├───────────────────────────────────────────────────────────────────────┤
│ utilitários genéricos e terceiros                                     │
│ utils/ (any, expected, string_view, shared_library) · 3rdparty/ ·     │
│ private/tinyxml2                                                      │
└───────────────────────────────────────────────────────────────────────┘
```

Nada em `utils/` conhece o conceito de árvore. É por isso que `BT::Any` e
`BT::SharedLibrary` podem ser usados isoladamente.

## 3.2 Árvore de diretórios comentada

```
BehaviorTree.CPP/
├── CMakeLists.txt          projeto behaviortree_cpp_v3; C++14; opções BUILD_*
├── conanfile.py            receita Conan 2 do fork (behaviortree.cpp.asa/3.5.6)
├── Makefile                atalhos do fork para os comandos do Conan
├── package.xml             manifesto ROS 1/ROS 2 (catkin / ament)
├── CHANGELOG.rst           histórico de versões; 3.5.6 é 2021-02-03
├── LICENSE                 MIT
├── MOOD2Be_final_report.pdf   relatório do projeto que financiou a v3
│
├── include/behaviortree_cpp_v3/
│   ├── basic_types.h       NodeType, NodeStatus, PortDirection, PortInfo,
│   │                       convertFromString, Optional/Result, PortsList
│   ├── tree_node.h         TreeNode, NodeConfiguration, TreeNodeManifest,
│   │                       getInput/setOutput, resolução de remapeamento
│   ├── leaf_node.h         LeafNode (0 filhos)
│   ├── control_node.h      ControlNode (N filhos), haltChild/haltChildren
│   ├── decorator_node.h    DecoratorNode (1 filho), SimpleDecoratorNode
│   ├── action_node.h       ActionNodeBase, SyncActionNode, SimpleActionNode,
│   │                       AsyncActionNode, StatefulActionNode, CoroActionNode
│   ├── condition_node.h    ConditionNode, SimpleConditionNode
│   ├── blackboard.h        Blackboard (get/set/getAny/remapeamento de subárvore)
│   ├── behavior_tree.h     inclui todos os nós; applyRecursiveVisitor,
│   │                       printTreeRecursively, getType<T>()
│   ├── bt_factory.h        BehaviorTreeFactory, Tree, NodeBuilder,
│   │                       BT_REGISTER_NODES, PLUGIN_SYMBOL
│   ├── bt_parser.h         interface abstrata Parser
│   ├── xml_parsing.h       XMLParser, VerifyXML, writeTreeNodesModelXML
│   ├── exceptions.h        BehaviorTreeException, LogicError, RuntimeError
│   ├── actions/            always_success, always_failure, set_blackboard
│   ├── controls/           fallback, sequence, sequence_star, reactive_*,
│   │                       parallel, switch, if_then_else, while_do_else, manual
│   ├── decorators/         inverter, repeat, retry, timeout, delay, force_*,
│   │                       keep_running_until_failure, blackboard_precondition,
│   │                       subtree_node, timer_queue
│   ├── loggers/            abstract_logger, bt_cout_logger, bt_file_logger,
│   │                       bt_minitrace_logger, bt_zmq_publisher
│   ├── flatbuffers/        FlatBuffers embutido + BT_logger_generated.h +
│   │                       bt_flatbuffer_helper.h (serialização dos loggers)
│   └── utils/              any.hpp, safe_any.hpp, convert_impl.hpp,
│                           simple_string.hpp, expected.hpp, string_view.hpp,
│                           strcat.hpp, demangle_util.h, shared_library.h,
│                           signal.h, platform.hpp, make_unique.hpp
│
├── src/                    implementação espelhando include/
│   ├── private/tinyxml2.*  tinyxml2 embutido, namespace BT_TinyXML2
│   ├── shared_library_UNIX.cpp / shared_library_WIN.cpp
│   └── example.cpp         NÃO entra na biblioteca
│
├── 3rdparty/
│   ├── minitrace/          trace do Chrome
│   └── filesystem/         filesystem::path (wjakob), usado pelo <include>
│
├── examples/               t01..t12 + test_files/*.xml
├── sample_nodes/           dummy_nodes, crossdoor_nodes, movebase_node
│                           (compilados como .a e como .so de plugin)
├── tests/                  gtest_*.cpp
├── tools/                  bt3_log_cat, bt3_recorder, bt3_plugin_manifest
├── docs/                   mkdocs: tutoriais, xml_format.md, MigrationGuide.md
└── cmake/FindZMQ.cmake
```

## 3.3 O que a biblioteca deliberadamente NÃO oferece

Vale registrar, para não procurar em vão:

- **Nenhum relógio ou agendador.** A frequência de *tick* é da aplicação.
- **Nenhum *pool* de *threads*.** Cada `AsyncActionNode` cria a sua; cada `TimerQueue`,
  a dela.
- **Nenhuma persistência de estado.** A árvore não sabe salvar nem restaurar em que ponto
  parou; o `FileLogger` grava um *traço*, não um *checkpoint*.
- **Nenhuma alocação controlada.** Não há *allocator* configurável; a construção da
  árvore aloca livremente — irrelevante na carga, relevante em tempo real rígido.
- **Nenhuma condição embutida.** Toda condição é código do usuário.
- **Nenhum mecanismo de prioridade dinâmica.** A prioridade é a ordem dos filhos no XML.

---

# 4. O MODELO DE EXECUÇÃO: O *TICK* E O `NodeStatus`

## 4.1 O que é um *tick*

Um ***tick*** é a única forma de executar uma árvore de comportamento. É uma chamada de
`TreeNode::executeTick()` propagada da raiz para baixo, segundo a lógica de cada nó de
controle. Ela retorna sempre um `BT::NodeStatus` e nunca bloqueia por conta própria.

Ponto de entrada normal: `BT::Tree::tickRoot()`.

## 4.2 Os dois enumerados fundamentais

```cpp
// include/behaviortree_cpp_v3/basic_types.h
/// Enumerates the possible types of nodes
enum class NodeType
{
    UNDEFINED = 0,
    ACTION,
    CONDITION,
    CONTROL,
    DECORATOR,
    SUBTREE
};

/// Enumerates the states every node can be in after execution during a particular
/// time step.
/// IMPORTANT: Your custom nodes should NEVER return IDLE.
enum class NodeStatus
{
    IDLE = 0,
    RUNNING,
    SUCCESS,
    FAILURE
};
```

## 4.3 A leitura correta dos quatro valores: resultado versus estado

| Valor | Papel | Significado |
|---|---|---|
| `SUCCESS` | resultado | terminei, e deu certo |
| `FAILURE` | resultado | terminei, e não deu certo |
| `RUNNING` | resultado | ainda não terminei; me chame de novo |
| `IDLE` | **estado** | não estou em execução; nunca fui *ticado* ou fui *halted* |

**REGRA — nunca devolva `IDLE`.** Um `tick()` de usuário deve devolver `SUCCESS`,
`FAILURE` ou `RUNNING`. `IDLE` é o estado em que um nó *está* antes do primeiro *tick* e
depois de um *halt*. Todos os nós de controle embutidos tratam um filho que devolve
`IDLE` como erro de programação e lançam:

```
LogicError("A child node must never return IDLE")
```

**ARMADILHA — três decoradores convertem esse erro em `RUNNING` silenciosamente.**
`ForceSuccessNode`, `ForceFailureNode` e `KeepRunningUntilFailureNode` tratam o caso
`IDLE` com um comentário `// TODO throw?` e devolvem `status()` — o próprio estado do
decorador, que acabou de ser posto em `RUNNING`. Um nó de usuário defeituoso pendurado
sob um `<ForceSuccess>` produz uma árvore que fica `RUNNING` para sempre, sem erro e sem
progresso. É o sintoma mais difícil de diagnosticar que esta biblioteca oferece.

## 4.4 Conversão de e para texto

```cpp
// src/basic_types.cpp
template <> std::string toStr<NodeStatus>(NodeStatus status);   // "SUCCESS", "FAILURE", ...
std::string toStr(NodeStatus status, bool colored);             // com sequências ANSI
template <> std::string toStr<NodeType>(NodeType type);         // "Action", "Condition", ...
template <> NodeStatus convertFromString<NodeStatus>(StringView str);
template <> NodeType   convertFromString<NodeType>(StringView str);
```

- `convertFromString<NodeStatus>` aceita exatamente `"IDLE"`, `"RUNNING"`, `"SUCCESS"`,
  `"FAILURE"` (maiúsculas); qualquer outra coisa lança
  `RuntimeError("Cannot convert this to NodeStatus: ...")`.
- `convertFromString<NodeType>` aceita `"Action"`, `"Condition"`, `"Control"`,
  `"Decorator"`, `"SubTree"` e `"SubTreePlus"`; qualquer outra coisa devolve
  `NodeType::UNDEFINED` **sem lançar**.
- `toStr<NodeType>` devolve `"Action"`, `"Condition"`, `"Decorator"`, `"Control"`,
  `"SubTree"` ou `"Undefined"`.

**ARMADILHA — os comentários de cor de `toStr(status, colored)` estão trocados.** O
`SUCCESS` recebe `\x1b[32m` (verde) comentado como `// RED`, e o `FAILURE` recebe
`\x1b[31m` (vermelho) comentado como `// GREEN`. As cores impressas estão corretas; os
comentários é que estão invertidos. O mesmo par invertido foi copiado para
`tools/bt_log_cat.cpp`.

## 4.5 A propagação de um *tick*

Um *tick* entra pela raiz e desce pela árvore segundo a lógica de cada nó de controle —
que é justamente o que distingue uma `Sequence` de um `Fallback`. **Nem todo nó é
visitado a cada *tick***: um `Fallback` para no primeiro filho que devolve `SUCCESS`, e
os irmãos à direita simplesmente não são chamados naquela passagem. O resultado sobe de
volta, transformado a cada nível.

Exemplo concreto, com a porta fechada:

```
          Fallback                 <- tick entra aqui
         /        \
    Sequence    PassThroughWindow
    /      \
IsDoorOpen  PassThroughDoor        <- NÃO é ticado
```

1. `Fallback` *tica* o filho 0 (`Sequence`).
2. `Sequence` *tica* o filho 0 (`IsDoorOpen`) → `FAILURE`.
3. `Sequence` interrompe o laço e devolve `FAILURE`. `PassThroughDoor` **não é ticado**.
4. `Fallback` recebe `FAILURE`, avança para o filho 1 (`PassThroughWindow`).
5. `PassThroughWindow` → `SUCCESS`. `Fallback` devolve `SUCCESS`.

## 4.6 O `RUNNING` e a duração

Se todo *tick* tivesse de terminar dentro de si mesmo, uma ação de dois segundos travaria
a árvore por dois segundos. O `RUNNING` é a resposta: a ação devolve `RUNNING`, o controle
sobe até a aplicação, e no *tick* seguinte a árvore retoma.

**A frequência dos *ticks* é a granularidade com que a árvore pode mudar de ideia.** Uma
árvore *ticada* a 1 Hz reage a uma queda de bateria em até um segundo; a 100 Hz, em até
10 ms. Escolher a frequência é uma decisão de projeto do sistema, não da biblioteca.

## 4.7 Memória e reatividade

Do `RUNNING` nasce a pergunta central de todo projeto de árvore: *quando um filho fica
`RUNNING`, o que acontece com os irmãos que já tinham terminado?* A biblioteca oferece as
duas respostas, e escolher errado é a fonte número um de comportamento inesperado.

- Um **nó com memória** guarda o índice do filho corrente e, no *tick* seguinte, **retoma
  dali**. Os filhos que já devolveram `SUCCESS` não são reexecutados.
  São `Sequence`, `SequenceStar`, `Fallback`, `IfThenElse`, `Switch<N>`.
- Um **nó reativo** **recomeça do primeiro filho** a cada *tick*. Os que já tinham dado
  `SUCCESS` são reavaliados; se algum mudou de ideia, a árvore reage imediatamente.
  São `ReactiveSequence`, `ReactiveFallback`, `WhileDoElse` e (quanto à seleção)
  `BlackboardCheck*`.

```
COM MEMÓRIA                          REATIVO
   Sequence                             ReactiveSequence
   /      \                             /              \
BateriaOK  MoveBase(RUNNING)      BateriaOK       MoveBase(RUNNING)
   ^                                  ^
2º tick: NÃO é reavaliada        2º tick: reavaliada;
                                 se falhar, MoveBase é halted
```

**POR QUÊ — por que as duas variantes coexistem.** Reatividade não é gratuita: reavaliar
tudo a cada *tick* custa tempo e exige que *todas* as condições à esquerda sejam baratas e
sem efeito colateral. E há lógicas em que reavaliar é simplesmente errado — uma sequência
de montagem não deve reabrir a garra porque a condição "garra aberta" voltou a ser falsa
*por causa* do passo seguinte. O par memória/reatividade é a forma de expressar essa
diferença **na estrutura**, e não no código das folhas.

**REGRA — um único filho assíncrono nos nós reativos.** Os cabeçalhos de
`ReactiveSequence` e `ReactiveFallback` são explícitos: *"to work properly, this node
should not have more than a single asynchronous child"*. Com dois filhos que devolvem
`RUNNING`, o primeiro deles impede o segundo de ser alcançado a cada *tick* — e, no caso
da `ReactiveSequence`, o código ainda *halta* todos os irmãos à direita do que ficou
`RUNNING`. O `docs/MigrationGuide.md` registra que essa recomendação é *"documented but
not enforced by the implementation"*.

## 4.8 O ciclo de vida de um nó

Juntando *tick*, resultado e *halt*, o ciclo de vida de qualquer nó cabe num diagrama de
quatro estados. Vale para todos os nós, dos embutidos aos do usuário.

```
                         tick conclui
             ┌───────────────────────────────►  SUCCESS
             │                                     │
  IDLE ──────┤ 1º tick                             │ o PAI repõe IDLE
    ▲        ▼                                     ▼
    │     RUNNING ─────────────────────────────►  FAILURE
    │        │              tick conclui           │
    └────────┘                                     │
       halt()                                      │
    ▲                                              │
    └──────────────── o PAI repõe IDLE ────────────┘
```

Três consequências, todas detalhadas adiante:

1. **Quem devolve a `IDLE` é o pai, não o nó.** Um `DecoratorNode` repõe o filho a `IDLE`
   assim que ele conclui; os nós de controle chamam `haltChildren()` ao encerrar um
   ciclo; e `Tree::tickRoot()` repõe a raiz quando ela devolve `SUCCESS` ou `FAILURE`.
2. **Um nó em `SUCCESS` ou `FAILURE` não é reexecutado** enquanto não voltar a `IDLE`. O
   aviso está no topo de `action_node.h`:

   ```cpp
   // IMPORTANT: Actions which returned SUCCESS or FAILURE will not be ticked
   // again unless setStatus(IDLE) is called first.
   // Keep this in mind when writing your custom Control and Decorator nodes.
   ```

3. **`halt()` só faz sentido para quem devolve `RUNNING`.** `SyncActionNode` e
   `ConditionNode` declaram `halt()` como `final` e ele apenas repõe o estado — não há o
   que interromper.

## 4.9 O que é `halt()`

***Halt*** é o cancelamento explícito de um nó que está `RUNNING`. Percorre a subárvore
devolvendo cada nó a `IDLE`; é o único mecanismo pelo qual uma ação assíncrona é
interrompida de fora.

Quem chama `halt()`, na prática:

- um nó de controle, ao encerrar um ciclo (`haltChildren()`);
- uma `ReactiveSequence`/`ReactiveFallback`, sobre os irmãos à direita do filho `RUNNING`;
- um `TimeoutNode`, quando o prazo expira;
- um `RetryNode`/`RepeatNode`, entre tentativas;
- um `SwitchNode`/`WhileDoElseNode`, ao trocar de ramo;
- `Tree::haltTree()` e o destrutor de `Tree`.

## 4.10 Os quatro passos de toda aplicação

```
1. registrar          2. carregar            3. instanciar         4. ticar
registerNodeType<T>   createTreeFromFile()   nós + blackboards     tree.tickRoot()
registerFromPlugin()  XML → validação        + remapeamentos       em laço
        └──────────────────┴──────────────────────┘                    ↺ enquanto RUNNING
                     uma vez, na carga
```

Os três primeiros passos são caros e o quarto é barato.

---

# 5. `BT::TreeNode` — A CLASSE BASE DE TODOS OS NÓS

`BT::TreeNode` é a classe abstrata da qual **todo** nó da BehaviorTree.CPP deriva. Ela é
deliberadamente magra: não sabe quantos filhos tem (isso é das subclasses de aridade),
não sabe o que faz (isso é do usuário) e não sabe quando será chamada (isso é da
aplicação). O que ela define é o **contrato**.

Arquivo: `include/behaviortree_cpp_v3/tree_node.h` e `src/tree_node.cpp`.

## 5.1 Interface pública essencial

```cpp
// include/behaviortree_cpp_v3/tree_node.h  (condensada)
class TreeNode
{
  public:
    typedef std::shared_ptr<TreeNode> Ptr;

    TreeNode(std::string name, NodeConfiguration config);
    virtual ~TreeNode() = default;

    /// The method that should be used to invoke tick() and setStatus();
    virtual BT::NodeStatus executeTick();

    /// The method used to interrupt the execution of a RUNNING node.
    /// Only Async nodes that may return RUNNING should implement it.
    virtual void halt() = 0;

    bool isHalted() const;
    NodeStatus status() const;
    const std::string& name() const;          // nome da INSTÂNCIA
    virtual NodeType type() const = 0;

    /// Blocking function that will sleep until the setStatus() is called with
    /// either RUNNING, FAILURE or SUCCESS.
    BT::NodeStatus waitValidStatus();

    using StatusChangeSignal     = Signal<TimePoint, const TreeNode&, NodeStatus, NodeStatus>;
    using StatusChangeSubscriber = StatusChangeSignal::Subscriber;
    using StatusChangeCallback   = StatusChangeSignal::CallableFunction;
    StatusChangeSubscriber subscribeToStatusChange(StatusChangeCallback callback);

    uint16_t UID() const;
    const std::string& registrationName() const;   // ID usado no XML
    const NodeConfiguration& config() const;

    template <typename T> Result      getInput (const std::string& key, T& destination) const;
    template <typename T> Optional<T> getInput (const std::string& key) const;
    template <typename T> Result      setOutput(const std::string& key, const T& value);

    StringView getRawPortValue(const std::string& key) const;

    static bool isBlackboardPointer(StringView str);
    static StringView stripBlackboardPointer(StringView str);
    static Optional<StringView> getRemappedKey(StringView port_name, StringView remapping_value);

  protected:
    /// Method to be implemented by the user
    virtual BT::NodeStatus tick() = 0;

    friend class BehaviorTreeFactory;
    friend class DecoratorNode;
    friend class ControlNode;
    friend class Tree;

    void setRegistrationID(StringView ID);
    void modifyPortsRemapping(const PortsRemapping& new_remapping);
    void setStatus(NodeStatus new_status);

  private:
    const std::string name_;
    NodeStatus status_;
    std::condition_variable state_condition_variable_;
    mutable std::mutex state_mutex_;
    StatusChangeSignal state_change_signal_;
    const uint16_t uid_;
    NodeConfiguration config_;
    std::string registration_ID_;
};
```

## 5.2 `tick()` versus `executeTick()`

Esta é a distinção mais importante da classe.

- **`tick()`** é `protected` e puramente virtual: é o que **você** escreve.
- **`executeTick()`** é público e virtual: é o que **o pai** chama.

A implementação base tem três linhas e explica a divisão de trabalho:

```cpp
// src/tree_node.cpp
NodeStatus TreeNode::executeTick()
{
    const NodeStatus status = tick();
    setStatus(status);
    return status;
}
```

`executeTick()` é o **envelope** que garante que o estado publicado do nó reflita o que o
`tick()` devolveu. Quem sobrescreve `executeTick()` está mudando o **protocolo**, não o
comportamento — e por isso são poucos os que fazem isso, todos na própria biblioteca:

| Classe | Por que sobrescreve `executeTick()` |
|---|---|
| `DecoratorNode` | repõe o filho a `IDLE` assim que ele conclui |
| `SyncActionNode` | lança `LogicError` se o `tick()` devolver `RUNNING` |
| `AsyncActionNode` | lança a *thread* e relança exceção pendente (`final`) |
| `CoroActionNode` | cria ou retoma a co-rotina (`final`) |

**REGRA — sobrescreva `tick()`, não `executeTick()`.** Num nó de usuário, `tick()` é o
ponto de extensão. Três das quatro classes de ação declaram `executeTick()` como `final`
justamente para impedir o engano; o comentário no cabeçalho de `AsyncActionNode` é
explícito: *"This method spawn a new thread. Do NOT remove the `final` keyword."*

## 5.3 `name()` versus `registrationName()`

- **`name()`** é o nome da **instância** — o atributo `name` do XML, opcional.
- **`registrationName()`** é o **ID de registro**, a *string* pela qual a fábrica sabe
  construir aquele tipo.

Um mesmo `registrationName()` aparece em muitas instâncias com `name()` diferentes.
Quando o XML omite `name`, o *parser* usa o próprio ID como nome da instância — o que faz
os dois coincidirem e alimenta a confusão.

```xml
<!-- registrationName() == "SaySomething" nos dois casos -->
<SaySomething message="ola"/>                  <!-- name() == "SaySomething" -->
<SaySomething name="cumprimento" message="ola"/> <!-- name() == "cumprimento" -->
```

`setRegistrationID()` é `protected` e o comentário diz *"Only BehaviorTreeFactory should
call this"*. Na prática, quase todo nó embutido também o chama no próprio construtor
(`setRegistrationID("Sequence")`), mas o valor é **sobrescrito** logo depois por
`BehaviorTreeFactory::instantiateTreeNode()`, que atribui o ID usado no registro. **Quem
vence é o ID do registro.** É exatamente por isso que o `RetryNode` acaba registrado com a
grafia errada (seção 11.6).

## 5.4 `status()` versus `isHalted()`

```cpp
// src/tree_node.cpp
NodeStatus TreeNode::status() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return status_;
}

bool TreeNode::isHalted() const
{
    return status_ == NodeStatus::IDLE;
}
```

**ARMADILHA — `isHalted()` não significa "foi interrompido".** O nome sugere um evento
("alguém chamou `halt()` em mim"); o código diz um estado ("estou em `IDLE`"). Um nó
recém-construído, que nunca foi *ticado*, responde `true` a `isHalted()`. Além disso,
essa leitura de `status_` é feita **sem tomar o *mutex***, ao contrário de `status()` — o
que é uma leitura sem sincronização quando usada de outra *thread*. Prefira comparar
`status() == NodeStatus::IDLE` explicitamente.

## 5.5 `setStatus()`: estado, notificação e concorrência

O estado de um nó não é um simples membro: escrevê-lo dispara duas notificações.

```cpp
// src/tree_node.cpp
void TreeNode::setStatus(NodeStatus new_status)
{
    NodeStatus prev_status;
    {
        std::unique_lock<std::mutex> UniqueLock(state_mutex_);
        prev_status = status_;
        status_ = new_status;
    }
    if (prev_status != new_status)
    {
        state_condition_variable_.notify_all();
        state_change_signal_.notify(std::chrono::high_resolution_clock::now(), *this,
                                    prev_status, new_status);
    }
}
```

Três consequências:

1. **A transição só é notificada se houver transição.** Um nó que devolve `RUNNING` dez
   *ticks* seguidos gera **um** evento, não dez. É o que torna viável ligar um *logger* a
   uma árvore *ticada* a 100 Hz.
2. **Há uma *condition variable* por nó**, usada por `waitValidStatus()` — uma espera
   bloqueante até o nó sair de `IDLE`. É o mecanismo que permite a uma *thread* externa
   sincronizar-se com um nó; na prática, quase ninguém a usa.
3. **O *callback* roda na *thread* de quem chamou `setStatus()`.** Para um
   `AsyncActionNode`, isso é a *thread* da ação, não a do *tick*. Um *logger* precisa ser
   seguro sob concorrência — e é por isso que `PublisherZMQ` protege o seu *buffer* com
   *mutex*.

```cpp
// src/tree_node.cpp
NodeStatus TreeNode::waitValidStatus()
{
    std::unique_lock<std::mutex> lock(state_mutex_);
    while( isHalted() )
    {
        state_condition_variable_.wait(lock);
    }
    return status_;
}
```

## 5.6 O `Signal` (padrão observador)

A notificação usa uma implementação própria e minúscula do padrão observador.

```cpp
// include/behaviortree_cpp_v3/utils/signal.h  (íntegro, sem os includes)
/**
 * Super simple Signal/Slop implementation, AKA "Observable pattern".
 * The subscriber is active until it goes out of scope or Subscriber::reset() is called.
 */
template <typename... CallableArgs>
class Signal
{
  public:
    using CallableFunction = std::function<void(CallableArgs...)>;
    using Subscriber = std::shared_ptr<CallableFunction>;

    void notify(CallableArgs... args)
    {
        for (size_t i = 0; i < subscribers_.size();)
        {
            if (auto sub = subscribers_[i].lock())
            {
                (*sub)(args...);
                i++;
            }
            else
            {
                subscribers_.erase(subscribers_.begin() + i);
            }
        }
    }

    Subscriber subscribe(CallableFunction func)
    {
        Subscriber sub = std::make_shared<CallableFunction>(std::move(func));
        subscribers_.emplace_back(sub);
        return sub;
    }

  private:
    std::vector<std::weak_ptr<CallableFunction>> subscribers_;
};
```

O assinante recebe um `shared_ptr` e o sinal guarda um `weak_ptr`: quando o assinante
morre, a próxima notificação percebe o ponteiro expirado e o remove da lista.

**REGRA — o *subscriber* precisa continuar vivo.** `subscribeToStatusChange()` devolve um
`shared_ptr`. Se você descartá-lo, o *callback* morre na hora — silenciosamente, sem
erro. É exatamente por isso que `StatusChangeLogger` guarda um `std::vector` de
assinaturas como membro, e por que os *loggers* dos exemplos são variáveis locais do
`main()` que vivem até o fim.

**ARMADILHA — o `Signal` não é seguro sob concorrência.** `notify()` e `subscribe()`
manipulam o mesmo `std::vector` sem *mutex*, e `notify()` é chamado de dentro de
`setStatus()` — que, num `AsyncActionNode`, roda na *thread* da ação. Assinar um nó
enquanto a árvore está sendo *ticada* é uma condição de corrida. **Ligue todos os
*loggers* antes do primeiro `tickRoot()`.**

## 5.7 O identificador único (`UID`)

Cada nó recebe, na construção, um `uint16_t` sequencial. É esse número — e não o nome —
que os *loggers* binários e o Groot usam para identificar um nó numa transição.

```cpp
// src/tree_node.cpp
static uint16_t getUID()
{
    static uint16_t uid = 1;
    return uid++;
}

TreeNode::TreeNode(std::string name, NodeConfiguration config)
  : name_(std::move(name)),
    status_(NodeStatus::IDLE),
    uid_(getUID()),
    config_(std::move(config))
{
}
```

**ARMADILHA — o contador de `UID` é global, não por árvore.** São quatro problemas num
contador de cinco linhas:

- **(a)** O contador é `static` do processo: a segunda árvore criada continua a numeração
  da primeira, e os `UID` de uma árvore não começam em 1.
- **(b)** Destruir uma árvore **não** devolve os números; um programa que reconstrói a
  árvore a cada minuto esgota os 65 535 valores.
- **(c)** No estouro, o contador volta a zero **sem aviso** e dois nós vivos passam a ter
  o mesmo `UID` — com o que o *logger* binário e o Groot passam a atribuir transições ao
  nó errado.
- **(d)** O incremento **não é atômico**: construir árvores em duas *threads* é uma
  corrida.

Nenhum desses casos é verificado em tempo de execução.

## 5.8 `NodeConfiguration` e `TreeNodeManifest`

```cpp
// include/behaviortree_cpp_v3/tree_node.h
/// This information is used mostly by the XMLParser.
struct TreeNodeManifest
{
    NodeType type;
    std::string registration_ID;
    PortsList ports;
};

typedef std::unordered_map<std::string, std::string> PortsRemapping;

struct NodeConfiguration
{
    NodeConfiguration() {}

    Blackboard::Ptr blackboard;
    PortsRemapping input_ports;
    PortsRemapping output_ports;
};
```

- **`TreeNodeManifest`** é o que a **fábrica** guarda por ID: tipo do nó, ID e lista de
  portas declaradas. É o que permite ao *parser* validar atributos do XML **antes** de
  instanciar qualquer coisa.
- **`NodeConfiguration`** é o que a **instância** recebe: um ponteiro para o *blackboard*
  e os dois mapas de remapeamento (nome de porta → valor do atributo do XML).

`config()` devolve uma referência constante e o comentário do cabeçalho é claro:
*"Configuration passed at construction time. Can never change after the creation of the
TreeNode instance."* O único jeito de alterá-la é `modifyPortsRemapping()`, que é
`protected` e não é chamado por nada na v3.5.6.

## 5.9 `getRawPortValue()`

```cpp
// src/tree_node.cpp
StringView TreeNode::getRawPortValue(const std::string& key) const
{
  auto remap_it = config_.input_ports.find(key);
  if (remap_it == config_.input_ports.end())
  {
    throw std::logic_error(StrCat("getInput() failed because "
      "NodeConfiguration::input_ports does not contain the key: [", key, "]"));
  }
  return remap_it->second;
}
```

Devolve o texto **cru** do atributo, sem remapeamento e sem conversão de tipo. O
comentário do cabeçalho diz: *"function provide mostrly for debugging purpose to see the
raw value in the port"*. Útil para descobrir se um atributo chegou como `"{chave}"` ou
como literal.

Note que ela lança `std::logic_error` **cru**, fora da hierarquia
`BehaviorTreeException`.

---

# 6. AS TRÊS ARIDADES: `LeafNode`, `ControlNode`, `DecoratorNode`

Sobre `TreeNode`, três subclasses fixam quantos filhos um nó pode ter. Não acrescentam
política nenhuma — apenas armazenamento e as operações de *halt* correspondentes.

```
                       TreeNode
                          │
        ┌─────────────────┼─────────────────┐
   ControlNode        LeafNode        DecoratorNode
    (N filhos)       (0 filhos)         (1 filho)
        │                 │                  │
   Sequence          ActionNodeBase      Inverter
   Fallback          ConditionNode       Repeat
   Parallel                              Timeout
   Switch<N>         ┌──────────────┐    SubtreeNode
   ...               │SyncActionNode│    ...
                     │AsyncActionNode│
                     │StatefulActionNode│
                     │CoroActionNode │
                     └──────────────┘
```

## 6.1 `LeafNode` — nenhum filho

```cpp
// include/behaviortree_cpp_v3/leaf_node.h  (íntegro)
class LeafNode : public TreeNode
{
  protected:
  public:
    LeafNode(const std::string& name, const NodeConfiguration& config)
      : TreeNode(name, config)
    { }

    virtual ~LeafNode() override = default;
};
```

É a classe mais simples da biblioteca: herda tudo e não acrescenta nada além do
construtor. Serve de raiz comum a `ActionNodeBase` e `ConditionNode`, e é o ponto onde
"não tenho filhos" vira um fato do sistema de tipos em vez de uma convenção.

## 6.2 `ControlNode` — N filhos

```cpp
// include/behaviortree_cpp_v3/control_node.h  (condensada)
class ControlNode : public TreeNode
{
  protected:
    std::vector<TreeNode*> children_nodes_;

  public:
    ControlNode(const std::string& name, const NodeConfiguration& config);
    virtual ~ControlNode() override = default;

    /// The method used to add nodes to the children vector
    void addChild(TreeNode* child);

    size_t childrenCount() const;
    const std::vector<TreeNode*>& children() const;
    const TreeNode* child(size_t index) const { return children().at(index); }

    virtual void halt() override;
    void haltChildren();

    [[deprecated("deprecated: please use explicitly haltChildren() or haltChild(i)")]]
    void haltChildren(size_t first);

    void haltChild(size_t i);

    virtual NodeType type() const override final { return NodeType::CONTROL; }
};
```

Implementação relevante:

```cpp
// src/control_node.cpp
void ControlNode::halt()
{
    haltChildren();
    setStatus(NodeStatus::IDLE);
}

void ControlNode::haltChild(size_t i)
{
    auto child = children_nodes_[i];
    if (child->status() == NodeStatus::RUNNING)
    {
        child->halt();
    }
    child->setStatus(NodeStatus::IDLE);
}

void ControlNode::haltChildren()
{
    for (size_t i = 0; i < children_nodes_.size(); i++)
    {
        haltChild(i);
    }
}
```

**Duas coisas acontecem em `haltChild()`, e confundi-las custa caro:**

- `halt()` só é chamado se o filho estiver `RUNNING` — é o **cancelamento** propriamente
  dito, que propaga para baixo.
- O `setStatus(IDLE)` é **incondicional**: mesmo um filho que terminou em `SUCCESS` volta
  a `IDLE`. É esse segundo efeito, e não o primeiro, que os nós de controle usam ao
  encerrar um ciclo — sem ele, o nó jamais seria executado de novo.

**ARMADILHA — os filhos são ponteiros crus, e o pai não é dono.** `children_nodes_` é um
`std::vector<TreeNode*>`. A posse dos nós é do `Tree`, que guarda `shared_ptr` em
`Tree::nodes`. Um `ControlNode` construído à mão, fora do *parser*, só é válido enquanto
alguém mantiver os filhos vivos — `addChild()` não faz nenhuma verificação e nem sequer
rejeita `nullptr`. O erro só aparece depois, em `applyRecursiveVisitor()`, como
`LogicError("One of the children of a DecoratorNode or ControlNode is nullptr")`.

**Nota sobre `haltChildren(size_t first)`:** está marcada `[[deprecated]]`, mas continua
sendo usada internamente por `SequenceStarNode` na forma de um laço explícito equivalente.

## 6.3 `DecoratorNode` — exatamente um filho

```cpp
// include/behaviortree_cpp_v3/decorator_node.h  (condensada)
class DecoratorNode : public TreeNode
{
  protected:
    TreeNode* child_node_;

  public:
    DecoratorNode(const std::string& name, const NodeConfiguration& config);
    virtual ~DecoratorNode() override = default;

    void setChild(TreeNode* child);
    const TreeNode* child() const;
    TreeNode* child();

    /// The method used to interrupt the execution of this node
    virtual void halt() override;
    /// Halt() the child node
    void haltChild();

    virtual NodeType type() const override { return NodeType::DECORATOR; }

    NodeStatus executeTick() override;
};
```

```cpp
// src/decorator_node.cpp
void DecoratorNode::setChild(TreeNode* child)
{
    if (child_node_)
    {
        throw BehaviorTreeException("Decorator [", name(), "] has already a child assigned");
    }
    child_node_ = child;
}

void DecoratorNode::halt()
{
    haltChild();
    setStatus(NodeStatus::IDLE);
}

void DecoratorNode::haltChild()
{
    if (child_node_->status() == NodeStatus::RUNNING)
    {
        child_node_->halt();
    }
    child_node_->setStatus(NodeStatus::IDLE);
}

NodeStatus DecoratorNode::executeTick()
{
    NodeStatus status = TreeNode::executeTick();
    NodeStatus child_status = child()->status();
    if( child_status == NodeStatus::SUCCESS || child_status == NodeStatus::FAILURE )
    {
        child()->setStatus(NodeStatus::IDLE);
    }
    return status;
}
```

**POR QUÊ — por que o decorador repõe o filho a `IDLE`.** Um decorador como `RepeatNode`
precisa executar o mesmo filho várias vezes. Mas um nó que terminou em `SUCCESS` não é
reexecutado enquanto não voltar a `IDLE`. Se a reposição ficasse a cargo de cada
decorador, qualquer decorador de usuário que a esquecesse teria um filho que executa uma
única vez e depois some. Colocá-la no `executeTick()` da classe base resolve o problema
para todos de uma vez — inclusive para os que ainda não foram escritos.

**ARMADILHA — `DecoratorNode::halt()` não tolera filho ausente.** `haltChild()`
desreferencia `child_node_` sem verificar `nullptr`. Um decorador construído em C++ ao
qual se esqueceu de chamar `setChild()` produz falha de segmentação no primeiro *halt* — e
o destrutor de `Tree` chama `haltTree()`, então basta a árvore sair de escopo. Pelo
caminho do XML isso não acontece: a validação exige exatamente um filho para todo
`<Decorator>`.

## 6.4 `SimpleDecoratorNode`

```cpp
// include/behaviortree_cpp_v3/decorator_node.h
/**
 * @brief The SimpleDecoratorNode provides an easy to use DecoratorNode.
 * The user should simply provide a callback with this signature
 *
 *    BT::NodeStatus functionName(BT::NodeStatus child_status)
 *
 * This avoids the hassle of inheriting from a DecoratorNode.
 *
 * Using lambdas or std::bind it is easy to pass a pointer to a method.
 * SimpleDecoratorNode does not support halting, NodeParameters, nor Blackboards.
 */
class SimpleDecoratorNode : public DecoratorNode
{
  public:
    typedef std::function<NodeStatus(NodeStatus, TreeNode&)> TickFunctor;
    SimpleDecoratorNode(const std::string& name, TickFunctor tick_functor,
                        const NodeConfiguration& config);
  protected:
    virtual NodeStatus tick() override;
    TickFunctor tick_functor_;
};
```

```cpp
// src/decorator_node.cpp
NodeStatus SimpleDecoratorNode::tick()
{
    return tick_functor_(child()->executeTick(), *this);
}
```

**ARMADILHA — o filho é *ticado* antes de o *functor* decidir.** Não dá para escrever uma
guarda com `registerSimpleDecorator()`: o filho executa **sempre**, e o *functor* só
transforma o resultado. Para *impedir* a execução do filho é preciso herdar de
`DecoratorNode`.

## 6.5 O tipo de um nó em tempo de compilação: `getType<T>()`

```cpp
// include/behaviortree_cpp_v3/behavior_tree.h
/// Simple way to extract the type of a TreeNode at COMPILE TIME.
/// Useful to avoid the cost of dynamic_cast or the virtual method TreeNode::type().
template <typename T>
inline NodeType getType()
{
    if( std::is_base_of<ActionNodeBase, T>::value )        return NodeType::ACTION;
    if( std::is_base_of<ConditionNode, T>::value )         return NodeType::CONDITION;
    if( std::is_base_of<SubtreeNode, T>::value )           return NodeType::SUBTREE;
    if( std::is_base_of<SubtreePlusNode, T>::value )       return NodeType::SUBTREE;
    if( std::is_base_of<DecoratorNode, T>::value )         return NodeType::DECORATOR;
    if( std::is_base_of<ControlNode, T>::value )           return NodeType::CONTROL;
    return NodeType::UNDEFINED;
}
```

A **ordem dos testes não é decorativa**: `SubtreeNode` *é* um `DecoratorNode`, e por isso
precisa ser testado antes — senão toda subárvore seria classificada como decorador no
manifesto e no Groot. É essa função, e não `type()`, que a fábrica usa ao montar o
manifesto de um nó registrado.

## 6.6 Percorrer a árvore: as três funções livres

```cpp
// include/behaviortree_cpp_v3/behavior_tree.h
void applyRecursiveVisitor(const TreeNode* root_node,
                           const std::function<void(const TreeNode*)>& visitor);
void applyRecursiveVisitor(TreeNode* root_node,
                           const std::function<void(TreeNode*)>& visitor);

/** Debug function to print on screen the hierarchy of the tree. */
void printTreeRecursively(const TreeNode* root_node);

typedef std::vector<std::pair<uint16_t, uint8_t>> SerializedTreeStatus;
void buildSerializedStatusSnapshot(const TreeNode* root_node,
                                   SerializedTreeStatus& serialized_buffer);
```

```cpp
// src/behavior_tree.cpp
void applyRecursiveVisitor(TreeNode* node, const std::function<void(TreeNode*)>& visitor)
{
    if (!node)
    {
        throw LogicError("One of the children of a DecoratorNode or ControlNode is nullptr");
    }
    visitor(node);

    if (auto control = dynamic_cast<BT::ControlNode*>(node))
    {
        for (const auto& child : control->children())
        {
            applyRecursiveVisitor(child, visitor);
        }
    }
    else if (auto decorator = dynamic_cast<BT::DecoratorNode*>(node))
    {
        applyRecursiveVisitor(decorator->child(), visitor);
    }
}
```

**A travessia usa `dynamic_cast`, não `type()`.** É uma escolha consciente: `type()` diria
`SUBTREE` para um `SubtreeNode`, e o visitante precisa saber que ele é *estruturalmente*
um decorador para descer ao filho. O custo é irrelevante, porque a travessia acontece na
carga, não a cada *tick*.

**ARMADILHA — `buildSerializedStatusSnapshot` não linka como declarada.** O cabeçalho
declara a função recebendo `const TreeNode*`; o `src/behavior_tree.cpp` a define recebendo
`TreeNode*`. São assinaturas diferentes: a declarada nunca é definida, e a definida nunca
é declarada. Chamá-la com um ponteiro `const` compila e falha na ligação com *undefined
reference*. Nada na biblioteca a chama — o `PublisherZMQ` monta o seu *snapshot* à mão,
com `applyRecursiveVisitor` — e por isso o defeito passa despercebido.

Saída típica de `printTreeRecursively`:

```
----------------
missao
   Aproximar
   estrategias
      ja_estava_aberta
         EstaAberta
         Atravessar
      PortaFechada
         destrancar_e_passar
            Inverter
               EstaAberta
            RetryUntilSuccesful
               uma_tentativa
                  Timeout
                     Destrancar
                  Abrir
            Atravessar
      Atravessar
   Falar
   Fechar
----------------
```

Note que a subárvore aparece com o **nome do seu `ID`**, não com um `name` próprio.

## 6.7 Erros e exceções

```cpp
// include/behaviortree_cpp_v3/exceptions.h  (condensada)
class BehaviorTreeException : public std::exception
{
  public:
    BehaviorTreeException(nonstd::string_view message)
      : message_(static_cast<std::string>(message)) {}

    template <typename... SV>
    BehaviorTreeException(const SV&... args): message_(StrCat(args...)) {}

    const char* what() const noexcept { return message_.c_str(); }
  private:
    std::string message_;
};

// This errors are usually related to problems that "probably" require code refactoring
// to be fixed.
class LogicError: public BehaviorTreeException { /* mesmos construtores */ };

// This errors are usually related to problems that are relted to data or conditions
// that happen only at run-time
class RuntimeError: public BehaviorTreeException { /* mesmos construtores */ };
```

| Classe | Quando é lançada (critério documentado) |
|---|---|
| `BehaviorTreeException` | base de todas; herda de `std::exception` |
| `LogicError` | erro que "provavelmente exige refatorar o código" |
| `RuntimeError` | erro ligado a dados ou condições de execução |

As três aceitam número variável de argumentos, concatenados por `StrCat` — daí o padrão
onipresente `throw RuntimeError("Missing parameter [", NUM_CYCLES, "] in RepeatNode")`.

**ARMADILHA — a fronteira `LogicError`/`RuntimeError` não é respeitada.** O critério
documentado é claro, mas o código não o segue. Um filho que devolve `IDLE` — erro de
programação — é `LogicError`, correto; mas o mesmo erro aparece como `std::logic_error`
**cru** em `IfThenElseNode`, `WhileDoElseNode` e `StatefulActionNode`, **fora** da
hierarquia da biblioteca. `TreeNode::getRawPortValue()` também lança `std::logic_error`
cru. Um `catch (BehaviorTreeException&)` **não** pega esses casos. Para capturar tudo, use
`catch (std::exception&)`.

**REGRA — `tick()` pode lançar, mas prefira `FAILURE`.** Uma exceção que escapa de um
`tick()` sobe até o `tickRoot()` da aplicação, deixando a árvore num estado intermediário:
os nós que já mudaram de estado permanecem como estavam, e nada foi *halted*. Isso é
aceitável para erro de **configuração** (porta obrigatória ausente, tipo incompatível).
Para falha de **domínio** — não achei o objeto, o motor não respondeu — devolva `FAILURE`
e deixe a árvore decidir. Num `AsyncActionNode` a exceção é capturada na *thread*,
guardada num `std::exception_ptr` e relançada no *tick* seguinte.

---

# 7. PORTAS (*PORTS*)

O *tick* carrega **controle**, não dados: ele diz *quando* um nó executa, mas nada sobre
*com que valores*. O segundo canal — o par **porta/blackboard** — é por onde a informação
circula.

A analogia que a própria documentação usa é a certa: **uma porta de entrada é um argumento
de função; uma porta de saída é um valor de retorno.** A diferença é que argumentos e
retornos, aqui, são ligados por **nome** em tempo de carga, e não por posição em tempo de
compilação.

## 7.1 Declarando portas: `providedPorts()`

Um nó anuncia as suas portas por um método estático de nome fixo:

```cpp
class MoverPara : public BT::SyncActionNode
{
  public:
    MoverPara(const std::string& name, const BT::NodeConfiguration& config)
      : BT::SyncActionNode(name, config) {}

    // O nome e a assinatura sao obrigatorios: a biblioteca detecta este metodo
    // por SFINAE, e nao por heranca. PRECISA ser public.
    static BT::PortsList providedPorts()
    {
        return { BT::InputPort<Pose2D>("goal", "destino, em x;y;theta"),
                 BT::InputPort<double>("tolerancia", 0.25, "raio de aceitacao [m]"),
                 BT::OutputPort<std::string>("resultado") };
    }

    BT::NodeStatus tick() override;
};
```

`PortsList` é um `std::unordered_map<std::string, PortInfo>`.

## 7.2 `PortInfo` e `PortDirection`

```cpp
// include/behaviortree_cpp_v3/basic_types.h
enum class PortDirection{
    INPUT,
    OUTPUT,
    INOUT
};

class PortInfo
{
public:
    PortInfo( PortDirection direction = PortDirection::INOUT ):
        _type(direction), _info(nullptr) {}

    PortInfo( PortDirection direction, const std::type_info& type_info, StringConverter conv):
        _type(direction), _info( &type_info ), _converter(conv) {}

    PortDirection direction() const;
    const std::type_info* type() const;

    Any parseString(const char *str) const;
    Any parseString(const std::string& str) const;
    template <typename T> Any parseString(const T& ) const { return {}; }  // evita erro de compilacao

    void setDescription(StringView description);
    void setDefaultValue(StringView default_value_as_string);
    const std::string& description() const;
    const std::string& defaultValue() const;

private:
    PortDirection _type;
    const std::type_info* _info;
    StringConverter _converter;
    std::string description_;
    std::string default_value_;
};
```

`PortInfo` guarda **quatro** coisas:

1. a **direção** (`INPUT`, `OUTPUT`, `INOUT`);
2. o `std::type_info` do **tipo declarado** (ou `nullptr` se a porta não tiver tipo);
3. uma **função de conversão** a partir de texto (`StringConverter`);
4. dois campos textuais: **descrição** e **valor padrão** (guardado como *string*).

```cpp
typedef std::function<Any(StringView)> StringConverter;
typedef std::unordered_map<const std::type_info*, StringConverter> StringConvertersMap;

template <typename T> inline
StringConverter GetAnyFromStringFunctor()
{
    return [](StringView str){ return Any(convertFromString<T>(str)); };
}

template <> inline
StringConverter GetAnyFromStringFunctor<void>()
{
    return {};        // porta sem tipo: nenhum conversor
}
```

## 7.3 As funções de construção de porta

```cpp
// include/behaviortree_cpp_v3/basic_types.h
template <typename T = void>
std::pair<std::string,PortInfo> CreatePort(PortDirection direction,
                                           StringView name,
                                           StringView description = {});

template <typename T = void> inline
std::pair<std::string,PortInfo> InputPort(StringView name, StringView description = {});

template <typename T = void> inline
std::pair<std::string,PortInfo> OutputPort(StringView name, StringView description = {});

template <typename T = void> inline
std::pair<std::string,PortInfo> BidirectionalPort(StringView name, StringView description = {});

// variantes COM valor padrao (so para INPUT e INOUT)
template <typename T = void> inline
std::pair<std::string,PortInfo> InputPort(StringView name, const T& default_value,
                                          StringView description);

template <typename T = void> inline
std::pair<std::string,PortInfo> BidirectionalPort(StringView name, const T& default_value,
                                                  StringView description);
```

Formas de uso, todas válidas:

```cpp
BT::InputPort<int>("contagem")                                  // tipada, sem descrição
BT::InputPort<int>("contagem", "quantas vezes")                 // tipada, com descrição
BT::InputPort<int>("contagem", 3, "quantas vezes")              // tipada, com padrão 3
BT::InputPort("qualquer")                                       // SEM tipo (T = void)
BT::OutputPort<std::string>("resultado")
BT::BidirectionalPort<std::string>("chave")
```

**REGRA — porta com tipo, sempre que possível.** `InputPort("chave")` sem parâmetro de
*template* é válido e cria uma porta de tipo `void`: sem `type_info` e sem conversor. Ela
funciona — o `getInput<T>()` converte na leitura — mas abre mão de duas verificações que a
biblioteca faria de graça:

- a checagem de consistência de tipo entre dois nós que compartilham a mesma entrada do
  *blackboard* (seção 8.5);
- a informação de tipo que o Groot exibe e que `writeTreeNodesModelXML()` gera.

Nós embutidos que usam portas **sem tipo**: `SetBlackboard` (`value`, `output_key`) e
`BlackboardCheck*` (`value_A`, `value_B`).

## 7.4 A detecção por SFINAE

A fábrica descobre se um nó tem portas testando se a expressão `T::providedPorts()` é
válida e devolve `PortsList`:

```cpp
// include/behaviortree_cpp_v3/basic_types.h
template <typename T, typename = void>
struct has_static_method_providedPorts: std::false_type {};

template <typename T>
struct has_static_method_providedPorts<T,
        typename std::enable_if<std::is_same<decltype(T::providedPorts()),
                                             PortsList>::value>::type>
    : std::true_type {};

template <typename T> inline
PortsList getProvidedPorts(enable_if< has_static_method_providedPorts<T> > = nullptr)
{
    return T::providedPorts();
}

template <typename T> inline
PortsList getProvidedPorts(enable_if_not< has_static_method_providedPorts<T> > = nullptr)
{
    return {};
}
```

**ARMADILHA — um `providedPorts()` privado é o mesmo que nenhum.** A detecção é sensível a
acesso: se `providedPorts()` for `private` ou `protected`, a expressão não é válida no
contexto do *trait*, a substituição falha e o nó é registrado **com a lista de portas
vazia** — sem erro de compilação.

Não é hipótese: `SubtreeNode` e `SubtreePlusNode` declaram `providedPorts()` na seção
`private`, e os seus manifestos são, de fato, vazios (confirmado por extração de
`factory.manifests()` em tempo de execução: `SubTree [SubTree] portas=0`). As portas
`__shared_blackboard` e `__autoremap` que eles anunciam ali **nunca chegam ao manifesto**;
funcionam porque o *parser* lê esses atributos diretamente do XML.

Num nó do usuário, o mesmo engano produz o erro na carga:

```
Possible typo? In the XML, you tried to remap port "X" in node [ID / nome],
but the manifest of this node does not contain a port with this name.
```

## 7.5 Lendo e escrevendo dentro do `tick()`

```cpp
BT::NodeStatus MoverPara::tick()
{
    // Forma 1: destino + Result. Conveniente quando ha um valor de fallback.
    double tol = 0.25;
    getInput("tolerancia", tol);                  // ignora a falha de proposito

    // Forma 2: Optional<T>. Preferivel quando a porta e obrigatoria, porque
    // res.error() traz a mensagem exata do que faltou.
    auto res = getInput<Pose2D>("goal");
    if (!res) {
        throw BT::RuntimeError("faltou a porta [goal]: ", res.error());
    }
    Pose2D destino = res.value();
    // ...
    setOutput("resultado", std::string("cheguei"));
    return BT::NodeStatus::SUCCESS;
}
```

## 7.6 `Optional<T>` e `Result`

```cpp
// include/behaviortree_cpp_v3/basic_types.h
template <typename T> using Optional = nonstd::expected<T, std::string>;
// note: we use the name Optional instead of expected because it is more intuitive
// for users that are not up to date with "modern" C++

using Result = Optional<void>;
```

Padrão de uso documentado no próprio cabeçalho:

```cpp
auto res = getAnswer();
if( res )
{
    std::cout << "answer was: " << res.value() << std::endl;
}
else{
    std::cerr << "failed to get the answer: " << res.error() << std::endl;
}
```

**ARMADILHA — `Optional` não é `std::optional`.** Apesar do nome, `BT::Optional<T>`
carrega uma **mensagem de erro** no caso negativo e não tem `value_or()` com a semântica
que se espera de `std::optional`. Confundir os dois leva a código que compila e ignora a
razão da falha — justamente a informação mais útil ao depurar uma árvore que não carrega.

## 7.7 A resolução de um nome de porta (mecanismo central)

Quando o XML diz `<MoverPara goal="{destino}"/>`, o *parser* guarda em
`config().input_ports` o par `("goal", "{destino}")`. Na leitura,
`getInput("goal", ...)` faz:

```cpp
// include/behaviortree_cpp_v3/tree_node.h  (condensada)
template <typename T>
inline Result TreeNode::getInput(const std::string& key, T& destination) const
{
    auto remap_it = config_.input_ports.find(key);
    if (remap_it == config_.input_ports.end())      // ① a porta nao foi configurada
    {
        return nonstd::make_unexpected(StrCat("getInput() failed because "
                                              "NodeConfiguration::input_ports "
                                              "does not contain the key: [", key, "]"));
    }
    auto remapped_res = getRemappedKey(key, remap_it->second);
    try
    {
        if (!remapped_res)                          // ② nao e ponteiro: e literal
        {
            destination = convertFromString<T>(remap_it->second);
            return {};
        }
        const auto& remapped_key = remapped_res.value();          // ③ e ponteiro

        if (!config_.blackboard)
        {
            return nonstd::make_unexpected("getInput() trying to access a Blackboard(BB) "
                                           "entry, but BB is invalid");
        }

        const Any* val = config_.blackboard->getAny(static_cast<std::string>(remapped_key));
        if (val && val->empty() == false)
        {
            if (std::is_same<T, std::string>::value == false && val->type() == typeid(std::string))
            {
                destination = convertFromString<T>(val->cast<std::string>());   // ④
            }
            else
            {
                destination = val->cast<T>();
            }
            return {};
        }
        return nonstd::make_unexpected(StrCat("getInput() failed because it was unable to "
                                              "find the key [", key, "] remapped to [",
                                              remapped_key, "]"));
    }
    catch (std::exception& err)
    {
        return nonstd::make_unexpected(err.what());
    }
}
```

E `getRemappedKey()` decide, em três linhas, o que é ponteiro e o que é literal:

```cpp
// src/tree_node.cpp
bool TreeNode::isBlackboardPointer(StringView str)
{
    const auto size = str.size();
    if( size >= 3 && str.back() == '}')
    {
        if( str[0] == '{')  { return true; }
        if( size >= 4 && str[0] == '$' && str[1] == '{') { return true; }
    }
    return false;
}

StringView TreeNode::stripBlackboardPointer(StringView str)
{
    const auto size = str.size();
    if( size >= 3 && str.back() == '}')
    {
        if( str[0] == '{')                    { return str.substr(1, size-2); }
        if( str[0] == '$' && str[1] == '{')   { return str.substr(2, size-3); }
    }
    return {};
}

Optional<StringView> TreeNode::getRemappedKey(StringView port_name, StringView remapping_value)
{
    if( remapping_value == "=" )
    {
        return {port_name};
    }
    if( isBlackboardPointer( remapping_value ) )
    {
        return {stripBlackboardPointer(remapping_value)};
    }
    return nonstd::make_unexpected("Not a blackboard pointer");
}
```

### Tabela das formas que um atributo de porta pode assumir

| No XML | Interpretação | De onde vem o valor |
|---|---|---|
| `msg="oi"` | literal | convertido do próprio texto |
| `msg="{k}"` | ponteiro | entrada `k` do *blackboard* |
| `msg="${k}"` | ponteiro (forma antiga) | entrada `k` do *blackboard* |
| `msg` ausente | padrão da porta, se houver | convertido do texto do padrão |
| (interno) `"="` | ponteiro homônimo | entrada de mesmo nome da porta |

O `"="` **não é escrito no XML**: é o que `assignDefaultRemapping<T>()` grava ao
configurar um nó criado diretamente em C++, sem *parser* — como fazem os testes da
biblioteca.

```cpp
// include/behaviortree_cpp_v3/tree_node.h
// Utility function to fill the list of ports using T::providedPorts();
template <typename T>
inline void assignDefaultRemapping(NodeConfiguration& config)
{
    for (const auto& it : getProvidedPorts<T>())
    {
        const auto& port_name = it.first;
        const auto direction = it.second.direction();
        if (direction != PortDirection::OUTPUT) { config.input_ports[port_name]  = "="; }
        if (direction != PortDirection::INPUT)  { config.output_ports[port_name] = "="; }
    }
}
```

### Os dois caminhos, em diagrama

```
XML  goal="{destino}"
  │ (carga)
  ▼
config().input_ports   "goal" ↦ "{destino}"
  │ getRemappedKey(): tira as chaves          ╲  sem {}: literal
  ▼                                            ╲ convertFromString<T> direto do texto
chave resolvida  "destino"                      ╲
  │ getAny()                                     ╲
  ▼                                               ╲
blackboard  "destino" ↦ Any                        ╲
  │ cast<T>()                                       ╲
  ▼                                                  ▼
                        Pose2D no tick()
```

**ARMADILHA — o literal é reconvertido a *cada* *tick*.** No caminho literal,
`convertFromString<T>()` é chamado toda vez que `getInput()` é chamado — não há
memorização. Para um `int` é irrelevante; para um tipo cuja conversão faça *parsing*
pesado, ou aloque, o custo aparece no perfil como tempo gasto no `tick()` da folha. Se o
valor é constante e caro, leia-o uma vez e guarde num membro.

## 7.8 `setOutput()`

```cpp
// include/behaviortree_cpp_v3/tree_node.h
template <typename T>
inline Result TreeNode::setOutput(const std::string& key, const T& value)
{
    if (!config_.blackboard)
    {
        return nonstd::make_unexpected("setOutput() failed: trying to access a "
                                       "Blackboard(BB) entry, but BB is invalid");
    }
    auto remap_it = config_.output_ports.find(key);
    if (remap_it == config_.output_ports.end())
    {
        return nonstd::make_unexpected(StrCat("setOutput() failed: "
                                              "NodeConfiguration::output_ports does not "
                                              "contain the key: [", key, "]"));
    }
    StringView remapped_key = remap_it->second;
    if (remapped_key == "=")                       { remapped_key = key; }
    if (isBlackboardPointer(remapped_key))         { remapped_key = stripBlackboardPointer(remapped_key); }

    config_.blackboard->set(static_cast<std::string>(remapped_key), value);
    return {};
}
```

Note a assimetria em relação a `getInput()`: `setOutput()` **não** tem caminho literal.
Se o atributo de uma porta de saída não estiver entre chaves, o texto cru é usado como
**nome de chave**. Ou seja, `<No resultado="minha_chave"/>` numa porta de saída escreve
na entrada `minha_chave` — o mesmo efeito de `resultado="{minha_chave}"`. Isso é
deliberado e é o que faz o `SetBlackboard` funcionar com `output_key="the_answer"`.

## 7.9 Valores padrão

O valor padrão é guardado **como texto** (via `BT::toStr`) e aplicado pelo *parser* na
construção do nó:

```cpp
// src/xml_parsing.cpp  (dentro de createNodeFromXML)
// use default value if available for empty ports. Only inputs
for (const auto& port_it: manifest.ports)
{
    const std::string& port_name =  port_it.first;
    const PortInfo& port_info = port_it.second;

    auto direction = port_info.direction();
    if( direction != PortDirection::OUTPUT &&
        config.input_ports.count(port_name) == 0 &&
        port_info.defaultValue().empty() == false)
    {
        config.input_ports.insert( { port_name, port_info.defaultValue() } );
    }
}
```

**ARMADILHA — o padrão só existe pelo caminho do XML.** A aplicação do valor padrão está
no *parser*, não no `TreeNode`. Um nó instanciado diretamente em C++, sem passar pelo XML,
recebe um `NodeConfiguration` vazio e o `getInput()` da porta com padrão **falha**.
Some-se a isso que:

- uma porta de **saída** nunca recebe padrão (a condição `direction != OUTPUT` é
  explícita);
- um padrão **vazio** é indistinguível de padrão ausente — não há como declarar `""` como
  valor padrão.

**Portas embutidas COM padrão** (as únicas quatro em toda a biblioteca):

| ID | Porta | Padrão |
|---|---|---|
| `Parallel` | `failure_threshold` | `1` |
| `ManualSelector` | `repeat_last_selection` | `false` |
| `SubTree` | `__shared_blackboard` | `false` (mas o manifesto é vazio — sem efeito) |
| `SubTreePlus` | `__autoremap` | `false` (idem) |

**Portas embutidas obrigatórias SEM padrão** (omiti-las gera `RuntimeError` no primeiro
*tick*, não na carga): `Parallel/success_threshold`, `Repeat/num_cycles`,
`RetryUntilSuccesful/num_attempts`, `Timeout/msec`, `Delay/delay_msec`.

## 7.10 Portas definidas em tempo de execução

Quando a lista de portas só é conhecida no momento do registro — porque vem de um arquivo
de configuração, por exemplo — há duas formas:

```cpp
// examples/t11_runtime_ports.cpp
// more verbose way
PortsList think_ports = {BT::OutputPort<std::string>("text")};
factory.registerBuilder(CreateManifest<ThinkRuntimePort>("ThinkRuntimePort", think_ports),
                        CreateBuilder<ThinkRuntimePort>());

// less verbose way
PortsList say_ports = {BT::InputPort<std::string>("message")};
factory.registerNodeType<SayRuntimePort>("SayRuntimePort", say_ports);
```

A sobrecarga com `PortsList` explícita exige o **oposto** da forma normal: o nó **não**
pode ter `providedPorts()` estático, *"to avoid ambiguities"*.

**REGRA — leia as portas dentro do `tick()`, não no construtor.** O `NodeConfiguration` é
fixado na construção e nunca muda, mas o **conteúdo** do *blackboard* muda a cada *tick*.
Ler uma entrada no construtor captura o valor que existia antes de qualquer nó ter escrito
nele — tipicamente, nenhum. Vários nós embutidos deixam isso explícito com o membro
`read_parameter_from_ports_`: quando construídos pelo XML, adiam a leitura para o
primeiro `tick()`.

---

# 8. O *BLACKBOARD*

O *blackboard* é a memória compartilhada da árvore: um dicionário de chaves textuais para
valores de **tipo apagado** (`BT::Any`), com um *mutex* próprio e um ponteiro fraco para o
*blackboard* do pai.

Arquivos: `include/behaviortree_cpp_v3/blackboard.h` e `src/blackboard.cpp`.

## 8.1 Estado interno

```cpp
// include/behaviortree_cpp_v3/blackboard.h
class Blackboard
{
  public:
    typedef std::shared_ptr<Blackboard> Ptr;

  protected:
    // This is intentionally protected. Use Blackboard::create instead
    Blackboard(Blackboard::Ptr parent): parent_bb_(parent) {}

  public:
    /** Use this static method to create an instance of the BlackBoard
    *   to share among all your NodeTrees. */
    static Blackboard::Ptr create(Blackboard::Ptr parent = {})
    {
        return std::shared_ptr<Blackboard>( new Blackboard(parent) );
    }
    // ...
  private:
    struct Entry{
        Any value;
        const PortInfo port_info;
        Entry( const PortInfo& info ): port_info(info) {}
        Entry(Any&& other_any, const PortInfo& info)
          : value(std::move(other_any)), port_info(info) {}
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> storage_;
    std::weak_ptr<Blackboard> parent_bb_;
    std::unordered_map<std::string,std::string> internal_to_external_;
};
```

Cada entrada guarda **dois** campos: o valor (`Any`) e a `PortInfo` da porta que a criou.
É o segundo que sustenta a verificação de tipo.

**REGRA — o *blackboard* nasce por `create()`, nunca por `new`.** O construtor é
`protected` de propósito: um *blackboard* é sempre um `shared_ptr`, obtido por
`Blackboard::create(pai)`. A ligação com o pai é um `weak_ptr`, então a pilha inteira só
permanece válida enquanto o `Tree` — que guarda todos eles em `blackboard_stack` —
estiver vivo.

## 8.2 API pública completa

```cpp
// include/behaviortree_cpp_v3/blackboard.h
static Blackboard::Ptr create(Blackboard::Ptr parent = {});

const Any* getAny(const std::string& key) const;
Any*       getAny(const std::string& key);

template <typename T> bool get(const std::string& key, T& value) const;  // false se ausente
template <typename T> T    get(const std::string& key) const;            // lança se ausente
template <typename T> void set(const std::string& key, const T& value);

void setPortInfo(std::string key, const PortInfo& info);
const PortInfo* portInfo(const std::string& key);
void addSubtreeRemapping(StringView internal, StringView external);
void debugMessage() const;
std::vector<StringView> getKeys() const;
```

## 8.3 `get()` e `set()` fora da árvore

Um uso comum é escrever no *blackboard* raiz antes de *ticar*, ou ler o resultado depois:

```cpp
auto tree = factory.createTreeFromFile("arvore.xml");

// escreve ANTES do primeiro tick
tree.rootBlackboard()->set("alvo", Pose2D{1.0, 2.0, 0.0});

while (tree.tickRoot() == BT::NodeStatus::RUNNING) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// le DEPOIS
std::string resultado;
if (tree.rootBlackboard()->get("resultado", resultado)) {
    std::cout << resultado << std::endl;
}
```

`Blackboard::rootBlackboard()` devolve `blackboard_stack.front()` ou `{}` se a pilha
estiver vazia. A forma `T get(key)` lança `RuntimeError("Blackboard::get() error. Missing
key [...]")` quando a chave não existe.

## 8.4 Encadeamento entre pai e filho

O encadeamento **não é uma busca hierárquica**: uma chave só sobe para o pai se houver um
remapeamento **explícito** para ela.

```cpp
// include/behaviortree_cpp_v3/blackboard.h
const Any* getAny(const std::string& key) const
{
    std::unique_lock<std::mutex> lock(mutex_);

    if( auto parent = parent_bb_.lock())
    {
        auto remapping_it = internal_to_external_.find(key);
        if( remapping_it != internal_to_external_.end())
        {
            return parent->getAny( remapping_it->second );
        }
    }
    auto it = storage_.find(key);
    return ( it == storage_.end()) ? nullptr : &(it->second.value);
}
```

```cpp
// src/blackboard.cpp
void Blackboard::addSubtreeRemapping(StringView internal, StringView external)
{
    internal_to_external_.insert( {static_cast<std::string>(internal),
                                   static_cast<std::string>(external)} );
}
```

**POR QUÊ — por que o isolamento é o padrão.** Numa árvore grande, chaves como `"goal"` ou
`"result"` aparecem em dezenas de subárvores escritas por pessoas diferentes. Se todas
compartilhassem um espaço de nomes único, cada reutilização de subárvore seria uma colisão
em potencial. A biblioteca escolhe o oposto: por padrão a subárvore não enxerga **nada** do
pai, e o que precisa atravessar a fronteira é declarado, chave por chave, no XML. O preço
é o **esquecimento silencioso** — uma chave não remapeada não dá erro, apenas não é
encontrada.

Note também que `addSubtreeRemapping` usa `insert()`, que **não substitui** uma chave
existente: remapear duas vezes a mesma chave interna mantém o primeiro remapeamento.

## 8.5 O travamento de tipo

Uma entrada criada por uma porta tipada memoriza esse tipo, e uma escrita posterior com
tipo diferente é recusada:

```cpp
// include/behaviortree_cpp_v3/blackboard.h  (Blackboard::set, condensada)
if( it != storage_.end() ) // already there. check the type
{
    const PortInfo& port_info = it->second.port_info;
    auto& previous_any = it->second.value;
    const auto locked_type = port_info.type();

    Any temp(value);

    if( locked_type && *locked_type != typeid(T) && *locked_type != temp.type() )
    {
        bool mismatching = true;
        if( std::is_constructible<StringView, T>::value )      // veio como texto?
        {
            Any any_from_string = port_info.parseString( value );   // tenta converter
            if( any_from_string.empty() == false)
            {
                mismatching = false;
                temp = std::move( any_from_string );
            }
        }
        if( mismatching )
        {
            debugMessage();
            throw LogicError( "Blackboard::set() failed: once declared, the type of a port "
                              "shall not change. Declared type [", demangle( locked_type ),
                              "] != current type [", demangle( typeid(T) ),"]" );
        }
    }
    previous_any = std::move(temp);
}
else{ // create for the first time without any info
    storage_.emplace( key, Entry( Any(value), PortInfo() ) );
}
```

A ressalva do meio é o que faz o `SetBlackboard` funcionar: escrever a *string* `"-1;3"`
numa entrada declarada como `Position2D` **não é erro** — a `PortInfo` sabe converter,
porque guardou o conversor no momento em que a porta foi declarada com
`InputPort<Position2D>`.

**ARMADILHA — a mesma verificação, feita de dois jeitos incompatíveis.**
`Blackboard::set()` compara **os objetos** (`*locked_type != typeid(T)`), que é a forma
correta. `Blackboard::setPortInfo()`, no mesmo arquivo, compara **os ponteiros**:

```cpp
// src/blackboard.cpp
auto old_type = it->second.port_info.type();
if( old_type && old_type != info.type() )     // <-- comparação de PONTEIROS
{
    throw LogicError( "Blackboard::set() failed: once declared, the type of a port "
                      "shall not change. Declared type [", BT::demangle( old_type ),
                      "] != current type [", BT::demangle( info.type() ), "]" );
}
```

Comparar ponteiros de `std::type_info` funciona dentro de um binário, mas **não há garantia
de unicidade** quando o tipo atravessa a fronteira de uma biblioteca compartilhada —
exatamente o caso de um *plugin* carregado com `dlopen`. O sintoma é um falso *"the type
of a port shall not change"* entre tipos idênticos, vindos de lados diferentes da ligação
dinâmica.

## 8.6 Inspeção: `debugMessage()` e `getKeys()`

```cpp
// src/blackboard.cpp
void Blackboard::debugMessage() const
{
    for(const auto& entry_it: storage_)
    {
        auto port_type = entry_it.second.port_info.type();
        if( !port_type )
        {
            port_type = &( entry_it.second.value.type() );
        }
        std::cout <<  entry_it.first << " (" << demangle( port_type ) << ") -> ";

        if( auto parent = parent_bb_.lock())
        {
            auto remapping_it = internal_to_external_.find( entry_it.first );
            if( remapping_it != internal_to_external_.end())
            {
                std::cout << "remapped to parent [" << remapping_it->second << "]" << std::endl;
                continue;
            }
        }
        std::cout << ((entry_it.second.value.empty()) ? "empty" : "full") <<  std::endl;
    }
}

std::vector<StringView> Blackboard::getKeys() const
{
    if( storage_.empty() ){ return {}; }
    std::vector<StringView> out;
    out.reserve( storage_.size() );
    for(const auto& entry_it: storage_)
    {
        out.push_back( entry_it.first );
    }
    return out;
}
```

Saída típica de `debugMessage()` nos dois *blackboards* do exemplo `t06`:

```
move_result (std::string) -> full
move_goal (Pose2D) -> full
--------------
output (std::string) -> remapped to parent [move_result]
target (Pose2D) -> remapped to parent [move_goal]
```

**ARMADILHA — `getKeys()` e `debugMessage()` têm defeitos reais.**

- `getKeys()` percorre `storage_` **sem tomar o *mutex***, ao contrário de todos os outros
  métodos da classe — e ainda devolve `StringView` apontando para as chaves do mapa, que
  morrem se a entrada for removida.
- `debugMessage()` faz `port_type = &(entry.value.type())` quando a porta não tem tipo
  declarado; `Any::type()` desreferencia um ponteiro que é `nullptr` num `Any` vazio — e
  entradas vazias são exatamente o que `setPortInfo()` cria.

São ferramentas de depuração, não de produção.

## 8.7 Como uma entrada do *blackboard* ganha tipo

O tipo de uma entrada é declarado pelo *parser*, na carga, a partir da porta que a
referencia:

```cpp
// src/xml_parsing.cpp  (dentro de createNodeFromXML)
// Initialize the ports in the BB to set the type
for(const auto& port_it: manifest.ports)
{
    const std::string& port_name = port_it.first;
    const auto& port_info = port_it.second;

    auto remap_it = port_remap.find(port_name);
    if( remap_it == port_remap.end()) { continue; }

    StringView param_value = remap_it->second;
    auto param_res = TreeNode::getRemappedKey(port_name, param_value);
    if( param_res )
    {
        const auto port_key = static_cast<std::string>(param_res.value());

        auto prev_info = blackboard->portInfo( port_key );
        if( !prev_info  )
        {
            // not found, insert for the first time.
            blackboard->setPortInfo( port_key, port_info );
        }
        else{
            // found. check consistency
            if( prev_info->type() && port_info.type()  && // null type means everything is valid
                *prev_info->type() != *port_info.type())
            {
                blackboard->debugMessage();
                throw RuntimeError( "The creation of the tree failed because the port [",
                                    port_key, "] was initially created with type [",
                                    demangle( prev_info->type() ), "] and, later type [",
                                    demangle( port_info.type() ),
                                    "] was used somewhere else." );
            }
        }
    }
}
```

Ou seja: **é na carga que dois nós que compartilham a mesma chave são conferidos entre
si**. Um `<A goal="{p}"/>` com `InputPort<Pose2D>("goal")` e um `<B alvo="{p}"/>` com
`InputPort<double>("alvo")` falham na carga com a mensagem acima. Esse é o principal
benefício de declarar portas com tipo.

---

# 9. `convertFromString` E TIPOS PRÓPRIOS

Toda a ponte entre o XML (texto) e o C++ (tipos) passa por uma única função *template*.

## 9.1 O caso geral

```cpp
// include/behaviortree_cpp_v3/basic_types.h
/**
 * convertFromString is used to convert a string into a custom type.
 *
 * This function is invoked under the hood by TreeNode::getInput(), but only when the
 * input port contains a string.
 *
 * If you have a custom type, you need to implement the corresponding template specialization.
 */
template <typename T> inline
T convertFromString(StringView /*str*/)
{
    auto type_name = BT::demangle( typeid(T) );

    std::cerr << "You (maybe indirectly) called BT::convertFromString() for type [" <<
                 type_name <<"], but I can't find the template specialization.\n" << std::endl;

    throw LogicError(std::string("You didn't implement the template specialization of "
                                 "convertFromString for this type: ") + type_name );
}
```

## 9.2 As especializações fornecidas

| Tipo | Implementação | Observações |
|---|---|---|
| `std::string` | `std::string(str.data(), str.size())` | cópia direta |
| `const char*` | idem, devolve ponteiro | |
| `int` | `std::stoi(str.data())` | |
| `unsigned` | `unsigned(std::stoul(str.data()))` | |
| `long` | `std::stol(str.data())` | |
| `unsigned long` | `std::stoul(str.data())` | |
| `float` | `std::stof` com `LC_NUMERIC="C"` | |
| `double` | `std::stod` com `LC_NUMERIC="C"` | correção da *issue* #120 |
| `bool` | seis grafias aceitas | ver armadilha abaixo |
| `std::vector<int>` | separados por `;` | usa `splitString` + `strtol` |
| `std::vector<double>` | separados por `;` | usa `splitString` + `strtod` |
| `NodeStatus` | nomes em maiúsculas | lança se inválido |
| `NodeType` | `"Action"`, `"Condition"`, … | devolve `UNDEFINED` se inválido |
| `PortDirection` | `"Input"`/`"INPUT"`, `"Output"`/`"OUTPUT"` | qualquer outra coisa → `INOUT` |

```cpp
// src/basic_types.cpp
template <>
bool convertFromString<bool>(StringView str)
{
    if (str.size() == 1)
    {
        if (str[0] == '0') { return false; }
        if (str[0] == '1') { return true;  }
    }
    else if (str.size() == 4)
    {
        if (str == "true" || str == "TRUE" || str == "True") { return true; }
    }
    else if (str.size() == 5)
    {
        if (str == "false" || str == "FALSE" || str == "False") { return false; }
    }
    throw RuntimeError("convertFromString(): invalid bool conversion");
}
```

**ARMADILHA — `bool` aceita seis grafias e recusa todo o resto.** `"yes"`, `"1 "` com
espaço, `"tRue"`, `"on"` → `RuntimeError("convertFromString(): invalid bool conversion")`.

```cpp
// src/basic_types.cpp
template <>
double convertFromString<double>(StringView str)
{
    // see issue #120
    // http://quick-bench.com/DWaXRWnxtxvwIMvZy2DxVPEKJnE
    std::string old_locale = setlocale(LC_NUMERIC,nullptr);
    setlocale(LC_NUMERIC,"C");
    double val = std::stod(str.data());
    setlocale(LC_NUMERIC, old_locale.c_str());
    return val;
}
```

**POR QUÊ — por que `double` mexe no *locale*.** Sem isso, uma aplicação rodando em
*locale* `pt_BR` interpretaria `"1.5"` como `1` — o separador decimal seria a vírgula. É a
correção da *issue* #120 do projeto.

**Efeito colateral:** a conversão faz duas chamadas a `setlocale` por leitura, e
`setlocale` é **global ao processo**: converter um `double` em duas *threads* ao mesmo
tempo é uma corrida.

**ARMADILHA — `StringView` não é terminada em `NUL`, mas o código age como se fosse.** As
conversões numéricas são implementadas como `std::stoi(str.data())`. Um `string_view`
**não** garante terminação em `NUL`, e `str.data()` devolve um ponteiro para o meio do
*buffer* original. Na prática funciona nos dois casos de uso reais — o atributo do
*tinyxml2* e a `std::string` do *blackboard* são terminados — mas quebra silenciosamente
quando a *view* é uma **fatia**, que é exatamente o que `splitString()` produz:
`convertFromString<double>(parts[0])` lê **além** do ponto e vírgula. O `std::stod` para
no primeiro caractere inválido, e por isso o resultado sai certo; a leitura fora dos
limites, porém, é real (e um *sanitizer* a acusa).

## 9.3 `splitString`

```cpp
// src/basic_types.cpp
// Small utility, unless you want to use <boost/algorithm/string.hpp>
std::vector<StringView> splitString(const StringView &strToSplit, char delimeter)
{
    std::vector<StringView> splitted_strings;
    splitted_strings.reserve(4);

    size_t pos = 0;
    while( pos < strToSplit.size())
    {
        size_t new_pos = strToSplit.find_first_of(delimeter, pos);
        if( new_pos == std::string::npos) { new_pos = strToSplit.size(); }
        StringView sv = { &strToSplit.data()[pos], new_pos - pos };
        splitted_strings.push_back( sv );
        pos = new_pos + 1;
    }
    return splitted_strings;
}
```

Comportamentos a conhecer:

- uma *string* vazia devolve vetor **vazio** (o laço nem começa);
- `"a;;b"` devolve três pedaços, sendo o do meio vazio;
- `"a;"` devolve **um** pedaço (`"a"`), porque `pos` passa do fim;
- os pedaços são `StringView` **não terminadas em NUL** — ver armadilha acima.

## 9.4 Especializando para um tipo próprio

A especialização precisa estar **dentro do *namespace* `BT`**:

```cpp
// padrao usado por todos os exemplos do repositorio
struct Position2D { double x, y; };

namespace BT
{
template <> inline Position2D convertFromString(StringView str)
{
    printf("Converting string: \"%s\"\n", str.data() );

    // real numbers separated by semicolons
    auto parts = splitString(str, ';');
    if (parts.size() != 2)
    {
        throw RuntimeError("invalid input)");
    }
    else{
        Position2D output;
        output.x     = convertFromString<double>(parts[0]);
        output.y     = convertFromString<double>(parts[1]);
        return output;
    }
}
} // end namespace BT
```

A convenção de separar campos por **ponto e vírgula** não é imposta pela biblioteca: vem
das especializações de `std::vector<int>`/`std::vector<double>`, e todos os exemplos a
seguem por consistência.

**REGRA — `inline` na especialização em cabeçalho.** Se a especialização estiver num
`.h` incluído por mais de uma unidade de tradução, ela precisa ser `inline` (ou estar num
`.cpp` único), senão o *linker* acusa símbolo duplicado. Os exemplos do repositório usam
`template <> inline Pose2D convertFromString(StringView key)` em
`sample_nodes/movebase_node.h` justamente por isso.

**REGRA — o tipo precisa ser copiável e ter construtor padrão utilizável.** `getInput(key,
destination)` escreve num `T&`; a forma `Optional<T> getInput(key)` faz `T out;` antes de
chamar a outra. Um tipo sem construtor padrão não compila nessa segunda forma.

## 9.5 `toStr()` — o caminho inverso

```cpp
// include/behaviortree_cpp_v3/basic_types.h
template <typename T> std::string toStr(T value) { return std::to_string(value); }
std::string toStr(std::string value);
template<> std::string toStr<BT::NodeStatus>(BT::NodeStatus status);
std::string toStr(BT::NodeStatus status, bool colored);
template<> std::string toStr<BT::NodeType>(BT::NodeType type);
template<> std::string toStr<BT::PortDirection>(BT::PortDirection direction);
```

`toStr()` é usado em **um** lugar importante: converter o valor padrão de uma porta para
texto, dentro de `InputPort<T>(name, default_value, description)`. Consequência prática:
**um tipo próprio só pode ter valor padrão se `toStr()` funcionar para ele** — ou seja,
se `std::to_string` aceitar o tipo. Para um `struct` próprio isso não compila, e o padrão
precisa ser dado de outra forma (por exemplo, lendo com a forma
`getInput(key, destino_já_inicializado)` e ignorando a falha).

---

# 10. NÓS DE CONTROLE

Os nós de controle são o lugar onde a lógica mora. Uma folha diz **o que** fazer; um nó de
controle diz **em que ordem**, **sob que condição** e **até quando**.

São nove famílias, treze IDs de registro (contando as cinco variações do `Switch`), mais o
`ManualSelector` condicional.

## 10.1 Visão de conjunto

| ID no XML | Sucesso quando | Falha quando | Memória | Portas |
|---|---|---|---|---|
| `Sequence` | todos têm sucesso | um falha | sim, zerada ao falhar | — |
| `SequenceStar` | todos têm sucesso | um falha | sim, mantida ao falhar | — |
| `ReactiveSequence` | todos têm sucesso | um falha | não | — |
| `Fallback` | um tem sucesso | todos falham | sim, zerada ao ter sucesso | — |
| `ReactiveFallback` | um tem sucesso | todos falham | não | — |
| `Parallel` | $M$ têm sucesso | $N$ falham | parcial (*skip list*) | `success_threshold`, `failure_threshold` |
| `IfThenElse` | o ramo escolhido | o ramo escolhido | sim | — |
| `WhileDoElse` | o ramo escolhido | o ramo escolhido | não | — |
| `Switch2`…`Switch6` | o caso escolhido | o caso escolhido | sim (qual ramo roda) | `variable`, `case_1`…`case_N` |
| `ManualSelector` | escolha do operador | escolha do operador | sim | `repeat_last_selection` |

Três observações que valem para **todos**:

1. **Nenhum deles usa *threads*.** Inclusive o `ParallelNode`: "paralelo" aqui quer dizer
   que vários filhos estão *logicamente* em curso, não que executem simultaneamente. A
   travessia é sequencial dentro de um mesmo *tick*.
2. **Quase todos começam com `setStatus(NodeStatus::RUNNING)`.** Isso publica a transição
   para os *loggers* antes de qualquer filho ser *ticado* — é o que faz o traço de
   execução mostrar o pai entrando antes dos filhos. **As duas exceções são os reativos**
   (`ReactiveSequence` e `ReactiveFallback`), que não o fazem; neles o `RUNNING` do pai
   aparece no traço **depois** do primeiro filho.
3. **Um filho que devolve `IDLE` é erro fatal.** Todos lançam, na mesma linha do `switch`:
   `LogicError("A child node must never return IDLE")`.

## 10.2 `Sequence` — o E lógico

**ID de registro:** `Sequence` · **Classe:** `BT::SequenceNode` ·
**Arquivos:** `include/behaviortree_cpp_v3/controls/sequence_node.h`,
`src/controls/sequence_node.cpp`

Executa os filhos da esquerda para a direita e para no primeiro que falha.

```cpp
// src/controls/sequence_node.cpp  (íntegro, sem o cabeçalho de licença)
SequenceNode::SequenceNode(const std::string& name)
    : ControlNode::ControlNode(name, {} )
  , current_child_idx_(0)
{
    setRegistrationID("Sequence");
}

void SequenceNode::halt()
{
    current_child_idx_ = 0;
    ControlNode::halt();
}

NodeStatus SequenceNode::tick()
{
    const size_t children_count = children_nodes_.size();

    setStatus(NodeStatus::RUNNING);

    while (current_child_idx_ < children_count)
    {
        TreeNode* current_child_node = children_nodes_[current_child_idx_];
        const NodeStatus child_status = current_child_node->executeTick();

        switch (child_status)
        {
            case NodeStatus::RUNNING:
            {
                return child_status;
            }
            case NodeStatus::FAILURE:
            {
                // Reset on failure
                haltChildren();
                current_child_idx_ = 0;
                return child_status;
            }
            case NodeStatus::SUCCESS:
            {
                current_child_idx_++;
            }
            break;

            case NodeStatus::IDLE:
            {
                throw LogicError("A child node must never return IDLE");
            }
        }   // end switch
    }       // end while loop

    // The entire while loop completed. This means that all the children returned SUCCESS.
    if (current_child_idx_ == children_count)
    {
        haltChildren();
        current_child_idx_ = 0;
    }
    return NodeStatus::SUCCESS;
}
```

**Como ler:** o laço `while` é a memória. Enquanto um filho devolve `SUCCESS`, o índice
avança e o *tick* continua **no mesmo tick**. Só o `RUNNING` devolve o controle à
aplicação. Ao concluir — por sucesso total ou por falha — o nó chama `haltChildren()` e
zera o índice, o que devolve todos os filhos a `IDLE` e os torna executáveis de novo no
próximo ciclo.

```xml
<Sequence name="montagem">
    <AbrirGarra/>
    <Aproximar goal="1;2;0"/>
    <FecharGarra/>
</Sequence>
```

**ARMADILHA — o `reset_on_failure` do cabeçalho não existe.** O comentário de
`sequence_node.h` diz:

```
 * - If a child returns FAILURE, stop the loop and return FAILURE.
 *   Restart the loop only if (reset_on_failure == true)
```

Não há membro, porta ou opção com esse nome em lugar nenhum da biblioteca. O
`docs/MigrationGuide.md` explica a origem: na v2 existia um `SequenceStar` com esse
parâmetro, e a variante `reset_on_failure=true` foi promovida a `Sequence` na v3 — o
comentário sobreviveu à promoção. O comportamento real é incondicional: **ao falhar, a
`Sequence` sempre recomeça do primeiro filho.**

**Quando usar:** quando reexecutar os passos já concluídos após uma falha é correto (ou
barato). É o padrão para sequências idempotentes.

## 10.3 `SequenceStar` — o E que não esquece

**ID de registro:** `SequenceStar` · **Classe:** `BT::SequenceStarNode` ·
**Arquivos:** `controls/sequence_star_node.h`, `src/controls/sequence_star_node.cpp`

A diferença em relação à `Sequence` cabe em duas linhas de código, e muda tudo.

```cpp
// src/controls/sequence_star_node.cpp
NodeStatus SequenceStarNode::tick()
{
    const size_t children_count = children_nodes_.size();

    setStatus(NodeStatus::RUNNING);

    while (current_child_idx_ < children_count)
    {
        TreeNode* current_child_node = children_nodes_[current_child_idx_];
        const NodeStatus child_status = current_child_node->executeTick();

        switch (child_status)
        {
            case NodeStatus::RUNNING:
            {
                return child_status;
            }
            case NodeStatus::FAILURE:
            {
                // DO NOT reset current_child_idx_ on failure
                for(size_t i=current_child_idx_; i < childrenCount(); i++)
                {
                    haltChild(i);
                }
                return child_status;
            }
            case NodeStatus::SUCCESS:
            {
                current_child_idx_++;
            }
            break;

            case NodeStatus::IDLE:
            {
                throw LogicError("A child node must never return IDLE");
            }
        }   // end switch
    }       // end while loop

    // The entire while loop completed. This means that all the children returned SUCCESS.
    if (current_child_idx_ == children_count)
    {
        haltChildren();
        current_child_idx_ = 0;
    }
    return NodeStatus::SUCCESS;
}

void SequenceStarNode::halt()
{
    current_child_idx_ = 0;
    ControlNode::halt();
}
```

Duas mudanças: o índice **não é zerado** e apenas os filhos **a partir** do corrente são
devolvidos a `IDLE` — os já concluídos permanecem em `SUCCESS`. No *tick* seguinte, o nó
retoma exatamente do filho que falhou.

**POR QUÊ — para que serve uma sequência que não recomeça.** Para procedimentos
irreversíveis. "Abra a garra, aproxime, feche a garra": se a aproximação falhar, recomeçar
do zero reabriria a garra — desfazendo trabalho que já estava feito. A `SequenceStar` é a
forma de dizer "o que já deu certo, ficou certo". É o padrão em máquinas de montagem e em
qualquer sequência com efeitos colaterais no mundo.

**ARMADILHA — a memória da `SequenceStar` sobrevive à falha, mas não ao *halt*.**
`SequenceStar::halt()` zera o índice. Como `ControlNode::haltChild()` só chama `halt()`
num filho que esteja `RUNNING`, e a `SequenceStar` está em `FAILURE` quando o pai a
recolhe, na prática a memória sobrevive — que é o comportamento desejado. Mas basta que
ela seja *halted* enquanto `RUNNING` — por um `Timeout` que expira, por um irmão à
esquerda de uma `ReactiveSequence` que passa a falhar, ou por `Tree::haltTree()` — para
que o progresso todo se perca sem aviso. **Se a retomada precisa ser garantida, guarde-a
numa entrada do *blackboard*, não no nó.**

## 10.4 `ReactiveSequence` — o E que reavalia tudo

**ID de registro:** `ReactiveSequence` · **Classe:** `BT::ReactiveSequence` ·
**Arquivos:** `controls/reactive_sequence.h`, `src/controls/reactive_sequence.cpp`

```cpp
// src/controls/reactive_sequence.cpp  (íntegro)
NodeStatus ReactiveSequence::tick()
{
    size_t success_count = 0;
    size_t running_count = 0;

    for (size_t index = 0; index < childrenCount(); index++)
    {
        TreeNode* current_child_node = children_nodes_[index];
        const NodeStatus child_status = current_child_node->executeTick();

        switch (child_status)
        {
            case NodeStatus::RUNNING:
            {
                running_count++;

                for(size_t i=index+1; i < childrenCount(); i++)
                {
                    haltChild(i);
                }
                return NodeStatus::RUNNING;
            }

            case NodeStatus::FAILURE:
            {
                haltChildren();
                return NodeStatus::FAILURE;
            }
            case NodeStatus::SUCCESS:
            {
                success_count++;
            }break;

            case NodeStatus::IDLE:
            {
                throw LogicError("A child node must never return IDLE");
            }
        }   // end switch
    } //end for

    if( success_count == childrenCount())
    {
        haltChildren();
        return NodeStatus::SUCCESS;
    }
    return NodeStatus::RUNNING;
}
```

Não há índice: o laço começa em zero a cada *tick*. Um filho que já tinha dado `SUCCESS` é
reexecutado, e se agora falhar, todo o restante é *halted*.

**REGRA — o padrão guarda-e-trabalho.** A forma canônica de usar uma `ReactiveSequence` é
pôr $N$ condições **baratas** nas primeiras posições e uma única ação assíncrona na
última. Toda condição vira, assim, uma **precondição contínua**: se qualquer uma deixar de
valer, a ação é interrompida no mesmo *tick*.

```xml
<ReactiveSequence>
    <BateriaOK/>            <!-- condição barata, reavaliada a cada tick -->
    <SemObstaculo/>         <!-- idem -->
    <MoveBase goal="{alvo}"/>  <!-- única ação assíncrona -->
</ReactiveSequence>
```

**ARMADILHA — o *halt* dos irmãos à direita é o que quebra dois assíncronos.** O laço
`for(i=index+1; ...) haltChild(i)` explica por que a `ReactiveSequence` não suporta dois
filhos assíncronos: assim que o primeiro deles devolve `RUNNING`, o segundo é *halted* —
perdendo todo o progresso — e recomeçado do zero assim que o primeiro terminar. Com duas
ações longas em sequência reativa, a segunda pode **nunca progredir**.

**Nota de código morto:** o contador `running_count` é incrementado e nunca lido.
Sobrevivente de uma versão anterior em que a `ReactiveSequence` era mais parecida com o
`ParallelNode`.

**ARMADILHA sutil — "reativo" não basta para tornar uma ação reexecutável.** Um
`StatefulActionNode` que já concluiu com `SUCCESS` continua sendo *ticado* pela
`ReactiveSequence` a cada passagem, mas `StatefulActionNode::tick()` só chama `onStart()`
quando o estado é `IDLE`; em `SUCCESS`, devolve o próprio estado sem executar nada. E a
`ReactiveSequence` **não** repõe a `IDLE` os filhos **à esquerda** do que está `RUNNING` —
apenas os à direita. Resultado: trocar uma `<Sequence>` por `<ReactiveSequence>` pode não
mudar nada do comportamento observável, apenas a ordem das linhas no traço.

## 10.5 `Fallback` — o OU lógico

**ID de registro:** `Fallback` · **Classe:** `BT::FallbackNode` ·
**Arquivos:** `controls/fallback_node.h`, `src/controls/fallback_node.cpp`

Espelho exato da `Sequence`: percorre os filhos e para no primeiro que **tem sucesso**; se
todos falharem, falha.

```cpp
// src/controls/fallback_node.cpp
NodeStatus FallbackNode::tick()
{
    const size_t children_count = children_nodes_.size();

    setStatus(NodeStatus::RUNNING);

    while (current_child_idx_ < children_count)
    {
        TreeNode* current_child_node = children_nodes_[current_child_idx_];
        const NodeStatus child_status = current_child_node->executeTick();

        switch (child_status)
        {
            case NodeStatus::RUNNING:
            {
                return child_status;
            }
            case NodeStatus::SUCCESS:
            {
                haltChildren();
                current_child_idx_ = 0;
                return child_status;
            }
            case NodeStatus::FAILURE:
            {
                current_child_idx_++;
            }
            break;

            case NodeStatus::IDLE:
            {
                throw LogicError("A child node must never return IDLE");
            }
        }   // end switch
    }       // end while loop

    // The entire while loop completed. This means that all the children returned FAILURE.
    if (current_child_idx_ == children_count)
    {
        haltChildren();
        current_child_idx_ = 0;
    }

    return NodeStatus::FAILURE;
}

void FallbackNode::halt()
{
    current_child_idx_ = 0;
    ControlNode::halt();
}
```

Leitura idiomática: **"tente A; se não der, B; se não der, C"** — a lista de estratégias
em ordem de preferência.

```xml
<Fallback name="estrategias">
    <Sequence>
        <IsDoorOpen/>
        <PassThroughDoor/>
    </Sequence>
    <SubTree ID="DoorClosed"/>
    <PassThroughWindow/>
</Fallback>
```

## 10.6 `ReactiveFallback` — o OU que reavalia

**ID de registro:** `ReactiveFallback` · **Classe:** `BT::ReactiveFallback`

```cpp
// src/controls/reactive_fallback.cpp  (íntegro)
NodeStatus ReactiveFallback::tick()
{
    size_t failure_count = 0;

    for (size_t index = 0; index < childrenCount(); index++)
    {
        TreeNode* current_child_node = children_nodes_[index];
        const NodeStatus child_status = current_child_node->executeTick();

        switch (child_status)
        {
            case NodeStatus::RUNNING:
            {
                for(size_t i=index+1; i < childrenCount(); i++)
                {
                    haltChild(i);
                }
                return NodeStatus::RUNNING;
            }

            case NodeStatus::FAILURE:
            {
                failure_count++;
            }break;

            case NodeStatus::SUCCESS:
            {
                haltChildren();
                return NodeStatus::SUCCESS;
            }

            case NodeStatus::IDLE:
            {
                throw LogicError("A child node must never return IDLE");
            }
        }   // end switch
    } //end for

    if( failure_count == childrenCount() )
    {
        haltChildren();
        return NodeStatus::FAILURE;
    }

    return NodeStatus::RUNNING;
}
```

Leitura: "tente a primeira estratégia; se ela falhar, a segunda; e **reavalie a
preferência a cada *tick***". É a forma de exprimir prioridade dinâmica: se a estratégia
preferida voltar a ser viável no meio da execução da alternativa, a alternativa é
*halted*.

Mesma restrição de um único filho assíncrono.

## 10.7 Os nomes da v2 significam o oposto na v3

Esta é a incompatibilidade mais perigosa que um XML antigo pode carregar, porque os nomes
continuam válidos e **mudaram de sentido**. Tabela do `docs/MigrationGuide.md`:

| Nome na v2 | Nome na v3 | É reativo? |
|---|---|---|
| `Sequence` | `ReactiveSequence` | sim |
| `SequenceStar (reset_on_failure=true)` | `Sequence` | não |
| `SequenceStar (reset_on_failure=false)` | `SequenceStar` | não |
| `Fallback` | `ReactiveFallback` | sim |
| `FallbackStar` | `Fallback` | não |
| `Parallel` | `Parallel` | sim (v2) / não (v3) |

**POR QUÊ — por que o padrão deixou de ser reativo.** O texto do guia é explícito:
*"I don't think reactive ControlNodes should be used mindlessly by default."* A v2 fazia
da reatividade o padrão, e o resultado prático era que a maioria dos usuários usava um nó
reativo onde queria um com memória — sem perceber, até que uma condição à esquerda
mudasse no meio de uma operação. A v3 inverteu a escolha e deu nome explícito à
reatividade.

Um `FallbackStar` num XML antigo dá `"Node not recognized"` na carga, porque o ID
desapareceu — o que, neste caso, é a falha **benigna**. A perigosa é o `<Sequence>` antigo,
que carrega e roda com semântica trocada.

## 10.8 `Parallel` — $M$ de $N$

**ID de registro:** `Parallel` · **Classe:** `BT::ParallelNode` ·
**Arquivos:** `controls/parallel_node.h`, `src/controls/parallel_node.cpp`

```cpp
// include/behaviortree_cpp_v3/controls/parallel_node.h
class ParallelNode : public ControlNode
{
  public:
    ParallelNode(const std::string& name, unsigned success_threshold,
                 unsigned failure_threshold = 1);
    ParallelNode(const std::string& name, const NodeConfiguration& config);

    static PortsList providedPorts()
    {
        return { InputPort<unsigned>(THRESHOLD_SUCCESS,
                     "number of childen which need to succeed to trigger a SUCCESS" ),
                 InputPort<unsigned>(THRESHOLD_FAILURE, 1,
                     "number of childen which need to fail to trigger a FAILURE" ) };
    }

    virtual void halt() override;

    unsigned int thresholdM();
    unsigned int thresholdFM();
    void setThresholdM(unsigned int threshold_M);
    void setThresholdFM(unsigned int threshold_M);

  private:
    unsigned int success_threshold_;
    unsigned int failure_threshold_;
    std::set<int> skip_list_;
    bool read_parameter_from_ports_;
    static constexpr const char* THRESHOLD_SUCCESS = "success_threshold";
    static constexpr const char* THRESHOLD_FAILURE = "failure_threshold";

    virtual BT::NodeStatus tick() override;
};
```

```cpp
// src/controls/parallel_node.cpp  (tick, condensado)
NodeStatus ParallelNode::tick()
{
    if(read_parameter_from_ports_)
    {
        if( !getInput(THRESHOLD_SUCCESS, success_threshold_) )
            throw RuntimeError("Missing parameter [", THRESHOLD_SUCCESS, "] in ParallelNode");
        if( !getInput(THRESHOLD_FAILURE, failure_threshold_) )
            throw RuntimeError("Missing parameter [", THRESHOLD_FAILURE, "] in ParallelNode");
    }

    size_t success_childred_num = 0;
    size_t failure_childred_num = 0;
    const size_t children_count = children_nodes_.size();

    if( children_count < success_threshold_)
        throw LogicError("Number of children is less than threshold. Can never succeed.");
    if( children_count < failure_threshold_)
        throw LogicError("Number of children is less than threshold. Can never fail.");

    for (unsigned int i = 0; i < children_count; i++)
    {
        TreeNode* child_node = children_nodes_[i];
        bool in_skip_list = (skip_list_.count(i) != 0);

        NodeStatus child_status;
        if( in_skip_list ) { child_status = child_node->status();      }
        else               { child_status = child_node->executeTick(); }

        switch (child_status)
        {
            case NodeStatus::SUCCESS:
            {
                if( !in_skip_list ) { skip_list_.insert(i); }
                success_childred_num++;
                if (success_childred_num == success_threshold_)
                {
                    skip_list_.clear();
                    haltChildren();
                    return NodeStatus::SUCCESS;
                }
            } break;

            case NodeStatus::FAILURE:
            {
                if( !in_skip_list ) { skip_list_.insert(i); }
                failure_childred_num++;
                // It fails if it is not possible to succeed anymore or if
                // number of failures are equal to failure_threshold_
                if ((failure_childred_num > children_count - success_threshold_)
                    || (failure_childred_num == failure_threshold_))
                {
                    skip_list_.clear();
                    haltChildren();
                    return NodeStatus::FAILURE;
                }
            } break;

            case NodeStatus::RUNNING:
            {
                // do nothing
            }  break;

            default:
            {
                throw LogicError("A child node must never return IDLE");
            }
        }
    }
    return NodeStatus::RUNNING;
}

void ParallelNode::halt()
{
    skip_list_.clear();
    ControlNode::halt();
}
```

**A condição de falha tem duas metades:** a explícita
(`failure_count == failure_threshold`) e a implícita — se já falharam tantos que os
restantes não bastam para atingir o limiar de sucesso, não há por que continuar.

```xml
<!-- sucesso quando 2 dos 3 concluírem; falha quando 2 falharem -->
<Parallel success_threshold="2" failure_threshold="2">
    <MonitorarBateria/>
    <MoveBase goal="{a}"/>
    <PublicarTelemetria/>
</Parallel>
```

**ARMADILHA — `success_threshold` é obrigatório; o outro, não.** Só `failure_threshold`
tem valor padrão (1). Um `<Parallel>` escrito sem `success_threshold` compila, carrega e
falha **no primeiro *tick*** com
`RuntimeError("Missing parameter [success_threshold] in ParallelNode")` — em tempo de
execução, não de carga. **Escreva sempre os dois.**

**ARMADILHA — um `Parallel` não paraleliza nada.** O nome vem da literatura de árvores de
comportamento, não de concorrência. Os filhos são *ticados* um após o outro, na mesma
*thread*, dentro do mesmo *tick*. Se dois deles forem `SyncActionNode` de 100 ms cada, o
*tick* leva 200 ms. Concorrência real exige que os filhos sejam **assíncronos**; o
`Parallel` apenas permite que várias ações assíncronas estejam **pendentes** ao mesmo
tempo.

**ARMADILHA — os limiares são lidos a cada *tick*, e podem mudar.** Quando construído pelo
XML, `read_parameter_from_ports_` é verdadeiro e os dois limiares são relidos no início de
**cada** `tick()`. Se estiverem remapeados para o *blackboard*, um nó anterior pode
alterá-los no meio de um ciclo — com a *skip list* já parcialmente preenchida e as
contagens recomeçando do zero.

**Nota sobre a *skip list*:** ela guarda os índices dos filhos que já concluíram, para não
os *ticar* de novo dentro do mesmo ciclo. Um filho na *skip list* tem o seu
`status()` **lido** em vez de ser *ticado* — e como ninguém o repôs a `IDLE`, o valor
`SUCCESS`/`FAILURE` persiste corretamente.

## 10.9 `IfThenElse`

**ID de registro:** `IfThenElse` · **Classe:** `BT::IfThenElseNode` ·
**Arquivos:** `controls/if_then_else_node.h`, `src/controls/if_then_else_node.cpp`

Aceita **2 ou 3** filhos e **não é reativo**.

```cpp
// src/controls/if_then_else_node.cpp  (íntegro)
IfThenElseNode::IfThenElseNode(const std::string &name)
  : ControlNode::ControlNode(name, {} )
  , child_idx_(0)
{
  setRegistrationID("IfThenElse");
}

void IfThenElseNode::halt()
{
  child_idx_ = 0;
  ControlNode::halt();
}

NodeStatus IfThenElseNode::tick()
{
  const size_t children_count = children_nodes_.size();

  if(children_count != 2 && children_count != 3)
  {
    throw std::logic_error("IfThenElseNode must have either 2 or 3 children");
  }

  setStatus(NodeStatus::RUNNING);

  if (child_idx_ == 0)
  {
    NodeStatus condition_status = children_nodes_[0]->executeTick();

    if (condition_status == NodeStatus::RUNNING)
    {
      return condition_status;
    }
    else if (condition_status == NodeStatus::SUCCESS)
    {
      child_idx_ = 1;
    }
    else if (condition_status == NodeStatus::FAILURE)
    {
      if( children_count == 3){
        child_idx_ = 2;
      }
      else{
        return condition_status;
      }
    }
  }
  // not an else
  if (child_idx_ > 0)
  {
    NodeStatus status = children_nodes_[child_idx_]->executeTick();
    if (status == NodeStatus::RUNNING)
    {
      return NodeStatus::RUNNING;
    }
    else{
      haltChildren();
      child_idx_ = 0;
      return status;
    }
  }

  throw std::logic_error("Something unexpected happened in IfThenElseNode");
}
```

Com apenas 2 filhos, a falha da condição é a falha do nó — equivalente a ter
`AlwaysFailure` como terceiro filho (é o que o cabeçalho diz).

Note que, quando a condição devolve `SUCCESS`, o ramo escolhido é *ticado* **no mesmo
tick** (o segundo `if` não é um `else`). Depois disso, `child_idx_` fixa o ramo e a
condição **só volta a ser avaliada quando o ramo terminar**.

```xml
<IfThenElse>
    <BateriaBaixa/>          <!-- condição -->
    <IrParaBase/>            <!-- então -->
    <ContinuarMissao/>       <!-- senão (opcional) -->
</IfThenElse>
```

**ARMADILHA — a exceção é `std::logic_error` cru**, fora da hierarquia
`BehaviorTreeException`, e só é lançada no primeiro *tick* — a validação do XML não confere
aridade de nós de controle escritos na forma compacta.

## 10.10 `WhileDoElse`

**ID de registro:** `WhileDoElse` · **Classe:** `BT::WhileDoElseNode`

É o `IfThenElse` **reativo**: a condição é reavaliada a cada *tick*, e se ela mudar de
resultado, o ramo em curso é interrompido antes de o outro começar.

```cpp
// src/controls/while_do_else_node.cpp  (íntegro)
WhileDoElseNode::WhileDoElseNode(const std::string &name)
  : ControlNode::ControlNode(name, {} )
{
  setRegistrationID("WhileDoElse");
}

void WhileDoElseNode::halt()
{
  ControlNode::halt();
}

NodeStatus WhileDoElseNode::tick()
{
  const size_t children_count = children_nodes_.size();

  if(children_count != 3)
  {
    throw std::logic_error("WhileDoElse must have either 2 or 3 children");
  }

  setStatus(NodeStatus::RUNNING);

  NodeStatus condition_status = children_nodes_[0]->executeTick();

  if (condition_status == NodeStatus::RUNNING)
  {
    return condition_status;
  }

  NodeStatus status = NodeStatus::IDLE;

  if (condition_status == NodeStatus::SUCCESS)
  {
    haltChild(2);
    status = children_nodes_[1]->executeTick();
  }
  else if (condition_status == NodeStatus::FAILURE)
  {
    haltChild(1);
    status = children_nodes_[2]->executeTick();
  }

  if (status == NodeStatus::RUNNING)
  {
    return NodeStatus::RUNNING;
  }
  else
  {
    haltChildren();
    return status;
  }
}
```

**ARMADILHA — `WhileDoElse` exige exatamente 3 filhos, apesar do que diz a mensagem.** O
cabeçalho anuncia *"must have exactly 2 or 3 children"* e a própria mensagem de erro
repete *"WhileDoElse must have either 2 or 3 children"* — mas a condição que a dispara é
`if(children_count != 3)`. **Com dois filhos, o nó lança.** Some-se a isso que a exceção é
um `std::logic_error` cru e que ela só ocorre no primeiro *tick*.

## 10.11 `Switch2` … `Switch6`

**IDs de registro:** `Switch2`, `Switch3`, `Switch4`, `Switch5`, `Switch6` ·
**Classe:** `BT::SwitchNode<N>` (*template*) ·
**Arquivo:** `include/behaviortree_cpp_v3/controls/switch_node.h` (implementação inteira
no cabeçalho; `src/controls/switch_node.cpp` só faz os `#include`)

```cpp
// include/behaviortree_cpp_v3/controls/switch_node.h
/**
 * @brief The SwitchNode is equivalent to a switch statement, where a certain
 * branch (child) is executed according to the value of a blackboard entry.
 *
 * Note that the same behaviour can be achieved with multiple Sequences, Fallbacks and
 * Conditions reading the blackboard, but switch is shorter and more readable.
 */
template <size_t NUM_CASES>
class SwitchNode : public ControlNode
{
  public:
    SwitchNode(const std::string& name, const BT::NodeConfiguration& config)
    : ControlNode::ControlNode(name, config ),
      running_child_(-1)
    {
        setRegistrationID("Switch");
    }

    void halt() override
    {
        running_child_ = -1;
        ControlNode::halt();
    }

    static PortsList providedPorts()
    {
        PortsList ports;
        ports.insert( BT::InputPort<std::string>("variable") );
        for(unsigned i=0; i < NUM_CASES; i++)
        {
            char case_str[20];
            sprintf(case_str, "case_%d", i+1);
            ports.insert( BT::InputPort<std::string>(case_str) );
        }
        return ports;
    }

  private:
    int running_child_;
    virtual BT::NodeStatus tick() override;
};

template<size_t NUM_CASES> inline
NodeStatus SwitchNode<NUM_CASES>::tick()
{
    constexpr const char * case_port_names[9] = {
      "case_1", "case_2", "case_3", "case_4", "case_5",
      "case_6", "case_7", "case_8", "case_9"};

    if( childrenCount() != NUM_CASES+1)
    {
        throw LogicError("Wrong number of children in SwitchNode; "
                         "must be (num_cases + default)");
    }

    std::string variable;
    std::string value;
    int child_index = NUM_CASES; // default index;

    if (getInput("variable", variable)) // no variable? jump to default
    {
        // check each case until you find a match
        for (unsigned index = 0; index < NUM_CASES; ++index)
        {
            bool found = false;
            if( index < 9 )
            {
                found = (bool)getInput(case_port_names[index], value);
            }
            else{
                char case_str[20];
                sprintf(case_str, "case_%d", index+1);
                found = (bool)getInput(case_str, value);
            }

            if (found && variable == value)
            {
                child_index = index;
                break;
            }
        }
    }

    // if another one was running earlier, halt it
    if( running_child_ != -1 && running_child_ != child_index)
    {
        haltChild(running_child_);
    }

    auto& selected_child = children_nodes_[child_index];
    NodeStatus ret = selected_child->executeTick();
    if( ret == NodeStatus::RUNNING )
    {
        running_child_ = child_index;
    }
    else{
        haltChildren();
        running_child_ = -1;
    }
    return ret;
}
```

Exemplo do próprio cabeçalho — note que um `Switch3` tem **quatro** filhos:

```xml
<Switch3 variable="{var}"  case_1="1" case_2="42" case_3="666" >
   <ActionA name="action_when_var_eq_1" />
   <ActionB name="action_when_var_eq_42" />
   <ActionC name="action_when_var_eq_666" />
   <ActionD name="default_action" />
</Switch3>
```

**ARMADILHA — a comparação é textual, e isso importa.** Tanto `variable` quanto os
`case_i` são lidos como `std::string`, e a comparação é `variable == value`. Se a entrada
do *blackboard* guardar um `double`, a conversão para texto passa por `std::to_string`,
que produz `"42.000000"` — e `case_1="42"` **não** casa. **Guarde `int` ou `std::string`
na variável do *switch*, nunca ponto flutuante.** Se `variable` não puder ser lida, o nó
cai silenciosamente no *default*; **não há erro**.

**ARMADILHA — o número de filhos é conferido só no primeiro *tick*.** Um `Switch3` com
três filhos carrega sem reclamar e explode na primeira execução com
`LogicError("Wrong number of children in SwitchNode; must be (num_cases + default)")`.

**Nota:** o construtor chama `setRegistrationID("Switch")`, mas o valor é sobrescrito por
`instantiateTreeNode()` com o ID real (`Switch3`, etc.). Só `Switch2`…`Switch6` estão
registrados; para `Switch7` ou mais, registre você mesmo:
`factory.registerNodeType<BT::SwitchNode<7>>("Switch7")`.

## 10.12 `ManualSelector`

**ID de registro:** `ManualSelector` · **Classe:** `BT::ManualSelectorNode` ·
**Arquivos:** `controls/manual_node.h`, `src/controls/manual_node.cpp`

Abre uma interface de texto em **ncurses** e pergunta ao operador qual filho executar —
ou, opcionalmente, qual `NodeStatus` devolver diretamente. Serve para depurar uma árvore
cujos ramos ainda não existem e para exercitar caminhos raros.

```cpp
// include/behaviortree_cpp_v3/controls/manual_node.h
/**
 * @brief Use a Terminal User Interface (ncurses) to select a certain child manually.
 */
class ManualSelectorNode : public ControlNode
{
  public:
    ManualSelectorNode(const std::string& name, const NodeConfiguration& config);
    virtual void halt() override;

    static PortsList providedPorts()
    {
        return { InputPort<bool>(REPEAT_LAST_SELECTION, false,
                     "If true, execute again the same child that was selected the last time") };
    }

  private:
    static constexpr const char* REPEAT_LAST_SELECTION = "repeat_last_selection";

    virtual BT::NodeStatus tick() override;
    int running_child_idx_;
    int previously_executed_idx_;

    enum NumericarStatus{
        NUM_SUCCESS = 253,
        NUM_FAILURE = 254,
        NUM_RUNNING = 255,
    };

    NodeStatus selectStatus() const;
    uint8_t selectChild() const;
};
```

Exemplo de uso (de `examples/t12_ncurses_manual_selector.cpp`):

```xml
<Repeat num_cycles="3">
    <ManualSelector repeat_last_selection="0">
        <SaySomething name="Option1"    message="Option1" />
        <SaySomething name="Option2"    message="Option2" />
        <SaySomething name="Option3"    message="Option3" />
        <SaySomething name="Option4"    message="Option4" />
        <ManualSelector name="YouChoose" />
    </ManualSelector>
</Repeat>
```

**ARMADILHA — não existe no empacotamento documentado.** O registro é condicionado a
`#ifdef NCURSES_FOUND`, e `src/controls/manual_node.cpp` só entra na compilação se
`BUILD_WITH_CURSES` estiver ligado **e** o CMake achar a biblioteca. A receita Conan
desliga `BUILD_WITH_CURSES`, então um XML que use `<ManualSelector>` falha na carga com
`"Node not recognized"` — e **não** com uma mensagem sobre *ncurses*.

## 10.13 Escolhendo o nó de controle

```
preciso de TODOS os filhos (E lógico)?
    ├── e reexecutar o que já deu certo é OK?        → Sequence
    ├── e o que já deu certo deve ficar feito?       → SequenceStar
    └── e preciso reavaliar guardas a cada tick?     → ReactiveSequence

preciso de UM dos filhos (OU lógico)?
    ├── com memória (não reavalia a preferência)     → Fallback
    └── reavaliando a preferência a cada tick        → ReactiveFallback

casos especiais:
    M de N concluírem                                → Parallel
    condição explícita, não reativa                  → IfThenElse
    condição explícita, reativa                      → WhileDoElse
    desvio por valor de uma entrada                  → Switch2..Switch6
    escolha humana (depuração)                       → ManualSelector
```

**Segunda pergunta, sempre:** *preciso reavaliar o que já deu certo?* É ela que separa as
variantes reativas.

---

# 11. DECORADORES

Um **decorador** tem exatamente um filho e existe para modificar alguma coisa a respeito
dele: o resultado, o número de execuções, a duração ou a permissão para executar.

São **onze** registrados com `NodeType::DECORATOR` — treze contando `SubTree` e
`SubTreePlus`, que herdam de `DecoratorNode` mas se declaram `SUBTREE`.

## 11.1 Visão de conjunto

| Família | ID no XML | O que faz | Portas |
|---|---|---|---|
| resultado | `Inverter` | troca `SUCCESS` por `FAILURE` e vice-versa | — |
| resultado | `ForceSuccess` | converte qualquer conclusão em `SUCCESS` | — |
| resultado | `ForceFailure` | converte qualquer conclusão em `FAILURE` | — |
| repetição | `Repeat` | repete enquanto o filho tiver sucesso | `num_cycles` |
| repetição | `RetryUntilSuccesful` | repete enquanto o filho falhar | `num_attempts` |
| repetição | `KeepRunningUntilFailure` | nunca conclui, até o filho falhar | — |
| tempo | `Timeout` | *halta* o filho após $N$ ms | `msec` |
| tempo | `Delay` | espera $N$ ms antes de *ticar* o filho | `delay_msec` |
| guarda | `BlackboardCheckInt` | executa o filho só se dois valores forem iguais | `value_A`, `value_B`, `return_on_mismatch` |
| guarda | `BlackboardCheckDouble` | idem, para `double` | idem |
| guarda | `BlackboardCheckString` | idem, para `std::string` | idem |
| subárvore | `SubTree` | encapsula outra `<BehaviorTree>` | (manifesto vazio) |
| subárvore | `SubTreePlus` | idem, com remapeamento mais rico | (manifesto vazio) |

Lembre-se de que `DecoratorNode::executeTick()` repõe o filho a `IDLE` assim que ele
conclui — é isso que permite aos decoradores de repetição executarem o mesmo filho várias
vezes sem nenhum cuidado adicional.

## 11.2 `Inverter` — o NÃO lógico

**ID:** `Inverter` · **Classe:** `BT::InverterNode`

```cpp
// include/behaviortree_cpp_v3/decorators/inverter_node.h
/**
 * @brief The InverterNode returns SUCCESS if child fails
 * of FAILURE is child succeeds.
 * RUNNING status is propagated
 */
```

```cpp
// src/decorators/inverter_node.cpp  (íntegro)
InverterNode::InverterNode(const std::string& name) :
    DecoratorNode(name, {} )
{
    setRegistrationID("Inverter");
}

NodeStatus InverterNode::tick()
{
    setStatus(NodeStatus::RUNNING);

    const NodeStatus child_state = child_node_->executeTick();

    switch (child_state)
    {
        case NodeStatus::SUCCESS:
        {
            return NodeStatus::FAILURE;
        }
        case NodeStatus::FAILURE:
        {
            return NodeStatus::SUCCESS;
        }
        case NodeStatus::RUNNING:
        {
            return NodeStatus::RUNNING;
        }
        default:
        {
            throw LogicError("A child node must never return IDLE");
        }
    }
    //return status();
}
```

É o mais simples e o mais usado. Forma idiomática de escrever "se a condição **não**
valer":

```xml
<Inverter>
    <IsDoorOpen/>
</Inverter>
```

Note que o `Inverter` é o **único** dos três decoradores de resultado que lança diante de
um filho `IDLE`.

## 11.3 `ForceSuccess` e `ForceFailure`

**IDs:** `ForceSuccess`, `ForceFailure` · **Classes:** `BT::ForceSuccessNode`,
`BT::ForceFailureNode` · **Arquivos:** implementação inteira nos cabeçalhos
`decorators/force_success_node.h` e `decorators/force_failure_node.h`

```cpp
// include/behaviortree_cpp_v3/decorators/force_failure_node.h  (íntegro, sem guards)
/**
 * @brief The ForceFailureNode returns always FAILURE or RUNNING.
 */
class ForceFailureNode : public DecoratorNode
{
  public:
    ForceFailureNode(const std::string& name) :
        DecoratorNode(name, {} )
    {
        setRegistrationID("ForceFailure");
    }

  private:
    virtual BT::NodeStatus tick() override;
};

inline NodeStatus ForceFailureNode::tick()
{
    setStatus(NodeStatus::RUNNING);

    const NodeStatus child_state = child_node_->executeTick();

    switch (child_state)
    {
        case NodeStatus::FAILURE:
        case NodeStatus::SUCCESS:
        {
            return NodeStatus::FAILURE;
        }

        case NodeStatus::RUNNING:
        {
            return NodeStatus::RUNNING;
        }

        default:
        {
            // TODO throw?
        }
    }
    return status();
}
```

`ForceSuccessNode` é idêntico com `SUCCESS` no lugar de `FAILURE`.

**Uso típico do `ForceSuccess`:** tornar opcional um passo dentro de uma `Sequence` —
"tente isto; se não der, siga em frente".

```xml
<Sequence>
    <ForceSuccess>
        <TirarFoto/>          <!-- se falhar, a missão continua -->
    </ForceSuccess>
    <Prosseguir/>
</Sequence>
```

**Uso típico do `ForceFailure`:** garantir que um ramo de um `Fallback` "não conte" como
sucesso, mesmo tendo executado (aparece no exemplo `t06` para registrar
`"mission failed"` no *blackboard* e ainda assim propagar a falha).

**ARMADILHA — o `// TODO throw?` não é decorativo.** Três decoradores —
`ForceSuccessNode`, `ForceFailureNode` e `KeepRunningUntilFailureNode` — tratam o caso
`IDLE` com um comentário e nada mais, devolvendo em seguida `status()`: o próprio estado
do decorador, que acabou de ser posto em `RUNNING`. Enquanto todos os outros nós de
controle lançam `LogicError` diante de um filho que devolve `IDLE`, **estes três
convertem o erro em `RUNNING`**. Um nó de usuário defeituoso pendurado sob um
`<ForceSuccess>` produz uma árvore que fica `RUNNING` para sempre, sem erro e sem
progresso — o sintoma mais difícil de diagnosticar que esta biblioteca oferece.

## 11.4 `KeepRunningUntilFailure`

**ID:** `KeepRunningUntilFailure` · **Classe:** `BT::KeepRunningUntilFailureNode`

```cpp
// include/behaviortree_cpp_v3/decorators/keep_running_until_failure_node.h  (íntegro)
/**
 * @brief The KeepRunningUntilFailureNode returns always FAILURE or RUNNING.
 */
class KeepRunningUntilFailureNode : public DecoratorNode
{
  public:
    KeepRunningUntilFailureNode(const std::string& name) :
        DecoratorNode(name, {} )
    {
        setRegistrationID("KeepRunningUntilFailure");
    }

  private:
    virtual BT::NodeStatus tick() override;
};

inline NodeStatus KeepRunningUntilFailureNode::tick()
{
    setStatus(NodeStatus::RUNNING);

    const NodeStatus child_state = child_node_->executeTick();

    switch (child_state)
    {
        case NodeStatus::FAILURE:
        {
            return NodeStatus::FAILURE;
        }
        case NodeStatus::SUCCESS:
        case NodeStatus::RUNNING:
        {
            return NodeStatus::RUNNING;
        }
        default:
        {
            // TODO throw?
        }
    }
    return status();
}
```

Converte `SUCCESS` em `RUNNING` e só conclui quando o filho falha. Serve para transformar
uma ação pontual em atividade contínua — "fique patrulhando até que algo dê errado".

```xml
<KeepRunningUntilFailure>
    <Patrulhar rota="{rota}"/>
</KeepRunningUntilFailure>
```

Lembre que o `DecoratorNode::executeTick()` repõe o filho a `IDLE` sempre que ele conclui;
por isso o filho é reexecutado do início a cada `SUCCESS`.

## 11.5 `Repeat`

**ID:** `Repeat` · **Classe:** `BT::RepeatNode` · **Porta:** `num_cycles` (`int`, **sem
padrão**)

```cpp
// include/behaviortree_cpp_v3/decorators/repeat_node.h
/**
 * @brief The RepeatNode is used to execute a child several times, as long
 * as it succeed.
 *
 * To succeed, the child must return SUCCESS N times (port "num_cycles").
 *
 * If the child returns FAILURE, the loop is stopped and this node
 * returns FAILURE.
 *
 * Example:
 *
 * <Repeat num_cycles="3">
 *   <ClapYourHandsOnce/>
 * </Repeat>
 */
class RepeatNode : public DecoratorNode
{
  public:
    RepeatNode(const std::string& name, int NTries);
    RepeatNode(const std::string& name, const NodeConfiguration& config);

    static PortsList providedPorts()
    {
        return { InputPort<int>(NUM_CYCLES,
                                "Repeat a succesful child up to N times. "
                                "Use -1 to create an infinite loop.") };
    }

  private:
    int num_cycles_;
    int try_index_;
    bool read_parameter_from_ports_;
    static constexpr const char* NUM_CYCLES = "num_cycles";

    virtual NodeStatus tick() override;
    void halt() override;
};
```

```cpp
// src/decorators/repeat_node.cpp  (íntegro)
constexpr const char* RepeatNode::NUM_CYCLES;

RepeatNode::RepeatNode(const std::string& name, int NTries)
    : DecoratorNode(name, {} ),
    num_cycles_(NTries), try_index_(0), read_parameter_from_ports_(false)
{
     setRegistrationID("Repeat");
}

RepeatNode::RepeatNode(const std::string& name, const NodeConfiguration& config)
  : DecoratorNode(name, config),
    num_cycles_(0), try_index_(0), read_parameter_from_ports_(true)
{
}

NodeStatus RepeatNode::tick()
{
    if( read_parameter_from_ports_ )
    {
        if( !getInput(NUM_CYCLES, num_cycles_) )
        {
            throw RuntimeError("Missing parameter [", NUM_CYCLES, "] in RepeatNode");
        }
    }

    setStatus(NodeStatus::RUNNING);

    while (try_index_ < num_cycles_ || num_cycles_== -1 )
    {
        NodeStatus child_state = child_node_->executeTick();

        switch (child_state)
        {
            case NodeStatus::SUCCESS:
            {
                try_index_++;
                haltChild();
            }
            break;

            case NodeStatus::FAILURE:
            {
                try_index_ = 0;
                haltChild();
                return (NodeStatus::FAILURE);
            }

            case NodeStatus::RUNNING:
            {
                return NodeStatus::RUNNING;
            }

            default:
            {
                throw LogicError("A child node must never return IDLE");
            }
        }
    }

    try_index_ = 0;
    return NodeStatus::SUCCESS;
}

void RepeatNode::halt()
{
    try_index_ = 0;
    DecoratorNode::halt();
}
```

## 11.6 `RetryUntilSuccesful`

**ID:** `RetryUntilSuccesful` (**com um `s` a menos** — ver armadilha) ·
**Classe:** `BT::RetryNode` · **Porta:** `num_attempts` (`int`, **sem padrão**)

```cpp
// include/behaviortree_cpp_v3/decorators/retry_node.h
/**
 * @brief The RetryNode is used to execute a child several times if it fails.
 *
 * If the child returns SUCCESS, the loop is stopped and this node
 * returns SUCCESS.
 *
 * If the child returns FAILURE, this node will try again up to N times
 * (N is read from port "num_attempts").
 *
 * Example:
 *
 * <RetryUntilSuccesful num_attempts="3">
 *     <OpenDoor/>
 * </RetryUntilSuccesful>
 */
```

```cpp
// src/decorators/retry_node.cpp  (íntegro)
constexpr const char* RetryNode::NUM_ATTEMPTS;

RetryNode::RetryNode(const std::string& name, int NTries)
    : DecoratorNode(name, {} ),
    max_attempts_(NTries), try_index_(0), read_parameter_from_ports_(false)
{
    setRegistrationID("RetryUntilSuccessful");
}

RetryNode::RetryNode(const std::string& name, const NodeConfiguration& config)
  : DecoratorNode(name, config),
    max_attempts_(0), try_index_(0), read_parameter_from_ports_(true)
{
}

void RetryNode::halt()
{
    try_index_ = 0;
    DecoratorNode::halt();
}

NodeStatus RetryNode::tick()
{
    if( read_parameter_from_ports_ )
    {
        if( !getInput(NUM_ATTEMPTS, max_attempts_) )
        {
            throw RuntimeError("Missing parameter [", NUM_ATTEMPTS,"] in RetryNode");
        }
    }

    setStatus(NodeStatus::RUNNING);

    while (try_index_ < max_attempts_ || max_attempts_ == -1)
    {
        NodeStatus child_state = child_node_->executeTick();
        switch (child_state)
        {
            case NodeStatus::SUCCESS:
            {
                try_index_ = 0;
                haltChild();
                return (NodeStatus::SUCCESS);
            }

            case NodeStatus::FAILURE:
            {
                try_index_++;
                haltChild();
            }
            break;

            case NodeStatus::RUNNING:
            {
                return NodeStatus::RUNNING;
            }

            default:
            {
                throw LogicError("A child node must never return IDLE");
            }
        }
    }

    try_index_ = 0;
    return NodeStatus::FAILURE;
}
```

**ARMADILHA — o ID de registro tem erro de grafia, e é ele que vale.** O construtor de
`RetryNode` chama `setRegistrationID("RetryUntilSuccessful")` — com **dois** `s`. A
fábrica, porém, registra o nó como:

```cpp
// src/bt_factory.cpp
registerNodeType<RetryNode>("RetryUntilSuccesful");     // com UM s
```

Como `instantiateTreeNode()` sobrescreve o ID com aquele usado no registro, **quem vale é
a grafia errada**: o XML precisa dizer `<RetryUntilSuccesful>`. O exemplo
`t05_crossdoor.cpp` e o comentário do próprio `retry_node.h` usam a forma errada — por
necessidade, não por descuido. Escrever o nome corretamente produz
`"Node not recognized: RetryUntilSuccessful"`.

**ARMADILHA — `num_cycles="-1"` (ou `num_attempts="-1"`) com filho síncrono trava o
processo.** Tanto `Repeat` quanto `RetryUntilSuccesful` aceitam $-1$ como "infinito". Mas
o laço de repetição está **dentro de um único `tick()`**: ele só devolve o controle quando
o filho responde `RUNNING`. Com um filho síncrono — que nunca devolve `RUNNING` — e limite
$-1$, o `tick()` entra num laço infinito e o processo trava, **sem consumir memória e sem
produzir erro**. Use $-1$ apenas com filhos assíncronos.

**Nota — o `haltChild()` entre tentativas** é a correção da *issue* #228, registrada no
`CHANGELOG.rst` da 3.5.3 como *"Retry and Repeat node need to halt the child"*. Sem ele,
um filho com estado interno — um `StatefulActionNode`, por exemplo — não seria reiniciado
entre as tentativas.

**Consequência prática do laço interno:** com um filho **síncrono** e `num_attempts="4"`,
as quatro tentativas acontecem **dentro de um mesmo *tick***. Com um filho **assíncrono**,
cada tentativa consome vários *ticks* e o `Retry` devolve `RUNNING` no meio.

## 11.7 `Timeout`

**ID:** `Timeout` · **Classe:** `BT::TimeoutNode<Clock, Duration>` (*template* com
parâmetros padrão `std::chrono::steady_clock`) · **Porta:** `msec` (`unsigned`, **sem
padrão**)

```cpp
// include/behaviortree_cpp_v3/decorators/timeout_node.h
/**
 * @brief The TimeoutNode will halt() a running child if
 * the latter has been RUNNING for more than a give time.
 * The timeout is in milliseconds and it is passed using the port "msec".
 *
 * If timeout is reached it returns FAILURE.
 *
 * Example:
 *
 * <Timeout msec="5000">
 *    <KeepYourBreath/>
 * </Timeout>
 */
template <typename _Clock = std::chrono::steady_clock,
          typename _Duration = std::chrono::steady_clock::duration>
class TimeoutNode : public DecoratorNode
{
  public:
    TimeoutNode(const std::string& name, unsigned milliseconds)
    : DecoratorNode(name, {} ),
      child_halted_(false), timer_id_(0), msec_(milliseconds),
      read_parameter_from_ports_(false), timeout_started_(false)
    {
        setRegistrationID("Timeout");
    }

    TimeoutNode(const std::string& name, const NodeConfiguration& config)
    : DecoratorNode(name, config),
      child_halted_(false), timer_id_(0), msec_(0),
      read_parameter_from_ports_(true), timeout_started_(false)
    {
    }

    ~TimeoutNode() override { timer_.cancelAll(); }

    static PortsList providedPorts()
    {
        return { InputPort<unsigned>("msec", "After a certain amount of time, "
                                             "halt() the child if it is still running.") };
    }

  private:
    TimerQueue<_Clock, _Duration> timer_ ;

    virtual BT::NodeStatus tick() override
    {
        if( read_parameter_from_ports_ )
        {
            if( !getInput("msec", msec_) )
            {
                throw RuntimeError("Missing parameter [msec] in TimeoutNode");
            }
        }

        if ( !timeout_started_ )
        {
            timeout_started_ = true;
            setStatus(NodeStatus::RUNNING);
            child_halted_ = false;

            if (msec_ > 0)
            {
                timer_id_ = timer_.add(std::chrono::milliseconds(msec_),
                                       [this](bool aborted)
                                       {
                                           std::unique_lock<std::mutex> lk( timeout_mutex_ );
                                           if (!aborted && child()->status() == NodeStatus::RUNNING)
                                           {
                                               child_halted_ = true;
                                               haltChild();
                                           }
                                       });
            }
        }

        std::unique_lock<std::mutex> lk( timeout_mutex_ );

        if (child_halted_)
        {
            timeout_started_ = false;
            return NodeStatus::FAILURE;
        }
        else
        {
            auto child_status = child()->executeTick();
            if (child_status != NodeStatus::RUNNING)
            {
                timeout_started_ = false;
                timeout_mutex_.unlock();
                timer_.cancel(timer_id_);
                timeout_mutex_.lock();
            }
            return child_status;
        }
    }

    std::atomic<bool> child_halted_;
    uint64_t timer_id_;
    unsigned msec_;
    bool read_parameter_from_ports_;
    bool timeout_started_;
    std::mutex timeout_mutex_;
};
```

**REGRA — `msec="0"` desliga o `Timeout`.** A guarda `if (msec_ > 0)` não arma
temporizador nenhum quando o prazo é zero. O nó vira um **repassador transparente**:
*tica* o filho e devolve o resultado dele, para sempre. Não é "prazo zero", é "sem
prazo".

**ARMADILHA — o `unlock()`/`lock()` manual sob um `unique_lock`.** Repare nas três linhas
dentro do `else`: o *mutex* é destravado e retravado **à mão**, enquanto um
`std::unique_lock` ainda pensa que o detém. Funciona porque o `unique_lock` destrava no
fim do escopo e o estado, ao final, é o mesmo — mas basta `timer_.cancel()` lançar para o
*mutex* ficar destravado com o `unique_lock` tentando destravar de novo, o que é
comportamento indefinido. O motivo do malabarismo é real: o *handler* do temporizador toma
o mesmo *mutex*, e `cancel()` espera o *handler* correr — segurar o *mutex* durante o
`cancel()` seria um *deadlock*.

**ARMADILHA — um `Timeout` só interrompe quem coopera.** O disparo do temporizador chama
`haltChild()`. Se o filho for um `SyncActionNode` que está dormindo dentro do seu `tick()`,
o `haltChild()` **não pode ser executado**, porque a *thread* do *tick* está bloqueada
dentro daquela ação. Na prática: `<Timeout>` sobre uma ação síncrona longa **não faz
nada**. Só funciona sobre ações que devolvem `RUNNING`.

## 11.8 `Delay`

**ID:** `Delay` · **Classe:** `BT::DelayNode` · **Porta:** `delay_msec` (`unsigned`,
**sem padrão**)

```cpp
// include/behaviortree_cpp_v3/decorators/delay_node.h
/**
 * @brief The delay node will introduce a delay of a few milliseconds
 * and then tick the child returning the status of the child as it is
 * upon completion
 * The delay is in milliseconds and it is passed using the port "delay_msec".
 *
 * During the delay the node changes status to RUNNING
 *
 * Example:
 *
 * <Delay delay_msec="5000">
 *    <KeepYourBreath/>
 * </Delay>
 */
class DelayNode : public DecoratorNode
{
  public:
    DelayNode(const std::string& name, unsigned milliseconds);
    DelayNode(const std::string& name, const NodeConfiguration& config);

    ~DelayNode() override { halt(); }

    static PortsList providedPorts()
    {
        return {InputPort<unsigned>("delay_msec", "Tick the child after a few milliseconds")};
    }

    void halt() override
    {
        timer_.cancelAll();
        DecoratorNode::halt();
    }

  private:
    TimerQueue<> timer_;
    uint64_t timer_id_;

    virtual BT::NodeStatus tick() override;

    bool delay_started_;
    bool delay_complete_;
    bool delay_aborted_;
    unsigned msec_;
    bool read_parameter_from_ports_;
    std::mutex delay_mutex_;
};
```

```cpp
// src/decorators/delay_node.cpp
NodeStatus DelayNode::tick()
{
    if (read_parameter_from_ports_)
    {
        if (!getInput("delay_msec", msec_))
        {
            throw RuntimeError("Missing parameter [delay_msec] in DelayNode");
        }
    }

    if (!delay_started_)
    {
        delay_complete_ = false;
        delay_started_ = true;
        setStatus(NodeStatus::RUNNING);

        timer_id_ = timer_.add(std::chrono::milliseconds(msec_),
                               [this](bool aborted)
        {
            std::unique_lock<std::mutex> lk(delay_mutex_);
            if (!aborted) { delay_complete_ = true;  }
            else          { delay_aborted_  = true;  }
        });
    }

    std::unique_lock<std::mutex> lk(delay_mutex_);

    if (delay_aborted_)
    {
        delay_aborted_ = false;
        delay_started_ = false;
        return NodeStatus::FAILURE;
    }
    else if (delay_complete_)
    {
        delay_started_ = false;
        delay_aborted_ = false;
        auto child_status = child()->executeTick();
        return child_status;
    }
    else
    {
        return NodeStatus::RUNNING;
    }
}
```

**ARMADILHA — `Delay` não se recupera de um *halt*.** `DelayNode::halt()` cancela os
temporizadores e chama `DecoratorNode::halt()`, mas **não repõe `delay_started_`**. O
cancelamento faz o *handler* rodar com `aborted = true`, o que liga `delay_aborted_`. No
*tick* seguinte o nó vê `delay_started_` ainda verdadeiro, cai no ramo do aborto e devolve
`FAILURE` — uma **falha espúria** — e só então volta ao estado inicial. Um `<Delay>` sob um
nó reativo falha uma vez a cada interrupção.

**Nota:** `delay_complete_` não é inicializado em nenhum dos dois construtores; só recebe
valor no primeiro `tick()`, antes de ser lido. Funciona, mas é frágil.

## 11.9 O custo de `Timeout` e `Delay`: uma *thread* cada

**ARMADILHA — cada `Timeout` e cada `Delay` custa uma *thread*.** Ambos têm um
`BT::TimerQueue` como **membro**, e o construtor de `TimerQueue` lança uma *thread*
dedicada. Uma árvore com dez `<Timeout>` tem dez *threads* paradas em
`condition_variable::wait_until`, criadas na carga e vivas até a destruição da árvore —
**mesmo que nenhum desses nós seja jamais *ticado***. Não há *pool* compartilhado.

Para uma árvore grande com muitos prazos, considere implementar um decorador próprio que
compare `steady_clock::now()` dentro do `tick()` (sem temporizador, sem *thread*):

```cpp
class PrazoSimples : public BT::DecoratorNode
{
  public:
    PrazoSimples(const std::string& name, const BT::NodeConfiguration& config)
      : BT::DecoratorNode(name, config) {}

    static BT::PortsList providedPorts()
    { return { BT::InputPort<unsigned>("msec", 1000, "prazo em ms") }; }

    BT::NodeStatus tick() override
    {
        unsigned msec = 1000;
        getInput("msec", msec);

        if (!iniciado_) {
            iniciado_ = true;
            fim_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(msec);
        }
        if (std::chrono::steady_clock::now() > fim_) {
            iniciado_ = false;
            haltChild();
            return BT::NodeStatus::FAILURE;
        }
        auto st = child_node_->executeTick();
        if (st != BT::NodeStatus::RUNNING) { iniciado_ = false; }
        return st;
    }

    void halt() override { iniciado_ = false; BT::DecoratorNode::halt(); }

  private:
    bool iniciado_ = false;
    std::chrono::steady_clock::time_point fim_{};
};
```

A diferença de comportamento: o prazo só é verificado **quando o nó é ticado**, e não de
forma assíncrona. Na prática isso costuma bastar, e a resolução passa a ser a do período
de *tick*.

## 11.10 `BlackboardCheckInt` / `BlackboardCheckDouble` / `BlackboardCheckString`

**Classe:** `BT::BlackboardPreconditionNode<T>` (*template*) ·
**Portas:** `value_A` (sem tipo), `value_B` (sem tipo), `return_on_mismatch`
(`NodeStatus`)

```cpp
// include/behaviortree_cpp_v3/decorators/blackboard_precondition.h  (íntegro)
/**
 * This node excute its child only if the value of a given input port
 * is equal to the expected one.
 * If this precondition is met, this node will return the same status of the
 * child, otherwise it will return the value specified in "return_on_mismatch".
 *
 * Example:
 *
 * <BlackboardCheckInt value_A="{the_answer}"
 *                     value_B="42"
 *                     return_on_mismatch="FAILURE" />
 */
template <typename T>
class BlackboardPreconditionNode : public DecoratorNode
{
  public:
    BlackboardPreconditionNode(const std::string& name, const NodeConfiguration& config)
      : DecoratorNode(name, config)
    {
        if( std::is_same<T,int>::value)              setRegistrationID("BlackboardCheckInt");
        else if( std::is_same<T,double>::value)      setRegistrationID("BlackboardCheckDouble");
        else if( std::is_same<T,std::string>::value) setRegistrationID("BlackboardCheckString");
    }

    virtual ~BlackboardPreconditionNode() override = default;

    static PortsList providedPorts()
    {
        return {InputPort("value_A"),
                InputPort("value_B"),
                InputPort<NodeStatus>("return_on_mismatch") };
    }

  private:
    virtual BT::NodeStatus tick() override;
};

template<typename T> inline
NodeStatus BlackboardPreconditionNode<T>::tick()
{
    T value_A;
    T value_B;
    NodeStatus default_return_status = NodeStatus::FAILURE;

    setStatus(NodeStatus::RUNNING);

    if( getInput("value_A", value_A) &&
        getInput("value_B", value_B) &&
        value_B == value_A )
    {
        return child_node_->executeTick();
    }

    if( child()->status() == NodeStatus::RUNNING )
    {
        haltChild();
    }
    getInput("return_on_mismatch", default_return_status);
    return default_return_status;
}
```

Duas leituras valem a pena:

1. **O nó é reativo:** a comparação é refeita a cada *tick*, e um filho em curso é
   interrompido assim que a igualdade deixa de valer.
2. **`return_on_mismatch` é lido com o valor de `getInput()` ignorado** — se a porta
   faltar, a variável mantém o `FAILURE` inicial. É um padrão deliberado de valor padrão
   **em código**, ao contrário do padrão declarado na porta.

**ARMADILHA — as portas de comparação não têm tipo declarado.** `providedPorts()` devolve
`InputPort("value_A")` e `InputPort("value_B")` — sem parâmetro de *template*. As entradas
correspondentes do *blackboard*, portanto, **não são travadas em nenhum tipo**, e a
conversão acontece só na leitura, dentro do `getInput<T>()` da instanciação. Comparar um
`BlackboardCheckInt` contra uma entrada que guarda um `double` não dá erro na carga: dá
uma conversão silenciosa no *tick*.

**POR QUÊ — por que só três tipos, e não um *template* aberto.** O registro na fábrica é
feito por *string*, em tempo de execução — não há como instanciar
`BlackboardPreconditionNode<MeuTipo>` a partir de um nome lido do XML. Cada instanciação
precisa existir como código compilado e ser registrada explicitamente. Para um tipo
próprio, registre a sua:

```cpp
factory.registerNodeType<BT::BlackboardPreconditionNode<Pose2D>>("BlackboardCheckPose");
// requer que Pose2D tenha operator==
```

## 11.11 `SubTree` e `SubTreePlus`

Tecnicamente decoradores; o comportamento de *blackboard* está detalhado na seção 16.

```cpp
// src/decorators/subtree_node.cpp  (íntegro)
BT::SubtreeNode::SubtreeNode(const std::string &name) :
    DecoratorNode(name, {} )
{
    setRegistrationID("SubTree");
}

BT::NodeStatus BT::SubtreeNode::tick()
{
    NodeStatus prev_status = status();
    if (prev_status == NodeStatus::IDLE)
    {
        setStatus(NodeStatus::RUNNING);
    }
    return child_node_->executeTick();
}

//--------------------------------
BT::SubtreePlusNode::SubtreePlusNode(const std::string &name) :
     DecoratorNode(name, {} )
{
  setRegistrationID("SubTreePlus");
}

BT::NodeStatus BT::SubtreePlusNode::tick()
{
    NodeStatus prev_status = status();
    if (prev_status == NodeStatus::IDLE)
    {
        setStatus(NodeStatus::RUNNING);
    }
    return child_node_->executeTick();
}
```

Os dois `tick()` são idênticos e triviais: repassam ao filho. **Toda a diferença entre
`SubTree` e `SubTreePlus` está no *parser***, não no nó.

`type()` é `override final` e devolve `NodeType::SUBTREE` nos dois.

## 11.12 Escrevendo um decorador próprio

```cpp
class NoMaximoACada : public BT::DecoratorNode
{
  public:
    NoMaximoACada(const std::string& name, const BT::NodeConfiguration& config)
      : BT::DecoratorNode(name, config) {}

    static BT::PortsList providedPorts()          // ① PUBLICO, senao e ignorado
    {
        return { BT::InputPort<unsigned>("periodo_ms", 1000, "intervalo minimo") };
    }

    BT::NodeStatus tick() override
    {
        unsigned periodo = 1000;
        getInput("periodo_ms", periodo);          // ② leitura no tick, nao no ctor

        auto agora = std::chrono::steady_clock::now();
        if (agora - ultimo_ < std::chrono::milliseconds(periodo)) {
            return BT::NodeStatus::FAILURE;       // ainda cedo: nao tica o filho
        }
        ultimo_ = agora;
        return child_node_->executeTick();        // ③ o pai repoe o filho a IDLE
    }

  private:
    std::chrono::steady_clock::time_point ultimo_{};
};
```

**REGRA — um decorador que pode devolver `RUNNING` precisa de `halt()`.** No exemplo acima
não é preciso sobrescrever `halt()`: o `DecoratorNode::halt()` herdado já propaga ao filho
e repõe o estado. Mas se o seu decorador guardar **estado próprio** — um contador, um
temporizador, uma posição num ciclo —, sobrescreva `halt()`, zere esse estado e **chame a
implementação da base**:

```cpp
void halt() override
{
    meu_contador_ = 0;
    BT::DecoratorNode::halt();      // NUNCA esqueça esta linha
}
```

Esquecer a chamada à base deixa o filho `RUNNING` pendurado.

**Acesso ao filho:** dentro de um decorador, use `child_node_` (membro `protected`) ou
`child()`. Chamar `child()->executeTick()` é o correto — **nunca** `child()->tick()`, que
é `protected` e não publicaria o estado.

---

# 12. NÓS DE AÇÃO E DE CONDIÇÃO

As folhas são onde a árvore toca o mundo. Tudo o que as seções anteriores descreveram —
controle, decoração, portas — existe para decidir **quando** uma folha executa; esta seção
trata do que acontece **dentro** dela.

A decisão que estrutura o capítulo inteiro é uma só: **a ação termina dentro do *tick*, ou
continua depois dele?**

## 12.1 Visão de conjunto

| Classe base | Devolve `RUNNING`? | Custo | Você implementa |
|---|---|---|---|
| `SyncActionNode` | nunca (lança se tentar) | nenhum | `tick()` |
| `StatefulActionNode` | sim | nenhum | `onStart()`, `onRunning()`, `onHalted()` |
| `AsyncActionNode` | sim | **uma *thread* por execução** | `tick()`, `halt()` |
| `CoroActionNode` | sim | uma pilha de co-rotina | `tick()` com *yield* |
| `ConditionNode` | não (por convenção) | nenhum | `tick()` |

**REGRA — escolha a mais simples que resolva.** A ordem de preferência é:
`SyncActionNode` se a ação for curta; `StatefulActionNode` se ela for do tipo
pedido-e-resposta; `AsyncActionNode` apenas se for preciso **bloquear de verdade** dentro
do `tick()`. O cabeçalho de `AsyncActionNode` é incomumente franco a respeito:
*"IMPORTANT: this action is quite hard to implement correctly. Please be sure that you
know what you are doing."*

```
a ação termina em menos de um período de tick?
    sim → SyncActionNode  (ou registerSimpleAction)
    não ↓
existe um jeito de PERGUNTAR se já terminou, sem bloquear?
    sim → StatefulActionNode  (onStart / onRunning / onHalted)
    não ↓
Boost está disponível?
    sim → CoroActionNode   (ausente no empacotamento documentado)
    não → AsyncActionNode  (uma thread por execução)
```

## 12.2 A hierarquia de ação

```cpp
// include/behaviortree_cpp_v3/action_node.h  (topo do arquivo)
// IMPORTANT: Actions which returned SUCCESS or FAILURE will not be ticked
// again unless setStatus(IDLE) is called first.
// Keep this in mind when writing your custom Control and Decorator nodes.

/**
 * @brief The ActionNodeBase is the base class to use to create any kind of action.
 * A particular derived class is free to override executeTick() as needed.
 */
class ActionNodeBase : public LeafNode
{
  public:
    ActionNodeBase(const std::string& name, const NodeConfiguration& config);
    ~ActionNodeBase() override = default;

    virtual NodeType type() const override final { return NodeType::ACTION; }
};
```

```
LeafNode
  ├── ActionNodeBase                (type() == ACTION, final)
  │     ├── SyncActionNode          (proíbe RUNNING; halt() final e vazio)
  │     │     └── SimpleActionNode  (functor)
  │     ├── AsyncActionNode         (thread; executeTick() final)
  │     ├── StatefulActionNode      (máquina de estados; tick()/halt() final)
  │     └── CoroActionNode          (co-rotina; executeTick() final) [#ifndef BT_NO_COROUTINES]
  └── ConditionNode                 (type() == CONDITION, final; halt() final e vazio)
        └── SimpleConditionNode     (functor)
```

## 12.3 `ConditionNode` — perguntas, não comandos

```cpp
// include/behaviortree_cpp_v3/condition_node.h  (íntegro, sem guards)
class ConditionNode : public LeafNode
{
  public:
    ConditionNode(const std::string& name, const NodeConfiguration& config);
    virtual ~ConditionNode() override = default;

    //Do nothing
    virtual void halt() override final
    {
        setStatus(NodeStatus::IDLE);
    }

    virtual NodeType type() const override final
    {
        return NodeType::CONDITION;
    }
};

/**
 * @brief The SimpleConditionNode provides an easy to use ConditionNode.
 * The user should simply provide a callback with this signature
 *
 *    BT::NodeStatus functionName(void)
 *
 * This avoids the hassle of inheriting from a ActionNode.
 *
 * Using lambdas or std::bind it is easy to pass a pointer to a method.
 * SimpleConditionNode does not support halting, NodeParameters, nor Blackboards.
 */
class SimpleConditionNode : public ConditionNode
{
  public:
    typedef std::function<NodeStatus(TreeNode&)> TickFunctor;

    SimpleConditionNode(const std::string& name, TickFunctor tick_functor,
                        const NodeConfiguration& config);
    ~SimpleConditionNode() override = default;

  protected:
    virtual NodeStatus tick() override;
    TickFunctor tick_functor_;
};
```

```cpp
// src/condition_node.cpp  (íntegro)
ConditionNode::ConditionNode(const std::string& name, const NodeConfiguration& config)
  : LeafNode::LeafNode(name, config)
{
}

SimpleConditionNode::SimpleConditionNode(const std::string& name, TickFunctor tick_functor,
                                         const NodeConfiguration& config)
  : ConditionNode(name, config), tick_functor_(std::move(tick_functor))
{
}

NodeStatus SimpleConditionNode::tick()
{
    return tick_functor_(*this);
}
```

**REGRA — uma condição precisa ser barata e sem efeito colateral.** A obrigação não vem do
compilador, vem do uso: numa `ReactiveSequence`, toda condição à esquerda é reavaliada a
**cada** *tick*. Uma condição que faz uma chamada de rede ou que altera o *blackboard*
transforma a frequência de *tick* da aplicação numa variável do comportamento — e a
árvore deixa de ser reproduzível.

**Nota importante:** o comentário do cabeçalho diz que `SimpleConditionNode` *"does not
support ... Blackboards"*, mas isso é **desatualizado**: o *functor* recebe `TreeNode&` e
`registerSimpleCondition()` aceita uma `PortsList`, então portas funcionam. O que não
funciona é `providedPorts()` estático (a lista tem de ser passada no registro).

### Registrando condições

```cpp
// forma 1: função livre sem portas
BT::NodeStatus BateriaOK() {
    return (nivel_bateria() > 0.2) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}
factory.registerSimpleCondition("BateriaOK", std::bind(BateriaOK));

// forma 2: lambda capturando estado
factory.registerSimpleCondition("EstaAberta", [&porta](BT::TreeNode&) {
    return porta.aberta ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
});

// forma 3: com portas (a lista é passada explicitamente)
BT::PortsList ports = { BT::InputPort<double>("limite") };
factory.registerSimpleCondition("BateriaAcima", [](BT::TreeNode& self) {
    double limite = 0.0;
    self.getInput("limite", limite);
    return (nivel_bateria() > limite) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}, ports);

// forma 4: classe própria (única que permite providedPorts() estático)
class BateriaAcima : public BT::ConditionNode
{
  public:
    BateriaAcima(const std::string& n, const BT::NodeConfiguration& c)
      : BT::ConditionNode(n, c) {}
    static BT::PortsList providedPorts() { return { BT::InputPort<double>("limite") }; }
    BT::NodeStatus tick() override { /* ... */ }
};
factory.registerNodeType<BateriaAcima>("BateriaAcima");
```

**ARMADILHA — `std::bind` de uma função sem argumentos.** `registerSimpleCondition`
espera um `std::function<NodeStatus(TreeNode&)>`. Uma função `NodeStatus f()` só é
aceitável porque `std::bind(f)` produz um objeto chamável que **ignora** argumentos extras.
Uma lambda `[]() { ... }` **não** compila no lugar; é preciso `[](BT::TreeNode&) { ... }`.
Isso explica o `std::bind(CheckBattery)` onipresente nos exemplos.

## 12.4 `SyncActionNode` — a ação que termina no *tick*

```cpp
// include/behaviortree_cpp_v3/action_node.h
/**
 * @brief The SyncActionNode is an ActionNode that
 * explicitly prevents the status RUNNING and doesn't require
 * an implementation of halt().
 */
class SyncActionNode : public ActionNodeBase
{
  public:
    SyncActionNode(const std::string& name, const NodeConfiguration& config);
    ~SyncActionNode() override = default;

    /// throws if the derived class return RUNNING.
    virtual NodeStatus executeTick() override;

    /// You don't need to override this
    virtual void halt() override final
    {
        setStatus(NodeStatus::IDLE);
    }
};
```

```cpp
// src/action_node.cpp
NodeStatus SyncActionNode::executeTick()
{
  auto stat = ActionNodeBase::executeTick();
  if( stat == NodeStatus::RUNNING)
  {
    throw LogicError("SyncActionNode MUST never return RUNNING");
  }
  return stat;
}
```

Exemplo mínimo:

```cpp
class ApproachObject : public BT::SyncActionNode
{
  public:
    ApproachObject(const std::string& name) : BT::SyncActionNode(name, {}) {}

    // You must override the virtual function tick()
    BT::NodeStatus tick() override
    {
        std::cout << "ApproachObject: " << this->name() << std::endl;
        return BT::NodeStatus::SUCCESS;
    }
};
```

**ARMADILHA — "síncrono" não significa "rápido".** Nada impede um `SyncActionNode` de
dormir dois segundos — e o exemplo `crossdoor_nodes.cpp` faz exatamente isso, com
`SleepMS(2000)` dentro de `OpenDoor`. O efeito é que **o *tick* inteiro** leva dois
segundos: nenhuma condição é reavaliada, nenhum irmão progride, e um `Timeout` que englobe
a árvore **não consegue interrompê-la**, porque o *halt* só chega quando o *tick* volta.
Se a ação pode demorar mais que o período de *tick* desejado, **ela não é síncrona**.

## 12.5 `SimpleActionNode`

```cpp
// include/behaviortree_cpp_v3/action_node.h
/**
 * @brief The SimpleActionNode provides an easy to use SyncActionNode.
 * The user should simply provide a callback with this signature
 *
 *    BT::NodeStatus functionName(TreeNode&)
 *
 * This avoids the hassle of inheriting from a ActionNode.
 *
 * Using lambdas or std::bind it is easy to pass a pointer to a method.
 * SimpleActionNode is executed synchronously and does not support halting.
 * NodeParameters aren't supported.
 */
class SimpleActionNode : public SyncActionNode
{
  public:
    typedef std::function<NodeStatus(TreeNode&)> TickFunctor;
    SimpleActionNode(const std::string& name, TickFunctor tick_functor,
                     const NodeConfiguration& config);
  protected:
    virtual NodeStatus tick() override final;
    TickFunctor tick_functor_;
};
```

```cpp
// src/action_node.cpp
NodeStatus SimpleActionNode::tick()
{
    NodeStatus prev_status = status();

    if (prev_status == NodeStatus::IDLE)
    {
        setStatus(NodeStatus::RUNNING);
        prev_status = NodeStatus::RUNNING;
    }

    NodeStatus status = tick_functor_(*this);
    if (status != prev_status)
    {
        setStatus(status);
    }
    return status;
}
```

**Detalhe observável importante:** `SimpleActionNode::tick()` chama `setStatus(RUNNING)`
**antes** de invocar o *functor*. Consequência prática no traço de um `StdCoutLogger`:

```
Fechar     IDLE    -> RUNNING      <- SimpleActionNode (registerSimpleAction)
Fechar     RUNNING -> SUCCESS
Abrir      IDLE    -> SUCCESS      <- classe derivada de SyncActionNode
```

Os dois são igualmente síncronos; a diferença é só de **instrumentação**. Saber disso
evita a conclusão errada de que um deles é assíncrono.

### Embrulhando código legado (padrão do `t07`)

```cpp
// examples/t07_wrap_legacy.cpp
class MyLegacyMoveTo
{
public:
    bool go(Point3D goal)
    {
        printf("Going to: %f %f %f\n", goal.x, goal.y, goal.z);
        return true; // true means success in my legacy code
    }
};

// ...
MyLegacyMoveTo move_to;

// Here we use a lambda that captures the reference of move_to
auto MoveToWrapperWithLambda = [&move_to](TreeNode& parent_node) -> NodeStatus
{
    Point3D goal;
    // thanks to paren_node, you can access easily the input and output ports.
    parent_node.getInput("goal", goal);

    bool res = move_to.go( goal );
    // convert bool to NodeStatus
    return res ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
};

PortsList ports = { BT::InputPort<Point3D>("goal") };
factory.registerSimpleAction("MoveTo", MoveToWrapperWithLambda, ports );
```

Este é o padrão recomendado para integrar código existente **sem modificá-lo**.

## 12.6 `StatefulActionNode` — pedido e resposta

Esta é a forma recomendada de escrever uma ação longa, e a que o cabeçalho chama de
*"the goto option"*.

```cpp
// include/behaviortree_cpp_v3/action_node.h
/**
 * @brief The ActionNode is the goto option for,
 * but it is actually much easier to use correctly.
 *
 * It is particularly useful when your code contains a request-reply pattern,
 * i.e. when the actions sends an asychronous request, then checks periodically
 * if the reply has been received and, eventually, analyze the reply to determine
 * if the result is SUCCESS or FAILURE.
 *
 * -) an action that was in IDLE state will call onStart()
 *
 * -) A RUNNING action will call onRunning()
 *
 * -) if halted, method onHalted() is invoked
 */
class StatefulActionNode : public ActionNodeBase
{
  public:
      StatefulActionNode(const std::string& name, const NodeConfiguration& config)
        : ActionNodeBase(name,config) {}

      // do not override this method
      NodeStatus tick() override final;
      // do not override this method
      void halt() override final;

      /// method to be called at the beginning.
      /// If it returns RUNNING, this becomes an asychronous node.
      virtual NodeStatus onStart() = 0;

      /// method invoked by a RUNNING action.
      virtual NodeStatus onRunning() = 0;

      /// when the method halt() is called and the action is RUNNING, this method is invoked.
      /// This is a convenient place todo a cleanup, if needed.
      virtual void onHalted() = 0;
};
```

```cpp
// src/action_node.cpp
NodeStatus StatefulActionNode::tick()
{
  const NodeStatus initial_status = status();

  if( initial_status == NodeStatus::IDLE )
  {
    NodeStatus new_status = onStart();
    if( new_status == NodeStatus::IDLE)
    {
      throw std::logic_error("AsyncActionNode2::onStart() must not return IDLE");
    }
    return new_status;
  }
  //------------------------------------------
  if( initial_status == NodeStatus::RUNNING )
  {
    NodeStatus new_status = onRunning();
    if( new_status == NodeStatus::IDLE)
    {
      throw std::logic_error("AsyncActionNode2::onRunning() must not return IDLE");
    }
    return new_status;
  }
  //------------------------------------------
  return initial_status;
}

void StatefulActionNode::halt()
{
  if( status() == NodeStatus::RUNNING)
  {
    onHalted();
  }
  setStatus(NodeStatus::IDLE);
}
```

### Esqueleto completo

```cpp
class ChamaServico : public BT::StatefulActionNode
{
  public:
    ChamaServico(const std::string& name, const BT::NodeConfiguration& config)
      : BT::StatefulActionNode(name, config) {}

    static BT::PortsList providedPorts()
    {
        return { BT::InputPort<std::string>("alvo") };
    }

    BT::NodeStatus onStart() override
    {
        std::string alvo;
        if (!getInput("alvo", alvo)) { return BT::NodeStatus::FAILURE; }
        pedido_ = cliente_.enviaAssincrono(alvo);        // NAO bloqueia
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override
    {
        if (!pedido_.pronto()) { return BT::NodeStatus::RUNNING; }
        return pedido_.sucesso() ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    }

    void onHalted() override { cliente_.cancela(pedido_); }

  private:
    Cliente cliente_;
    Pedido  pedido_;
};
```

### Variante temporizada (sem serviço externo)

```cpp
class Aproximar : public BT::StatefulActionNode
{
  public:
    Aproximar(const std::string& name, const BT::NodeConfiguration& config)
      : BT::StatefulActionNode(name, config) {}

    static BT::PortsList providedPorts()
    {
        return { BT::InputPort<Pose2D>("goal", "destino, no formato x;y;theta"),
                 BT::InputPort<unsigned>("duracao_ms", 300, "quanto o trajeto demora") };
    }

    BT::NodeStatus onStart() override
    {
        Pose2D goal;
        if (!getInput("goal", goal)) {
            throw BT::RuntimeError("[", name(), "] faltou a porta [goal]");
        }
        unsigned dur = 300;
        getInput("duracao_ms", dur);
        fim_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(dur);
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override
    {
        if (std::chrono::steady_clock::now() < fim_) { return BT::NodeStatus::RUNNING; }
        return BT::NodeStatus::SUCCESS;
    }

    void onHalted() override { /* nada a limpar */ }

  private:
    std::chrono::steady_clock::time_point fim_;
};
```

**ARMADILHA — a mensagem de erro cita uma classe que não existe.** Os dois `throw` dizem
`"AsyncActionNode2::onStart()"` e `"AsyncActionNode2::onRunning()"`. **Não há classe
`AsyncActionNode2`** na biblioteca — é o nome que `StatefulActionNode` tinha antes de ser
renomeada. Procurar por ele no código-fonte, diante do erro, não leva a lugar nenhum. São
ainda `std::logic_error` **crus**, fora da hierarquia `BehaviorTreeException`.

**REGRA — `onHalted()` é chamado só se a ação estava `RUNNING`.**
`StatefulActionNode::halt()` testa o estado antes de chamar `onHalted()`. Uma ação que
ainda não começou, ou que já concluiu, **não** recebe a notificação de cancelamento — o
que é o desejado, mas significa que `onHalted()` **não** é um destrutor lógico e não deve
ser o único lugar onde recursos são liberados.

**Nota sobre reexecução:** um `StatefulActionNode` que devolveu `SUCCESS` continua em
`SUCCESS`; a terceira ramificação do `tick()` (`return initial_status`) devolve o mesmo
valor sem executar nada. Ele só volta a chamar `onStart()` depois que alguém o repuser a
`IDLE` — tipicamente o `haltChildren()` do pai. É por isso que trocar uma `Sequence` por
`ReactiveSequence` **não** faz um `StatefulActionNode` reiniciar.

## 12.7 `AsyncActionNode` — uma *thread* por execução

```cpp
// include/behaviortree_cpp_v3/action_node.h
/**
 * @brief The AsyncActionNode uses a different thread, where the action will be
 * executed.
 *
 * IMPORTANT: this action is quite hard to implement correctly.
 * Please be sure that you know what you are doing.
 *
 * - In your overriden tick() method, you must check periodically
 *   the result of the method isHaltRequested() and stop your execution accordingly.
 *
 * - in the overriden halt() method, you can do some cleanup, but do not forget to
 *   invoke the base class method AsyncActionNode::halt();
 *
 * - remember, with few exceptions, a halted AsyncAction must return NodeStatus::IDLE.
 *
 * For a complete example, look at __AsyncActionTest__ in action_test_node.h in the folder test.
 */
class AsyncActionNode : public ActionNodeBase
{
  public:
    AsyncActionNode(const std::string& name, const NodeConfiguration& config)
      : ActionNodeBase(name, config) {}

    bool isHaltRequested() const { return halt_requested_.load(); }

    // This method spawn a new thread. Do NOT remove the "final" keyword.
    virtual NodeStatus executeTick() override final;

    virtual void halt() override;

  private:
    std::exception_ptr exptr_;
    std::atomic_bool halt_requested_;
    std::future<NodeStatus> thread_handle_;
};
```

```cpp
// src/action_node.cpp
NodeStatus BT::AsyncActionNode::executeTick()
{
    //send signal to other thread.
    // The other thread is in charge for changing the status
    if (status() == NodeStatus::IDLE)
    {
        setStatus( NodeStatus::RUNNING );
        halt_requested_ = false;
        thread_handle_ = std::async(std::launch::async, [this]() {

            try {
                setStatus(tick());
            }
            catch (std::exception&)
            {
                std::cerr << "\nUncaught exception from the method tick(): ["
                          << registrationName() << "/" << name() << "]\n" << std::endl;
                exptr_ = std::current_exception();
                thread_handle_.wait();
            }
            return status();
        });
    }

    if( exptr_ )
    {
        std::rethrow_exception(exptr_);
    }
    return status();
}

void AsyncActionNode::halt()
{
    halt_requested_.store(true);

    if( thread_handle_.valid() ){
        thread_handle_.wait();
    }
    thread_handle_ = {};
}
```

O *tick* da árvore, portanto, **não executa a ação**: ele a **lança** e devolve
imediatamente o estado corrente. Nos *ticks* seguintes, como o estado já é `RUNNING`, nada
é lançado — apenas se lê o estado que a outra *thread* vai atualizar quando terminar.

**REGRA — o contrato do `halt()` cooperativo.** O cancelamento não é forçado:
`AsyncActionNode::halt()` apenas liga um `std::atomic_bool` e **espera a *thread*
terminar** (`thread_handle_.wait()`). É o seu `tick()` que precisa consultar
`isHaltRequested()` periodicamente e voltar. Se ele não consultar, o `halt()` **bloqueia a
árvore inteira** até a ação acabar por conta própria — um `Timeout` sobre uma ação que
ignora o pedido de parada não interrompe nada; apenas espera.

### O padrão correto

```cpp
// sample_nodes/movebase_node.cpp
BT::NodeStatus MoveBaseAction::tick()
{
    Pose2D goal;
    if ( !getInput<Pose2D>("goal", goal)) {
        throw BT::RuntimeError("missing required input [goal]");
    }

    printf("[ MoveBase: STARTED ]. goal: x=%.f y=%.1f theta=%.2f\n",
           goal.x, goal.y, goal.theta);

    _halt_requested.store(false);
    int count = 0;

    // Pretend that "computing" takes 250 milliseconds.
    // It is up to you to check periodicall _halt_requested and interrupt
    // this tick() if it is true.
    while (!_halt_requested && count++ < 25)
    {
        SleepMS(10);
    }

    std::cout << "[ MoveBase: FINISHED ]" << std::endl;
    return _halt_requested ? BT::NodeStatus::FAILURE : BT::NodeStatus::SUCCESS;
}

void MoveBaseAction::halt() { _halt_requested.store(true); }
```

**ARMADILHA — este `halt()` do exemplo oficial NÃO chama o da classe base.** O cabeçalho
instrui: *"in the overriden halt() method, you can do some cleanup, but do not forget to
invoke the base class method AsyncActionNode::halt()"*. O exemplo `movebase_node.cpp`
**não a invoca**. A consequência é que o `halt()` devolve o controle sem esperar a
*thread*: o nó é posto em `IDLE` enquanto a *thread* antiga ainda roda e ainda pode chamar
`setStatus()`. Se o nó for *ticado* de novo antes de ela terminar, **duas *threads*
escrevem o mesmo estado**. Siga o cabeçalho, não o exemplo:

```cpp
void MinhaAcao::halt()
{
    _halt_requested.store(true);
    // limpeza opcional aqui
    BT::AsyncActionNode::halt();     // <-- ESTA linha é obrigatória
}
```

**ARMADILHA — a exceção chega um *tick* atrasada, e o *handler* espera a si mesmo.** O
bloco `catch` guarda a exceção em `exptr_` e ela só é relançada na **próxima** chamada a
`executeTick()` — o *tick* em que o erro ocorreu devolve `RUNNING` normalmente. Pior: o
`catch` chama `thread_handle_.wait()` sobre o *future* que a própria *thread* está
preenchendo — e que pode nem ter sido atribuído ainda, porque a atribuição
`thread_handle_ = std::async(...)` só acontece **depois** de a tarefa ser lançada. Chamar
`wait()` num `std::future` inválido é comportamento indefinido. A leitura de `exptr_`
também não é sincronizada: é um `std::exception_ptr` cru, escrito numa *thread* e lido em
outra sem *mutex* nem atômico.

**ARMADILHA — uma *thread* nova a cada execução.** `std::async(std::launch::async, ...)`
cria uma *thread* do sistema operacional **por execução da ação** — não há *pool*. Uma
ação assíncrona dentro de um `Repeat` de mil ciclos cria mil *threads* ao longo do tempo.
Para ações de curta duração e alta frequência, `StatefulActionNode` é ordens de grandeza
mais barato.

**ARMADILHA — `halt_requested_` não é inicializado no construtor.** `std::atomic_bool
halt_requested_;` não tem inicializador de membro e o construtor não o inicializa. O valor
é indeterminado até o primeiro `executeTick()`, que faz `halt_requested_ = false`. Se
`halt()` for chamado antes de qualquer *tick* — o que `Tree::haltTree()` faz —, a leitura
é de um valor não inicializado.

## 12.8 `CoroActionNode` — suspender em vez de bloquear

**Disponível apenas se compilado com Boost.** Todo o código está dentro de
`#ifndef BT_NO_COROUTINES`.

```cpp
// include/behaviortree_cpp_v3/action_node.h
#ifndef BT_NO_COROUTINES
/**
 * @brief The CoroActionNode class is an ideal candidate for asynchronous actions
 * which need to communicate with an external service using an asynch request/reply interface
 * (being notable examples ActionLib in ROS, MoveIt clients or move_base clients).
 *
 * It is up to the user to decide when to suspend execution of the Action and resume
 * the parent node, invoking the method setStatusRunningAndYield().
 */
class CoroActionNode : public ActionNodeBase
{
  public:
    CoroActionNode(const std::string& name, const NodeConfiguration& config);
    virtual ~CoroActionNode() override;

    /// Use this method to return RUNNING and temporary "pause" the Action.
    void setStatusRunningAndYield();

    // This method triggers the TickEngine. Do NOT remove the "final" keyword.
    virtual NodeStatus executeTick() override final;

    /** You may want to override this method. But still, remember to call this
    * implementation too.
    *
    * Example:
    *
    *     void MyAction::halt()
    *     {
    *         // do your stuff here
    *         CoroActionNode::halt();
    *     }
    */
    void halt() override;

  protected:
    struct Pimpl; // The Pimpl idiom
    std::unique_ptr<Pimpl> _p;
};
#endif
```

```cpp
// src/action_node.cpp
struct CoroActionNode::Pimpl
{
    std::unique_ptr<coroutine<void>::pull_type> coro;
    std::function<void(coroutine<void>::push_type & yield)> func;
    coroutine<void>::push_type * yield_ptr;
};

CoroActionNode::CoroActionNode(const std::string &name, const NodeConfiguration& config):
 ActionNodeBase (name, config), _p( new Pimpl)
{
    _p->func = [this](coroutine<void>::push_type & yield) {
        _p->yield_ptr = &yield;
        setStatus(tick());
    };
}

void CoroActionNode::setStatusRunningAndYield()
{
    setStatus( NodeStatus::RUNNING );
    (*_p->yield_ptr)();
}

NodeStatus CoroActionNode::executeTick()
{
    if( !(_p->coro) || !(*_p->coro) )
    {
        _p->coro.reset( new coroutine<void>::pull_type(_p->func) );
        return status();
    }

    if( status() == NodeStatus::RUNNING && (bool)_p->coro )
    {
        (*_p->coro)();
    }

    return status();
}

void CoroActionNode::halt()
{
    _p->coro.reset();
}
```

Escrever a ação fica quase idêntico a escrevê-la de forma bloqueante — basta trocar cada
espera por `setStatusRunningAndYield()`. É a **melhor ergonomia das quatro**, e a única que
não obriga a explodir a lógica em *callbacks* ou em máquina de estados.

### O esqueleto ideal (de `examples/t09_async_actions_coroutines.cpp`)

```cpp
class MyAsyncAction: public CoroActionNode
{
  public:
    MyAsyncAction(const std::string& name): CoroActionNode(name, {}) {}

  private:
    // This is the ideal skeleton/template of an async action:
    //  - A request to a remote service provider.
    //  - A loop where we check if the reply has been received.
    //  - You may call setStatusRunningAndYield() to "pause".
    //  - Code to execute after the reply.
    //  - A simple way to handle halt().

    NodeStatus tick() override
    {
        std::cout << name() <<": Started. Send Request to server." << std::endl;

        auto Now = [](){ return std::chrono::high_resolution_clock::now(); };
        TimePoint initial_time = Now();
        TimePoint time_before_reply = initial_time + std::chrono::milliseconds(100);

        int count = 0;
        bool reply_received = false;

        while( !reply_received )
        {
            if( count++ == 0) {
                // call this only once
                std::cout << name() <<": Waiting Reply..." << std::endl;
            }
            // pretend that we received a reply
            if( Now() >= time_before_reply ) { reply_received = true; }

            if( !reply_received )
            {
                // set status to RUNNING and "pause/sleep"
                // If halt() is called, we will not resume execution (stack destroyed)
                setStatusRunningAndYield();
            }
        }

        // This part of the code is never reached if halt() is invoked,
        // only if reply_received == true;
        std::cout << name() <<": Done. 'Waiting Reply' loop repeated "
                  << count << " times" << std::endl;
        cleanup(false);
        return NodeStatus::SUCCESS;
    }

    // you might want to cleanup differently if it was halted or successful
    void cleanup(bool halted)
    {
        if( halted ) { std::cout << name() <<": cleaning up after an halt()\n" << std::endl; }
        else         { std::cout << name() <<": cleaning up after SUCCESS\n" << std::endl; }
    }

    void halt() override
    {
        std::cout << name() <<": Halted." << std::endl;
        cleanup(true);
        // Do not forget to call this at the end.
        CoroActionNode::halt();
    }
};
```

**ARMADILHA — não existe no empacotamento documentado.** Toda a classe está dentro de
`#ifndef BT_NO_COROUTINES`, e esse macro é definido pelo `CMakeLists.txt` quando Boost não
é encontrado. A receita Conan do *fork* desabilita a busca por Boost deliberadamente e
propaga `BT_NO_COROUTINES` aos consumidores. Na prática: `CoroActionNode` **não existe no
binário**, o exemplo `t09` e o teste `gtest_coroutines.cpp` não são compilados, e um
`#include` de `action_node.h` não declara a classe. É a maior funcionalidade ausente dessa
configuração.

**ARMADILHA — `CoroActionNode::halt()` descarta a pilha sem desenrolá-la explicitamente.**
`halt()` é `_p->coro.reset();` — e só. A co-rotina é destruída no ponto em que estava
suspensa. O Boost desenrola a pilha nesse caso, então os destrutores locais rodam; mas o
seu `tick()` **não recebe aviso nenhum**, e qualquer limpeza precisa estar em objetos RAII
na pilha da co-rotina, nunca em código **após** o ponto de suspensão. Se você sobrescrever
`halt()`, o cabeçalho lembra de chamar `CoroActionNode::halt()` ao final.

## 12.9 Ações auxiliares embutidas

Três ações vêm registradas de fábrica e não precisam de código nenhum.

### `AlwaysSuccess` e `AlwaysFailure`

```cpp
// include/behaviortree_cpp_v3/actions/always_success_node.h  (íntegro, sem licença/guards)
/**
 * Simple actions that always returns SUCCESS.
 */
class AlwaysSuccessNode : public SyncActionNode
{
  public:
    AlwaysSuccessNode(const std::string& name) :
        SyncActionNode(name, {} )
    {
        setRegistrationID("AlwaysSuccess");
    }

  private:
    virtual BT::NodeStatus tick() override
    {
        return NodeStatus::SUCCESS;
    }
};
```

`AlwaysFailureNode` é o simétrico. Usos:

- *stub* em árvores em construção;
- terceiro filho explícito num `IfThenElse` de dois ramos;
- último filho de um `Fallback` para forçar sucesso do bloco todo (`AlwaysSuccess`);
- último filho de um `Switch` como *default* inerte.

### `SetBlackboard`

```cpp
// include/behaviortree_cpp_v3/actions/set_blackboard_node.h  (íntegro)
/**
 * @brief The SetBlackboard is action used to store a string
 * into an entry of the Blackboard specified in "output_key".
 *
 * Example usage:
 *
 *  <SetBlackboard value="42" output_key="the_answer" />
 *
 * Will store the string "42" in the entry with key "the_answer".
 */
class SetBlackboard : public SyncActionNode
{
  public:
    SetBlackboard(const std::string& name, const NodeConfiguration& config)
      : SyncActionNode(name, config)
    {
        setRegistrationID("SetBlackboard");
    }

    static PortsList providedPorts()
    {
        return { InputPort("value",
                   "Value represented as a string. convertFromString must be implemented."),
                 BidirectionalPort("output_key",
                   "Name of the blackboard entry where the value should be written") };
    }

  private:
    virtual BT::NodeStatus tick() override
    {
        std::string key, value;
        if ( !getInput("output_key", key) )
        {
            throw RuntimeError("missing port [output_key]");
        }
        if ( !getInput("value", value) )
        {
            throw RuntimeError("missing port [value]");
        }
        setOutput("output_key", value);
        return NodeStatus::SUCCESS;
    }
};
```

**ARMADILHA — `SetBlackboard` escreve texto, não o tipo de destino.** O valor é sempre
lido e escrito como `std::string`. Que ele acabe virando um `Position2D` depende da
conversão diferida em `Blackboard::set()`, que só funciona se a entrada **já** tiver sido
declarada com tipo por alguma porta tipada. Se o `SetBlackboard` for o **primeiro** a tocar
a chave, ela fica sendo uma *string* — e o `getInput<Position2D>` do nó seguinte a converte
a cada leitura, ou falha, conforme a especialização exista.

Repare também que `output_key` é declarada **bidirecional**: ela é lida como entrada (para
obter o nome da chave) e usada como saída (para escrever nela). É a única porta `INOUT` de
toda a biblioteca.

Exemplo do `t03`:

```xml
<Sequence name="root">
    <CalculateGoal   goal="{GoalPosition}" />
    <PrintTarget     target="{GoalPosition}" />
    <SetBlackboard   output_key="OtherGoal" value="-1;3" />
    <PrintTarget     target="{OtherGoal}" />
</Sequence>
```

Aqui `OtherGoal` recebe a *string* `"-1;3"`, e o `PrintTarget` seguinte, cuja porta é
`InputPort<Position2D>("target")`, aciona `convertFromString<Position2D>` na leitura.

## 12.10 Argumentos que não são portas

Nem toda configuração precisa vir do XML. Para parâmetros conhecidos em tempo de
compilação ou de *deployment* — um ponteiro para um cliente, um identificador de
*hardware*, uma referência ao mundo simulado — portas são exagero. O exemplo `t08` mostra
as duas saídas.

### Forma 1 (recomendada): *builder* próprio

```cpp
// examples/t08_additional_node_args.cpp
class Action_A: public SyncActionNode
{
public:
    // additional arguments passed to the constructor
    Action_A(const std::string& name, const NodeConfiguration& config,
             int arg1, double arg2, std::string arg3 ):
        SyncActionNode(name, config), _arg1(arg1), _arg2(arg2), _arg3(arg3) {}

    NodeStatus tick() override { /* ... */ return NodeStatus::SUCCESS; }
    static PortsList providedPorts() { return {}; }

private:
    int _arg1;  double _arg2;  std::string _arg3;
};

// ...
// A node builder is nothing more than a function pointer to create a
// std::unique_ptr<TreeNode>.
// Using lambdas or std::bind, we can easily "inject" additional arguments.
NodeBuilder builder_A = [](const std::string& name, const NodeConfiguration& config)
{
    return std::make_unique<Action_A>( name, config, 42, 3.14, "hello world" );
};

// BehaviorTreeFactory::registerBuilder is the more general way to register a custom node.
// Not the most user friendly, but definitely the most flexible one.
factory.registerBuilder<Action_A>( "Action_A", builder_A);
```

### Forma 2: método `init()` chamado após a construção

```cpp
class Action_B: public SyncActionNode
{
public:
    Action_B(const std::string& name, const NodeConfiguration& config):
        SyncActionNode(name, config) {}

    // we want this method to be called ONCE and BEFORE the first tick()
    void init( int arg1, double arg2, std::string arg3 )
    {
        _arg1 = (arg1);  _arg2 = (arg2);  _arg3 = (arg3);
    }
    // ...
};

// ...
auto tree = factory.createTreeFromText(xml_text);

// Iterate through all the nodes and call init if it is an Action_B
for( auto& node: tree.nodes )
{
    if( auto action_B_node = dynamic_cast<Action_B*>( node.get() ))
    {
        action_B_node->init( 69, 9.99, "interesting_value" );
    }
}
```

**REGRA — prefira o *builder*.** A segunda forma depende de o programador lembrar de
chamar `init()` antes do primeiro *tick*, e nada verifica isso: esquecer produz um nó com
membros não inicializados. A primeira torna a injeção parte da construção e não tem como
ser esquecida. Use `init()` apenas quando o argumento só existir **depois** da árvore — por
exemplo, um ponteiro para a própria `Tree`.

### Forma 3: lambda com captura, para nós simples

```cpp
Porta porta;    // o "mundo"

factory.registerSimpleCondition("EstaAberta", [&porta](BT::TreeNode&) {
    return porta.aberta ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
});

factory.registerBuilder<Destrancar>("Destrancar",
    [&porta](const std::string& name, const BT::NodeConfiguration& config) {
        return std::make_unique<Destrancar>(name, config, &porta);
    });
```

**ARMADILHA — o tempo de vida do capturado.** A lambda é copiada para dentro da fábrica e
depois para dentro de cada nó. Se ela capturar por referência (`[&porta]`) algo que morra
antes da árvore, todo *tick* posterior é comportamento indefinido. Como a `Tree` costuma
viver no `main()` junto com o objeto capturado, na prática funciona — mas em código que
constrói árvores dentro de funções, capture por valor ou use `shared_ptr`.

---

# 13. O FORMATO XML

O formato é deliberadamente simples: **cada tag é um nó, cada atributo é uma porta.**

## 13.1 Estrutura mínima

```xml
<root main_tree_to_execute="MainTree">

    <BehaviorTree ID="MainTree">
        <Sequence name="raiz">
            <CheckBattery/>
            <SaySomething message="ola"/>
            <MoveBase     goal="{destino}"/>
        </Sequence>
    </BehaviorTree>

</root>
```

Regras:

- `<root>` é obrigatório e deve ser o elemento de topo.
- `<root>` contém **uma ou mais** tags `<BehaviorTree>`.
- `main_tree_to_execute` é **obrigatório** se houver mais de uma `<BehaviorTree>`;
  opcional se houver exatamente uma.
- Cada `<BehaviorTree>` tem exatamente **um** filho.
- O nome da tag é o **ID de registro**; o atributo `name` é o nome da instância e é
  **opcional**.
- Todo atributo que não seja `ID` nem `name` é tratado como **porta**.
- Aridade: `ControlNode` de 1 a N filhos; `DecoratorNode` e subárvores exatamente 1;
  `ActionNode` e `ConditionNode` nenhum.

## 13.2 Forma compacta e forma explícita

As duas linhas abaixo são equivalentes:

```xml
<SaySomething             name="ola" message="Hello World"/>
<Action ID="SaySomething" name="ola" message="Hello World"/>
```

A forma **explícita** usa uma das cinco tags genéricas — `<Action>`, `<Condition>`,
`<Control>`, `<Decorator>`, `<SubTree>` — e põe o ID num atributo. É mais verbosa e
carrega mais informação: o **Groot** precisa dela (ou de um `<TreeNodesModel>`) para saber
o tipo de cada nó sem consultar o binário.

```cpp
// src/xml_parsing.cpp (createNodeFromXML)
const std::string element_name = element->Name();
std::string ID;
std::string instance_name;

// Actions and Decorators have their own ID
if (element_name == "Action" || element_name == "Decorator" ||
    element_name == "Condition" || element_name == "Control")
{
    ID = element->Attribute("ID");
}
else
{
    ID = element_name;
}
```

**Consequência:** a forma explícita **exige** o atributo `ID` (a validação confere), e a
compacta **não pode** ter `ID` (seria ignorado como porta desconhecida — na verdade, `ID`
e `name` são sempre filtrados da lista de portas).

## 13.3 A validação: `VerifyXML()`

Antes de instanciar coisa alguma, o *parser* chama `VerifyXML()`, uma função livre de
cerca de duzentas linhas em `src/xml_parsing.cpp`.

| Tag | Exigência |
|---|---|
| `<root>` | é o elemento de topo (senão: `"The XML must have a root node called <root>"`) |
| `<BehaviorTree>` | exatamente 1 filho |
| `<Decorator>` | exatamente 1 filho, e atributo `ID` |
| `<Action>`, `<Condition>` | nenhum filho, e atributo `ID` |
| `<Control>` | ao menos 1 filho, e atributo `ID` |
| `<Sequence>`, `<SequenceStar>`, `<Fallback>` | ao menos 1 filho |
| `<SubTree>` | nenhum filho, e atributo `ID` |
| `<TreeNodesModel>` | no máximo um por arquivo |
| qualquer outra tag | precisa ser um ID registrado **ou** o ID de uma `<BehaviorTree>` |

E, ao final:

- se `main_tree_to_execute` estiver presente, a árvore citada precisa existir
  (`"The tree specified in [main_tree_to_execute] can't be found"`);
- se estiver ausente, o arquivo precisa conter **exatamente uma** `<BehaviorTree>`
  (`"If you don't specify the attribute [main_tree_to_execute], Your file must contain a
  single BehaviorTree"`).

**ARMADILHA — a aridade só é conferida nas tags genéricas.** `<Decorator ID="Timeout">`
tem a aridade verificada, mas `<Timeout>` **não**. Da mesma forma, a exigência de "ao menos
1 filho" cobre `Sequence`, `SequenceStar` e `Fallback` — e **não** cobre
`ReactiveSequence`, `ReactiveFallback`, `Parallel`, `IfThenElse`, `WhileDoElse` nem
`Switch<N>`. Escrever a árvore na forma compacta troca uma parte da validação de carga por
exceções em tempo de *tick*.

**ARMADILHA — uma mensagem de erro que imprime o próprio formato.** Dentro da verificação
do `<TreeNodesModel>`, a chamada é:

```cpp
// src/xml_parsing.cpp
ThrowError(node->GetLineNum(),
           "Error at line %d: -> The attribute [ID] is mandatory");
```

Mas `ThrowError` já formata `"Error at line %d: -> %s"` com o número da linha. O resultado
é a mensagem `"Error at line 12: -> Error at line %d: -> The attribute [ID] is
mandatory"` — com o `%d` literal, e o prefixo duplicado. Todas as outras chamadas passam
apenas o texto.

**ARMADILHA — `<TreeNodesModel>` ou `<TreeNodeModel>`?** O código lê `TreeNodesModel`
(com `s`), e é isso que `writeTreeNodesModelXML()` gera. A documentação em
`docs/xml_format.md` escreve `<TreeNodeModel>` (sem `s`) em todos os exemplos. Como a
validação só percorre recursivamente o que está sob `<BehaviorTree>`, uma tag de nome
errado no nível de `<root>` é **silenciosamente ignorada**: nem erro, nem modelo. Um Groot
que não enxerga os seus nós costuma ser isto. O mesmo vale para `<Subtree>` minúsculo, que
a documentação também usa: o *parser* exige `<SubTree>`.

## 13.4 O modelo para o Groot: `<TreeNodesModel>`

Para que o Groot entenda uma árvore escrita na forma compacta, o XML pode declarar o
modelo dos nós:

```xml
<root main_tree_to_execute="MainTree">
    <BehaviorTree ID="MainTree">
        <Sequence name="root_sequence">
           <SaySomething   name="action_hello" message="Hello"/>
           <OpenGripper    name="open_gripper"/>
           <ApproachObject name="approach_object"/>
           <CloseGripper   name="close_gripper"/>
        </Sequence>
    </BehaviorTree>

    <!-- the BT executor don't require this, but Groot does -->
    <TreeNodesModel>
        <Action ID="SaySomething">
            <input_port name="message" type="std::string" />
        </Action>
        <Action ID="OpenGripper"/>
        <Action ID="ApproachObject"/>
        <Action ID="CloseGripper"/>
    </TreeNodesModel>
</root>
```

(Note que este trecho da documentação oficial usa `<TreeNodeModel>`; a grafia correta,
aceita pelo código, é `<TreeNodesModel>`.)

Esse bloco pode ser **gerado** a partir da fábrica:

```cpp
// include/behaviortree_cpp_v3/xml_parsing.h
std::string writeTreeNodesModelXML(const BehaviorTreeFactory& factory);
```

```cpp
// src/xml_parsing.cpp (condensado)
for (auto& model_it : factory.manifests())
{
    const auto& registration_ID = model_it.first;
    const auto& model = model_it.second;

    if( factory.builtinNodes().count( registration_ID ) != 0) { continue; }  // pula embutidos
    if (model.type == NodeType::CONTROL)                      { continue; }  // pula controles

    XMLElement* element = doc.NewElement( toStr(model.type).c_str() );
    element->SetAttribute("ID", model.registration_ID.c_str());

    for (auto& port : model.ports)
    {
        XMLElement* port_element = nullptr;
        switch(  port.second.direction() )
        {
        case PortDirection::INPUT:  port_element = doc.NewElement("input_port");  break;
        case PortDirection::OUTPUT: port_element = doc.NewElement("output_port"); break;
        case PortDirection::INOUT:  port_element = doc.NewElement("inout_port");  break;
        }
        port_element->SetAttribute("name", port.first.c_str() );
        if( port.second.type() )
            port_element->SetAttribute("type", BT::demangle( port.second.type() ).c_str() );
        if( !port.second.defaultValue().empty() )
            port_element->SetAttribute("default", port.second.defaultValue().c_str() );
        if( !port.second.description().empty() )
            port_element->SetText( port.second.description().c_str() );
        element->InsertEndChild(port_element);
    }
    model_root->InsertEndChild(element);
}
```

**Duas exclusões explícitas:** nós **embutidos** e nós de **controle** nunca entram no
modelo gerado. A primeira porque o Groot já os conhece; a segunda porque nós de controle
customizados não são bem suportados pela ferramenta.

## 13.5 `<include>`: dividir em arquivos

Desde a versão 2.4, um `<include>` no nível de `<root>` carrega outro arquivo antes de
processar o corrente, à maneira de um `#include`:

```xml
<!-- examples/test_files/subtree_test.xml -->
<root main_tree_to_execute="BehaviorTree">

    <include path="Check.xml" />
    <include path="subtrees/Talk.xml" />

    <BehaviorTree ID="BehaviorTree">
        <Sequence>
            <CheckStatus/>
            <OpenGripper/>
            <CloseGripper/>
            <SayStuff/>
        </Sequence>
    </BehaviorTree>

</root>
```

```xml
<!-- examples/test_files/Check.xml -->
<root>
    <BehaviorTree ID="CheckStatus">
        <SequenceStar>
            <Action ID="CheckBattery"/>
            <Action ID="CheckTemperature"/>
        </SequenceStar>
    </BehaviorTree>
</root>
```

Repare que `<CheckStatus/>` e `<SayStuff/>` **não são nós registrados**: são os `ID` das
`<BehaviorTree>` definidas nos arquivos incluídos, usados diretamente como tag. É uma forma
abreviada de `<SubTree ID="CheckStatus"/>`, e o *parser* a reconhece explicitamente:

```cpp
// src/xml_parsing.cpp (createNodeFromXML)
if( factory.builders().count(ID) != 0)  { /* nó registrado */ }
else if( tree_roots.count(ID) != 0)     { child_node = std::make_unique<SubtreeNode>(instance_name); }
else                                     { throw RuntimeError( ID, " is not a registered node, nor a Subtree"); }
```

### Resolução de caminho

```cpp
// src/xml_parsing.cpp (loadDocImpl)
filesystem::path file_path( include_node->Attribute("path") );

if( include_node->Attribute("ros_pkg") )
{
#ifdef USING_ROS
    if( file_path.is_absolute() )
    {
        std::cout << "WARNING: <include path=\"...\"> containes an absolute path.\n"
                  << "Attribute [ros_pkg] will be ignored."<< std::endl;
    }
    else {
        auto ros_pkg_path = ros::package::getPath( include_node->Attribute("ros_pkg") );
        file_path = filesystem::path( ros_pkg_path ) / file_path;
    }
#else
    throw RuntimeError("Using attribute [ros_pkg] in <include>, but this library was compiled "
                       "without ROS support. Recompile the BehaviorTree.CPP using catkin");
#endif
}

if( !file_path.is_absolute() )
{
    file_path = current_path / file_path;
}
```

Caminhos relativos são resolvidos a partir do **diretório do arquivo que inclui** — não do
diretório de trabalho. `current_path` é definido em `loadFromFile()` como
`file_path.parent_path().make_absolute()`.

**ARMADILHA — `loadFromText()` não define `current_path`.** Nesse caminho,
`current_path` continua sendo `filesystem::path::getcwd()` (o valor inicial do `Pimpl`).
Um `<include path="rel.xml">` dentro de um XML passado como *string* é resolvido a partir
do **diretório de trabalho do processo**, e não do lugar onde o XML "estaria".

**ARMADILHA — não há detecção de inclusão circular.** `loadDocImpl()` chama a si mesma
para cada `<include>`, **sem manter um conjunto de arquivos já vistos**. Dois arquivos que
se incluam mutuamente produzem recursão infinita e estouro de pilha — sem mensagem. Não há
guarda equivalente ao `#pragma once`.

**ARMADILHA — cada arquivo é validado sozinho, na ordem em que é lido.** A validação de um
arquivo incluído enxerga apenas as árvores registradas **até aquele momento**. Um arquivo
que referencie uma `<BehaviorTree>` definida no arquivo que o inclui falha com
`"Node not recognized"`, ainda que o conjunto final seja consistente. A dependência tem de
fluir num sentido só — do incluído para o includente — e os `<include>` precisam vir antes
das `<BehaviorTree>` que os usam.

**ARMADILHA — erro de leitura do arquivo incluído vira "Error parsing the XML".** Se o
caminho estiver errado, `next_doc->LoadFile()` falha e o `loadDocImpl` seguinte detecta
`doc->Error()`, lançando `RuntimeError("Error parsing the XML: XML_ERROR_FILE_NOT_FOUND")`
— uma mensagem que **não diz qual arquivo**.

## 13.6 Comentários e entidades XML

Comentários XML normais funcionam e são ignorados pelo *tinyxml2*:

```xml
<root main_tree_to_execute = "MainTree">
	<!--------------------------------------->
    <BehaviorTree ID="DoorClosed">
        ...
    </BehaviorTree>
    <!--------------------------------------->
</root>
```

Atributos precisam escapar caracteres especiais pelas regras do XML (`&amp;`, `&lt;`,
`&gt;`, `&quot;`, `&apos;`). Isso importa para portas cujo valor contenha `<` ou `&`.

## 13.7 Onde escrever o XML: arquivo ou *string*

```cpp
Tree createTreeFromText(const std::string& text,
                        Blackboard::Ptr blackboard = Blackboard::create());
Tree createTreeFromFile(const std::string& file_path,
                        Blackboard::Ptr blackboard = Blackboard::create());
```

Os exemplos do repositório usam *raw string literals* para embutir o XML no `.cpp`:

```cpp
// clang-format off
static const char* xml_text = R"(

 <root main_tree_to_execute = "MainTree" >
     <BehaviorTree ID="MainTree">
        <Sequence name="root_sequence">
            <CheckBattery   name="battery_ok"/>
            <OpenGripper    name="open_gripper"/>
            <ApproachObject name="approach_object"/>
            <CloseGripper   name="close_gripper"/>
        </Sequence>
     </BehaviorTree>
 </root>
 )";
// clang-format on
```

Os comentários `// clang-format off/on` existem para o formatador não destruir a
indentação do XML.

---

# 14. `BehaviorTreeFactory` — REFERÊNCIA

`BT::BehaviorTreeFactory` é um dicionário de *string* para função construtora, mais um
dicionário paralelo de manifestos.

Arquivos: `include/behaviortree_cpp_v3/bt_factory.h`, `src/bt_factory.cpp`.

## 14.1 Estado interno e tipos

```cpp
// include/behaviortree_cpp_v3/bt_factory.h
/// The term "Builder" refers to the Builder Pattern
typedef std::function<std::unique_ptr<TreeNode>(const std::string&, const NodeConfiguration&)>
NodeBuilder;

class BehaviorTreeFactory
{
    // ...
private:
    std::unordered_map<std::string, NodeBuilder>      builders_;
    std::unordered_map<std::string, TreeNodeManifest> manifests_;
    std::set<std::string>                             builtin_IDs_;
};
```

## 14.2 API pública completa

```cpp
BehaviorTreeFactory();

/// Remove a registered ID.
bool unregisterBuilder(const std::string& ID);

/// The most generic way to register your own builder.
void registerBuilder(const TreeNodeManifest& manifest, const NodeBuilder& builder);

template <typename T>
void registerBuilder(const std::string& ID, const NodeBuilder& builder);

void registerSimpleAction(const std::string& ID,
                          const SimpleActionNode::TickFunctor& tick_functor,
                          PortsList ports = {});
void registerSimpleCondition(const std::string& ID,
                             const SimpleConditionNode::TickFunctor& tick_functor,
                             PortsList ports = {});
void registerSimpleDecorator(const std::string& ID,
                             const SimpleDecoratorNode::TickFunctor& tick_functor,
                             PortsList ports = {});

/// load a shared library and execute the function BT_REGISTER_NODES
void registerFromPlugin(const std::string &file_path);

/// finds all shared libraries that export ROS plugins for behaviortree_cpp
void registerFromROSPlugins();

std::unique_ptr<TreeNode> instantiateTreeNode(const std::string& name, const std::string& ID,
                                              const NodeConfiguration& config) const;

template <typename T> void registerNodeType(const std::string& ID);
template <typename T> void registerNodeType(const std::string& ID, PortsList ports);

/// All the builders. Made available mostly for debug purposes.
const std::unordered_map<std::string, NodeBuilder>& builders() const;
/// Manifests of all the registered TreeNodes.
const std::unordered_map<std::string, TreeNodeManifest>& manifests() const;
/// List of builtin IDs.
const std::set<std::string>& builtinNodes() const;

Tree createTreeFromText(const std::string& text, Blackboard::Ptr blackboard = Blackboard::create());
Tree createTreeFromFile(const std::string& file_path, Blackboard::Ptr blackboard = Blackboard::create());
```

## 14.3 O construtor: os nós embutidos

```cpp
// src/bt_factory.cpp  (íntegro)
BehaviorTreeFactory::BehaviorTreeFactory()
{
    registerNodeType<FallbackNode>("Fallback");
    registerNodeType<SequenceNode>("Sequence");
    registerNodeType<SequenceStarNode>("SequenceStar");
    registerNodeType<ParallelNode>("Parallel");
    registerNodeType<ReactiveSequence>("ReactiveSequence");
    registerNodeType<ReactiveFallback>("ReactiveFallback");
    registerNodeType<IfThenElseNode>("IfThenElse");
    registerNodeType<WhileDoElseNode>("WhileDoElse");

    registerNodeType<InverterNode>("Inverter");
    registerNodeType<RetryNode>("RetryUntilSuccesful");
    registerNodeType<KeepRunningUntilFailureNode>("KeepRunningUntilFailure");
    registerNodeType<RepeatNode>("Repeat");
    registerNodeType<TimeoutNode<>>("Timeout");
    registerNodeType<DelayNode>("Delay");

    registerNodeType<ForceSuccessNode>("ForceSuccess");
    registerNodeType<ForceFailureNode>("ForceFailure");

    registerNodeType<AlwaysSuccessNode>("AlwaysSuccess");
    registerNodeType<AlwaysFailureNode>("AlwaysFailure");
    registerNodeType<SetBlackboard>("SetBlackboard");

    registerNodeType<SubtreeNode>("SubTree");
    registerNodeType<SubtreePlusNode>("SubTreePlus");

    registerNodeType<BlackboardPreconditionNode<int>>("BlackboardCheckInt");
    registerNodeType<BlackboardPreconditionNode<double>>("BlackboardCheckDouble");
    registerNodeType<BlackboardPreconditionNode<std::string>>("BlackboardCheckString");

    registerNodeType<SwitchNode<2>>("Switch2");
    registerNodeType<SwitchNode<3>>("Switch3");
    registerNodeType<SwitchNode<4>>("Switch4");
    registerNodeType<SwitchNode<5>>("Switch5");
    registerNodeType<SwitchNode<6>>("Switch6");

#ifdef NCURSES_FOUND
    registerNodeType<ManualSelectorNode>("ManualSelector");
#endif
    for( const auto& it: builders_)
    {
        builtin_IDs_.insert( it.first );
    }
}
```

É essa última varredura que povoa `builtin_IDs_` — a lista que distingue o que veio de
fábrica do que o usuário acrescentou.

## 14.4 `registerNodeType<T>()` e os cinco `static_assert`

```cpp
// include/behaviortree_cpp_v3/bt_factory.h
template <typename T>
void registerNodeType(const std::string& ID)
{
    static_assert(std::is_base_of<ActionNodeBase, T>::value ||
                  std::is_base_of<ControlNode, T>::value ||
                  std::is_base_of<DecoratorNode, T>::value ||
                  std::is_base_of<ConditionNode, T>::value,
                  "[registerNode]: accepts only classed derived from either ActionNodeBase, "
                  "DecoratorNode, ControlNode or ConditionNode");

    static_assert(!std::is_abstract<T>::value,
                  "[registerNode]: Some methods are pure virtual. "
                  "Did you override the methods tick() and halt()?");

    constexpr bool default_constructable = std::is_constructible<T, const std::string&>::value;
    constexpr bool param_constructable =
            std::is_constructible<T, const std::string&, const NodeConfiguration&>::value;
    constexpr bool has_static_ports_list = has_static_method_providedPorts<T>::value;

    static_assert(default_constructable || param_constructable,
                  "[registerNode]: the registered class must have at least one of these two "
                  "constructors: "
                  "  (const std::string&, const NodeConfiguration&) or (const std::string&).");

    static_assert(!(param_constructable && !has_static_ports_list),
                  "[registerNode]: you MUST implement the static method: "
                  "  PortsList providedPorts();\n");

    static_assert(!(has_static_ports_list && !param_constructable),
                  "[registerNode]: since you have a static method providedPorts(), "
                  "you MUST add a constructor sign signature (const std::string&, const "
                  "NodeParameters&)\n");

    registerBuilder( CreateManifest<T>(ID), CreateBuilder<T>());
}
```

As duas últimas formam um **par**: ou o nó tem **ambos** — construtor com
`NodeConfiguration` e `providedPorts()` — ou **nenhum** dos dois. É a regra que impede o
erro mais comum de quem começa: declarar portas e esquecer o construtor que recebe a
configuração.

**REGRA — as mensagens de erro mentem sobre um nome.** Duas delas falam em
`NodeParameters`. Esse tipo **não existe na v3** — é o nome que `NodeConfiguration` tinha na
v2. Ao ler *"you MUST add a constructor sign signature (const std::string&, const
NodeParameters&)"*, entenda `NodeConfiguration`. (O "sign signature" é do original.)

### A sobrecarga com `PortsList` explícita

```cpp
template <typename T>
void registerNodeType(const std::string& ID, PortsList ports)
{
  // ... mesmos asserts de tipo e de abstração ...

  static_assert(!has_static_ports_list,
                "[registerNode]: ports are passed to this node explicitly. The static method"
                "providedPorts() should be removed to avoid ambiguities\n");

  static_assert(param_constructable,
                "[registerNode]: since this node has ports, "
                "you MUST add a constructor sign signature (const std::string&, const "
                "NodeParameters&)\n");

  registerBuilder( CreateManifest<T>(ID, ports), CreateBuilder<T>());
}
```

## 14.5 `CreateBuilder<T>()` e o construtor escolhido

```cpp
// include/behaviortree_cpp_v3/bt_factory.h
template <typename T>
using has_default_constructor = typename std::is_constructible<T, const std::string&>;

template <typename T>
using has_params_constructor  = typename std::is_constructible<T, const std::string&,
                                                               const NodeConfiguration&>;

// (1) tem OS DOIS construtores
template <typename T> inline
  NodeBuilder CreateBuilder(typename std::enable_if<has_default_constructor<T>::value &&
                                        has_params_constructor<T>::value >::type* = nullptr)
{
  return [](const std::string& name, const NodeConfiguration& config)
  {
    // Special case. Use default constructor if parameters are empty
    if( config.input_ports.empty() &&
        config.output_ports.empty() &&
        has_default_constructor<T>::value)
    {
      return std::make_unique<T>(name);
    }
    return std::make_unique<T>(name, config);
  };
}

// (2) SÓ o construtor com config
template <typename T> inline
  NodeBuilder CreateBuilder(typename std::enable_if<!has_default_constructor<T>::value &&
                                        has_params_constructor<T>::value >::type* = nullptr)
{
  return [](const std::string& name, const NodeConfiguration& params)
  {
    return std::unique_ptr<TreeNode>(new T(name, params));
  };
}

// (3) SÓ o construtor com nome
template <typename T> inline
  NodeBuilder CreateBuilder(typename std::enable_if<has_default_constructor<T>::value &&
                                        !has_params_constructor<T>::value >::type* = nullptr)
{
  return [](const std::string& name, const NodeConfiguration&)
  {
    return std::unique_ptr<TreeNode>(new T(name));
  };
}

template <typename T>
TreeNodeManifest CreateManifest(const std::string& ID, PortsList portlist = getProvidedPorts<T>())
{
  return { getType<T>(), ID, portlist };
}
```

**ARMADILHA — com os dois construtores, um nó sem atributos perde o *blackboard*.** Se a
sua classe tiver **ambos** os construtores e o XML não fornecer **nenhum** atributo de
porta, a fábrica usa o construtor de um argumento — e o `NodeConfiguration`, **inclusive o
ponteiro para o *blackboard***, é descartado. O nó fica sem acesso a portas e sem acesso ao
*blackboard*, e qualquer `getInput()` falha com *"NodeConfiguration::input_ports does not
contain the key"* — inclusive para portas que teriam valor padrão, porque o padrão só é
aplicado quando há o que preencher.

**A solução é não declarar o construtor de um argumento em nós que tenham portas.**

## 14.6 Registro e desregistro

```cpp
// src/bt_factory.cpp
void BehaviorTreeFactory::registerBuilder(const TreeNodeManifest& manifest,
                                          const NodeBuilder& builder)
{
    auto it = builders_.find( manifest.registration_ID);
    if (it != builders_.end())
    {
        throw BehaviorTreeException("ID [", manifest.registration_ID, "] already registered");
    }

    builders_.insert(  {manifest.registration_ID, builder} );
    manifests_.insert( {manifest.registration_ID, manifest} );
}

bool BehaviorTreeFactory::unregisterBuilder(const std::string& ID)
{
    if( builtinNodes().count(ID) )
    {
        throw LogicError("You can not remove the builtin registration ID [", ID, "]");
    }
    auto it = builders_.find(ID);
    if (it == builders_.end())
    {
        return false;
    }
    builders_.erase(ID);
    manifests_.erase(ID);
    return true;
}
```

**ARMADILHA — registrar duas vezes o mesmo ID é exceção, não substituição.** Não há
sobreposição: para trocar a implementação de um ID é preciso `unregisterBuilder()` antes —
e esse método **recusa** os IDs embutidos. Ou seja, **não é possível redefinir
`Sequence`, `Timeout` ou qualquer outro nó de fábrica**. Se dois *plugins* registrarem o
mesmo nome, o segundo `registerFromPlugin()` lança no meio da carga, deixando a fábrica
parcialmente povoada.

## 14.7 `instantiateTreeNode()`

```cpp
// src/bt_factory.cpp
std::unique_ptr<TreeNode> BehaviorTreeFactory::instantiateTreeNode(
        const std::string& name, const std::string& ID, const NodeConfiguration& config) const
{
    auto it = builders_.find(ID);
    if (it == builders_.end())
    {
        std::cerr << ID << " not included in this list:" << std::endl;
        for (const auto& builder_it: builders_)
        {
            std::cerr << builder_it.first << std::endl;
        }
        throw RuntimeError("BehaviorTreeFactory: ID [", ID, "] not registered");
    }

    std::unique_ptr<TreeNode> node = it->second(name, config);
    node->setRegistrationID( ID );
    return node;
}
```

**Comportamento útil de depuração:** ao não encontrar o ID, a fábrica imprime em
`std::cerr` **a lista completa** de IDs registrados antes de lançar. É a forma mais rápida
de descobrir um erro de grafia.

**Note a linha final:** `node->setRegistrationID(ID)` **sobrescreve** o que o construtor
do nó tiver definido. É por isso que o `RetryNode` acaba com a grafia errada do registro.

## 14.8 Os registros "simples"

```cpp
// src/bt_factory.cpp
void BehaviorTreeFactory::registerSimpleCondition(const std::string& ID,
                                                  const SimpleConditionNode::TickFunctor& tick_functor,
                                                  PortsList ports)
{
    NodeBuilder builder = [tick_functor, ID](const std::string& name,
                                             const NodeConfiguration& config) {
        return std::make_unique<SimpleConditionNode>(name, tick_functor, config);
    };

    TreeNodeManifest manifest = { NodeType::CONDITION, ID, std::move(ports) };
    registerBuilder(manifest, builder);
}
```

`registerSimpleAction` e `registerSimpleDecorator` são idênticos, com
`NodeType::ACTION`/`NodeType::DECORATOR` e as classes correspondentes.

**Note que o *functor* é copiado para dentro da lambda do *builder*, e a lambda é copiada
para a fábrica.** Um *functor* que capture por referência precisa que o capturado
sobreviva à árvore.

## 14.9 Consultando a fábrica

```cpp
BT::BehaviorTreeFactory factory;
// ... registros ...

// quantos IDs existem?
std::cout << factory.manifests().size() << std::endl;

// listar tudo, ordenado
std::map<std::string, BT::TreeNodeManifest> ordenado;
for (const auto& it : factory.manifests()) { ordenado.insert(it); }
for (const auto& it : ordenado)
{
    const auto& m = it.second;
    std::cout << m.registration_ID << "  [" << BT::toStr(m.type) << "]"
              << "  portas=" << m.ports.size() << "\n";
    for (const auto& p : m.ports)
    {
        std::cout << "    - " << p.first
                  << "  dir="  << BT::toStr(p.second.direction())
                  << "  tipo=" << (p.second.type() ? BT::demangle(p.second.type())
                                                   : std::string("(nenhum)"))
                  << "  default=" << (p.second.defaultValue().empty()
                                      ? std::string("(nenhum)") : p.second.defaultValue())
                  << "\n";
    }
}

// só o que NÃO é embutido
for (const auto& it : factory.manifests())
{
    if (factory.builtinNodes().count(it.first) == 0)
        std::cout << "meu: " << it.first << std::endl;
}
```

Este é exatamente o mecanismo da ferramenta `bt3_plugin_manifest`.

---

# 15. O *PARSER*: DA CARGA À ÁRVORE

## 15.1 A interface `Parser` e o `XMLParser`

```cpp
// include/behaviortree_cpp_v3/bt_parser.h  (íntegro)
/**
 * @brief The BehaviorTreeParser is a class used to read the model
 * of a BehaviorTree from file or text and instantiate the
 * corresponding tree using the BehaviorTreeFactory.
 */
class Parser
{
  public:
    Parser() = default;
    virtual ~Parser() = default;

    Parser(const Parser& other) = delete;
    Parser& operator=(const Parser& other) = delete;

    virtual void loadFromFile(const std::string& filename) = 0;
    virtual void loadFromText(const std::string& xml_text) = 0;
    virtual Tree instantiateTree(const Blackboard::Ptr &root_blackboard) = 0;
};
```

```cpp
// include/behaviortree_cpp_v3/xml_parsing.h
class XMLParser: public Parser
{
  public:
    XMLParser(const BehaviorTreeFactory& factory);
    ~XMLParser();

    XMLParser(const XMLParser& other) = delete;
    XMLParser& operator=(const XMLParser& other) = delete;

    void loadFromFile(const std::string& filename) override;
    void loadFromText(const std::string& xml_text) override;
    Tree instantiateTree(const Blackboard::Ptr &root_blackboard) override;

  private:
    struct Pimpl;
    Pimpl* _p;
};

void VerifyXML(const std::string& xml_text, const std::set<std::string> &registered_nodes);
std::string writeTreeNodesModelXML(const BehaviorTreeFactory& factory);
```

Há ainda duas funções livres de conveniência:

```cpp
Tree buildTreeFromText(const BehaviorTreeFactory& factory, const std::string& text,
                       const Blackboard::Ptr& blackboard);
Tree buildTreeFromFile(const BehaviorTreeFactory& factory, const std::string& filename,
                       const Blackboard::Ptr& blackboard);
```

A diferença para `factory.createTreeFromText()` é que estas **não** preenchem
`tree.manifests`, o que quebra os *loggers* que dependem dele (`FileLogger`,
`PublisherZMQ`). **Prefira sempre os métodos da fábrica.**

## 15.2 O `Pimpl` do parser

```cpp
// src/xml_parsing.cpp
struct XMLParser::Pimpl
{
    TreeNode::Ptr createNodeFromXML(const XMLElement* element,
                                    const Blackboard::Ptr& blackboard,
                                    const TreeNode::Ptr& node_parent);

    void recursivelyCreateTree(const std::string& tree_ID,
                               Tree& output_tree,
                               Blackboard::Ptr blackboard,
                               const TreeNode::Ptr& root_parent);

    void loadDocImpl(BT_TinyXML2::XMLDocument* doc);

    std::list<std::unique_ptr<BT_TinyXML2::XMLDocument> > opened_documents;
    std::unordered_map<std::string,const XMLElement*>  tree_roots;

    const BehaviorTreeFactory& factory;
    filesystem::path current_path;
    int suffix_count;

    explicit Pimpl(const BehaviorTreeFactory &fact):
        factory(fact),
        current_path( filesystem::path::getcwd() ),
        suffix_count(0)
    {}

    void clear()
    {
        suffix_count = 0;
        current_path = filesystem::path::getcwd();
        opened_documents.clear();
        tree_roots.clear();
    }
};
```

**Note que o parser guarda uma referência à fábrica** (`const BehaviorTreeFactory& factory`).
A fábrica precisa continuar viva durante toda a carga.

## 15.3 As seis etapas da carga

```
1. loadFromFile()          lê o arquivo com tinyxml2, fixa current_path
2. loadDocImpl()           resolve <include>, coleta as <BehaviorTree> em tree_roots
3. VerifyXML()             aridade, atributos obrigatórios, IDs conhecidos
4. instantiateTree()       escolhe main_tree_to_execute, empilha o blackboard raiz
5. recursivelyCreateTree() desce a árvore; cada subárvore ganha (ou não) blackboard próprio
6. createNodeFromXML()     valida portas contra o manifesto, monta NodeConfiguration,
                           chama a fábrica                       ↺ por filho
```

```cpp
// src/xml_parsing.cpp
void XMLParser::loadFromFile(const std::string& filename)
{
    _p->opened_documents.emplace_back(new BT_TinyXML2::XMLDocument());

    BT_TinyXML2::XMLDocument* doc = _p->opened_documents.back().get();
    doc->LoadFile(filename.c_str());

    filesystem::path file_path( filename );
    _p->current_path = file_path.parent_path().make_absolute();

    _p->loadDocImpl( doc );
}

void XMLParser::loadFromText(const std::string& xml_text)
{
    _p->opened_documents.emplace_back(new BT_TinyXML2::XMLDocument());

    BT_TinyXML2::XMLDocument* doc = _p->opened_documents.back().get();
    doc->Parse(xml_text.c_str(), xml_text.size());

    _p->loadDocImpl( doc );
}
```

```cpp
// src/xml_parsing.cpp
Tree XMLParser::instantiateTree(const Blackboard::Ptr& root_blackboard)
{
    Tree output_tree;

    XMLElement* xml_root = _p->opened_documents.front()->RootElement();

    std::string main_tree_ID;
    if (xml_root->Attribute("main_tree_to_execute"))
    {
        main_tree_ID = xml_root->Attribute("main_tree_to_execute");
    }
    else if( _p->tree_roots.size() == 1)
    {
        main_tree_ID = _p->tree_roots.begin()->first;
    }
    else{
        throw RuntimeError("[main_tree_to_execute] was not specified correctly");
    }
    //--------------------------------------
    if( !root_blackboard )
    {
        throw RuntimeError("XMLParser::instantiateTree needs a non-empty root_blackboard");
    }
    // first blackboard
    output_tree.blackboard_stack.push_back( root_blackboard );

    _p->recursivelyCreateTree(main_tree_ID, output_tree, root_blackboard, TreeNode::Ptr() );
    return output_tree;
}
```

## 15.4 `createNodeFromXML()` — os quatro passos

É onde a maior parte das verificações úteis acontece.

```cpp
// src/xml_parsing.cpp  (condensada)
TreeNode::Ptr XMLParser::Pimpl::createNodeFromXML(const XMLElement *element,
                                                  const Blackboard::Ptr &blackboard,
                                                  const TreeNode::Ptr &node_parent)
{
    const std::string element_name = element->Name();
    std::string ID;
    std::string instance_name;

    // Actions and Decorators have their own ID
    if (element_name == "Action" || element_name == "Decorator" ||
        element_name == "Condition" || element_name == "Control")
    { ID = element->Attribute("ID"); }
    else
    { ID = element_name; }

    const char* attr_alias = element->Attribute("name");
    if (attr_alias) { instance_name = attr_alias; }
    else            { instance_name = ID;         }

    PortsRemapping port_remap;

    if (element_name == "SubTree" || element_name == "SubTreePlus" )
    {
        instance_name = element->Attribute("ID");
    }
    else{
        // do this only if it NOT a Subtree
        for (const XMLAttribute* att = element->FirstAttribute(); att; att = att->Next())
        {
            const std::string attribute_name = att->Name();
            if (attribute_name != "ID" && attribute_name != "name")
            {
                port_remap[attribute_name] = att->Value();
            }
        }
    }
    NodeConfiguration config;
    config.blackboard = blackboard;

    TreeNode::Ptr child_node;

    if( factory.builders().count(ID) != 0)
    {
        const auto& manifest = factory.manifests().at(ID);

        // ① Check that name in remapping can be found in the manifest
        for(const auto& remap_it: port_remap)
        {
            if( manifest.ports.count( remap_it.first ) == 0 )
            {
                throw RuntimeError("Possible typo? In the XML, you tried to remap port \"",
                                   remap_it.first, "\" in node [", ID," / ", instance_name,
                                   "], but the manifest of this node does not contain a port "
                                   "with this name.");
            }
        }

        // ② Initialize the ports in the BB to set the type    (ver seção 8.7)

        // ③ use manifest to initialize NodeConfiguration
        for(const auto& remap_it: port_remap)
        {
            const auto& port_name = remap_it.first;
            auto port_it = manifest.ports.find( port_name );
            if( port_it != manifest.ports.end() )
            {
                auto direction = port_it->second.direction();
                if( direction != PortDirection::OUTPUT ) { config.input_ports.insert( remap_it ); }
                if( direction != PortDirection::INPUT )  { config.output_ports.insert( remap_it ); }
            }
        }

        // ④ use default value if available for empty ports. Only inputs   (ver seção 7.9)

        child_node = factory.instantiateTreeNode(instance_name, ID, config);
    }
    else if( tree_roots.count(ID) != 0) {
        child_node = std::make_unique<SubtreeNode>( instance_name );
    }
    else{
        throw RuntimeError( ID, " is not a registered node, nor a Subtree");
    }

    if (node_parent)
    {
        if (auto control_parent = dynamic_cast<ControlNode*>(node_parent.get()))
        {
            control_parent->addChild(child_node.get());
        }
        if (auto decorator_parent = dynamic_cast<DecoratorNode*>(node_parent.get()))
        {
            decorator_parent->setChild(child_node.get());
        }
    }
    return child_node;
}
```

**A mensagem "Possible typo?" é a mais amigável de toda a biblioteca**, e vale conhecê-la
de cor porque é o erro mais frequente ao escrever XML.

**Nota:** uma porta `INOUT` entra nos **dois** mapas (`input_ports` e `output_ports`).

## 15.5 `recursivelyCreateTree()`

```cpp
// src/xml_parsing.cpp
void BT::XMLParser::Pimpl::recursivelyCreateTree(const std::string& tree_ID,
                                                 Tree& output_tree,
                                                 Blackboard::Ptr blackboard,
                                                 const TreeNode::Ptr& root_parent)
{
    std::function<void(const TreeNode::Ptr&, const XMLElement*)> recursiveStep;

    recursiveStep = [&](const TreeNode::Ptr& parent, const XMLElement* element)
    {
        auto node = createNodeFromXML(element, blackboard, parent);
        output_tree.nodes.push_back(node);

        if( node->type() == NodeType::SUBTREE )
        {
            // ... (ver seção 16) ...
        }
        else
        {
            for (auto child_element = element->FirstChildElement(); child_element;
                 child_element = child_element->NextSiblingElement())
            {
                recursiveStep(node, child_element);
            }
        }
    };

    auto root_element = tree_roots[tree_ID]->FirstChildElement();

    // start recursion
    recursiveStep(root_parent, root_element);
}
```

**Consequência importante:** `output_tree.nodes` fica em **ordem de pré-ordem** (raiz
primeiro, depois cada subárvore em profundidade). `Tree::rootNode()` é
`nodes.front().get()` justamente por isso.

## 15.6 Coleta das árvores e nomes gerados

```cpp
// src/xml_parsing.cpp (loadDocImpl)
for (auto bt_node = xml_root->FirstChildElement("BehaviorTree");
     bt_node != nullptr;
     bt_node = bt_node->NextSiblingElement("BehaviorTree"))
{
    std::string tree_name;
    if (bt_node->Attribute("ID"))
    {
        tree_name = bt_node->Attribute("ID");
    }
    else{
        tree_name = "BehaviorTree_" + std::to_string( suffix_count++ );
    }
    tree_roots.insert( {tree_name, bt_node} );
}
```

**ARMADILHA — IDs de árvore duplicados são silenciosamente ignorados.** `tree_roots` é um
`unordered_map` povoado com `insert()`, que **não** substitui uma chave existente. Duas
`<BehaviorTree ID="X">` — por exemplo, uma no arquivo principal e outra num arquivo
incluído — não geram aviso: vale a que foi lida **primeiro**, e a outra desaparece. Como os
`<include>` são processados antes, quem vence é a do **arquivo incluído**.

**ARMADILHA — uma `<BehaviorTree>` sem `ID` recebe um nome gerado.** Se o atributo faltar,
o *parser* inventa `"BehaviorTree_" + suffix_count++`. Isso é útil no caso de arquivo com
uma árvore só — mas com duas árvores anônimas o `main_tree_to_execute` teria de citar
`BehaviorTree_0`, um nome que depende da ordem de leitura dos arquivos. **Dê `ID` a
todas.**

---

# 16. SUBÁRVORES

Uma **subárvore** é uma `<BehaviorTree>` usada como nó dentro de outra. Estruturalmente é
um decorador; o que a distingue é o tratamento do *blackboard*. A v3.5.6 oferece dois nós
com políticas diferentes.

## 16.1 `<SubTree>`: isolamento com remapeamento

Por padrão, a subárvore recebe um *blackboard* **novo**, filho do atual, e **cada atributo
do XML vira um remapeamento** de uma chave interna para uma chave externa.

```xml
<!-- examples/t06_subtree_port_remapping.cpp -->
<BehaviorTree ID="MainTree">
    <Sequence name="main_sequence">
        <SetBlackboard output_key="move_goal" value="1;2;3" />
        <SubTree ID="MoveRobot" target="move_goal" output="move_result" />
        <SaySomething message="{move_result}"/>
    </Sequence>
</BehaviorTree>

<BehaviorTree ID="MoveRobot">
    <Fallback name="move_robot_main">
        <SequenceStar>
            <MoveBase       goal="{target}"/>
            <SetBlackboard output_key="output" value="mission accomplished" />
        </SequenceStar>
        <ForceFailure>
            <SetBlackboard output_key="output" value="mission failed" />
        </ForceFailure>
    </Fallback>
</BehaviorTree>
```

Lê-se: **dentro de `MoveRobot`, a chave `target` é a chave `move_goal` do pai, e `output` é
`move_result`.**

**REGRA — num `<SubTree>` clássico, os valores NÃO levam chaves.** Todo atributo é nome de
chave externa, nunca literal. Escrever `target="{move_goal}"` cria um remapeamento para a
chave literalmente chamada `"{move_goal}"`, que não existe.

```cpp
// src/xml_parsing.cpp (dentro de recursivelyCreateTree)
if( dynamic_cast<const SubtreeNode*>(node.get()) )
{
    bool is_isolated = true;

    for (const XMLAttribute* attr = element->FirstAttribute(); attr != nullptr;
         attr = attr->Next())
    {
        if( strcmp(attr->Name(), "__shared_blackboard") == 0  &&
            convertFromString<bool>(attr->Value()) == true )
        {
            is_isolated = false;
        }
    }

    if( !is_isolated )
    {
        recursivelyCreateTree( node->name(), output_tree, blackboard, node );
    }
    else{
        // Creating an isolated
        auto new_bb = Blackboard::create(blackboard);

        for (const XMLAttribute* attr = element->FirstAttribute(); attr != nullptr;
             attr = attr->Next())
        {
            if( strcmp(attr->Name(), "ID") == 0 ) { continue; }
            new_bb->addSubtreeRemapping( attr->Name(), attr->Value() );
        }
        output_tree.blackboard_stack.emplace_back(new_bb);
        recursivelyCreateTree( node->name(), output_tree, new_bb, node );
    }
}
```

O atributo especial `__shared_blackboard="1"` desliga o isolamento: a subárvore passa a
usar o **mesmo** *blackboard* do pai, e nenhum remapeamento é necessário — nem possível.

```xml
<SubTree ID="Passo2" __shared_blackboard="1"/>
```

**Note que, com `__shared_blackboard`, nenhum novo *blackboard* é empilhado**, e
`tree.blackboard_stack` não cresce.

## 16.2 `<SubTreePlus>`: três formas de ligar

`SubtreePlusNode` generaliza o remapeamento com uma sintaxe que distingue chave de literal,
e acrescenta o remapeamento automático.

```xml
<!-- do cabeçalho decorators/subtree_node.h -->
<root main_tree_to_execute = "MainTree" >

    <BehaviorTree ID="MainTree">
        <Sequence>

        <SetBlackboard value="Hello" output_key="myParam" />
        <SubTreePlus ID="Talk" param="{myParam}" />

        <SubTreePlus ID="Talk" param="World" />

        <SetBlackboard value="Auto remapped" output_key="param" />
        <SubTreePlus ID="Talk" __autoremap="1"  />

        </Sequence>
    </BehaviorTree>

    <BehaviorTree ID="Talk">
        <SaySomething message="{param}" />
    </BehaviorTree>
</root>
```

As três abordagens, segundo o próprio comentário do cabeçalho:

1. **`param="{myParam}"`** — `Subtree: "{param}" -> Parent: "{myParam}" -> Value: "Hello"`.
   Remapeamento clássico de porta para porta, com a sintaxe `{}` explicitando que se está
   remapeando **outra porta**.
2. **`param="World"`** — `Subtree: "{param}" -> Value: "World"`. Sintaxe **sem** `{}`: o
   valor literal é **escrito** na entrada `param` do *blackboard* novo. Não há ligação com
   o pai.
3. **`__autoremap="1"`** — `Subtree: "{param}" -> Parent: "{parent}"`. Toda chave da
   subárvore que não tenha sido citada explicitamente é remapeada para a chave **de mesmo
   nome** no pai. *"Usefull to avoid some boilerplate."*

```cpp
// src/xml_parsing.cpp (dentro de recursivelyCreateTree)
else if( dynamic_cast<const SubtreePlusNode*>(node.get()) )
{
    auto new_bb = Blackboard::create(blackboard);
    output_tree.blackboard_stack.emplace_back(new_bb);
    std::set<StringView> mapped_keys;

    bool do_autoremap = false;

    for (const XMLAttribute* attr = element->FirstAttribute(); attr != nullptr;
         attr = attr->Next())
    {
        if( strcmp(attr->Name(), "ID") == 0 ) { continue; }
        if( strcmp(attr->Name(), "__autoremap") == 0 )
        {
            if( convertFromString<bool>(attr->Value()) ) { do_autoremap = true; }
            continue;
        }

        StringView str =  attr->Value();
        if( TreeNode::isBlackboardPointer(str))
        {
            StringView port_name = TreeNode::stripBlackboardPointer(str);
            new_bb->addSubtreeRemapping( attr->Name(), port_name);
            mapped_keys.insert(attr->Name());
        }
        else{
            new_bb->set(attr->Name(), static_cast<std::string>(str) );
            mapped_keys.insert(attr->Name());
        }
    }
    recursivelyCreateTree( node->name(), output_tree, new_bb, node );

    if( do_autoremap )
    {
        auto keys = new_bb->getKeys();
        for( StringView key: keys)
        {
            if( mapped_keys.count(key) == 0)
            {
                new_bb->addSubtreeRemapping( key, key );
            }
        }
    }
}
```

**ARMADILHA — o `__autoremap` só enxerga chaves que já existem.** O bloco que aplica o
`__autoremap` roda **depois** de `recursivelyCreateTree()`, e percorre `new_bb->getKeys()`
— as chaves que a **construção** da subárvore já criou (via `setPortInfo`, para toda porta
remapeada com `{}`). Uma chave que só passe a existir no primeiro *tick* — porque nenhum nó
a declarou como porta tipada na carga — **não é remapeada**. O "automático" é "automático
para o que o *parser* viu", e a diferença aparece como uma entrada que não atravessa a
fronteira sem que nada tenha sido escrito errado.

**ARMADILHA — o atributo `name` de um `<SubTree>` é ignorado.** Para `SubTree` e
`SubTreePlus`, o *parser* faz `instance_name = element->Attribute("ID")` — **sobrescrevendo
qualquer `name` fornecido**. A razão é operacional: a recursão localiza a árvore por
`node->name()`. O efeito colateral é que, num traço de execução ou no Groot, **todas as
instâncias de uma mesma subárvore aparecem com o mesmo nome**, e não há como
distingui-las.

**ARMADILHA — um `<SubTree>` sem `ID` é falha de segmentação.** `element->Attribute("ID")`
devolve `nullptr` se o atributo faltar, e `instance_name = nullptr` numa `std::string` é
comportamento indefinido. Na prática, a validação `VerifyXML` exige `ID` em `<SubTree>` e
protege esse caso — mas não em `<SubTreePlus>`, que não está na lista da validação.

## 16.3 A pilha de *blackboards*

```
Tree::blackboard_stack
  [0] blackboard raiz              <- passado a createTreeFromFile(), ou criado
  [1] blackboard da 1ª subárvore isolada
  [2] blackboard da 2ª subárvore isolada
  ...
```

`tree.rootBlackboard()` devolve `blackboard_stack.front()`.

```
blackboard raiz
  move_goal   ↦ Pose2D
  move_result ↦ string
        ▲
        │  internal_to_external_
        │
blackboard de MoveRobot
  target → pai[move_goal]
  output → pai[move_result]

  (chave "tmp" criada dentro da subárvore: NÃO atravessa)
```

## 16.4 Quando usar cada forma

| Situação | Use |
|---|---|
| subárvore reutilizável, poucas chaves atravessam | `<SubTree>` com remapeamento |
| protótipo rápido, tudo compartilhado | `<SubTree __shared_blackboard="1"/>` |
| precisa passar um **literal** para dentro | `<SubTreePlus param="valor"/>` |
| muitas chaves com o mesmo nome dos dois lados | `<SubTreePlus __autoremap="1"/>` |

**REGRA — prefira `<SubTree>` com remapeamento explícito.** O isolamento é a proteção que
torna a subárvore reutilizável. `__shared_blackboard` desfaz exatamente essa proteção e
transforma toda chave num nome global.

**Nota sobre a v4:** na versão 4, o `SubTreePlus` foi promovido a `SubTree` e o
comportamento antigo foi removido. Se você encontrar documentação que descreve
`<SubTree>` aceitando literais e `_autoremap`, é v4.

---

# 17. *PLUGINS*

Um *plugin* é uma biblioteca compartilhada que exporta uma função de nome fixo e registra
os seus nós na fábrica em tempo de execução.

## 17.1 A macro e o símbolo

```cpp
// include/behaviortree_cpp_v3/bt_factory.h
constexpr const char* PLUGIN_SYMBOL = "BT_RegisterNodesFromPlugin";

#ifndef BT_PLUGIN_EXPORT

/* Use this macro to automatically register one or more custom Nodes
into a factory. For instance:

BT_REGISTER_NODES(factory)
{
    factory.registerNodeType<MoveBaseAction>("MoveBase");
}

IMPORTANT: this must funtion MUST be declared in a cpp file, NOT a header file.
See examples for more information about configuring CMake correctly
*/
#define BT_REGISTER_NODES(factory)                                                    \
        static void BT_RegisterNodesFromPlugin(BT::BehaviorTreeFactory& factory)

#else

#if defined(__linux__) || defined __APPLE__

#define BT_REGISTER_NODES(factory)                                                    \
    extern "C" void __attribute__((visibility("default")))                            \
        BT_RegisterNodesFromPlugin(BT::BehaviorTreeFactory& factory)

#elif _WIN32

#define BT_REGISTER_NODES(factory)                                                    \
    extern "C" void __declspec(dllexport) BT_RegisterNodesFromPlugin(BT::BehaviorTreeFactory& factory)
#endif

#endif
```

## 17.2 Do lado do código

```cpp
// sample_nodes/crossdoor_nodes.cpp
#include "crossdoor_nodes.h"

// This function must be implemented in the .cpp file to create
// a plugin that can be loaded at run-time
BT_REGISTER_NODES(factory)
{
    CrossDoor::RegisterNodes(factory);
}
```

```cpp
// sample_nodes/movebase_node.cpp
BT_REGISTER_NODES(factory)
{
    factory.registerNodeType<MoveBaseAction>("MoveBase");
}
```

```cpp
// sample_nodes/dummy_nodes.h — a função que o plugin chama
inline void RegisterNodes(BT::BehaviorTreeFactory& factory)
{
    static GripperInterface grip_singleton;

    factory.registerSimpleCondition("CheckBattery", std::bind(CheckBattery));
    factory.registerSimpleAction("OpenGripper",  std::bind(&GripperInterface::open,  &grip_singleton));
    factory.registerSimpleAction("CloseGripper", std::bind(&GripperInterface::close, &grip_singleton));
    factory.registerNodeType<ApproachObject>("ApproachObject");
    factory.registerNodeType<SaySomething>("SaySomething");
}
```

## 17.3 Do lado do *build*

```cmake
# sample_nodes/CMakeLists.txt
# to create a plugin, compile them in this way instead
add_library(crossdoor_nodes_dyn SHARED crossdoor_nodes.cpp )
target_link_libraries(crossdoor_nodes_dyn PRIVATE ${BEHAVIOR_TREE_LIBRARY})
target_compile_definitions(crossdoor_nodes_dyn PRIVATE  BT_PLUGIN_EXPORT )
set_target_properties(crossdoor_nodes_dyn PROPERTIES LIBRARY_OUTPUT_DIRECTORY
    ${BEHAVIOR_TREE_BIN_DESTINATION} )
```

Note que os mesmos `.cpp` também são compilados numa biblioteca **estática**
(`bt_sample_nodes`) **sem** `BT_PLUGIN_EXPORT`, para uso por ligação direta.

## 17.4 Do lado da aplicação

```cpp
// examples/t01_build_your_first_tree.cpp
#ifdef MANUAL_STATIC_LINKING
    using namespace DummyNodes;
    factory.registerNodeType<ApproachObject>("ApproachObject");
    factory.registerSimpleCondition("CheckBattery", std::bind(CheckBattery));
    GripperInterface gripper;
    factory.registerSimpleAction("OpenGripper",  std::bind(&GripperInterface::open,  &gripper));
    factory.registerSimpleAction("CloseGripper", std::bind(&GripperInterface::close, &gripper));
#else
    // Load dynamically a plugin and register the TreeNodes it contains
    // it automated the registering step.
    factory.registerFromPlugin("./libdummy_nodes_dyn.so");
#endif
```

```cpp
// src/bt_factory.cpp
void BehaviorTreeFactory::registerFromPlugin(const std::string& file_path)
{
    BT::SharedLibrary loader;
    loader.load(file_path);
    typedef void (*Func)(BehaviorTreeFactory&);

    if (loader.hasSymbol(PLUGIN_SYMBOL))
    {
        Func func = (Func)loader.getSymbol(PLUGIN_SYMBOL);
        func(*this);
    }
    else
    {
        std::cout << "ERROR loading library [" << file_path << "]: can't find symbol ["
                  << PLUGIN_SYMBOL << "]" << std::endl;
    }
}
```

**ARMADILHA — esquecer `BT_PLUGIN_EXPORT` produz um erro silencioso.** Sem a definição, a
macro expande para uma função `static` — invisível ao `dlsym`. `registerFromPlugin()`
então **não lança**: imprime `"ERROR loading library [...]: can't find symbol
[BT_RegisterNodesFromPlugin]"` em **`std::cout`** e retorna normalmente. A carga do XML
falha depois, com `"Node not recognized"`, e a mensagem que explica o porquê já rolou para
fora da tela.

**ARMADILHA — o `SharedLibrary` local morre no fim da função.** `registerFromPlugin` cria
um `BT::SharedLibrary loader;` na pilha. O destrutor de `SharedLibrary` é
`virtual ~SharedLibrary() = default;` — com o comentário *"Destroys the SharedLibrary. The
actual library remains loaded."* — ou seja, **não** chama `dlclose()`. Isso é o
comportamento **correto** aqui (fechar a biblioteca invalidaria as lambdas registradas),
mas significa que a biblioteca nunca é descarregada e não há como desregistrar um
*plugin*.

**REGRA — o *plugin* e a aplicação precisam da mesma biblioteca.** O *plugin* é carregado
com `RTLD_NOW | RTLD_GLOBAL` e recebe uma referência à `BehaviorTreeFactory` da aplicação.
Se os dois lados tiverem sido compilados contra versões diferentes da biblioteca, ou com
*flags* de ABI incompatíveis, o resultado é corrupção silenciosa. Some-se a isso a
comparação de `type_info` **por ponteiro** em `Blackboard::setPortInfo()`, que pode
produzir falsos erros de tipo entre os dois lados.

**REGRA — prefira a biblioteca compartilhada.** Com a BehaviorTree.CPP ligada
**estaticamente** duas vezes (uma na aplicação, outra no *plugin*), cada lado tem os seus
próprios `type_info` e os seus próprios contadores estáticos — inclusive o contador de
`UID` e os `ref_count` dos *loggers*. Os sintomas são obscuros.

## 17.5 *Plugins* ROS

```cpp
// src/bt_factory.cpp
#ifdef USING_ROS
void BehaviorTreeFactory::registerFromROSPlugins()
{
    std::vector<std::string> plugins;
    ros::package::getPlugins("behaviortree_cpp_v3", "bt_lib_plugin", plugins, true);
    std::vector<std::string> catkin_lib_paths = getCatkinLibraryPaths();

    for (const auto& plugin : plugins)
    {
        auto filename = filesystem::path(plugin + BT::SharedLibrary::suffix());
        for (const auto& lib_path : catkin_lib_paths)
        {
            const auto full_path = filesystem::path(lib_path) / filename;
            if (full_path.exists())
            {
                std::cout << "Registering ROS plugins from " << full_path.str() << std::endl;
                registerFromPlugin(full_path.str());
                break;
            }
        }
    }
}
#else
void BehaviorTreeFactory::registerFromROSPlugins()
{
    throw RuntimeError("Using attribute [ros_pkg] in <include>, but this library was compiled "
                       "without ROS support. Recompile the BehaviorTree.CPP using catkin");
}
#endif
```

Só existe se a biblioteca tiver sido compilada com **catkin** (ROS 1). Ver seção 21.8.

---

# 18. O OBJETO `Tree`

```cpp
// include/behaviortree_cpp_v3/bt_factory.h
/**
 * @brief Struct used to store a tree.
 * If this object goes out of scope, the tree is destroyed.
 *
 * To tick the tree, simply call:
 *
 *    NodeStatus status = my_tree.tickRoot();
 */
class Tree
{
public:
    std::vector<TreeNode::Ptr> nodes;
    std::vector<Blackboard::Ptr> blackboard_stack;
    std::unordered_map<std::string, TreeNodeManifest> manifests;

    Tree(){}

    // non-copyable. Only movable
    Tree(const Tree& ) = delete;
    Tree& operator=(const Tree&) = delete;

    Tree(Tree&& other) { (*this) = std::move(other); }

    Tree& operator=(Tree&& other)
    {
        nodes = std::move(other.nodes);
        blackboard_stack = std::move(other.blackboard_stack);
        manifests = std::move(other.manifests);
        return *this;
    }

    void haltTree()
    {
        if(!rootNode()) { return; }
        // the halt should propagate to all the node if the nodes
        // have been implemented correctly
        rootNode()->halt();
        rootNode()->setStatus(NodeStatus::IDLE);

        //but, just in case.... this should be no-op
        auto visitor = [](BT::TreeNode * node) {
            node->halt();
            node->setStatus(BT::NodeStatus::IDLE);
        };
        BT::applyRecursiveVisitor(rootNode(), visitor);
    }

    TreeNode* rootNode() const
    {
      return nodes.empty() ? nullptr : nodes.front().get();
    }

    NodeStatus tickRoot()
    {
      if(!rootNode()) { throw RuntimeError("Empty Tree"); }
      NodeStatus ret = rootNode()->executeTick();
      if( ret == NodeStatus::SUCCESS || ret == NodeStatus::FAILURE){
        rootNode()->setStatus(BT::NodeStatus::IDLE);
      }
      return ret;
    }

    ~Tree();
    Blackboard::Ptr rootBlackboard();
};
```

```cpp
// src/bt_factory.cpp
Tree::~Tree()
{
    haltTree();
}

Blackboard::Ptr Tree::rootBlackboard()
{
    if( blackboard_stack.size() > 0) { return blackboard_stack.front(); }
    return {};
}
```

**REGRA — o `Tree` é o dono, e não é copiável.** `Tree` guarda os `shared_ptr` de todos os
nós, a pilha de *blackboards* e os manifestos. Ele é *move-only*: copiar está
explicitamente removido (`= delete`). Quando o objeto sai de escopo, o destrutor chama
`haltTree()` e destrói todos os nós — **guardar um `TreeNode*` que sobreviva ao `Tree` é um
*dangling pointer***.

**A reposição a `IDLE` em `tickRoot()`** é o que fecha o ciclo de vida no topo da árvore:
sem ela, a raiz nunca voltaria a executar depois de concluir uma vez.

**ARMADILHA — `haltTree()` passa duas vezes por cada nó.** Chama `rootNode()->halt()`, que
já propaga para toda a subárvore, e **depois** percorre a árvore inteira com
`applyRecursiveVisitor` chamando `halt()` de novo em cada nó — o comentário do fonte
admite: *"but, just in case.... this should be no-op"*. Para um `AsyncActionNode` cujo
`halt()` espera a *thread*, a segunda passagem é inofensiva; para um `halt()` de usuário
com efeito colateral, não é. **Escreva `halt()` idempotente.**

**ARMADILHA — `manifests` só é preenchido pelos métodos da fábrica.**

```cpp
// src/bt_factory.cpp
Tree BehaviorTreeFactory::createTreeFromText(const std::string &text, Blackboard::Ptr blackboard)
{
    XMLParser parser(*this);
    parser.loadFromText(text);
    auto tree = parser.instantiateTree(blackboard);
    tree.manifests = this->manifests();     // <-- esta linha
    return tree;
}
```

As funções livres `buildTreeFromText()`/`buildTreeFromFile()` **não** fazem isso, e um
`FileLogger` construído sobre uma árvore assim grava um cabeçalho sem modelos de nó.

## 18.1 Usos comuns de `Tree`

```cpp
auto tree = factory.createTreeFromFile("arvore.xml");

// 1. imprimir a estrutura
BT::printTreeRecursively(tree.rootNode());

// 2. iterar todos os nós (o padrão do t08)
for (auto& node : tree.nodes) {
    if (auto p = dynamic_cast<MinhaAcao*>(node.get())) { p->init(...); }
}

// 3. acessar o blackboard raiz
tree.rootBlackboard()->set("alvo", 42);

// 4. acessar o blackboard de uma subárvore (índice na ordem de criação)
tree.blackboard_stack[1]->debugMessage();

// 5. tickar
while (tree.tickRoot() == BT::NodeStatus::RUNNING) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

// 6. interromper de fora (de outra thread — cuidado com a concorrência)
tree.haltTree();
```

---

# 19. *LOGGERS* E OBSERVABILIDADE

Uma árvore que não se pode observar é indepurável: o comportamento emerge da interação
entre dezenas de nós, e nenhum *breakpoint* mostra isso. A biblioteca resolve o problema
com **um mecanismo só** — assinar as transições de estado — e quatro consumidores
diferentes para elas.

## 19.1 `StatusChangeLogger`: a base comum

```cpp
// include/behaviortree_cpp_v3/loggers/abstract_logger.h  (íntegro, sem guards)
enum class TimestampType
{
    ABSOLUTE,
    RELATIVE
};

typedef std::array<uint8_t, 12> SerializedTransition;

class StatusChangeLogger
{
  public:
    StatusChangeLogger(TreeNode *root_node);
    virtual ~StatusChangeLogger() = default;

    virtual void callback(BT::Duration timestamp, const TreeNode& node,
                          NodeStatus prev_status, NodeStatus status) = 0;

    virtual void flush() = 0;

    void setEnabled(bool enabled)          { enabled_ = enabled; }
    void seTimestampType(TimestampType type) { type_ = type; }
    bool enabled() const                   { return enabled_; }

    // false by default.
    bool showsTransitionToIdle() const     { return show_transition_to_idle_; }
    void enableTransitionToIdle(bool enable) { show_transition_to_idle_ = enable; }

  private:
    bool enabled_;
    bool show_transition_to_idle_;
    std::vector<TreeNode::StatusChangeSubscriber> subscribers_;
    TimestampType type_;
    BT::TimePoint first_timestamp_;
};

inline StatusChangeLogger::StatusChangeLogger(TreeNode* root_node)
  : enabled_(true), show_transition_to_idle_(true), type_(TimestampType::ABSOLUTE)
{
    first_timestamp_ = std::chrono::high_resolution_clock::now();

    auto subscribeCallback = [this](TimePoint timestamp, const TreeNode& node,
                                    NodeStatus prev, NodeStatus status) {
        if (enabled_ && (status != NodeStatus::IDLE || show_transition_to_idle_))
        {
            if (type_ == TimestampType::ABSOLUTE)
            {
                this->callback(timestamp.time_since_epoch(), node, prev, status);
            }
            else
            {
                this->callback(timestamp - first_timestamp_, node, prev, status);
            }
        }
    };

    auto visitor = [this, subscribeCallback](TreeNode* node) {
        subscribers_.push_back(node->subscribeToStatusChange(std::move(subscribeCallback)));
    };

    applyRecursiveVisitor(root_node, visitor);
}
```

O construtor faz o trabalho inteiro: percorre a árvore e assina a mudança de estado de
**cada nó**. A subclasse implementa apenas `callback()` e `flush()`.

Três controles ficam disponíveis para todas:

| Método | Efeito |
|---|---|
| `setEnabled(bool)` | liga/desliga a gravação **sem** desfazer as assinaturas |
| `seTimestampType(TimestampType)` | carimbo absoluto (desde a época) ou relativo (desde a construção) |
| `enableTransitionToIdle(bool)` | filtra as transições **para** `IDLE` |

**ARMADILHA — o comentário e o construtor discordam sobre o padrão.** O cabeçalho anota
`// false by default` sobre `showsTransitionToIdle()`, e o construtor inicializa
`show_transition_to_idle_(true)`. **O padrão real é MOSTRAR as transições para `IDLE`** —
que num traço de `StdCoutLogger` facilmente dobram o volume de saída. Se o seu traço
parece ruidoso demais, é isto: chame `enableTransitionToIdle(false)`.

**ARMADILHA — o nome do método tem um `t` a menos.** É `seTimestampType()`, não
`setTimestampType()`. Procurar pelo nome correto no cabeçalho não encontra nada, e o erro
de compilação (*"no member named setTimestampType"*) não sugere a grafia real.

**REGRA — construa os *loggers* DEPOIS da árvore e ANTES do primeiro *tick*.** As
assinaturas são feitas uma única vez, no construtor, sobre os nós que existiam naquele
momento. E o `Signal` não é seguro sob concorrência: assinar enquanto a árvore está sendo
*ticada* é uma condição de corrida. O *logger* também precisa **continuar vivo** — é ele
quem detém os `shared_ptr` das assinaturas.

```cpp
auto tree = factory.createTreeFromFile("arvore.xml");

BT::StdCoutLogger logger_cout(tree);          // <- variáveis locais do main()
BT::FileLogger    logger_file(tree, "trace.fbl");

while (tree.tickRoot() == BT::NodeStatus::RUNNING) { /* ... */ }
// os loggers morrem aqui, junto com as assinaturas
```

## 19.2 `StdCoutLogger` — o traço no terminal

```cpp
// include/behaviortree_cpp_v3/loggers/bt_cout_logger.h
/**
 * @brief AddStdCoutLoggerToTree. Give  the root node of a tree,
 * a simple callback is subscribed to any status change of each node.
 *
 * @param root_node
 * @return Important: the returned shared_ptr must not go out of scope,
 *         otherwise the logger is removed.
 */
class StdCoutLogger : public StatusChangeLogger
{
    static std::atomic<bool> ref_count;

  public:
    StdCoutLogger(const BT::Tree& tree);
    ~StdCoutLogger() override;

    virtual void callback(Duration timestamp, const TreeNode& node,
                          NodeStatus prev_status, NodeStatus status) override;
    virtual void flush() override;
};
```

```cpp
// src/loggers/bt_cout_logger.cpp  (íntegro)
std::atomic<bool> StdCoutLogger::ref_count(false);

StdCoutLogger::StdCoutLogger(const BT::Tree& tree) : StatusChangeLogger(tree.rootNode())
{
    bool expected = false;
    if (!ref_count.compare_exchange_strong(expected, true))
    {
        throw LogicError("Only one instance of StdCoutLogger shall be created");
    }
}
StdCoutLogger::~StdCoutLogger()
{
    ref_count.store(false);
}

void StdCoutLogger::callback(Duration timestamp, const TreeNode& node, NodeStatus prev_status,
                             NodeStatus status)
{
    using namespace std::chrono;

    constexpr const char* whitespaces = "                         ";
    constexpr const size_t ws_count = 25;

    double since_epoch = duration<double>(timestamp).count();
    printf("[%.3f]: %s%s %s -> %s",
           since_epoch, node.name().c_str(),
           &whitespaces[std::min(ws_count, node.name().size())],
           toStr(prev_status, true).c_str(),
           toStr(status, true).c_str() );
    std::cout << std::endl;
}

void StdCoutLogger::flush()
{
    std::cout << std::flush;
	ref_count = false;
}
```

Saída típica (cores removidas):

```
[..8.721]: missao                    IDLE    -> RUNNING
[..8.722]: Aproximar                 IDLE    -> RUNNING
[..9.028]: Aproximar                 RUNNING -> SUCCESS
[..9.028]: estrategias               IDLE    -> RUNNING
[..9.028]: EstaAberta                IDLE    -> FAILURE
```

O alinhamento é feito indexando uma *string* de 25 espaços — um truque compacto que
degrada com elegância: nomes com 25 caracteres ou mais simplesmente não recebem espaço
nenhum antes da seta.

**ARMADILHA — o `flush()` desarma o guarda de instância única.** A classe protege-se contra
duas instâncias com um `std::atomic<bool>` estático. Mas `flush()` termina com
`ref_count = false;` — e `flush()` é **público e virtual**. Depois de chamá-lo, nada impede
criar um segundo `StdCoutLogger`, e os dois passam a imprimir cada transição em duplicata.
A linha em questão é **a única de todo o arquivo indentada com tabulação**, o que sugere um
acréscimo apressado.

**ARMADILHA — os carimbos são absolutos por padrão** (segundos desde a época), o que
produz números de dez dígitos. Para um traço legível:

```cpp
BT::StdCoutLogger logger(tree);
logger.seTimestampType(BT::TimestampType::RELATIVE);
logger.enableTransitionToIdle(false);
```

## 19.3 `FileLogger` — o traço binário

```cpp
// include/behaviortree_cpp_v3/loggers/bt_file_logger.h  (íntegro)
class FileLogger : public StatusChangeLogger
{
  public:
    FileLogger(const Tree &tree, const char* filename, uint16_t buffer_size = 10);
    virtual ~FileLogger() override;

    virtual void callback(Duration timestamp, const TreeNode& node,
                          NodeStatus prev_status, NodeStatus status) override;
    virtual void flush() override;

  private:
    std::ofstream file_os_;
    std::chrono::high_resolution_clock::time_point start_time;
    std::vector<SerializedTransition> buffer_;
    size_t buffer_max_size_;
};
```

```cpp
// src/loggers/bt_file_logger.cpp  (íntegro)
FileLogger::FileLogger(const BT::Tree& tree, const char* filename, uint16_t buffer_size)
  : StatusChangeLogger(tree.rootNode()), buffer_max_size_(buffer_size)
{
    if (buffer_max_size_ != 0)
    {
        buffer_.reserve(buffer_max_size_);
    }

    enableTransitionToIdle(true);

    flatbuffers::FlatBufferBuilder builder(1024);
    CreateFlatbuffersBehaviorTree(builder, tree);

    //-------------------------------------

    file_os_.open(filename, std::ofstream::binary | std::ofstream::out);

    // serialize the length of the buffer in the first 4 bytes
    char size_buff[4];
    flatbuffers::WriteScalar(size_buff, static_cast<int32_t>(builder.GetSize()));

    file_os_.write(size_buff, 4);
    file_os_.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
}

FileLogger::~FileLogger()
{
    this->flush();
    file_os_.close();
}

void FileLogger::callback(Duration timestamp, const TreeNode& node, NodeStatus prev_status,
                          NodeStatus status)
{
    SerializedTransition buffer = SerializeTransition(node.UID(), timestamp,
                                                      prev_status, status);
    if (buffer_max_size_ == 0)
    {
        file_os_.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }
    else
    {
        buffer_.push_back(buffer);
        if (buffer_.size() >= buffer_max_size_)
        {
            this->flush();
        }
    }
}

void FileLogger::flush()
{
    for (const auto& array : buffer_)
    {
        file_os_.write(reinterpret_cast<const char*>(array.data()), array.size());
    }
    file_os_.flush();
    buffer_.clear();
}
```

### Formato do arquivo

```
[0..3]        int32  tamanho do cabeçalho FlatBuffers (little-endian)
[4..4+N-1]    bytes  cabeçalho FlatBuffers: topologia + manifestos
[4+N..]       12 bytes por transição, repetidos até o fim do arquivo
```

O cabeçalho é o que torna o arquivo **autocontido**: ele carrega o `UID`, o nome de
instância, o ID de registro, os filhos e as portas de cada nó, mais os manifestos de todos
os nós registrados. Um leitor não precisa do XML nem do binário original para reconstruir
a árvore — é o que faz a ferramenta `bt3_log_cat`.

**REGRA — o *buffer* do `FileLogger` custa dados num travamento.** O construtor aceita
`buffer_size` (padrão **10**): as transições são acumuladas em memória e só vão para o
disco a cada dez. Se o processo morrer, as **últimas até nove transições** — justamente as
que interessam — não estão no arquivo. Passe `0` para escrita imediata quando estiver
caçando um travamento:

```cpp
BT::FileLogger logger(tree, "trace.fbl", 0);    // sem buffer
```

**Nota:** o construtor força `enableTransitionToIdle(true)`, sobrescrevendo qualquer coisa
— o formato binário grava tudo, e o filtro é do leitor.

## 19.4 A serialização das transições

```cpp
// include/behaviortree_cpp_v3/flatbuffers/bt_flatbuffer_helper.h
/** Serialize manually the informations about state transition
 * No flatbuffer serialization here
 */
inline SerializedTransition SerializeTransition(uint16_t UID,
                                                Duration timestamp,
                                                NodeStatus prev_status,
                                                NodeStatus status)
{
    using namespace std::chrono;
    SerializedTransition buffer;
    int64_t usec = duration_cast<microseconds>(timestamp).count();
    int64_t t_sec = usec / 1000000;
    int64_t t_usec = usec % 1000000;

    flatbuffers::WriteScalar(&buffer[0], t_sec);
    flatbuffers::WriteScalar(&buffer[4], t_usec);
    flatbuffers::WriteScalar(&buffer[8], UID);

    flatbuffers::WriteScalar(&buffer[10], static_cast<int8_t>(convertToFlatbuffers(prev_status)));
    flatbuffers::WriteScalar(&buffer[11], static_cast<int8_t>(convertToFlatbuffers(status)));

    return buffer;
}
```

**ARMADILHA — o leiaute de 12 bytes só funciona por acidente.** O formato pretendido é:
4 bytes de segundos, 4 de microssegundos, 2 de `UID`, 1 de estado anterior, 1 de estado
novo. Mas `t_sec` e `t_usec` são **`int64_t`**, e `WriteScalar` escreve **oito** bytes cada:

- a escrita em `&buffer[0]` ocupa os bytes 0–7;
- a em `&buffer[4]` ocupa 4–11, **sobrescrevendo metade da primeira**;
- as três escritas seguintes sobrescrevem 8–11.

O resultado sai correto **porque** a máquina é *little-endian* e os valores cabem em 32
bits: em cada posição sobrevivem justamente os quatro bytes menos significativos. Num
processador *big-endian*, ou passados 68 anos de *uptime* (estouro de `int32` em segundos),
o arquivo sai corrompido — **sem nenhum aviso**. Não há `static_assert`, e o comentário do
fonte (*"Serialize manually"*) é a única pista de que o leiaute é manual.

## 19.5 A serialização da árvore

```cpp
// include/behaviortree_cpp_v3/flatbuffers/bt_flatbuffer_helper.h  (condensada)
inline void CreateFlatbuffersBehaviorTree(flatbuffers::FlatBufferBuilder& builder,
                                          const BT::Tree& tree)
{
    std::vector<flatbuffers::Offset<Serialization::TreeNode>> fb_nodes;

    applyRecursiveVisitor(tree.rootNode(), [&](BT::TreeNode* node)
    {
        std::vector<uint16_t> children_uid;
        if (auto control = dynamic_cast<BT::ControlNode*>(node))
        {
            for (const auto& child : control->children()) { children_uid.push_back(child->UID()); }
        }
        else if (auto decorator = dynamic_cast<BT::DecoratorNode*>(node))
        {
            children_uid.push_back(decorator->child()->UID());
        }

        std::vector<flatbuffers::Offset<Serialization::PortConfig>> ports;
        for (const auto& it : node->config().input_ports)  { /* nome, valor */ }
        for (const auto& it : node->config().output_ports) { /* nome, valor */ }

        auto tn = Serialization::CreateTreeNode(builder, node->UID(),
                    builder.CreateVector(children_uid),
                    convertToFlatbuffers(node->status()),
                    builder.CreateString(node->name().c_str()),
                    builder.CreateString(node->registrationName().c_str()),
                    builder.CreateVector(ports));
        fb_nodes.push_back(tn);
    });

    std::vector<flatbuffers::Offset<Serialization::NodeModel>> node_models;
    for (const auto& node_it: tree.manifests)  { /* ID, tipo, portas do manifesto */ }

    auto behavior_tree = Serialization::CreateBehaviorTree(builder, tree.rootNode()->UID(),
                                                          builder.CreateVector(fb_nodes),
                                                          builder.CreateVector(node_models));
    builder.Finish(behavior_tree);
}
```

Note que ela usa `tree.manifests` — que só é preenchido pelos métodos da fábrica
(seção 18).

## 19.6 `MinitraceLogger` — a linha do tempo

Grava um JSON no formato de *trace* do Chrome, que se abre em `chrome://tracing` como um
gráfico de barras temporal — a melhor ferramenta disponível para responder "o que estava
demorando".

```cpp
// include/behaviortree_cpp_v3/loggers/bt_minitrace_logger.h  (íntegro)
class MinitraceLogger : public StatusChangeLogger
{
    static std::atomic<bool> ref_count;

  public:
    MinitraceLogger(const BT::Tree& tree, const char* filename_json);
    virtual ~MinitraceLogger() override;

    virtual void callback(Duration timestamp, const TreeNode& node,
                          NodeStatus prev_status, NodeStatus status) override;
    virtual void flush() override;

  private:
    TimePoint prev_time_;
};
```

```cpp
// src/loggers/bt_minitrace_logger.cpp  (íntegro)
std::atomic<bool> MinitraceLogger::ref_count(false);

MinitraceLogger::MinitraceLogger(const Tree &tree, const char* filename_json)
  : StatusChangeLogger( tree.rootNode() )
{
    bool expected = false;
    if (!ref_count.compare_exchange_strong(expected, true))
    {
        throw LogicError("Only one instance of StdCoutLogger shall be created");
    }

    minitrace::mtr_register_sigint_handler();
    minitrace::mtr_init(filename_json);
    this->enableTransitionToIdle(true);
}

MinitraceLogger::~MinitraceLogger()
{
    minitrace::mtr_flush();
    minitrace::mtr_shutdown();
    ref_count = false;
}

const char* toConstStr(NodeType type)
{
  switch (type)
  {
    case NodeType::ACTION:    return "Action";
    case NodeType::CONDITION: return "Condition";
    case NodeType::DECORATOR: return "Decorator";
    case NodeType::CONTROL:   return "Control";
    case NodeType::SUBTREE:   return "SubTree";
    default:                  return "Undefined";
  }
}

void MinitraceLogger::callback(Duration /*timestamp*/,
                               const TreeNode& node, NodeStatus prev_status,
                               NodeStatus status)
{
    using namespace minitrace;

    const bool statusCompleted = (status == NodeStatus::SUCCESS || status == NodeStatus::FAILURE);

    const char* category = toConstStr(node.type());
    const char* name = node.name().c_str();

    if (prev_status == NodeStatus::IDLE && statusCompleted)
    {
        MTR_INSTANT(category, name);
    }
    else if (status == NodeStatus::RUNNING)
    {
        MTR_BEGIN(category, name);
    }
    else if (prev_status == NodeStatus::RUNNING && statusCompleted)
    {
        MTR_END(category, name);
    }
}

void MinitraceLogger::flush()
{
    minitrace::mtr_flush();
}
```

A leitura é elegante: um nó que vai de `IDLE` direto ao resultado é um evento
**instantâneo**; um que entra em `RUNNING` abre uma **barra**, fechada quando ele conclui.
A **categoria** é o tipo do nó, o que permite filtrar ações, condições e controles
separadamente no `chrome://tracing`.

**ARMADILHA — a mensagem de erro cita a classe errada.** O guarda de instância única do
`MinitraceLogger` lança `LogicError("Only one instance of StdCoutLogger shall be
created")` — copiado do *logger* de terminal e nunca ajustado. A classe também declara um
membro `TimePoint prev_time_` que nada lê nem escreve.

**Nota — `mtr_register_sigint_handler()`** instala um tratador de `SIGINT` que descarrega o
JSON antes de sair. Isso **substitui** qualquer tratador de `SIGINT` da aplicação.

## 19.7 `PublisherZMQ` e o Groot

O quarto *logger* publica o estado da árvore em rede, e é o que dá ao **Groot** a
visualização ao vivo.

```cpp
// include/behaviortree_cpp_v3/loggers/bt_zmq_publisher.h  (íntegro)
class PublisherZMQ : public StatusChangeLogger
{
    static std::atomic<bool> ref_count;

  public:
    PublisherZMQ(const BT::Tree& tree,
                 unsigned max_msg_per_second = 25,
                 unsigned publisher_port = 1666,
                 unsigned server_port = 1667);

    virtual ~PublisherZMQ();

  private:
    virtual void callback(Duration timestamp, const TreeNode& node,
                          NodeStatus prev_status, NodeStatus status) override;
    virtual void flush() override;

    const BT::Tree& tree_;
    std::vector<uint8_t> tree_buffer_;
    std::vector<uint8_t> status_buffer_;
    std::vector<SerializedTransition> transition_buffer_;
    std::chrono::microseconds min_time_between_msgs_;

    std::atomic_bool active_server_;
    std::thread thread_;

    void createStatusBuffer();

    TimePoint deadline_;
    std::mutex mutex_;
    std::atomic_bool send_pending_;
    std::future<void> send_future_;

    struct Pimpl;
    Pimpl* zmq_;
};
```

Ele abre **dois** *sockets* ZeroMQ:

| *Socket* | Porta padrão | Papel |
|---|---|---|
| `ZMQ_PUB` | **1666** | atualizações de estado, continuamente |
| `ZMQ_REP` | **1667** | servido por *thread* própria; responde a qualquer pedido com o FlatBuffers da topologia |

A separação existe porque as duas informações têm frequências opostas: a topologia é fixa e
grande, os estados mudam rápido e são pequenos. O Groot pede a topologia **uma vez**, ao
conectar, e depois só escuta as atualizações.

```cpp
// src/loggers/bt_zmq_publisher.cpp  (construtor, condensado)
PublisherZMQ::PublisherZMQ(const BT::Tree& tree, unsigned max_msg_per_second,
                           unsigned publisher_port, unsigned server_port)
  : StatusChangeLogger(tree.rootNode())
  , tree_(tree)
  , min_time_between_msgs_(std::chrono::microseconds(1000 * 1000) / max_msg_per_second)
  , send_pending_(false)
  , zmq_(new Pimpl())
{
    bool expected = false;
    if (!ref_count.compare_exchange_strong(expected, true))
    {
        throw LogicError("Only one instance of PublisherZMQ shall be created");
    }
    if( publisher_port == server_port)
    {
        throw LogicError("The TCP ports of the publisher and the server must be different");
    }

    flatbuffers::FlatBufferBuilder builder(1024);
    CreateFlatbuffersBehaviorTree(builder, tree);
    tree_buffer_.resize(builder.GetSize());
    memcpy(tree_buffer_.data(), builder.GetBufferPointer(), builder.GetSize());

    char str[100];
    sprintf(str, "tcp://*:%d", publisher_port);   zmq_->publisher.bind(str);
    sprintf(str, "tcp://*:%d", server_port);      zmq_->server.bind(str);

    int timeout_ms = 100;
    zmq_->server.set(zmq::sockopt::rcvtimeo, timeout_ms);
    active_server_ = true;

    thread_ = std::thread([this]() {
        while (active_server_)
        {
            zmq::message_t req;
            try
            {
                zmq::recv_result_t received = zmq_->server.recv(req);
                if (received)
                {
                    zmq::message_t reply(tree_buffer_.size());
                    memcpy(reply.data(), tree_buffer_.data(), tree_buffer_.size());
                    zmq_->server.send(reply, zmq::send_flags::none);
                }
            }
            catch (zmq::error_t& err) { /* ... */ active_server_ = false; }
        }
    });

    createStatusBuffer();
}
```

### A limitação de taxa

```cpp
// src/loggers/bt_zmq_publisher.cpp
void PublisherZMQ::callback(Duration timestamp, const TreeNode& node, NodeStatus prev_status,
                            NodeStatus status)
{
    SerializedTransition transition = SerializeTransition(node.UID(), timestamp,
                                                          prev_status, status);
    {
        std::unique_lock<std::mutex> lock(mutex_);
        transition_buffer_.push_back(transition);
    }

    if (!send_pending_)
    {
        send_pending_ = true;
        send_future_ = std::async(std::launch::async, [this]() {
            std::this_thread::sleep_for(min_time_between_msgs_);
            flush();
        });
    }
}
```

O construtor recebe `max_msg_per_second` (padrão **25**) e o converte em um intervalo
mínimo. A primeira transição de uma rajada agenda um envio para daí a 40 ms; as demais
apenas se acumulam no *buffer*, e todas saem juntas. É um agrupamento simples e eficaz, ao
custo de **uma *thread* nova a cada janela**.

### O formato da mensagem publicada

```cpp
// src/loggers/bt_zmq_publisher.cpp (flush, condensado)
const size_t msg_size = status_buffer_.size() + 8 + (transition_buffer_.size() * 12);
message.rebuild(msg_size);
uint8_t* data_ptr = static_cast<uint8_t*>(message.data());

// first 4 bytes are the side of the header
flatbuffers::WriteScalar<uint32_t>(data_ptr, static_cast<uint32_t>(status_buffer_.size()));
data_ptr += sizeof(uint32_t);
memcpy(data_ptr, status_buffer_.data(), status_buffer_.size());
data_ptr += status_buffer_.size();

// first 4 bytes are the side of the transition buffer
flatbuffers::WriteScalar<uint32_t>(data_ptr, static_cast<uint32_t>(transition_buffer_.size()));
data_ptr += sizeof(uint32_t);

for (auto& transition : transition_buffer_)
{
    memcpy(data_ptr, transition.data(), transition.size());
    data_ptr += transition.size();
}
transition_buffer_.clear();
createStatusBuffer();
```

O `status_buffer_` tem **3 bytes por nó** (`uint16_t` UID + `int8_t` status), montado por
`createStatusBuffer()` com `applyRecursiveVisitor`.

**ARMADILHA — não é compilado no empacotamento documentado.**
`src/loggers/bt_zmq_publisher.cpp` só entra na lista de fontes se o CMake achar ZeroMQ, e a
receita Conan desabilita essa busca. Sem ele, **não há monitoramento ao vivo pelo Groot** —
apenas a leitura *a posteriori* do arquivo do `FileLogger`. A ferramenta `bt3_recorder`
também deixa de ser construída. O CMake avisa com um
`message(WARNING "ZeroMQ NOT found. Skipping the build of [PublisherZMQ] and
[bt_recorder].")`, que se perde no meio da saída de configuração.

**REGRA — um `PublisherZMQ` por processo, e portas distintas.** O construtor lança se já
houver instância viva, e lança de novo se as duas portas forem iguais. Trocar de árvore em
tempo de execução exige **destruir o *publisher* antigo antes de criar o novo** — o
`CHANGELOG` registra essa correção na versão 3.5.4, sob o título *"Improved switching BTs
with active Groot monitoring (ZMQ logger destruction)"*.

**ARMADILHA — `PublisherZMQ` guarda uma referência à árvore.** `const BT::Tree& tree_;` —
se a `Tree` for movida ou destruída antes do *publisher*, a referência fica pendurada. Como
o destrutor do *publisher* chama `flush()`, que chama `createStatusBuffer()`, que percorre
`tree_.rootNode()`, isso é falha de segmentação na saída. **Declare o *publisher* depois da
árvore** (o que garante destruição antes dela).

## 19.8 Groot

O **Groot** é o editor e monitor gráfico do projeto (repositório
`github.com/BehaviorTree/Groot`). Dois modos:

1. **Editor** — lê o XML e desenha a árvore; permite criar/editar visualmente. Precisa do
   `<TreeNodesModel>` ou da forma explícita para conhecer os nós customizados.
2. **Monitor** — conecta-se ao `PublisherZMQ` (porta 1666 para as atualizações, 1667 para
   pedir a topologia) e pinta os nós conforme o estado em tempo real.

Fluxo típico para deixar uma árvore "pronta para o Groot":

```cpp
// 1. gerar o modelo dos nós customizados
std::cout << BT::writeTreeNodesModelXML(factory) << std::endl;
// 2. colar o <TreeNodesModel> gerado dentro do <root> do seu XML
// 3. no programa, ligar o publisher (requer ZMQ compilado)
#ifdef ZMQ_FOUND
BT::PublisherZMQ publisher_zmq(tree);
#endif
```

## 19.9 As ferramentas de linha de comando

O diretório `tools/` produz três executáveis, todos condicionados a `BUILD_TOOLS`.

| Executável | Entrada | O que faz |
|---|---|---|
| `bt3_log_cat` | arquivo do `FileLogger` | imprime a árvore e as transições |
| `bt3_recorder` | rede (porta 1666) | grava o que o `PublisherZMQ` publica |
| `bt3_plugin_manifest` | um `.so` de *plugin* | lista os nós e as portas que ele registra |

```cmake
# tools/CMakeLists.txt
cmake_minimum_required(VERSION 2.8)

add_executable(bt3_log_cat         bt_log_cat.cpp )
target_link_libraries(bt3_log_cat  ${BEHAVIOR_TREE_LIBRARY} )
install(TARGETS bt3_log_cat DESTINATION ${BEHAVIOR_TREE_BIN_DESTINATION} )

if( ZMQ_FOUND )
    add_executable(bt3_recorder         bt_recorder.cpp )
    target_link_libraries(bt3_recorder  ${BEHAVIOR_TREE_LIBRARY} ${ZMQ_LIBRARIES})
    install(TARGETS bt3_recorder DESTINATION ${BEHAVIOR_TREE_BIN_DESTINATION} )
endif()

add_executable(bt3_plugin_manifest         bt_plugin_manifest.cpp )
target_link_libraries(bt3_plugin_manifest  ${BEHAVIOR_TREE_LIBRARY} )
install(TARGETS bt3_plugin_manifest DESTINATION ${BEHAVIOR_TREE_BIN_DESTINATION} )
```

### `bt3_plugin_manifest` — o mais útil no dia a dia

```cpp
// tools/bt_plugin_manifest.cpp  (íntegro)
int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("Wrong number of command line arguments\nUsage: %s [filename]\n", argv[0]);
        return 1;
    }

    BT::BehaviorTreeFactory factory;

    std::unordered_set<std::string> default_nodes;
    for (auto& it : factory.manifests())
    {
        const auto& manifest = it.second;
        default_nodes.insert(manifest.registration_ID);
    }

    factory.registerFromPlugin(argv[1]);

    for (auto& it : factory.manifests())
    {
        const auto& manifest = it.second;
        if (default_nodes.count(manifest.registration_ID) > 0) { continue; }

        auto& params = manifest.ports;
        std::cout << "---------------\n"
                  << manifest.registration_ID << " [" << manifest.type
                  << "]\n  NodeParameters: " << params.size();
        if (params.size() > 0) { std::cout << ":"; }
        std::cout << std::endl;

        for (auto& param : params)
        {
            std::cout << "    - [Key]: \"" << param.first << "\"" << std::endl;
        }
    }
    return 0;
}
```

Ele cria uma fábrica vazia, anota os IDs embutidos, carrega o *plugin* e imprime a
**diferença** — ou seja, exatamente o que aquela biblioteca acrescenta. É a forma de
conferir se um *plugin* exporta o que se espera, **sem escrever nenhum XML**.

### `bt3_log_cat`

Lê o `.fbl` do `FileLogger`, reconstrói a árvore a partir do cabeçalho FlatBuffers,
imprime a hierarquia e depois cada transição com nome e cores. Não precisa de ZMQ.

**ARMADILHA — as ferramentas não são instaladas pelo pacote Conan.** A receita define
`BUILD_TOOLS=False`, de modo que o pacote contém apenas a biblioteca e os cabeçalhos. Para
usar `bt3_log_cat` é preciso compilar o repositório à parte, com o CMake direto.

## 19.10 Depuração sem *logger*

Duas funções cobrem os casos em que instrumentar é exagero:

```cpp
// 1. confirmar que o XML produziu a árvore esperada
BT::printTreeRecursively(tree.rootNode());

// 2. descobrir por que uma porta não atravessou a fronteira de uma subárvore
tree.blackboard_stack[0]->debugMessage();
tree.blackboard_stack[1]->debugMessage();

// 3. ver o texto CRU de um atributo, sem remapeamento nem conversão
std::cout << node.getRawPortValue("goal") << std::endl;
```

## 19.11 Estratégia de depuração por sintoma

| Sintoma | Instrumento |
|---|---|
| a árvore não carrega | mensagem do *parser* + `printTreeRecursively` |
| um nó não recebe o dado | `debugMessage()` nos dois *blackboards* |
| o ramo errado executa | `StdCoutLogger` + `enableTransitionToIdle(false)` |
| está lento | `MinitraceLogger` + `chrome://tracing` |
| trava e não volta | `Repeat`/`Retry` com `-1` e filho síncrono; ou `AsyncAction` que ignora `isHaltRequested()` |
| fica `RUNNING` para sempre | filho devolvendo `IDLE` sob `ForceSuccess`/`ForceFailure`/`KeepRunningUntilFailure` |
| um *plugin* "não existe" | `bt3_plugin_manifest` no `.so`; conferir `BT_PLUGIN_EXPORT` |

Os quatro primeiros não exigem nenhuma dependência externa.

---

# 20. A INFRAESTRUTURA DE SUPORTE (`utils/`)

Sob o núcleo há uma camada de utilitários que não sabe nada de árvores de comportamento.
Parte dela é código próprio; a maior parte, em linhas, é biblioteca de terceiros embutida.

## 20.1 `BT::Any` — o valor de tipo apagado

`Any` é o que o *blackboard* guarda. Envolve um `linb::any` — a implementação de
`std::any` embutida — e acrescenta duas coisas que a versão padrão não tem:
**normalização** de tipos numéricos e **conversão segura** entre eles.

```cpp
// include/behaviortree_cpp_v3/utils/safe_any.hpp  (condensada)
// Rational: since type erased numbers will always use at least 8 bytes
// it is faster to cast everything to either double, uint64_t or int64_t.
class Any
{
  public:
    Any(): _original_type(nullptr) {}
    ~Any() = default;

    Any(const Any& other) : _any(other._any), _original_type( other._original_type ) {}
    Any(Any&& other)      : _any( std::move(other._any) ), _original_type( other._original_type ) {}

    explicit Any(const double& value)   : _any(value),         _original_type(&typeid(double)) {}
    explicit Any(const uint64_t& value) : _any(value),         _original_type(&typeid(uint64_t)) {}
    explicit Any(const float& value)    : _any(double(value)), _original_type(&typeid(float)) {}
    explicit Any(const std::string& str): _any(SafeAny::SimpleString(str)),
                                          _original_type(&typeid(std::string)) {}
    explicit Any(const char* str)       : _any(SafeAny::SimpleString(str)),
                                          _original_type(&typeid(std::string)) {}
    explicit Any(const SafeAny::SimpleString& str) : _any(str),
                                          _original_type(&typeid(std::string)) {}

    // all the other integrals are casted to int64_t
    template <typename T>
    explicit Any(const T& value, EnableIntegral<T> = 0)
      : _any(int64_t(value)), _original_type( &typeid(T) ) {}

    // default for other custom types
    template <typename T>
    explicit Any(const T& value, EnableNonIntegral<T> = 0)
      : _any(value), _original_type( &typeid(T) ) {}

    Any& operator = (const Any& other);

    bool isNumber() const;
    bool isString() const;

    // this is different from any_cast, because if allows safe
    // conversions between arithmetic values.
    template <typename T> T cast() const;

    const std::type_info& type()       const noexcept { return *_original_type; }
    const std::type_info& castedType() const noexcept { return _any.type(); }
    bool empty() const noexcept { return _any.empty(); }

  private:
    linb::any _any;
    const std::type_info* _original_type;
    // convert<DST>() com quatro sobrecargas SFINAE: string, aritmético, enum, desconhecido
};
```

Todo inteiro vira `int64_t`, todo real vira `double`, toda *string* vira `SimpleString` —
mas o `type_info` **original** é guardado à parte. Daí os dois acessores:

- `type()` devolve o **tipo declarado pelo usuário**;
- `castedType()` devolve o **tipo realmente armazenado**.

**POR QUÊ — por que normalizar.** Sem normalização, um *blackboard* escrito por um nó como
`int` e lido por outro como `unsigned` falharia — `any_cast` exige tipo **exato**. Com a
normalização, ambos falam `int64_t` internamente, e a conversão de saída passa por
`convertNumber()`, que verifica limites. O custo é uma indireção e um ponteiro extra por
valor; o ganho é que o XML pode escrever `"42"` sem que o autor da árvore precise saber a
largura exata do inteiro do outro lado.

```cpp
// include/behaviortree_cpp_v3/utils/safe_any.hpp
template <typename T>
T cast() const
{
    if( _any.empty() )
    {
        throw std::runtime_error("Any::cast failed because it is empty");
    }
    if (_any.type() == typeid(T))
    {
        return linb::any_cast<T>(_any);
    }
    else
    {
        auto res = convert<T>();
        if( !res ) { throw std::runtime_error( res.error() ); }
        return res.value();
    }
}
```

**ARMADILHA — `Any::type()` desreferencia um ponteiro que pode ser nulo.** O construtor
padrão faz `_original_type(nullptr)`, e `type()` é `return *_original_type;`, **sem
verificação**. Chamar `type()` num `Any` vazio é comportamento indefinido — e
`Blackboard::debugMessage()` faz exatamente isso para entradas que foram declaradas mas
nunca escritas.

## 20.2 `convertNumber()` — conversões numéricas verificadas

Em `utils/convert_impl.hpp` há uma família de sobrecargas selecionadas por SFINAE, uma
para cada categoria de conversão numérica.

| Categoria | Verificação |
|---|---|
| mesmo tipo, ou destino `bool` | nenhuma |
| inteiro → inteiro maior, mesmo sinal | nenhuma (é segura por construção) |
| real → real de outra largura | truncamento |
| sem sinal → menor sem sinal | limite superior |
| com sinal → menor com sinal | limites inferior e superior |
| com sinal → sem sinal | recusa negativos, e limite superior |

```cpp
// include/behaviortree_cpp_v3/utils/convert_impl.hpp  (amostra)
template <typename SRC, typename DST>
inline EnableIf<std::is_same<bool, DST>> convertNumber(const SRC& from, DST& target)
{
    target = (from != 0);
}

template <typename SRC, typename DST>
inline EnableIf<is_safe_integer_conversion<SRC, DST>> convertNumber(const SRC& from, DST& target)
{
    target = static_cast<DST>(from);
}

template <typename SRC, typename DST>
inline EnableIf<float_conversion<SRC, DST>> convertNumber(const SRC& from, DST& target)
{
    checkTruncation<SRC, DST>(from);
    target = static_cast<DST>(from);
}

template <typename SRC, typename DST>
inline EnableIf<signed_to_smaller_unsigned_conversion<SRC, DST>>
convertNumber(const SRC& from, DST& target)
{
    if (from < 0)
    {
        throw std::runtime_error("Value is negative and can't be converted to signed");
    }
    checkUpperLimit<SRC, DST>(from);
    target = static_cast<DST>(from);
}
```

Uma conversão que perderia informação lança `std::runtime_error`. É esse mecanismo que
transforma um erro de tipo do XML em exceção legível, em vez de lixo numérico silencioso.

**ARMADILHA (menor) — a mensagem diz "signed" onde queria dizer "unsigned".** O destino de
`signed_to_smaller_unsigned_conversion` é *unsigned*.

## 20.3 `SimpleString` — *small object optimization*

Uma `std::string` da libstdc++ ocupa 32 bytes, o que estoura o armazenamento interno do
`linb::any` e força alocação. `SimpleString` existe para caber em **duas palavras**.

```cpp
// include/behaviortree_cpp_v3/utils/simple_string.hpp  (íntegro, condensado)
// Version of string that uses only two words. Good for small object optimization in linb::any
class SimpleString
{
  public:
    SimpleString(const std::string& str) : SimpleString(str.data(), str.size()) {}
    SimpleString(const char* input_data)  : SimpleString(input_data, strlen(input_data)) {}

    SimpleString(const char* input_data, std::size_t size) : _size(size)
    {
        if(size >= sizeof(void*) ) { _data.ptr = new char[_size + 1]; }
        std::memcpy(data(), input_data, _size);
        data()[_size] = '\0';
    }

    SimpleString(const SimpleString& other) : SimpleString(other.data(), other.size()) {}

    SimpleString& operator = (const SimpleString& other)
    {
      _data = other._data;
      _size = other._size;
      return *this;
    }

    ~SimpleString()
    {
        if ( _size >= sizeof(void*) && _data.ptr ) { delete[] _data.ptr; }
    }

    std::string toStdString() const { return std::string(data(), _size); }
    const char* data() const;
    char* data();
    std::size_t size() const { return _size; }

  private:
    union{
        char*  ptr;
        char   soo[sizeof(void*)] ;
    }_data;
    std::size_t _size;
};
```

Cadeias de até 7 caracteres vivem dentro do próprio objeto; a partir de 8, num
`new char[]`.

**ARMADILHA — o `operator=` de `SimpleString` é uma dupla liberação à espera.** Para uma
cadeia de 8 caracteres ou mais, os dois objetos passam a apontar para o **mesmo *buffer***:
o antigo vaza, e o destrutor de ambos executa `delete[]` sobre o mesmo endereço. A
biblioteca **não exercita esse caminho** — o `linb::any` atribui por *copy-and-swap*, o que
usa o construtor de cópia, que é correto — mas quem usar `SimpleString` diretamente
encontra o defeito. O construtor de cópia e o destrutor estão certos; **só a atribuição
não está**.

## 20.4 `TimerQueue` — temporizadores

Uma fila de prioridade de tarefas agendadas, servida por uma *thread* dedicada. É o motor
do `TimeoutNode` e do `DelayNode`. O cabeçalho credita a origem: um artigo de blog de 2016
sobre filas de temporizador portáveis (`crazygaze.com`).

```cpp
// include/behaviortree_cpp_v3/decorators/timer_queue.h
// Timer Queue
//
// Allows execution of handlers at a specified time in the future
// Guarantees:
//  - All handlers are executed ONCE, even if canceled (aborted parameter will
//    be set to true)
//      - If TimerQueue is destroyed, it will cancel all handlers.
//  - Handlers are ALWAYS executed in the Timer Queue worker thread.
//  - Handlers execution order is NOT guaranteed
template <typename _Clock = std::chrono::steady_clock,
          typename _Duration = std::chrono::steady_clock::duration>
class TimerQueue
{
  public:
    TimerQueue()
    {
        m_th = std::thread([this] { run(); });
    }

    ~TimerQueue()
    {
        cancelAll();
        // Abusing the timer queue to trigger the shutdown.
        add(std::chrono::milliseconds(0), [this](bool) { m_finish = true; });
        m_th.join();
    }

    //! Adds a new timer
    // \return Returns the ID of the new timer. You can use this ID to cancel the timer
    uint64_t add(std::chrono::milliseconds milliseconds, std::function<void(bool)> handler);

    //! Cancels the specified timer
    // \return 1 if the timer was cancelled. 0 if you were too late to cancel
    size_t cancel(uint64_t id);

    //! Cancels all timers
    size_t cancelAll();
    // ...
};
```

A **primeira garantia** é a que estrutura o código dos dois decoradores: o *handler* recebe
um `bool aborted` e **sempre** roda, mesmo cancelado.

```cpp
// include/behaviortree_cpp_v3/decorators/timer_queue.h  (cancel)
// Instead of removing the item from the container (thus breaking the
// heap integrity), we set the item as having no handler, and put
// that handler on a new item at the top for immediate execution
// The timer thread will then ignore the original item, since it has no handler.
std::unique_lock<std::mutex> lk(m_mtx);
for (auto&& item : m_items.getContainer())
{
    if (item.id == id && item.handler)
    {
        WorkItem newItem;
        // Zero time, so it stays at the top for immediate execution
        newItem.end = std::chrono::time_point<_Clock, _Duration>();
        newItem.id = 0;   // Means it is a canceled item
        // Move the handler from item to newitem.
        // Also, we need to manually set the handler to nullptr, since
        // the standard does not guarantee moving an std::function will
        // empty it. Some STL implementation will empty it, others will not.
        newItem.handler = std::move(item.handler);
        item.handler = nullptr;
        m_items.push(std::move(newItem));
        lk.unlock();
        m_checkWork.notify();
        return 1;
    }
}
return 0;
```

A classe `Queue` interna herda de `std::priority_queue` apenas para expor o contêiner
protegido `c`:

```cpp
// Inheriting from priority_queue, so we can access the internal container
class Queue
  : public std::priority_queue<WorkItem, std::vector<WorkItem>, std::greater<WorkItem>>
{
  public:
    std::vector<WorkItem>& getContainer() { return this->c; }
} m_items;
```

**ARMADILHA — uma *thread* por instância, e o destrutor abusa da fila.** O construtor lança
a *thread*; o destrutor cancela tudo e agenda um *handler* de zero milissegundo para
desligar o laço — o comentário admite: *"Abusing the timer queue to trigger the
shutdown"*. Como cada `<Timeout>` e cada `<Delay>` tem a sua **própria instância**, o número
de *threads* do processo cresce com o número desses nós na árvore, **não com a carga**.

**Uso isolado (fora de uma árvore):**

```cpp
BT::TimerQueue<> fila;
uint64_t id = fila.add(std::chrono::milliseconds(500), [](bool aborted) {
    if (!aborted) { std::cout << "disparou" << std::endl; }
});
// ...
fila.cancel(id);      // o handler ainda roda, com aborted == true
```

## 20.5 `SharedLibrary` — carga dinâmica

Deriva do POCO (a licença original está no topo do arquivo) e envolve `dlopen`/`dlsym` em
UNIX e `LoadLibrary` em Windows, com uma implementação por plataforma.

```cpp
// include/behaviortree_cpp_v3/utils/shared_library.h  (condensada)
class SharedLibrary
{
  public:
    enum Flags
    {
        SHLIB_GLOBAL = 1,   /// use RTLD_GLOBAL. This is the default if no flags are given.
        SHLIB_LOCAL  = 2    /// use RTLD_LOCAL instead of RTLD_GLOBAL.
                            /// Note: RTTI (dynamic_cast and throw) will not work for types
                            /// defined in the shared library with GCC.
    };

    SharedLibrary();
    SharedLibrary(const std::string& path, int flags = 0);
    virtual ~SharedLibrary() = default;   /// The actual library remains loaded.

    void load(const std::string& path, int flags = 0);
    void unload();
    bool isLoaded() const;
    bool hasSymbol(const std::string& name);
    void* getSymbol(const std::string& name);
    const std::string& getPath() const;

    static std::string prefix();          /// "lib" (ou "cyg" no Cygwin)
    static std::string suffix();          /// ".so" / ".dylib" / ".dll" / ".sl"
    static std::string getOSName(const std::string& name);   /// prefix() + name + suffix()
};
```

```cpp
// src/shared_library_UNIX.cpp
void SharedLibrary::load(const std::string& path, int)
{
    std::unique_lock<std::mutex> lock(_mutex);

    if (_handle)
    {
        throw RuntimeError("Library already loaded: " + path);
    }

    _handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!_handle)
    {
        const char* err = dlerror();
        throw RuntimeError("Could not load library: " + (err ? std::string(err) : path));
    }
    _path = path;
}
```

```cpp
// src/shared_library.cpp
void* BT::SharedLibrary::getSymbol(const std::string& name)
{
    void* result = findSymbol(name);
    if (result) return result;
    else throw RuntimeError( "[SharedLibrary::getSymbol]: can't find symbol ", name );
}

bool BT::SharedLibrary::hasSymbol(const std::string& name)
{
    return findSymbol(name) != nullptr;
}

std::string BT::SharedLibrary::getOSName(const std::string& name)
{
    return prefix() + name + suffix();
}
```

**ARMADILHA — o parâmetro `flags` é aceito e descartado.** A interface documenta
`SHLIB_GLOBAL` e `SHLIB_LOCAL`, com um comentário longo explicando que `RTLD_LOCAL` quebra
RTTI entre bibliotecas. Mas a implementação UNIX declara o segundo parâmetro **sem nome** —
`load(const std::string& path, int)` — e usa sempre `RTLD_NOW | RTLD_GLOBAL`. Pedir
`SHLIB_LOCAL` **não tem efeito nenhum**. Neste caso, felizmente, o comportamento fixo é o
correto para *plugins*.

`suffix()` acrescenta um `"d"` antes da extensão em compilações de depuração
(`#if defined(_DEBUG) && !defined(CL_NO_SHARED_LIBRARY_DEBUG_SUFFIX)` → `"d.so"`).

## 20.6 `StrCat` e `demangle`

```cpp
// include/behaviortree_cpp_v3/utils/strcat.hpp
// -----------------------------------------------------------------------------
// StrCat()
// -----------------------------------------------------------------------------
//
// Merges given strings, using no delimiter(s).
//
// `StrCat()` is designed to be the fastest possible way to construct a string
// out of a mix of raw C strings, string_views, strings.
inline std::string StrCat();
inline std::string StrCat(const nonstd::string_view& a);
inline std::string StrCat(const nonstd::string_view& a, const nonstd::string_view& b);
// ... sobrecargas até 5+ argumentos ...
```

Todas as exceções da biblioteca a usam, o que explica a forma
`throw RuntimeError("a", b, "c")` onipresente no código.

**ARMADILHA — `StrCat` só aceita coisas conversíveis para `string_view`.** Um `int` **não
compila**. Por isso o código faz
`StrCat("Missing parameter [", NUM_CYCLES, "]")` com `NUM_CYCLES` sendo um
`const char*`, e nunca passa números diretamente. Para incluir um número, converta antes
com `std::to_string()`.

```cpp
// include/behaviortree_cpp_v3/utils/demangle_util.h
inline std::string demangle( char const * name );
inline std::string demangle( const std::type_info& info );
inline std::string demangle( const std::type_info* info );
```

Traduz um `std::type_info` para um nome legível, via `abi::__cxa_demangle` quando
disponível (`__GLIBCXX__` ou Clang com `<cxxabi.h>`). É o que produz as mensagens de erro
de tipo do *blackboard* e os nomes de tipo no `<TreeNodesModel>` gerado. Em compiladores
sem `cxxabi.h` — MSVC, por exemplo — devolve o nome cru do `type_info`, que já é legível
naquele compilador.

## 20.7 Código de terceiros embutido

Esta é a parte que mais surpreende numa auditoria: **em número de linhas, a maioria do
repositório não é do projeto.**

| Componente | Onde | Linhas | Papel |
|---|---|---:|---|
| tinyxml2 | `src/private/` | 2 837 | *parser* XML |
| FlatBuffers | `include/.../flatbuffers/` | 2 747 | serialização dos *loggers* |
| *expected* (`nonstd`) | `utils/expected.hpp` | 1 957 | `Optional`/`Result` |
| *string_view* (`nonstd`) | `utils/string_view.hpp` | 1 532 | `StringView` |
| `linb::any` | `utils/any.hpp` | 463 | base do `BT::Any` |
| Minitrace | `3rdparty/minitrace/` | — | *trace* do Chrome |
| *filesystem* (wjakob) | `3rdparty/filesystem/` | — | caminhos, para o `<include>` |
| POCO (parcial) | `utils/shared_library.h` | — | carga dinâmica |
| cppzmq | `src/loggers/zmq.hpp` | — | *binding* C++ do ZeroMQ |

**POR QUÊ — por que embutir em vez de depender.** A biblioteca é feita para ser consumida
por sistemas robóticos, onde a árvore de dependências já é longa e a versão do compilador
raramente é a mais recente. Cada dependência externa é um item a mais na receita do
integrador, e **três das quatro maiores existem apenas para simular funcionalidades de
C++17** (`std::any`, `std::string_view`, `std::expected`) num projeto fixado em C++14.
Embuti-las torna a biblioteca compilável com `find_package(Threads)` e mais nada.

**REGRA — o tinyxml2 está num *namespace* renomeado.** O arquivo em
`src/private/tinyxml2.h` declara `namespace BT_TinyXML2`, não `tinyxml2`. É proposital: uma
aplicação pode usar a sua própria versão do tinyxml2 sem colidir com a da biblioteca. O
cabeçalho é **privado** — não é instalado — então isso é invisível ao consumidor.

**ARMADILHA — embutir não dispensa a licença.** A biblioteca é MIT, mas o pacote entregue
contém código sob outras licenças — POCO (Boost Software License), tinyxml2 (zlib),
FlatBuffers (Apache 2.0), *expected*/*string_view* (Boost), Minitrace (MIT). A receita
Conan copia apenas o `LICENSE` da raiz para `licenses/`, o que **subdocumenta a
distribuição**.

---

# 21. SISTEMA DE COMPILAÇÃO

A BehaviorTree.CPP compila com **CMake** e, no *fork* documentado, é empacotada com
**Conan 2**. O `CMakeLists.txt` tem uma característica incomum que precisa ser entendida
antes de integrá-la: **o conteúdo do binário depende do que estiver instalado na máquina
de quem compila.**

## 21.1 O topo do `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.5.1) # version on Ubuntu Xenial
project(behaviortree_cpp_v3)

#---- Add the subdirectory cmake ----
set(CMAKE_CONFIG_PATH ${CMAKE_MODULE_PATH}  "${CMAKE_CURRENT_LIST_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CONFIG_PATH})

#---- Enable C++14 ----
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

#---- Include boost to add coroutines ----
find_package(Boost COMPONENTS coroutine QUIET)

if(Boost_FOUND)
    string(REPLACE "." "0" Boost_VERSION_NODOT ${Boost_VERSION})
    if(NOT Boost_VERSION_NODOT VERSION_LESS 105900)
        message(STATUS "Found boost::coroutine2.")
        add_definitions(-DBT_BOOST_COROUTINE2)
        set(BT_COROUTINES true)
    elseif(NOT Boost_VERSION_NODOT VERSION_LESS 105300)
        message(STATUS "Found boost::coroutine.")
        add_definitions(-DBT_BOOST_COROUTINE)
        set(BT_COROUTINES true)
    endif()
    include_directories(${Boost_INCLUDE_DIRS})
endif()

if(NOT DEFINED BT_COROUTINES)
    message(STATUS "Coroutines disabled. Install Boost to enable them (version 1.59+ recommended).")
    add_definitions(-DBT_NO_COROUTINES)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
```

O padrão **C++14** é uma decisão estrutural, não um detalhe: é ela que obriga às
implementações embutidas de `any`, `string_view` e `expected`.

## 21.2 As opções

```cmake
#---- project configuration ----
option(BUILD_EXAMPLES   "Build tutorials and examples" ON)
option(BUILD_UNIT_TESTS "Build the unit tests" ON)
option(BUILD_TOOLS "Build commandline tools" ON)
option(BUILD_WITH_CURSES "Build with curses" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
```

| Opção | Padrão | Efeito |
|---|---|---|
| `BUILD_EXAMPLES` | `ON` | compila `examples/` e `sample_nodes/` |
| `BUILD_UNIT_TESTS` | `ON` | compila `tests/` (**exige GTest**) |
| `BUILD_TOOLS` | `ON` | compila `tools/` |
| `BUILD_WITH_CURSES` | `ON` | procura *ncurses*; se achar, inclui o `ManualSelector` |
| `BUILD_SHARED_LIBS` | `ON` | biblioteca dinâmica **em UNIX** |

**ARMADILHA — `BUILD_UNIT_TESTS=ON` torna o GTest obrigatório.** Fora de um ambiente ROS, o
`else if(BUILD_UNIT_TESTS)` final faz `find_package(GTest REQUIRED)`. Como a opção é `ON`
por padrão, uma máquina sem GTest **falha na configuração** — e a mensagem fala de GTest,
não de que basta desligar uma opção. **A primeira coisa a fazer numa integração é passar
`-DBUILD_UNIT_TESTS=OFF`.**

## 21.3 As quatro sondagens que mudam o binário

```
núcleo sempre presente (29 .cpp do projeto + tinyxml2 + minitrace)
    │
    ├── find_package(Boost)     achou → CoroActionNode
    │                           não achou → -DBT_NO_COROUTINES
    ├── find_package(ZMQ)       achou → PublisherZMQ + bt3_recorder + -DZMQ_FOUND
    ├── find_package(Curses)    achou → ManualSelectorNode + -DNCURSES_FOUND
    └── ament / catkin          muda o layout de instalação
                                e (no catkin) define -DUSING_ROS
```

```cmake
#---- Find other packages ----
find_package(Threads)
find_package(ZMQ)

list(APPEND BEHAVIOR_TREE_PUBLIC_LIBRARIES
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

if( ZMQ_FOUND )
    message(STATUS "ZeroMQ found.")
    add_definitions( -DZMQ_FOUND )
    list(APPEND BT_SOURCE src/loggers/bt_zmq_publisher.cpp)
else()
    message(WARNING "ZeroMQ NOT found. Skipping the build of [PublisherZMQ] and [bt_recorder].")
endif()
```

```cmake
if(BUILD_WITH_CURSES)
    find_package(Curses QUIET)
    if(CURSES_FOUND)
        list(APPEND BT_SOURCE src/controls/manual_node.cpp)
        list(APPEND BEHAVIOR_TREE_PUBLIC_LIBRARIES ${CURSES_LIBRARIES})
        add_definitions(-DNCURSES_FOUND)
    endif()
endif()
```

**ARMADILHA — a mesma versão do fonte produz binários diferentes.** Não há como pedir
explicitamente "com co-rotinas" ou "sem ZeroMQ". O *build* descobre sozinho, e o resultado
depende da máquina. Duas consequências práticas:

- uma imagem de CI com Boost instalado por outro motivo produz uma biblioteca com
  `CoroActionNode` que a máquina de produção não tem;
- um binário compilado numa máquina **não é intercambiável** com um compilado noutra,
  mesmo com o mesmo compilador e a mesma *tag*.

É exatamente o problema que a receita Conan do *fork* existe para resolver.

## 21.4 A lista de fontes

```cmake
list(APPEND BT_SOURCE
    src/action_node.cpp
    src/basic_types.cpp
    src/behavior_tree.cpp
    src/blackboard.cpp
    src/bt_factory.cpp
    src/decorator_node.cpp
    src/condition_node.cpp
    src/control_node.cpp
    src/shared_library.cpp
    src/tree_node.cpp
    src/xml_parsing.cpp

    src/decorators/inverter_node.cpp
    src/decorators/repeat_node.cpp
    src/decorators/retry_node.cpp
    src/decorators/subtree_node.cpp
    src/decorators/delay_node.cpp

    src/controls/if_then_else_node.cpp
    src/controls/fallback_node.cpp
    src/controls/parallel_node.cpp
    src/controls/reactive_sequence.cpp
    src/controls/reactive_fallback.cpp
    src/controls/sequence_node.cpp
    src/controls/sequence_star_node.cpp
    src/controls/switch_node.cpp
    src/controls/while_do_else_node.cpp

    src/loggers/bt_cout_logger.cpp
    src/loggers/bt_file_logger.cpp
    src/loggers/bt_minitrace_logger.cpp
    src/private/tinyxml2.cpp

    3rdparty/minitrace/minitrace.cpp
    )
```

Note o que **não** está na lista: `timeout_node`, `force_success_node`,
`force_failure_node`, `keep_running_until_failure_node`, `blackboard_precondition`,
`switch_node` (só o `.h`), `always_success_node`, `always_failure_node`,
`set_blackboard_node` — todos são **implementados inteiramente no cabeçalho** (`inline` ou
*template*). E `src/example.cpp` **não entra na biblioteca**.

## 21.5 Plataforma e tipo de biblioteca

```cmake
if (UNIX)
    list(APPEND BT_SOURCE src/shared_library_UNIX.cpp )
    if (BUILD_SHARED_LIBS)
        add_library(${BEHAVIOR_TREE_LIBRARY} SHARED ${BT_SOURCE})
    else()
        add_library(${BEHAVIOR_TREE_LIBRARY} STATIC ${BT_SOURCE})
    endif()
endif()

if (WIN32)
    set(CMAKE_DEBUG_POSTFIX "d")
    list(APPEND BT_SOURCE src/shared_library_WIN.cpp )
    add_library(${BEHAVIOR_TREE_LIBRARY} STATIC ${BT_SOURCE} )
endif()
```

**ARMADILHA — `BUILD_SHARED_LIBS` é ignorado no Windows.** No ramo `WIN32` a biblioteca é
sempre `STATIC`, independentemente da opção — o que faz sentido, dado que a biblioteca não
exporta símbolos com `__declspec(dllexport)`, mas contraria o nome da opção sem dizer nada.
E há um caso pior: numa plataforma que **não seja nem `UNIX` nem `WIN32`**, *nenhum*
`add_library` é executado, e a configuração falha adiante com um erro sobre alvo
inexistente.

## 21.6 Avisos, ligação e inclusão

```cmake
target_link_libraries(${BEHAVIOR_TREE_LIBRARY} PUBLIC
    ${BEHAVIOR_TREE_PUBLIC_LIBRARIES})

target_link_libraries(${BEHAVIOR_TREE_LIBRARY} PRIVATE
    ${Boost_LIBRARIES}
    ${ZMQ_LIBRARIES})

target_compile_definitions(${BEHAVIOR_TREE_LIBRARY} PRIVATE $<$<CONFIG:Debug>:TINYXML2_DEBUG>)

target_include_directories(${BEHAVIOR_TREE_LIBRARY} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/3rdparty>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${BUILD_TOOL_INCLUDE_DIRS})

if( ZMQ_FOUND )
    target_compile_definitions(${BEHAVIOR_TREE_LIBRARY} PUBLIC ZMQ_FOUND)
endif()

if(MSVC)
    target_compile_options(${BEHAVIOR_TREE_LIBRARY} PRIVATE /W3 /WX)
else()
    target_compile_options(${BEHAVIOR_TREE_LIBRARY} PRIVATE
        -Wall -Wextra -Werror=return-type)
endif()
```

**No MSVC todo aviso é erro** (`/WX`).

**ARMADILHA — o `BT_NO_COROUTINES` não atravessa a exportação.** Repare na assimetria:

- o ZeroMQ é propagado corretamente:
  `target_compile_definitions(... PUBLIC ZMQ_FOUND)` — portanto o consumidor recebe;
- já o `BT_NO_COROUTINES` é definido com **`add_definitions()`**, que é uma propriedade de
  **diretório** e **não é exportada** com o alvo.

O consumidor compila **sem** esse macro, `action_node.h` declara `CoroActionNode`, e o erro
aparece só na ligação, como um símbolo indefinido de construtor. É a razão de a receita
Conan do *fork* acrescentar o macro à mão em `cpp_info.defines`.

**Nota — o `3rdparty` está no `BUILD_INTERFACE` mas não no `INSTALL_INTERFACE`.** Quem
compila contra a árvore de fontes tem `3rdparty/` no caminho de inclusão; quem consome o
pacote **instalado**, não. Isso importa apenas para quem inclui `minitrace/minitrace.h`
diretamente.

## 21.7 Instalação e exportação

```cmake
INSTALL(TARGETS ${BEHAVIOR_TREE_LIBRARY}
    EXPORT BehaviorTreeV3Config
    ARCHIVE DESTINATION ${BEHAVIOR_TREE_LIB_DESTINATION}
    LIBRARY DESTINATION ${BEHAVIOR_TREE_LIB_DESTINATION}
    RUNTIME DESTINATION ${BEHAVIOR_TREE_BIN_DESTINATION}
    )

INSTALL( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${BEHAVIOR_TREE_INC_DESTINATION}
    FILES_MATCHING PATTERN "*.h*")

install(EXPORT BehaviorTreeV3Config
    DESTINATION "${BEHAVIOR_TREE_LIB_DESTINATION}/BehaviorTreeV3/cmake"
    NAMESPACE BT::)

export(TARGETS ${PROJECT_NAME}
    NAMESPACE BT::
    FILE "${CMAKE_CURRENT_BINARY_DIR}/BehaviorTreeV3Config.cmake")

export(PACKAGE ${PROJECT_NAME})
```

Do lado do consumidor, com CMake puro:

```cmake
find_package(BehaviorTreeV3 REQUIRED)
add_executable(minha_app main.cpp)
target_link_libraries(minha_app PRIVATE BT::behaviortree_cpp_v3)
```

**ARMADILHA — os `cmake_minimum_required` são antigos demais.** A raiz pede `3.5.1`, com o
comentário *"version on Ubuntu Xenial"*; `tools/CMakeLists.txt` pede `2.8`. O CMake 3.27
emite um aviso de depreciação para políticas anteriores a 3.5, e o **CMake 4 recusa** a
configuração de projetos que pedem menos de 3.5. Numa distribuição recente, o *build* pode
parar antes de compilar a primeira linha — e a mensagem fala de compatibilidade de
políticas, não do arquivo a corrigir.

Contorno, se preciso:

```bash
cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5
```

## 21.8 ROS 1 e ROS 2

O `CMakeLists.txt` reconhece três ambientes e escolhe um leiaute de instalação para cada. A
ordem importa: **ament primeiro, catkin depois, CMake puro por último.**

```cmake
# Update the policy setting to avoid an error when loading the ament_cmake package
if(POLICY CMP0057)
    cmake_policy(SET CMP0057 NEW)
endif()

find_package(ament_cmake QUIET)

if ( ament_cmake_FOUND )

    # Not adding -DUSING_ROS since xml_parsing.cpp hasn't been ported to ROS2

    message(STATUS "------------------------------------------")
    message(STATUS "BehaviourTree is being built using AMENT.")
    message(STATUS "------------------------------------------")
    set(BUILD_TOOL_INCLUDE_DIRS ${ament_INCLUDE_DIRS})

elseif( CATKIN_DEVEL_PREFIX OR CATKIN_BUILD_BINARY_PACKAGE)

    set(catkin_FOUND 1)
    add_definitions( -DUSING_ROS )
    find_package(catkin REQUIRED COMPONENTS roslib)
    find_package(GTest)

    message(STATUS "------------------------------------------")
    message(STATUS "BehaviourTree is being built using CATKIN.")
    message(STATUS "------------------------------------------")

    catkin_package(
        INCLUDE_DIRS include # do not include "3rdparty" here
        LIBRARIES ${BEHAVIOR_TREE_LIBRARY}
        CATKIN_DEPENDS roslib
        )

    list(APPEND BEHAVIOR_TREE_PUBLIC_LIBRARIES ${catkin_LIBRARIES})
    set(BUILD_TOOL_INCLUDE_DIRS ${catkin_INCLUDE_DIRS})

elseif(BUILD_UNIT_TESTS)
    find_package(GTest REQUIRED)
endif()
```

```xml
<!-- package.xml -->
<package format="3">
  <name>behaviortree_cpp_v3</name>
  <version>3.5.6</version>

  <buildtool_depend condition="$ROS_VERSION == 1">catkin</buildtool_depend>
  <depend condition="$ROS_VERSION == 1">roslib</depend>

  <buildtool_depend condition="$ROS_VERSION == 2">ament_cmake</buildtool_depend>
  <depend condition="$ROS_VERSION == 2">rclcpp</depend>

  <depend>libzmq3-dev</depend>
  <depend>libncurses-dev</depend>

  <test_depend condition="$ROS_VERSION == 2">ament_cmake_gtest</test_depend>

  <export>
      <build_type condition="$ROS_VERSION == 1">catkin</build_type>
      <build_type condition="$ROS_VERSION == 2">ament_cmake</build_type>
  </export>
</package>
```

Note que `libzmq3-dev` e `libncurses-dev` são dependências **incondicionais** — coerente
com o fato de que, num *build* ROS, ambas serão encontradas e o binário resultante terá o
`PublisherZMQ` e o `ManualSelectorNode`.

**ARMADILHA — `ros_pkg` e `registerFromROSPlugins()` não funcionam no ROS 2.** O comentário
é explícito: o `USING_ROS` **não** é definido no caminho do ament, porque
`xml_parsing.cpp` nunca foi portado. Consequência: sob ROS 2, o atributo `ros_pkg` de um
`<include>` e o método `registerFromROSPlugins()` lançam a mesma `RuntimeError` de sempre —
*"this library was compiled without ROS support. Recompile the BehaviorTree.CPP using
catkin"* — que é um conselho **impossível de seguir** num ambiente ROS 2. Use caminhos
relativos e `registerFromPlugin()` explícito.

## 21.9 O empacotamento Conan 2 (o *fork*)

```python
# conanfile.py
class BehaviorTreeCppAsaConan(ConanFile):
    name = "behaviortree.cpp.asa"
    version = "3.5.6"
    license = "MIT"
    url = "https://github.com/BehaviorTree/BehaviorTree.CPP"
    author = "Davide Faconti <davide.faconti@gmail.com>"
    topics = ("behaviortree", "ai", "robotics", "games", "coordination")
    description = (
        "This C++ library provides a framework to create BehaviorTrees. "
        "It was designed to be flexible, easy to use and fast."
    )

    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": True, "fPIC": True}

    exports_sources = (
        "CMakeLists.txt", "cmake/*", "include/*", "src/*", "3rdparty/*", "LICENSE",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        tc.cache_variables["BUILD_EXAMPLES"] = False
        tc.cache_variables["BUILD_UNIT_TESTS"] = False
        tc.cache_variables["BUILD_TOOLS"] = False
        tc.cache_variables["BUILD_WITH_CURSES"] = False
        # Determinismo: o CMakeLists procura estes pacotes no sistema e, se os
        # achar, muda o conteudo/ABI do pacote (coroutines com Boost, logger
        # ZMQ, layout de instalacao do ament) sem que a receita os declare.
        tc.cache_variables["CMAKE_DISABLE_FIND_PACKAGE_Boost"] = True
        tc.cache_variables["CMAKE_DISABLE_FIND_PACKAGE_ZMQ"] = True
        tc.cache_variables["CMAKE_DISABLE_FIND_PACKAGE_ament_cmake"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder,
             os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["behaviortree_cpp_v3"]
        # Sem Boost a lib e compilada com -DBT_NO_COROUTINES; o mesmo define
        # precisa chegar ao consumidor, senao action_node.h declara
        # CoroActionNode, que nao existe no binario.
        self.cpp_info.defines = ["BT_NO_COROUTINES"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["pthread", "dl"]
```

**POR QUÊ — por que desligar tudo em vez de declarar as dependências.** Um pacote binário
precisa ser **reprodutível**: o mesmo *package ID* tem de corresponder ao mesmo conteúdo,
sempre. Como o `CMakeLists.txt` muda o binário conforme o que houver na máquina, a receita
tem duas saídas — declarar Boost, ZeroMQ, *ncurses* e ament como *requires* (e arrastar
quatro dependências para todo consumidor) ou desligá-las e documentar a ausência. A receita
escolhe a segunda, e as três variáveis `CMAKE_DISABLE_FIND_PACKAGE_*` são a forma padrão do
CMake de dizer "não procure, mesmo que esteja aí".

## 21.10 O `Makefile` de conveniência do *fork*

```makefile
PKG_NAME := behaviortree.cpp.asa
PKG_VERSION := 3.5.6
PKG_REF := $(PKG_NAME)/$(PKG_VERSION)

BUILD_TYPE := Debug
SHARED := True

CONAN_OPTS := \
	--build=missing \
	--settings=build_type=$(BUILD_TYPE) \
	--options=$(PKG_NAME)/*:shared=$(SHARED)
```

| Alvo | O que faz |
|---|---|
| `make clean` | apaga `build/` |
| `make configure` | resolve dependências e gera o *toolchain* em `build/` |
| `make build` | compila localmente, sem tocar no *cache* do Conan |
| `make create` | compila e instala o pacote no *cache* local |
| `make create-all` | faz o mesmo para Debug **e** Release |
| `make list` | lista as revisões no *cache* |
| `make path` | imprime a pasta do pacote no *cache* |
| `make remove` | remove o pacote do *cache* |
| `make upload REMOTE=<nome>` | envia o pacote a um *remote* |

```bash
make create BUILD_TYPE=Release     # gera e instala no cache local
make list                          # confere a revisão
make path                          # onde o pacote ficou
```

**ARMADILHA — o padrão do `Makefile` é Debug.** `BUILD_TYPE := Debug` está no topo do
arquivo. Um `make create` sem argumentos produz um pacote de **depuração** — mais lento,
maior, e possivelmente incompatível de ABI com o resto da aplicação em algumas combinações
de MSVC. Para produção, sempre `BUILD_TYPE=Release`, ou `make create-all` para ter os dois
no *cache*.

## 21.11 Consumindo o pacote

```python
# conanfile.py da aplicação
from conan import ConanFile
from conan.tools.cmake import cmake_layout

class MinhaApp(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("behaviortree.cpp.asa/3.5.6")
```

```cmake
# CMakeLists.txt da aplicação
cmake_minimum_required(VERSION 3.16)
project(minha_app CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(behaviortree.cpp.asa REQUIRED)

add_executable(minha_app main.cpp)
target_link_libraries(minha_app PRIVATE behaviortree.cpp.asa::behaviortree.cpp.asa)
```

```bash
conan install . --build=missing -s build_type=Release
cmake --preset conan-release
cmake --build --preset conan-release
./build/Release/minha_app
```

**REGRA — `shared=True` é o padrão, e importa para *plugins*.** Um *plugin* carregado com
`RTLD_GLOBAL` precisa compartilhar com a aplicação **a mesma instância** da biblioteca — em
particular, os mesmos `type_info` e os mesmos contadores estáticos como o de `UID`. Com a
biblioteca **estática** ligada duas vezes, cada lado tem os seus.

## 21.12 Compilando o repositório diretamente

Quando o objetivo é rodar os exemplos, os testes ou as ferramentas — que o pacote Conan não
traz — o caminho é o CMake puro:

```bash
cd BehaviorTree.CPP
cmake -B build-cmake -S . \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_UNIT_TESTS=OFF          # senão exige GTest
cmake --build build-cmake -j

./build-cmake/bin/t05_cross_door
./build-cmake/bin/bt3_log_cat bt_trace.fbl
```

Os executáveis vão para `build-cmake/bin/` e a biblioteca para `build-cmake/lib/`,
conforme os `CMAKE_*_OUTPUT_DIRECTORY` definidos no ramo não-ROS:

```cmake
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${BEHAVIOR_TREE_BIN_DESTINATION}" )
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${BEHAVIOR_TREE_LIB_DESTINATION}" )
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${BEHAVIOR_TREE_BIN_DESTINATION}" )
```

### Compilando um único arquivo contra a árvore de fontes já construída

```bash
REPO=/caminho/para/BehaviorTree.CPP
g++ -std=c++14 -DBT_NO_COROUTINES \
    -I$REPO/include -I$REPO/3rdparty \
    meu_programa.cpp -o meu_programa \
    -L$REPO/build/Debug/lib -lbehaviortree_cpp_v3 -lpthread -ldl

LD_LIBRARY_PATH=$REPO/build/Debug/lib ./meu_programa
```

O `-DBT_NO_COROUTINES` é **obrigatório** se a biblioteca foi compilada sem Boost, e é
justamente o macro que a exportação do CMake não propaga.

## 21.13 Resumo das dependências

| Dependência | Obrigatória? | O que se perde sem ela |
|---|---|---|
| *Threads* (`pthread`) | **sim** | nada compila |
| `libdl` | **sim** (UNIX) | `registerFromPlugin()` |
| Boost *coroutine* | não | `CoroActionNode` |
| ZeroMQ | não | `PublisherZMQ`, Groot ao vivo, `bt3_recorder` |
| *ncurses* | não | `ManualSelectorNode` |
| GTest | só para os testes | `tests/` |
| catkin / ament | não | *layout* ROS e `ros_pkg` |

No empacotamento do *fork*, apenas as duas primeiras estão presentes.

## 21.14 Os testes

```cmake
# tests/CMakeLists.txt
set(BT_TESTS
  src/action_test_node.cpp
  src/condition_test_node.cpp
  gtest_tree.cpp
  gtest_sequence.cpp
  gtest_parallel.cpp
  gtest_fallback.cpp
  gtest_factory.cpp
  gtest_decorator.cpp
  gtest_blackboard.cpp
  gtest_ports.cpp
  navigation_test.cpp
  gtest_subtree.cpp
  gtest_switch.cpp
)

if (BT_COROUTINES)
    list(APPEND BT_TESTS  gtest_coroutines.cpp)
endif()
```

Os testes são úteis como **documentação executável** do comportamento esperado de cada nó
— em particular `gtest_sequence.cpp`, `gtest_fallback.cpp` e `gtest_parallel.cpp`, que
cobrem as combinações de estado que este documento descreve em prosa.

---

# 22. OS DOZE TUTORIAIS DO REPOSITÓRIO

O diretório `examples/` traz doze programas numerados, cada um isolando um conceito. São a
melhor referência de **estilo idiomático** da biblioteca, e vale conhecê-los pelo que
ensinam e pelo que (às vezes) ensinam errado.

## 22.1 `t01_build_your_first_tree` — a primeira árvore

**Ensina:** criar nós customizados; as duas formas de registrar (estática e por *plugin*);
o ciclo mínimo fábrica → XML → *tick*.

```cpp
// examples/t01_build_your_first_tree.cpp
/** Behavior Tree are used to create a logic to decide what
 * to "do" and when. For this reason, our main building blocks are
 * Actions and Conditions.
 *
 * In this tutorial, we will learn how to create custom ActionNodes.
 * It is important to remember that NodeTree are just a way to
 * invoke callbacks (called tick() ). These callbacks are implemented by the user.
 */

static const char* xml_text = R"(

 <root main_tree_to_execute = "MainTree" >

     <BehaviorTree ID="MainTree">
        <Sequence name="root_sequence">
            <CheckBattery   name="battery_ok"/>
            <OpenGripper    name="open_gripper"/>
            <ApproachObject name="approach_object"/>
            <CloseGripper   name="close_gripper"/>
        </Sequence>
     </BehaviorTree>

 </root>
 )";

int main()
{
    // We use the BehaviorTreeFactory to register our custom nodes
    BehaviorTreeFactory factory;

    /* There are two ways to register nodes:
    *    - statically, i.e. registering all the nodes one by one.
    *    - dynamically, loading the TreeNodes from a shared library (plugin).
    * */

#ifdef MANUAL_STATIC_LINKING
    using namespace DummyNodes;

    // The recommended way to create a Node is through inheritance.
    factory.registerNodeType<ApproachObject>("ApproachObject");

    // Registering a SimpleActionNode using a function pointer.
    factory.registerSimpleCondition("CheckBattery", std::bind(CheckBattery));

    //You can also create SimpleActionNodes using methods of a class
    GripperInterface gripper;
    factory.registerSimpleAction("OpenGripper",  std::bind(&GripperInterface::open,  &gripper));
    factory.registerSimpleAction("CloseGripper", std::bind(&GripperInterface::close, &gripper));
#else
    // Load dynamically a plugin and register the TreeNodes it contains
    factory.registerFromPlugin("./libdummy_nodes_dyn.so");
#endif

    // Trees are created at deployment-time (i.e. at run-time, but only once at the beginning).
    // The currently supported format is XML.
    // IMPORTANT: when the object "tree" goes out of scope, all the TreeNodes are destroyed
    auto tree = factory.createTreeFromText(xml_text);

    // To "execute" a Tree you need to "tick" it.
    // The tick is propagated to the children based on the logic of the tree.
    tree.tickRoot();

    return 0;
}
```

Saída esperada:

```
[ Battery: OK ]
GripperInterface::open
ApproachObject: approach_object
GripperInterface::close
```

**Note:** `tree.tickRoot()` é chamado **uma vez**, sem laço — porque todas as ações são
síncronas e a `Sequence` inteira resolve dentro de um único *tick*.

**ARMADILHA do exemplo — `GripperInterface gripper;` é uma variável local do `main()`**
capturada por ponteiro no `std::bind`. Funciona porque a árvore também vive no `main()`. A
versão do `dummy_nodes.h` usada pelo *plugin* usa `static GripperInterface grip_singleton;`
justamente para evitar o problema.

## 22.2 `t02_basic_ports` — portas de entrada e saída

**Ensina:** `InputPort`/`OutputPort`; `{chave}` versus literal; conectar saída de um nó à
entrada de outro; `registerSimpleAction` com `PortsList` explícita.

```cpp
// examples/t02_basic_ports.cpp
/** This tutorial will teach you how basic input/output ports work.
 *
 * Ports are a mechanism to exchange information between Nodes using
 * a key/value storage called "Blackboard".
 * The type and number of ports of a Node is statically defined.
 *
 * Input Ports are like "argument" of a functions.
 * Output ports are, conceptually, like "return values".
 */

static const char* xml_text = R"(

 <root main_tree_to_execute = "MainTree" >

     <BehaviorTree ID="MainTree">
        <Sequence name="root">
            <SaySomething     message="start thinking..." />
            <ThinkWhatToSay   text="{the_answer}"/>
            <SaySomething     message="{the_answer}" />
            <SaySomething2    message="SaySomething2 works too..." />
            <SaySomething2    message="{the_answer}" />
        </Sequence>
     </BehaviorTree>

 </root>
 )";

class ThinkWhatToSay : public BT::SyncActionNode
{
  public:
    ThinkWhatToSay(const std::string& name, const BT::NodeConfiguration& config)
      : BT::SyncActionNode(name, config) {}

    // This Action simply write a value in the port "text"
    BT::NodeStatus tick() override
    {
        setOutput("text", "The answer is 42" );
        return BT::NodeStatus::SUCCESS;
    }

    // A node having ports MUST implement this STATIC method
    static BT::PortsList providedPorts()
    {
        return { BT::OutputPort<std::string>("text") };
    }
};

int main()
{
    using namespace DummyNodes;
    BehaviorTreeFactory factory;

    // The class SaySomething has a method called providedPorts() that define the INPUTS.
    factory.registerNodeType<SaySomething>("SaySomething");

    // Similarly to SaySomething, ThinkWhatToSay has an OUTPUT port called "text"
    // Both these ports are std::string, therefore they can connect to each other
    factory.registerNodeType<ThinkWhatToSay>("ThinkWhatToSay");

    // SimpleActionNodes can not define their own method providedPorts(), therefore
    // we have to pass the PortsList explicitly if we want the Action to use getInput()
    // or setOutput();
    PortsList say_something_ports = { InputPort<std::string>("message") };
    factory.registerSimpleAction("SaySomething2", SaySomethingSimple, say_something_ports );

    auto tree = factory.createTreeFromText(xml_text);
    tree.tickRoot();
    return 0;
}
```

Saída esperada:

```
Robot says: start thinking...
Robot says: The answer is 42
Robot says: SaySomething2 works too...
Robot says: The answer is 42
```

E o nó que lê a porta:

```cpp
// sample_nodes/dummy_nodes.cpp
BT::NodeStatus SaySomething::tick()
{
    auto msg = getInput<std::string>("message");
    if (!msg)
    {
        throw BT::RuntimeError( "missing required input [message]: ", msg.error() );
    }

    std::cout << "Robot says: " << msg.value() << std::endl;
    return BT::NodeStatus::SUCCESS;
}
```

**Lição central:** *"The way we 'connect' output ports to input ports is to 'point' to the
same Blackboard entry."*

## 22.3 `t03_generic_ports` — tipos próprios

**Ensina:** `convertFromString<T>` para um `struct`; porta com descrição; `SetBlackboard`
com conversão diferida.

```cpp
// examples/t03_generic_ports.cpp
// We want to be able to use this custom type
struct Position2D { double x,y; };

// It is recommended (or, in some cases, mandatory) to define a template
// specialization of convertFromString that converts a string to Position2D.
namespace BT
{
template <> inline Position2D convertFromString(StringView str)
{
    printf("Converting string: \"%s\"\n", str.data() );

    // real numbers separated by semicolons
    auto parts = splitString(str, ';');
    if (parts.size() != 2)
    {
        throw RuntimeError("invalid input)");
    }
    else{
        Position2D output;
        output.x     = convertFromString<double>(parts[0]);
        output.y     = convertFromString<double>(parts[1]);
        return output;
    }
}
} // end namespace BT

class CalculateGoal: public SyncActionNode
{
public:
    CalculateGoal(const std::string& name, const NodeConfiguration& config):
        SyncActionNode(name,config) {}

    NodeStatus tick() override
    {
        Position2D mygoal = {1.1, 2.3};
        setOutput("goal", mygoal);
        return NodeStatus::SUCCESS;
    }
    static PortsList providedPorts()
    {
        return { OutputPort<Position2D>("goal") };
    }
};

class PrintTarget: public SyncActionNode
{
public:
    PrintTarget(const std::string& name, const NodeConfiguration& config):
        SyncActionNode(name,config) {}

    NodeStatus tick() override
    {
        auto res = getInput<Position2D>("target");
        if( !res )
        {
            throw RuntimeError("error reading port [target]:", res.error() );
        }
        Position2D goal = res.value();
        printf("Target positions: [ %.1f, %.1f ]\n", goal.x, goal.y );
        return NodeStatus::SUCCESS;
    }

    static PortsList providedPorts()
    {
        // Optionally, a port can have a human readable description
        const char*  description = "Simply print the target on console...";
        return { InputPort<Position2D>("target", description) };
    }
};
```

```xml
<Sequence name="root">
    <CalculateGoal   goal="{GoalPosition}" />
    <PrintTarget     target="{GoalPosition}" />
    <SetBlackboard   output_key="OtherGoal" value="-1;3" />
    <PrintTarget     target="{OtherGoal}" />
</Sequence>
```

Saída esperada:

```
Target positions: [ 1.1, 2.3 ]
Converting string: "-1;3"
Target positions: [ -1.0, 3.0 ]
```

**Note que a conversão só é chamada uma vez** — no `PrintTarget` que lê `OtherGoal`, que
guarda uma *string*. O primeiro `PrintTarget` lê um `Position2D` já tipado e não converte.

## 22.4 `t04_reactive_sequence` — reatividade e ação assíncrona

**Ensina:** a diferença entre `Sequence` e `ReactiveSequence`; `AsyncActionNode`.

```cpp
// examples/t04_reactive_sequence.cpp
static const char* xml_text_sequence = R"(
 <root main_tree_to_execute = "MainTree" >
     <BehaviorTree ID="MainTree">
        <Sequence name="root">
            <BatteryOK/>
            <SaySomething   message="mission started..." />
            <MoveBase       goal="1;2;3"/>
            <SaySomething   message="mission completed!" />
        </Sequence>
     </BehaviorTree>
 </root>
 )";

static const char* xml_text_reactive = R"(
 <root main_tree_to_execute = "MainTree" >
     <BehaviorTree ID="MainTree">
        <ReactiveSequence name="root">
            <BatteryOK/>
            <Sequence>
                <SaySomething   message="mission started..." />
                <MoveBase       goal="1;2;3"/>
                <SaySomething   message="mission completed!" />
            </Sequence>
        </ReactiveSequence>
     </BehaviorTree>
 </root>
 )";
```

Saída esperada, transcrita do próprio arquivo:

```
------------ BUILDING A NEW TREE ------------      (Sequence)

--- 1st executeTick() ---
[ Battery: OK ]
Robot says: "mission started..."
[ MoveBase: STARTED ]. goal: x=1 y=2.0 theta=3.00

--- 2nd executeTick() ---
[ Battery: OK ]                                     <- reavaliada!
[ MoveBase: FINISHED ]

--- 3rd executeTick() ---
[ Battery: OK ]
Robot says: "mission completed!"

------------ BUILDING A NEW TREE ------------      (ReactiveSequence)

--- 1st executeTick() ---
[ Battery: OK ]
Robot says: "mission started..."
[ MoveBase: STARTED ]. goal: x=1 y=2.0 theta=3.00

--- 2nd executeTick() ---
[ MoveBase: FINISHED ]                              <- BatteryOK NÃO reavaliada?!

--- 3rd executeTick() ---
Robot says: "mission completed!"
```

**ARMADILHA do exemplo — os comentários do código estão desatualizados.** O `main()` diz:

```cpp
    // The main difference that you should notice is:
    //  1) When Sequence is used, BatteryOK is executed at __each__ tick()
    //  2) When SequenceStar is used, those ConditionNodes are executed only __once__.
```

Mas o segundo XML usa **`ReactiveSequence`**, não `SequenceStar` — o comentário sobrou da
v2, quando `Sequence` era o nome do nó reativo (ver a tabela de migração). A saída
comentada no arquivo corresponde ao comportamento da **v2**, não ao da v3. Não use este
exemplo como referência de semântica; use a seção 10 deste documento.

## 22.5 `t05_crossdoor` — subárvores, decoradores e *loggers*

**Ensina:** `Fallback`, `Inverter`, `RetryUntilSuccesful`, `SubTree`, e os quatro
*loggers*. É o exemplo mais completo do repositório.

```cpp
// examples/t05_crossdoor.cpp
static const char* xml_text = R"(
<root main_tree_to_execute = "MainTree">
	<!--------------------------------------->
    <BehaviorTree ID="DoorClosed">
        <Sequence name="door_closed_sequence">
            <Inverter>
                <Condition ID="IsDoorOpen"/>
            </Inverter>
            <RetryUntilSuccesful num_attempts="4">
                <OpenDoor/>
            </RetryUntilSuccesful>
            <PassThroughDoor/>
        </Sequence>
    </BehaviorTree>
    <!--------------------------------------->
    <BehaviorTree ID="MainTree">
        <Sequence>
            <Fallback name="root_Fallback">
                <Sequence name="door_open_sequence">
                    <IsDoorOpen/>
                    <PassThroughDoor/>
                </Sequence>
                <SubTree ID="DoorClosed"/>
                <PassThroughWindow/>
            </Fallback>
            <CloseDoor/>
        </Sequence>
    </BehaviorTree>
    <!--------------------------------------->
</root>
 )";

int main(int argc, char** argv)
{
    BT::BehaviorTreeFactory factory;

    // register all the actions into the factory
    CrossDoor::RegisterNodes(factory);

    // Important: when the object tree goes out of scope, all the TreeNodes are destroyed
    auto tree = factory.createTreeFromText(xml_text);

    // This logger prints state changes on console
    StdCoutLogger logger_cout(tree);

    // This logger saves state changes on file
    FileLogger logger_file(tree, "bt_trace.fbl");

    // This logger stores the execution time of each node
    MinitraceLogger logger_minitrace(tree, "bt_trace.json");

#ifdef ZMQ_FOUND
    // This logger publish status changes using ZeroMQ. Used by Groot
    PublisherZMQ publisher_zmq(tree);
#endif

    printTreeRecursively(tree.rootNode());

    const bool LOOP = ( argc == 2 && strcmp( argv[1], "loop") == 0);

    do
    {
        NodeStatus status = NodeStatus::RUNNING;
        // Keep on ticking until you get either a SUCCESS or FAILURE state
        while( status == NodeStatus::RUNNING)
        {
            status = tree.tickRoot();
            CrossDoor::SleepMS(1);   // optional sleep to avoid "busy loops"
        }
        CrossDoor::SleepMS(1000);
    }
    while(LOOP);

    return 0;
}
```

Os nós:

```cpp
// sample_nodes/crossdoor_nodes.cpp
// For simplicity, in this example the status of the door is not shared
// using ports and blackboards
static bool _door_open   = false;
static bool _door_locked = true;

NodeStatus CrossDoor::IsDoorOpen()   { SleepMS(500); return _door_open ? SUCCESS : FAILURE; }
NodeStatus CrossDoor::IsDoorLocked() { SleepMS(500); return _door_locked ? SUCCESS : FAILURE; }
NodeStatus CrossDoor::UnlockDoor()   { if(_door_locked){ SleepMS(2000); _door_locked=false; } return SUCCESS; }
NodeStatus CrossDoor::PassThroughDoor()   { SleepMS(1000); return _door_open ? SUCCESS : FAILURE; }
NodeStatus CrossDoor::PassThroughWindow() { SleepMS(1000); return SUCCESS; }
NodeStatus CrossDoor::OpenDoor()  { if(_door_locked) return FAILURE; SleepMS(2000); _door_open=true; return SUCCESS; }
NodeStatus CrossDoor::CloseDoor() { if(_door_open){ SleepMS(1500); _door_open=false; } return SUCCESS; }

void CrossDoor::RegisterNodes(BehaviorTreeFactory& factory)
{
    factory.registerSimpleCondition("IsDoorOpen",        std::bind(IsDoorOpen));
    factory.registerSimpleAction("PassThroughDoor",      std::bind(PassThroughDoor));
    factory.registerSimpleAction("PassThroughWindow",    std::bind(PassThroughWindow));
    factory.registerSimpleAction("OpenDoor",             std::bind(OpenDoor));
    factory.registerSimpleAction("CloseDoor",            std::bind(CloseDoor));
    factory.registerSimpleCondition("IsDoorLocked",      std::bind(IsDoorLocked));
    factory.registerSimpleAction("UnlockDoor",           std::bind(UnlockDoor));
}
```

**Três coisas a aprender deste exemplo, e uma a não copiar:**

1. Os *loggers* são **variáveis locais** declaradas logo após a criação da árvore e antes
   do laço — exatamente como a seção 19.1 exige.
2. O `<SubTree ID="DoorClosed"/>` **sem atributos** é gratuito porque nada precisa
   atravessar a fronteira do *blackboard*.
3. `<Condition ID="IsDoorOpen"/>` (forma explícita) e `<IsDoorOpen/>` (forma compacta)
   aparecem **no mesmo arquivo**, e são equivalentes.
4. **A não copiar:** o estado da porta vive em duas variáveis `static` do arquivo, e o
   próprio comentário admite a escolha didática. Assim que houver dois robôs, ou duas
   instâncias da árvore, o estado global vira um defeito. Além disso, `IsDoorOpen()` dorme
   500 ms **dentro de uma condição** — o antípoda da regra "condições precisam ser
   baratas".

Note também que `UnlockDoor` e `IsDoorLocked` são registrados mas **não aparecem no XML**.

## 22.6 `t06_subtree_port_remapping` — a fronteira do *blackboard*

**Ensina:** remapeamento de portas de subárvore; inspeção com `debugMessage()`.

```cpp
// examples/t06_subtree_port_remapping.cpp
/** In the CrossDoor example we did not exchange any information
 * between the Maintree and the DoorClosed subtree.
 *
 * If we tried to do that, we would have noticed that it can't be done, because
 * each of the tree/subtree has its own Blackboard, to avoid the problem of name
 * clashing in very large trees.
 *
 * But a SubTree can have its own input/output ports.
 * In practice, these ports are nothing more than "soft links" between the
 * ports inside the SubTree (called "internal") and those in the parent
 * Tree (called "external").
 */

static const char* xml_text = R"(
<root main_tree_to_execute = "MainTree">

    <BehaviorTree ID="MainTree">
        <Sequence name="main_sequence">
            <SetBlackboard output_key="move_goal" value="1;2;3" />
            <SubTree ID="MoveRobot" target="move_goal" output="move_result" />
            <SaySomething message="{move_result}"/>
        </Sequence>
    </BehaviorTree>

    <BehaviorTree ID="MoveRobot">
        <Fallback name="move_robot_main">
            <SequenceStar>
                <MoveBase       goal="{target}"/>
                <SetBlackboard output_key="output" value="mission accomplished" />
            </SequenceStar>
            <ForceFailure>
                <SetBlackboard output_key="output" value="mission failed" />
            </ForceFailure>
        </Fallback>
    </BehaviorTree>

</root>
 )";

int main()
{
    BehaviorTreeFactory factory;
    factory.registerNodeType<SaySomething>("SaySomething");
    factory.registerNodeType<MoveBaseAction>("MoveBase");

    auto tree = factory.createTreeFromText(xml_text);

    NodeStatus status = NodeStatus::RUNNING;
    while( status == NodeStatus::RUNNING)
    {
        status = tree.tickRoot();
        SleepMS(1);
    }

    // let's visualize some information about the current state of the blackboards.
    std::cout << "--------------" << std::endl;
    tree.blackboard_stack[0]->debugMessage();
    std::cout << "--------------" << std::endl;
    tree.blackboard_stack[1]->debugMessage();
    std::cout << "--------------" << std::endl;

    return 0;
}
```

Saída esperada:

```
[ MoveBase: STARTED ]. goal: x=1 y=2.0 theta=3.00
[ MoveBase: FINISHED ]
Robot says: mission accomplished
--------------
move_result (std::string) -> full
move_goal (Pose2D) -> full
--------------
output (std::string) -> remapped to parent [move_result]
target (Pose2D) -> remapped to parent [move_goal]
--------------
```

**Note:** `move_goal` aparece no *blackboard* raiz com tipo `Pose2D` — apesar de ter sido
escrito pelo `SetBlackboard` como *string* — porque a porta `goal` do `MoveBase` é
`InputPort<Pose2D>` e o *parser* declarou o tipo na carga.

Note também o uso de `<ForceFailure>` para registrar `"mission failed"` no *blackboard*
**e ainda assim** propagar a falha.

## 22.7 `t07_wrap_legacy` — embrulhar código existente

**Ensina:** integrar uma classe legada sem modificá-la, via lambda + `registerSimpleAction`.

Ver o código completo na seção 12.5.

**Lição:** o parâmetro `TreeNode&` do *functor* é o que dá acesso a `getInput()`/
`setOutput()` sem herança.

## 22.8 `t08_additional_node_args` — argumentos que não são portas

**Ensina:** as duas formas de injetar argumentos de construção (`NodeBuilder` e `init()`).

Ver o código completo na seção 12.10.

Saída esperada:

```
Action_A: 42 / 3.14 / hello world
Action_B: 69 / 9.99 / interesting_value
```

## 22.9 `t09_async_actions_coroutines` — co-rotinas

**Ensina:** `CoroActionNode`, `setStatusRunningAndYield()`, `halt()` com limpeza.

```xml
<root >
    <BehaviorTree>
       <Timeout msec="150">
           <SequenceStar name="sequence">
               <MyAsyncAction name="action_A"/>
               <MyAsyncAction name="action_B"/>
           </SequenceStar>
       </Timeout>
    </BehaviorTree>
</root>
```

```cpp
int main()
{
    // Simple tree: a sequence of two asycnhronous actions,
    // but the second will be halted because of the timeout.
    BehaviorTreeFactory factory;
    factory.registerNodeType<MyAsyncAction>("MyAsyncAction");

    auto tree = factory.createTreeFromText(xml_text);

    // keep executin tick until it returns etiher SUCCESS or FAILURE
    while( tree.tickRoot() == NodeStatus::RUNNING)
    {
        // ...
    }
}
```

Cada `MyAsyncAction` leva ~100 ms; o `<Timeout msec="150">` expira durante a segunda,
demonstrando o `halt()` com limpeza (`cleanup(true)`).

**Só compila com Boost.** Ver o código completo na seção 12.8.

## 22.10 `t10_include_trees` — carregar de arquivo

**Ensina:** `createTreeFromFile()` e `<include>`.

```cpp
// examples/t10_include_trees.cpp  (íntegro)
int main(int argc, char** argv)
{
    BehaviorTreeFactory factory;
    DummyNodes::RegisterNodes(factory);

    if( argc != 2)
    {
        std::cout <<" missing name of the XML file to open" << std::endl;
        return 1;
    }

    // IMPORTANT: when the object tree goes out of scope,
    // all the TreeNodes are destroyed
    auto tree = factory.createTreeFromFile(argv[1]);

    printTreeRecursively( tree.rootNode() );

    tree.tickRoot();

    return 0;
}
```

Usa-se com `examples/test_files/subtree_test.xml`, que inclui `Check.xml` e
`subtrees/Talk.xml`.

## 22.11 `t11_runtime_ports` — portas em tempo de execução

**Ensina:** registrar um nó cuja lista de portas só é conhecida no registro.

Ver o código completo na seção 7.10.

## 22.12 `t12_ncurses_manual_selector` — o operador no laço

**Ensina:** `ManualSelector` (requer *ncurses*).

```cpp
/* Try also
*      <ManualSelector repeat_last_selection="1">
*  to see the difference.
*/
static const char* xml_text = R"(
 <root main_tree_to_execute = "MainTree" >
     <BehaviorTree ID="MainTree">
        <Repeat num_cycles="3">
            <ManualSelector repeat_last_selection="0">
                <SaySomething name="Option1"    message="Option1" />
                <SaySomething name="Option2"    message="Option2" />
                <SaySomething name="Option3"    message="Option3" />
                <SaySomething name="Option4"    message="Option4" />
                <ManualSelector name="YouChoose" />
            </ManualSelector>
        </Repeat>
     </BehaviorTree>
 </root>
 )";
```

Note o `<ManualSelector name="YouChoose" />` **sem filhos** como último filho: nesse caso
ele pergunta qual `NodeStatus` devolver diretamente.

## 22.13 `broken_sequence.cpp`

Um arquivo extra em `examples/`, não numerado, que demonstra o comportamento de uma
sequência com um filho que devolve `IDLE` — o erro que a biblioteca proíbe.

## 22.14 O que os exemplos ensinam (e o que não ensinam)

| Ensinam bem | Não ensinam / ensinam errado |
|---|---|
| a estrutura fábrica → XML → *tick* | tratamento de exceção no laço principal |
| portas e `convertFromString` | uso de `StatefulActionNode` (não há exemplo!) |
| remapeamento de subárvore | `ReactiveFallback`, `Parallel`, `Switch`, `IfThenElse`, `WhileDoElse` (nenhum exemplo) |
| injeção de argumentos | o `halt()` correto de `AsyncActionNode` (o exemplo omite a chamada à base) |
| ligar *loggers* | condições baratas (o `crossdoor` dorme 500 ms numa condição) |
| *plugins* e `BT_PLUGIN_EXPORT` | estado fora de variáveis globais (o `crossdoor` usa `static`) |

**Não existe exemplo de `StatefulActionNode` no repositório da v3.5.6** — apesar de ser a
classe recomendada pelo próprio cabeçalho. A seção 12.6 deste documento supre a lacuna.

---

# 23. RECEITAS E PADRÕES DE PROJETO

Esta seção reúne padrões prontos, na forma "problema → solução → por quê".

## 23.1 O esqueleto mínimo de uma aplicação

```cpp
#include "behaviortree_cpp_v3/bt_factory.h"
#include "behaviortree_cpp_v3/loggers/bt_cout_logger.h"
#include <chrono>
#include <thread>

int main()
{
    // 1. REGISTRAR
    BT::BehaviorTreeFactory factory;
    factory.registerNodeType<MinhaAcao>("MinhaAcao");
    // ... ou factory.registerFromPlugin("./libmeus_nos.so");

    // 2. CARREGAR
    auto tree = factory.createTreeFromFile("arvore.xml");

    // 3. OBSERVAR (depois da árvore, antes do primeiro tick)
    BT::StdCoutLogger logger(tree);
    logger.seTimestampType(BT::TimestampType::RELATIVE);
    logger.enableTransitionToIdle(false);

    // 4. TICAR
    try
    {
        BT::NodeStatus status = BT::NodeStatus::RUNNING;
        while (status == BT::NodeStatus::RUNNING)
        {
            status = tree.tickRoot();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cout << "resultado: " << status << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "erro na execução da árvore: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

**Note o `catch (const std::exception&)`, e não `catch (BT::BehaviorTreeException&)`** —
três nós da biblioteca lançam `std::logic_error` cru.

## 23.2 Laço com frequência fixa

O laço acima "dorme 10 ms **depois** do *tick*", o que faz o período ser
`10 ms + duração do tick`. Para frequência estável:

```cpp
using clock = std::chrono::steady_clock;
const auto periodo = std::chrono::milliseconds(20);   // 50 Hz

auto proximo = clock::now();
BT::NodeStatus status = BT::NodeStatus::RUNNING;
while (status == BT::NodeStatus::RUNNING)
{
    status = tree.tickRoot();
    proximo += periodo;
    std::this_thread::sleep_until(proximo);
}
```

**REGRA — meça o *tick*.** Se `tree.tickRoot()` levar mais que o período, o `sleep_until`
retorna imediatamente e a frequência cai sem aviso. Instrumente:

```cpp
auto t0 = clock::now();
status = tree.tickRoot();
auto dt = clock::now() - t0;
if (dt > periodo) { /* avisar */ }
```

## 23.3 Laço que reinicia a árvore ao final

Para um comportamento que deve rodar indefinidamente:

```cpp
while (!encerrar)
{
    BT::NodeStatus status = BT::NodeStatus::RUNNING;
    while (status == BT::NodeStatus::RUNNING && !encerrar)
    {
        status = tree.tickRoot();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // tickRoot() já repôs a raiz a IDLE; o próximo tick recomeça
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}
tree.haltTree();
```

Alternativa **dentro da árvore**, sem laço externo:

```xml
<Repeat num_cycles="-1">
    <SuaArvore/>
</Repeat>
```

**ARMADILHA:** isso só funciona se `SuaArvore` puder devolver `RUNNING`; com uma árvore
inteiramente síncrona, o `Repeat` de `-1` entra em laço infinito **dentro de um tick**.

## 23.4 Encerrar de fora (sinal, botão, temporizador)

```cpp
std::atomic_bool encerrar{false};

// tratador de SIGINT
std::signal(SIGINT, [](int){ /* setar a flag; ver nota */ });

while (!encerrar && tree.tickRoot() == BT::NodeStatus::RUNNING)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
tree.haltTree();     // interrompe ações RUNNING e repõe tudo a IDLE
```

**REGRA — não chame `tickRoot()` e `haltTree()` de *threads* diferentes.** Não há
sincronização entre eles. Sinalize por uma *flag* atômica lida pelo laço, e chame
`haltTree()` **na mesma *thread*** que *tica*.

## 23.5 Guarda contínua (o padrão mais útil de todos)

**Problema:** uma ação longa precisa ser interrompida quando uma condição deixa de valer.

```xml
<ReactiveSequence>
    <BateriaOK/>              <!-- condição barata, reavaliada a cada tick -->
    <SemObstaculo/>           <!-- idem -->
    <MoveBase goal="{alvo}"/> <!-- única ação assíncrona -->
</ReactiveSequence>
```

**Por quê:** a `ReactiveSequence` reavalia todos os filhos a cada *tick*; se uma condição
falhar, ela chama `haltChildren()` e a ação em curso é cancelada no mesmo *tick*.

**Requisitos:** a ação precisa ser assíncrona (`StatefulActionNode`, `AsyncActionNode` ou
`CoroActionNode`) e cooperar com o `halt()`. Uma ação síncrona não pode ser interrompida.

## 23.6 Prioridade com alternativa (*fallback*)

```xml
<Fallback>
    <Sequence>              <!-- estratégia preferida -->
        <PortaAberta/>
        <PassarPelaPorta/>
    </Sequence>
    <SubTree ID="AbrirPorta"/>   <!-- segunda opção -->
    <PassarPelaJanela/>          <!-- último recurso -->
</Fallback>
```

Use `<ReactiveFallback>` se a preferência deve ser reavaliada durante a execução da
alternativa.

## 23.7 Tentativas com desistência

```xml
<RetryUntilSuccesful num_attempts="3">
    <Sequence>
        <PrepararTentativa/>
        <Tentar/>
    </Sequence>
</RetryUntilSuccesful>
```

**Cuidado:** com filhos **síncronos**, as três tentativas acontecem dentro de um mesmo
*tick*, sem pausa. Para espaçar as tentativas:

```xml
<RetryUntilSuccesful num_attempts="3">
    <Sequence>
        <Tentar/>
        <!-- Delay depois da falha não funciona: a Sequence já saiu.
             Coloque o Delay ANTES, ou use um StatefulActionNode que
             já incorpore o intervalo. -->
    </Sequence>
</RetryUntilSuccesful>
```

A forma correta de espaçar é fazer a própria ação ser assíncrona e incorporar o intervalo:

```cpp
BT::NodeStatus onStart() override
{
    proxima_tentativa_ = clock::now() + intervalo_;
    return BT::NodeStatus::RUNNING;
}
BT::NodeStatus onRunning() override
{
    if (clock::now() < proxima_tentativa_) return BT::NodeStatus::RUNNING;
    return tentar() ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}
```

## 23.8 Prazo (*timeout*) que realmente interrompe

```xml
<Timeout msec="5000">
    <MinhaAcaoAssincrona/>
</Timeout>
```

**Requisitos, todos obrigatórios:**

1. o filho precisa devolver `RUNNING` (ação assíncrona);
2. se for `AsyncActionNode`, o `tick()` precisa consultar `isHaltRequested()`;
3. `msec` maior que zero (com `0`, o prazo é desligado).

**Custo:** uma *thread* por nó `<Timeout>`, viva desde a carga.

## 23.9 Passo opcional

```xml
<Sequence>
    <ForceSuccess>
        <TirarFoto/>      <!-- se falhar, a missão continua -->
    </ForceSuccess>
    <Prosseguir/>
</Sequence>
```

## 23.10 Executar até que algo dê errado

```xml
<KeepRunningUntilFailure>
    <Patrulhar/>
</KeepRunningUntilFailure>
```

Combinado com uma guarda:

```xml
<ReactiveSequence>
    <BateriaOK/>
    <KeepRunningUntilFailure>
        <Patrulhar/>
    </KeepRunningUntilFailure>
</ReactiveSequence>
```

## 23.11 Sequência que não desfaz o trabalho feito

```xml
<SequenceStar>
    <AbrirGarra/>
    <Aproximar goal="{alvo}"/>
    <FecharGarra/>
</SequenceStar>
```

Se `Aproximar` falhar, a próxima passagem **retoma de `Aproximar`**, sem reabrir a garra.

**ARMADILHA:** a memória se perde se a `SequenceStar` for *halted* enquanto `RUNNING`. Se
a retomada precisar sobreviver a isso, guarde o progresso no *blackboard*:

```xml
<SequenceStar>
    <BlackboardCheckInt value_A="{passo}" value_B="0" return_on_mismatch="SUCCESS">
        <Sequence>
            <AbrirGarra/>
            <SetBlackboard output_key="passo" value="1"/>
        </Sequence>
    </BlackboardCheckInt>
    <!-- ... -->
</SequenceStar>
```

## 23.12 Máquina de estados dentro da árvore

```xml
<Switch3 variable="{estado}" case_1="ocioso" case_2="movendo" case_3="carregando">
    <Ocioso/>
    <Movendo/>
    <Carregando/>
    <Erro/>                <!-- default -->
</Switch3>
```

**Requisitos:** `estado` precisa ser `std::string` (ou `int`) no *blackboard*; **nunca**
`double`.

## 23.13 Condição explícita, com "senão"

```xml
<!-- não reativo: decide uma vez e leva até o fim -->
<IfThenElse>
    <BateriaBaixa/>
    <IrParaBase/>
    <ContinuarMissao/>
</IfThenElse>

<!-- reativo: troca de ramo se a condição mudar -->
<WhileDoElse>
    <BateriaBaixa/>
    <IrParaBase/>
    <ContinuarMissao/>     <!-- os 3 filhos são OBRIGATÓRIOS aqui -->
</WhileDoElse>
```

## 23.14 Várias coisas ao mesmo tempo

```xml
<Parallel success_threshold="1" failure_threshold="2">
    <MoveBase goal="{alvo}"/>
    <MonitorarObstaculos/>
    <PublicarTelemetria/>
</Parallel>
```

Semântica: sucesso quando **1** filho tiver sucesso; falha quando **2** falharem (ou quando
os restantes já não bastarem para atingir 1).

**Requisitos:** os filhos precisam ser assíncronos para que a "simultaneidade" seja real;
`success_threshold` é obrigatório.

## 23.15 Subárvore reutilizável com interface explícita

```xml
<!-- chamada -->
<SubTree ID="Aproximar" alvo="posicao_atual" resultado="ultimo_resultado"/>

<!-- definição -->
<BehaviorTree ID="Aproximar">
    <Sequence>
        <MoveBase goal="{alvo}"/>
        <SetBlackboard output_key="resultado" value="ok"/>
    </Sequence>
</BehaviorTree>
```

**REGRA — documente a interface da subárvore num comentário.** A biblioteca não tem
declaração de portas de subárvore na v3; a única documentação é a convenção:

```xml
<!-- SubTree "Aproximar"
     ENTRADA:  alvo      (Pose2D)   destino
     SAÍDA:    resultado (string)   "ok" | "falhou"
-->
<BehaviorTree ID="Aproximar">
```

## 23.16 Injetar o "mundo" nos nós

```cpp
struct Mundo { Robo* robo; Mapa* mapa; };

Mundo mundo{&robo, &mapa};

// via builder — o modo recomendado
factory.registerBuilder<IrPara>("IrPara",
    [&mundo](const std::string& name, const BT::NodeConfiguration& config) {
        return std::make_unique<IrPara>(name, config, mundo);
    });
```

E o nó:

```cpp
class IrPara : public BT::StatefulActionNode
{
  public:
    IrPara(const std::string& n, const BT::NodeConfiguration& c, Mundo m)
      : BT::StatefulActionNode(n, c), mundo_(m) {}
    static BT::PortsList providedPorts() { return { BT::InputPort<Pose2D>("goal") }; }
    // ...
  private:
    Mundo mundo_;
};
```

**Por que não usar o *blackboard* para isso:** o *blackboard* é para dados que **fluem**
entre nós e mudam entre *ticks*. Ponteiros de infraestrutura são constantes de construção.

## 23.17 Duas árvores no mesmo processo

```cpp
BT::BehaviorTreeFactory factory;     // uma fábrica basta
registrarTudo(factory);

auto tree_a = factory.createTreeFromFile("a.xml");
auto tree_b = factory.createTreeFromFile("b.xml");
```

**Restrições:**

- **um `StdCoutLogger` por processo** (guarda estático) — o mesmo para `MinitraceLogger` e
  `PublisherZMQ`;
- os `UID` da segunda árvore **continuam** a numeração da primeira;
- se `tree_a` e `tree_b` compartilharem um `Blackboard::Ptr`, passe-o explicitamente:
  `factory.createTreeFromFile("b.xml", tree_a.rootBlackboard())`.

## 23.18 Trocar de árvore em tempo de execução

```cpp
{
    auto tree = factory.createTreeFromFile("fase1.xml");
    BT::PublisherZMQ pub(tree);
    rodar(tree);
}   // pub destruído ANTES de tree — obrigatório

{
    auto tree = factory.createTreeFromFile("fase2.xml");
    BT::PublisherZMQ pub(tree);
    rodar(tree);
}
```

**REGRA — escopos aninhados, na ordem certa.** O `PublisherZMQ` guarda uma referência à
`Tree` e precisa morrer antes dela. Declarar os dois no mesmo escopo, com o *publisher*
**depois** da árvore, garante isso pela ordem inversa de destruição.

## 23.19 Testar um nó isoladamente, sem XML

```cpp
#include "behaviortree_cpp_v3/behavior_tree.h"

TEST(MeuNo, DevolveSucesso)
{
    BT::NodeConfiguration config;
    config.blackboard = BT::Blackboard::create();
    BT::assignDefaultRemapping<MinhaAcao>(config);      // portas ↔ chaves homônimas

    config.blackboard->set("goal", Pose2D{1,2,0});

    MinhaAcao no("teste", config);
    EXPECT_EQ(no.executeTick(), BT::NodeStatus::SUCCESS);
}
```

`assignDefaultRemapping<T>()` é a peça que falta para usar um nó fora do *parser*: ela
preenche `config.input_ports`/`output_ports` com `"="`, o que faz cada porta apontar para a
entrada de mesmo nome. É o que os testes da própria biblioteca fazem
(`tests/gtest_blackboard.cpp`).

## 23.20 Montar uma árvore em C++, sem XML

Possível, mas desaconselhado. Se for necessário:

```cpp
// os nós precisam sobreviver à árvore; guarde-os
BT::NodeConfiguration cfg;
cfg.blackboard = BT::Blackboard::create();

auto raiz  = std::make_unique<BT::SequenceNode>("raiz");
auto acao1 = std::make_unique<MinhaAcao>("a1", cfg);
auto acao2 = std::make_unique<MinhaAcao>("a2", cfg);

raiz->addChild(acao1.get());
raiz->addChild(acao2.get());

BT::NodeStatus st = raiz->executeTick();
```

**Por que desaconselhado:** você perde a validação do *parser*, a aplicação de valores
padrão, a declaração de tipos no *blackboard* e a construção do `Tree` (e portanto os
*loggers*, que exigem `Tree`). Além disso, gerenciar o tempo de vida vira sua
responsabilidade.

## 23.21 Ler o resultado da árvore no C++

```cpp
auto tree = factory.createTreeFromFile("arvore.xml");
rodar(tree);

// via blackboard raiz
std::string resultado;
if (tree.rootBlackboard()->get("resultado", resultado)) {
    std::cout << resultado << std::endl;
}

// ou, com exceção se ausente
auto r = tree.rootBlackboard()->get<std::string>("resultado");
```

## 23.22 Escrever no *blackboard* antes de *ticar*

```cpp
auto tree = factory.createTreeFromFile("arvore.xml");
tree.rootBlackboard()->set("alvo", Pose2D{1.0, 2.0, 0.0});
rodar(tree);
```

**ARMADILHA:** se nenhuma porta tipada tiver declarado `alvo` na carga, o `set()` cria a
entrada **sem tipo travado**. Se um nó a ler como `Pose2D`, funciona; se outro a ler como
`std::string`, a conversão falha em tempo de execução.

## 23.23 Uma condição que consulta um serviço lento

**Problema:** condições precisam ser baratas, mas a informação vem de uma chamada lenta.

**Solução:** separe a **coleta** da **decisão**. Um nó assíncrono coleta e escreve no
*blackboard*; a condição só lê.

```xml
<Parallel success_threshold="1" failure_threshold="1">
    <!-- coleta contínua, assíncrona -->
    <KeepRunningUntilFailure>
        <AtualizarEstadoDoSensor saida="{leitura}"/>
    </KeepRunningUntilFailure>

    <!-- decisão, barata -->
    <ReactiveSequence>
        <BlackboardCheckString value_A="{leitura}" value_B="ok" return_on_mismatch="FAILURE">
            <AlwaysSuccess/>
        </BlackboardCheckString>
        <FazerOTrabalho/>
    </ReactiveSequence>
</Parallel>
```

## 23.24 Contador / limitador de taxa

Não existe nó embutido. Escreva um decorador (ver seção 11.12) ou use a combinação:

```xml
<Sequence>
    <Delay delay_msec="1000"/>   <!-- ERRADO: Delay precisa de um filho -->
</Sequence>
```

`<Delay>` é um **decorador**, não uma ação: precisa de exatamente um filho.

```xml
<Delay delay_msec="1000">
    <MinhaAcao/>
</Delay>
```

## 23.25 Emitir *log* de dentro de um nó

Não há infraestrutura de *log* na biblioteca. Use o que a sua aplicação já tem, e prefira
incluir `name()` e `registrationName()` na mensagem:

```cpp
BT::NodeStatus tick() override
{
    LOG_INFO("[%s/%s] iniciando", registrationName().c_str(), name().c_str());
    // ...
}
```

## 23.26 Descobrir, em tempo de execução, o que está registrado

```cpp
for (const auto& it : factory.manifests())
{
    std::cout << it.first << " (" << BT::toStr(it.second.type) << ")";
    if (factory.builtinNodes().count(it.first)) { std::cout << " [embutido]"; }
    std::cout << std::endl;
}
```

## 23.27 Gerar o `<TreeNodesModel>` para o Groot

```cpp
BT::BehaviorTreeFactory factory;
registrarTudo(factory);
std::cout << BT::writeTreeNodesModelXML(factory) << std::endl;
```

Redirecione para um arquivo e cole o `<TreeNodesModel>` gerado dentro do `<root>` do seu
XML.

## 23.28 Reagir a uma mudança de estado sem *logger*

```cpp
// assinatura direta num nó específico
auto sub = algum_no->subscribeToStatusChange(
    [](BT::TimePoint t, const BT::TreeNode& node, BT::NodeStatus prev, BT::NodeStatus cur)
    {
        std::cout << node.name() << ": " << prev << " -> " << cur << std::endl;
    });
// 'sub' precisa continuar vivo
```

Para achar o nó:

```cpp
BT::TreeNode* alvo = nullptr;
BT::applyRecursiveVisitor(tree.rootNode(), [&](BT::TreeNode* n) {
    if (n->name() == "meu_no") { alvo = n; }
});
```

---

# 24. BOAS PRÁTICAS

Reunidas dos exemplos, da documentação e das armadilhas do código.

## 24.1 Sobre a estrutura da árvore

1. **Condições à esquerda, trabalho à direita.** Numa `ReactiveSequence`, as guardas vêm
   primeiro. É o padrão que a biblioteca foi feita para expressar.
2. **Um único filho assíncrono por nó reativo.** Documentado, não verificado.
3. **Prefira `Sequence`/`Fallback` (com memória) por padrão.** Só use as variantes reativas
   quando precisar de reação a mudanças. A v3 inverteu o padrão da v2 exatamente por isso.
4. **Subárvores para reuso, não para organização visual.** Cada `<SubTree>` isolado custa
   um *blackboard* e um remapeamento a manter. Para agrupar visualmente, basta um
   `<Sequence name="...">`.
5. **Dê `name` às instâncias.** Sem `name`, o traço e o Groot mostram o ID, e duas
   instâncias do mesmo nó ficam indistinguíveis.
6. **Nomeie também os nós de controle.** `<Sequence name="preparar_garra">` é a diferença
   entre um traço legível e um ilegível.
7. **Não use `num_cycles="-1"`/`num_attempts="-1"` com filho síncrono.** Trava o processo.
8. **Sempre os dois limiares no `<Parallel>`.** `success_threshold` não tem padrão.
9. **`WhileDoElse` exige três filhos.** A mensagem de erro diz o contrário.
10. **`Switch<N>` exige N+1 filhos.** O último é o *default*.

## 24.2 Sobre os nós

11. **Escolha a classe base mais simples que resolva:** `SyncActionNode` →
    `StatefulActionNode` → `AsyncActionNode`.
12. **"Síncrono" significa "termina dentro do *tick*", não "rápido".** Se pode demorar mais
    que um período de *tick*, não é síncrono.
13. **Condições precisam ser baratas e sem efeito colateral.**
14. **`providedPorts()` precisa ser `public` e `static`.** Se for privado, é silenciosamente
    ignorado.
15. **Nós com portas precisam do construtor `(const std::string&, const NodeConfiguration&)`
    — e não devem ter o construtor de um argumento.** Ter os dois faz a fábrica descartar o
    *blackboard* quando o XML não fornece atributos.
16. **Leia portas no `tick()`, nunca no construtor.**
17. **Declare o tipo das portas** (`InputPort<T>`, não `InputPort`). É o que dá a
    verificação de consistência na carga.
18. **Sobrescreva `tick()`, não `executeTick()`.**
19. **Se o seu nó guarda estado e pode devolver `RUNNING`, sobrescreva `halt()` — e chame a
    implementação da base.**
20. **`halt()` deve ser idempotente.** `Tree::haltTree()` chama duas vezes.
21. **Num `AsyncActionNode`, consulte `isHaltRequested()` periodicamente** e chame
    `AsyncActionNode::halt()` na sua sobrescrita.
22. **Prefira devolver `FAILURE` a lançar**, exceto para erro de configuração.

## 24.3 Sobre dados

23. **O *blackboard* é para dados que fluem entre nós.** Ponteiros de infraestrutura vão
    pelo construtor.
24. **Toda chave que atravessa uma subárvore precisa de remapeamento explícito.**
25. **Nunca guarde ponto flutuante numa variável de `Switch`.** A comparação é textual.
26. **Especialize `convertFromString<T>` dentro do *namespace* `BT`, com `inline` se
    estiver em cabeçalho.**
27. **Se o `SetBlackboard` for o primeiro a tocar uma chave, ela vira `std::string`.**
    Declare o tipo com uma porta tipada antes, ou aceite a conversão a cada leitura.
28. **Não confie em `getKeys()` nem em `debugMessage()` em produção.** São ferramentas de
    depuração com defeitos conhecidos.

## 24.4 Sobre a aplicação

29. **O laço é seu — e precisa de pausa.**
30. **Ligue os *loggers* depois da árvore e antes do primeiro *tick*, e mantenha-os vivos.**
31. **Envolva o laço num `try/catch (std::exception&)`.**
32. **Passe `buffer_size = 0` ao `FileLogger` quando estiver caçando um travamento.**
33. **Um `StdCoutLogger`/`MinitraceLogger`/`PublisherZMQ` por processo.** E não chame
    `flush()` no `StdCoutLogger`, que desarma o guarda.
34. **Guardar um `TreeNode*` que sobreviva ao `Tree` é *dangling pointer*.**
35. **Construa a árvore uma vez.** O contador de `UID` é global e finito.

## 24.5 Sobre o *build*

36. **`-DBUILD_UNIT_TESTS=OFF` na primeira integração**, senão o GTest vira obrigatório.
37. **`-DBT_NO_COROUTINES` precisa chegar ao consumidor** se a biblioteca foi compilada sem
    Boost. O CMake **não** o propaga.
38. **Fixe as dependências opcionais**, com `CMAKE_DISABLE_FIND_PACKAGE_*` ou instalando
    todas — senão o binário muda conforme a máquina.
39. **Prefira a biblioteca compartilhada** quando houver *plugins*.
40. **Compile *plugins* com `BT_PLUGIN_EXPORT`.** Sem isso, o símbolo é `static` e o erro é
    silencioso.

## 24.6 Sobre o XML

41. **`main_tree_to_execute` sempre**, mesmo com uma árvore só.
42. **`ID` em toda `<BehaviorTree>`.**
43. **Cuidado com IDs de árvore duplicados entre arquivos incluídos** — vale o primeiro
    lido, sem aviso.
44. **`<include>` antes das `<BehaviorTree>` que os usam**, e sem ciclos.
45. **É `RetryUntilSuccesful`, com um `s` a menos.**
46. **É `<SubTree>` com `T` maiúsculo, e `<TreeNodesModel>` com `s`.**
47. **Use a forma explícita (`<Action ID="...">`) se pretende usar o Groot**, ou gere um
    `<TreeNodesModel>`.

---

# 25. ÍNDICE DE MENSAGENS DE ERRO

Todas as mensagens que a biblioteca pode produzir, com causa e correção. Organizadas pelo
texto que aparece no terminal.

## 25.1 Erros de carga do XML (`RuntimeError`)

### `The XML must have a root node called <root>`

**Onde:** `VerifyXML`, em `src/xml_parsing.cpp`.
**Causa:** o elemento de topo não é `<root>`.
**Correção:** envolva tudo em `<root>...</root>`.

### `Error parsing the XML: <XML_ERROR_...>`

**Onde:** `loadDocImpl` e `VerifyXML`.
**Causa:** XML malformado, ou arquivo não encontrado num `<include>`
(`XML_ERROR_FILE_NOT_FOUND`).
**Correção:** valide o XML; confira o caminho do `<include>` **relativo ao diretório do
arquivo que inclui**.
**ARMADILHA:** a mensagem **não diz qual arquivo** falhou.

### `The node <BehaviorTree> must have exactly 1 child`

**Causa:** uma `<BehaviorTree>` com zero ou dois filhos diretos.
**Correção:** envolva os filhos num nó de controle.

### `The node <Decorator> must have exactly 1 child`
### `The node <Action> must not have any child`
### `The node <Condition> must not have any child`
### `The node <Control> must have at least 1 child`
### `A Control node must have at least 1 child`
### `<SubTree> should not have any child`

**Causa:** aridade errada na **forma explícita** (ou em `Sequence`/`SequenceStar`/
`Fallback`).
**Correção:** ajuste os filhos.
**Nota:** a mesma aridade **não** é conferida na forma compacta de outros nós.

### `The node <Decorator> must have the attribute [ID]`
### `The node <Action> must have the attribute [ID]`
### `The node <Condition> must have the attribute [ID]`
### `The node <Control> must have the attribute [ID]`
### `The node <SubTree> must have the attribute [ID]`

**Causa:** forma explícita sem `ID`.
**Correção:** `<Action ID="MinhaAcao"/>`.

### `<remap> was deprecated`

**Causa:** um `<remap>` dentro de `<SubTree>`, sintaxe da v2.
**Correção:** use atributos no próprio `<SubTree>`:
`<SubTree ID="X" interna="externa"/>`.

### `Only a single node <TreeNodesModel> is supported`

**Causa:** dois blocos `<TreeNodesModel>` no mesmo arquivo.

### `Error at line N: -> Error at line %d: -> The attribute [ID] is mandatory`

**Causa:** um `<Action>`/`<Decorator>`/`<SubTree>`/`<Condition>`/`<Control>` sem `ID`
**quando existe um `<TreeNodesModel>` no arquivo**.
**ARMADILHA:** a duplicação do prefixo e o `%d` literal são um defeito da própria mensagem
(seção 13.3). A informação útil é o **primeiro** número de linha.

### `Node not recognized: <nome>`

**Causa:** a tag não é um ID registrado nem o ID de uma `<BehaviorTree>` conhecida.
**Correções possíveis, em ordem de frequência:**
1. erro de grafia — em especial `RetryUntilSuccessful` (o correto é `RetryUntilSuccesful`,
   com um `s`), `Subtree` (é `SubTree`), `FallbackStar` (removido na v3);
2. o nó não foi registrado antes de `createTreeFromFile()`;
3. um *plugin* não carregou (ver `"can't find symbol"` abaixo);
4. o nó depende de *ncurses*/ZMQ e a biblioteca foi compilada sem;
5. a `<BehaviorTree>` referenciada está num arquivo que **não** foi incluído, ou foi
   incluído **depois**.

### `<ID> is not a registered node, nor a Subtree`

**Onde:** `createNodeFromXML`.
**Causa:** a mesma do anterior, mas detectada na instanciação (não na validação) — ocorre
quando a validação passou porque o nome estava em `registered_nodes` de outra passagem.

### `Possible typo? In the XML, you tried to remap port "X" in node [ID / nome], but the manifest of this node does not contain a port with this name.`

**Causa:** um atributo que não corresponde a nenhuma porta declarada.
**Correções:**
1. erro de grafia no nome da porta;
2. `providedPorts()` está **privado** (silenciosamente ignorado);
3. você registrou o nó com `registerSimpleAction` sem passar a `PortsList`;
4. a porta existe na v4 mas não na v3.

### `The creation of the tree failed because the port [K] was initially created with type [A] and, later type [B] was used somewhere else.`

**Causa:** duas portas tipadas apontam para a mesma chave do *blackboard* com tipos
diferentes.
**Correção:** unifique o tipo, ou use chaves diferentes. A mensagem vem acompanhada de um
`debugMessage()` do *blackboard*.

### `The tree specified in [main_tree_to_execute] can't be found`

**Causa:** `main_tree_to_execute="X"` sem uma `<BehaviorTree ID="X">`.
**Correção:** confira a grafia; lembre que IDs duplicados fazem valer o **primeiro** lido.

### `If you don't specify the attribute [main_tree_to_execute], Your file must contain a single BehaviorTree`

**Causa:** duas ou mais `<BehaviorTree>` sem `main_tree_to_execute`.

### `[main_tree_to_execute] was not specified correctly`

**Onde:** `instantiateTree`.
**Causa:** mesma situação, detectada na instanciação.

### `XMLParser::instantiateTree needs a non-empty root_blackboard`

**Causa:** `createTreeFromFile(path, nullptr)` ou um `Blackboard::Ptr` vazio.
**Correção:** omita o argumento (o padrão é `Blackboard::create()`).

### `Using attribute [ros_pkg] in <include>, but this library was compiled without ROS support. Recompile the BehaviorTree.CPP using catkin`

**Causa:** `<include ros_pkg="...">` numa biblioteca sem `USING_ROS`.
**Correção:** use caminho relativo. **Sob ROS 2 esta mensagem é enganosa**: o `USING_ROS`
nunca é definido no caminho do ament, e recompilar com catkin não é opção.

## 25.2 Erros da fábrica

### `ID [X] already registered`  (`BehaviorTreeException`)

**Causa:** dois registros com o mesmo ID — frequentemente dois *plugins*, ou um registro
manual duplicando um embutido.
**Correção:** `unregisterBuilder(ID)` antes (não funciona para embutidos).

### `You can not remove the builtin registration ID [X]`  (`LogicError`)

**Causa:** `unregisterBuilder()` sobre um nó de fábrica.
**Correção:** não é possível redefinir nós embutidos; escolha outro ID.

### `BehaviorTreeFactory: ID [X] not registered`  (`RuntimeError`)

**Causa:** `instantiateTreeNode()` com um ID desconhecido.
**Nota útil:** logo antes de lançar, a fábrica imprime em `std::cerr` **a lista completa**
de IDs registrados.

### `ERROR loading library [X]: can't find symbol [BT_RegisterNodesFromPlugin]`

**Não é exceção** — é uma linha em `std::cout`, e `registerFromPlugin()` **retorna
normalmente**.
**Causa:** o *plugin* foi compilado sem `BT_PLUGIN_EXPORT`.
**Correção:** `target_compile_definitions(meu_plugin PRIVATE BT_PLUGIN_EXPORT)`.

### `Could not load library: <erro do dlerror>`  (`RuntimeError`)

**Causa:** caminho errado, arquitetura incompatível, ou dependência não resolvida do `.so`.
**Diagnóstico:** `ldd ./libmeu_plugin.so`.

### `Library already loaded: <path>`  (`RuntimeError`)

**Causa:** `SharedLibrary::load()` chamado duas vezes no mesmo objeto.

### `[SharedLibrary::getSymbol]: can't find symbol <nome>`  (`RuntimeError`)

**Causa:** símbolo ausente ou não exportado.

## 25.3 Erros de execução (durante o *tick*)

### `A child node must never return IDLE`  (`LogicError`)

**Causa:** um `tick()` de usuário devolveu `NodeStatus::IDLE`.
**Correção:** devolva `SUCCESS`, `FAILURE` ou `RUNNING`.
**ARMADILHA:** sob `ForceSuccess`, `ForceFailure` ou `KeepRunningUntilFailure`, este erro
**não é lançado** — vira `RUNNING` silencioso.

### `SyncActionNode MUST never return RUNNING`  (`LogicError`)

**Causa:** um `SyncActionNode` (ou `SimpleActionNode`) devolveu `RUNNING`.
**Correção:** mude a classe base para `StatefulActionNode`.

### `Missing parameter [X] in <Nó>`  (`RuntimeError`)

Variantes: `[success_threshold]`/`[failure_threshold]` em `ParallelNode`, `[num_cycles]` em
`RepeatNode`, `[num_attempts]` em `RetryNode`, `[msec]` em `TimeoutNode`, `[delay_msec]` em
`DelayNode`.
**Causa:** porta obrigatória **sem valor padrão** omitida no XML.
**Correção:** forneça o atributo. **Não é detectado na carga.**

### `Number of children is less than threshold. Can never succeed.` / `... Can never fail.`  (`LogicError`)

**Causa:** `<Parallel success_threshold="5">` com menos de 5 filhos.

### `Wrong number of children in SwitchNode; must be (num_cases + default)`  (`LogicError`)

**Causa:** `<Switch3>` sem exatamente 4 filhos.

### `IfThenElseNode must have either 2 or 3 children`  (`std::logic_error` **cru**)

### `WhileDoElse must have either 2 or 3 children`  (`std::logic_error` **cru**)

**ARMADILHA:** apesar da mensagem, `WhileDoElse` exige **exatamente 3**.

### `Something unexpected happened in IfThenElseNode`  (`std::logic_error` cru)

**Causa:** teoricamente inalcançável.

### `Decorator [nome] has already a child assigned`  (`BehaviorTreeException`)

**Causa:** `setChild()` chamado duas vezes — na prática, um decorador com dois filhos no
XML numa forma que escapou da validação.

### `One of the children of a DecoratorNode or ControlNode is nullptr`  (`LogicError`)

**Onde:** `applyRecursiveVisitor`.
**Causa:** árvore montada à mão com `addChild(nullptr)` ou decorador sem `setChild()`.

### `Empty Tree`  (`RuntimeError`)

**Causa:** `tickRoot()` num `Tree` sem nós (por exemplo, movido de).

### `AsyncActionNode2::onStart() must not return IDLE` / `...onRunning()...`  (`std::logic_error` cru)

**Causa:** `StatefulActionNode::onStart()`/`onRunning()` devolveu `IDLE`.
**ARMADILHA:** **não existe classe `AsyncActionNode2`** — o nome é resquício. A classe é
`StatefulActionNode`.

## 25.4 Erros de dados (*blackboard* e conversão)

### `Blackboard::set() failed: once declared, the type of a port shall not change. Declared type [A] != current type [B]`  (`LogicError`)

**Causa:** escrever numa entrada já tipada com um tipo incompatível — e sem que a conversão
a partir de texto resolva.
**Correção:** unifique os tipos das portas que usam a chave.

### `Blackboard::get() error. Missing key [K]`  (`RuntimeError`)

**Causa:** `blackboard->get<T>("K")` numa chave inexistente.
**Correção:** use a forma `bool get(key, dest)` se a ausência for aceitável.

### `getInput() failed because NodeConfiguration::input_ports does not contain the key: [K]`

**Devolvido em `Result::error()`, não lançado.**
**Causas:**
1. a porta não foi declarada em `providedPorts()`;
2. a porta foi declarada mas o XML não a forneceu **e** ela não tem valor padrão;
3. o nó foi construído pelo construtor de um argumento (fábrica descartou a configuração);
4. o nó foi construído em C++ sem `assignDefaultRemapping`.

### `getInput() failed because it was unable to find the key [K] remapped to [R]`

**Causa:** a chave existe na configuração, aponta para uma entrada do *blackboard*, e essa
entrada **está vazia ou não existe**.
**Causas típicas:** ninguém escreveu ainda; a chave não atravessou a fronteira de uma
subárvore; erro de grafia no remapeamento.

### `getInput() trying to access a Blackboard(BB) entry, but BB is invalid`

**Causa:** `config().blackboard` é nulo — o nó foi construído sem configuração.

### `setOutput() failed: NodeConfiguration::output_ports does not contain the key: [K]`

**Causa:** a porta não foi declarada como `OutputPort` (ou `BidirectionalPort`).

### `You didn't implement the template specialization of convertFromString for this type: T`  (`LogicError`)

Precedida de uma linha em `std::cerr`.
**Causa:** um tipo próprio usado numa porta sem especialização de `convertFromString`.
**Correção:** implemente-a **dentro do *namespace* `BT`**.

### `convertFromString(): invalid bool conversion`  (`RuntimeError`)

**Causa:** valor fora de `0`, `1`, `true`, `TRUE`, `True`, `false`, `FALSE`, `False`.

### `Cannot convert this to NodeStatus: X`  (`RuntimeError`)

**Causa:** `return_on_mismatch` com valor diferente de `IDLE`/`RUNNING`/`SUCCESS`/
`FAILURE` (maiúsculas).

### `Any::cast failed because it is empty`  (`std::runtime_error`)

**Causa:** leitura de um `Any` vazio.

### `[Any::convert]: no known safe conversion between [A] and [B]`

**Causa:** conversão impossível entre os tipos guardados e pedidos.

### `Value is negative and can't be converted to signed`  (`std::runtime_error`)

**Causa:** valor negativo lido numa porta `unsigned`.
**Nota:** a mensagem diz "signed" onde queria dizer "unsigned".

### `Value too large.` / `Value too small.` / `Floating point truncated`  (`std::runtime_error`)

**Causa:** conversão numérica que perderia informação (`convertNumber`).

## 25.5 Erros dos *loggers*

### `Only one instance of StdCoutLogger shall be created`  (`LogicError`)

**Causa:** dois `StdCoutLogger` vivos — **ou** um `MinitraceLogger`, que reaproveita a
mesma mensagem por engano.
**ARMADILHA:** chamar `StdCoutLogger::flush()` **desarma** o guarda; depois disso é
possível criar duas instâncias sem erro, e as transições saem duplicadas.

### `Only one instance of PublisherZMQ shall be created`  (`LogicError`)

### `The TCP ports of the publisher and the server must be different`  (`LogicError`)

**Causa:** `PublisherZMQ(tree, 25, 1666, 1666)`.

---

# 26. CATÁLOGO CONSOLIDADO DE ARMADILHAS

Todas as divergências entre expectativa e código, reunidas para varredura rápida. Cada
entrada aponta a seção que a detalha.

## 26.1 Nomes e grafias

| # | Armadilha | Onde |
|---|---|---|
| 1 | O ID registrado é **`RetryUntilSuccesful`** (um `s`), embora a classe diga `RetryUntilSuccessful` | 11.6 |
| 2 | O método é **`seTimestampType()`** (falta o `t` de "set") | 19.1 |
| 3 | `StatefulActionNode` lança mensagens citando **`AsyncActionNode2`**, classe inexistente | 12.6 |
| 4 | `MinitraceLogger` lança *"Only one instance of **StdCoutLogger**"* | 19.6 |
| 5 | Os `static_assert` falam em **`NodeParameters`**, tipo que não existe na v3 | 14.4 |
| 6 | A documentação usa `<Subtree>` e `<TreeNodeModel>`; o código exige `<SubTree>` e `<TreeNodesModel>` | 13.3 |
| 7 | `isHalted()` significa "está em `IDLE`", não "foi interrompido" | 5.4 |
| 8 | `Optional<T>` não é `std::optional` | 7.6 |
| 9 | `Parallel` não paraleliza (sem *threads*) | 10.8 |
| 10 | Os comentários `// RED` / `// GREEN` em `toStr(status, colored)` estão trocados | 4.4 |

## 26.2 Comentários desatualizados

| # | Armadilha | Onde |
|---|---|---|
| 11 | `sequence_node.h` cita um `reset_on_failure` que não existe | 10.2 |
| 12 | `WhileDoElse` diz "2 ou 3 filhos" e exige exatamente 3 | 10.10 |
| 13 | `showsTransitionToIdle()` é comentado como `false by default` e é `true` | 19.1 |
| 14 | O `main()` do `t04` fala em `SequenceStar` mas o XML usa `ReactiveSequence` | 22.4 |
| 15 | `SimpleConditionNode` diz não suportar *blackboards*, mas suporta | 12.3 |
| 16 | O `flags` de `SharedLibrary::load()` é documentado e ignorado | 20.5 |

## 26.3 Defeitos de código

| # | Armadilha | Onde |
|---|---|---|
| 17 | `StdCoutLogger::flush()` desarma o guarda de instância única | 19.2 |
| 18 | `SerializeTransition` escreve `int64_t` num *buffer* de 12 bytes; funciona só em *little-endian* | 19.4 |
| 19 | `buildSerializedStatusSnapshot` é declarada `const` e definida não-`const`: não linka | 6.6 |
| 20 | `SimpleString::operator=` copia o ponteiro: dupla liberação (latente) | 20.3 |
| 21 | `Blackboard::getKeys()` percorre o mapa sem *mutex* | 8.6 |
| 22 | `Blackboard::debugMessage()` pode desreferenciar `nullptr` em `Any::type()` | 8.6, 20.1 |
| 23 | `setPortInfo` compara `type_info` por **ponteiro**; `set` compara por objeto | 8.5 |
| 24 | `AsyncActionNode::executeTick()` chama `wait()` num *future* possivelmente inválido | 12.7 |
| 25 | `exptr_` é lido/escrito entre *threads* sem sincronização | 12.7 |
| 26 | `AsyncActionNode::halt_requested_` não é inicializado no construtor | 12.7 |
| 27 | `TimeoutNode` faz `unlock()`/`lock()` manual sob um `unique_lock` | 11.7 |
| 28 | `DelayNode::halt()` não repõe `delay_started_`: falha espúria no *tick* seguinte | 11.8 |
| 29 | A mensagem do `<TreeNodesModel>` duplica o prefixo e imprime `%d` literal | 13.3 |
| 30 | O contador de `UID` é global, não atômico e estoura em 65 535 | 5.7 |
| 31 | O `Signal` não é seguro sob concorrência | 5.6 |
| 32 | `ReactiveSequence::running_count` é código morto | 10.4 |
| 33 | `MinitraceLogger::prev_time_` é membro morto | 19.6 |

## 26.4 Comportamentos surpreendentes

| # | Armadilha | Onde |
|---|---|---|
| 34 | `providedPorts()` privado → manifesto vazio, **sem erro** | 7.4 |
| 35 | `SubTree`/`SubTreePlus` têm manifesto vazio por causa do item 34 | 7.4, 27 |
| 36 | Nó com **os dois** construtores perde o *blackboard* se o XML não der atributos | 14.5 |
| 37 | Valor padrão de porta só é aplicado pelo caminho do XML | 7.9 |
| 38 | Porta de saída **nunca** recebe valor padrão | 7.9 |
| 39 | `success_threshold` do `Parallel` é obrigatório; `failure_threshold` tem padrão 1 | 10.8 |
| 40 | `num_cycles="-1"` com filho síncrono trava o processo | 11.6 |
| 41 | `msec="0"` **desliga** o `Timeout` | 11.7 |
| 42 | `Timeout` não interrompe ação síncrona | 11.7 |
| 43 | `ForceSuccess`/`ForceFailure`/`KeepRunningUntilFailure` engolem o erro de `IDLE` | 11.3 |
| 44 | O `Switch` compara **texto**: `double` nunca casa | 10.11 |
| 45 | O `Switch` cai no *default* em silêncio se `variable` não puder ser lida | 10.11 |
| 46 | O atributo `name` de um `<SubTree>` é ignorado | 16.2 |
| 47 | `__autoremap` só remapeia chaves que já existem na carga | 16.2 |
| 48 | IDs de `<BehaviorTree>` duplicados: vale o primeiro lido, sem aviso | 15.6 |
| 49 | `<include>` não detecta ciclos | 13.5 |
| 50 | Arquivos incluídos são validados sozinhos, na ordem de leitura | 13.5 |
| 51 | `loadFromText()` resolve `<include>` a partir do diretório de trabalho | 13.5 |
| 52 | A memória da `SequenceStar` se perde num `halt()` | 10.3 |
| 53 | `StatefulActionNode` em `SUCCESS` não reinicia, nem sob nó reativo | 10.4, 12.6 |
| 54 | Nós reativos **não** chamam `setStatus(RUNNING)` no início (ordem no traço) | 10.1 |
| 55 | `SimpleActionNode` passa por `RUNNING` no traço; `SyncActionNode` derivado, não | 12.5 |
| 56 | `haltTree()` chama `halt()` duas vezes em cada nó | 18 |
| 57 | Não é possível redefinir um nó embutido | 14.6 |
| 58 | Um `<Delay>` sob nó reativo falha uma vez a cada interrupção | 11.8 |

## 26.5 Concorrência e recursos

| # | Armadilha | Onde |
|---|---|---|
| 59 | Cada `Timeout`/`Delay` cria **uma *thread*** na carga | 11.9, 20.4 |
| 60 | `AsyncActionNode` cria **uma *thread* por execução** | 12.7 |
| 61 | O exemplo oficial de `AsyncActionNode::halt()` **não** chama a base | 12.7 |
| 62 | `setlocale` em `convertFromString<double>` é global ao processo | 9.2 |
| 63 | `MinitraceLogger` instala um tratador de `SIGINT` | 19.6 |
| 64 | `PublisherZMQ` guarda referência à `Tree`: ordem de destruição importa | 19.7 |
| 65 | `StringView` não terminada em `NUL` passada a `std::stoi`/`stod` | 9.2 |

## 26.6 *Build* e empacotamento

| # | Armadilha | Onde |
|---|---|---|
| 66 | `BUILD_UNIT_TESTS=ON` por padrão torna GTest obrigatório | 21.2 |
| 67 | `BT_NO_COROUTINES` não é exportado pelo CMake | 21.6 |
| 68 | `BUILD_SHARED_LIBS` é ignorado no Windows | 21.5 |
| 69 | Fora de UNIX/WIN32, nenhum `add_library` é executado | 21.5 |
| 70 | `cmake_minimum_required` de 2.8/3.5.1 quebra no CMake 4 | 21.7 |
| 71 | O mesmo fonte produz binários diferentes conforme a máquina | 21.3 |
| 72 | `ros_pkg` e `registerFromROSPlugins()` não funcionam no ROS 2 | 21.8 |
| 73 | `make create` do *fork* usa Debug por padrão | 21.10 |
| 74 | As licenças dos terceiros embutidos não acompanham o pacote | 1.6, 20.7 |
| 75 | Esquecer `BT_PLUGIN_EXPORT` produz erro silencioso | 17.4 |

---

# 27. CATÁLOGO DOS NÓS EMBUTIDOS

Extraído em tempo de execução de `factory.manifests()`, com uma fábrica recém-construída e
sem *ncurses*: **29 IDs**.

## 27.1 Nós de controle

| ID | Porta | Tipo | Padrão |
|---|---|---|---|
| `Sequence` | — | — | — |
| `SequenceStar` | — | — | — |
| `ReactiveSequence` | — | — | — |
| `Fallback` | — | — | — |
| `ReactiveFallback` | — | — | — |
| `IfThenElse` | — | — | — |
| `WhileDoElse` | — | — | — |
| `Parallel` | `success_threshold` | `unsigned` | **nenhum** |
| `Parallel` | `failure_threshold` | `unsigned` | `1` |
| `Switch2` | `variable`, `case_1`..`case_2` | `std::string` | nenhum |
| `Switch3` | `variable`, `case_1`..`case_3` | `std::string` | nenhum |
| `Switch4` | `variable`, `case_1`..`case_4` | `std::string` | nenhum |
| `Switch5` | `variable`, `case_1`..`case_5` | `std::string` | nenhum |
| `Switch6` | `variable`, `case_1`..`case_6` | `std::string` | nenhum |
| `ManualSelector`† | `repeat_last_selection` | `bool` | `false` |

† registrado apenas com *ncurses*.

**Aridades:** `Switch`$N$ exige $N+1$ filhos; `IfThenElse` aceita 2 ou 3; `WhileDoElse`
exige exatamente 3.

## 27.2 Decoradores

| ID | Porta | Tipo | Padrão |
|---|---|---|---|
| `Inverter` | — | — | — |
| `ForceSuccess` | — | — | — |
| `ForceFailure` | — | — | — |
| `KeepRunningUntilFailure` | — | — | — |
| `Repeat` | `num_cycles` | `int` | **nenhum** |
| `RetryUntilSuccesful` | `num_attempts` | `int` | **nenhum** |
| `Timeout` | `msec` | `unsigned` | **nenhum** |
| `Delay` | `delay_msec` | `unsigned` | **nenhum** |
| `BlackboardCheckInt` | `value_A`, `value_B` | (sem tipo) | nenhum |
| `BlackboardCheckInt` | `return_on_mismatch` | `BT::NodeStatus` | nenhum |
| `BlackboardCheckDouble` | idênticas às de `BlackboardCheckInt` | | |
| `BlackboardCheckString` | idênticas às de `BlackboardCheckInt` | | |

## 27.3 Subárvores

| ID | Manifesto | Atributos lidos pelo *parser* |
|---|---|---|
| `SubTree` | **vazio** | `ID`, `__shared_blackboard`, e todo atributo como remapeamento |
| `SubTreePlus` | **vazio** | `ID`, `__autoremap`, e todo atributo como remapeamento ou literal |

O manifesto é vazio porque `providedPorts()` está `private` nas duas classes.

## 27.4 Ações

| ID | Porta | Direção | Tipo |
|---|---|---|---|
| `AlwaysSuccess` | — | — | — |
| `AlwaysFailure` | — | — | — |
| `SetBlackboard` | `value` | entrada | (sem tipo) |
| `SetBlackboard` | `output_key` | **bidirecional** | (sem tipo) |

## 27.5 Resumo por tipo

| `NodeType` | Quantidade | Quais |
|---|---:|---|
| `Control` | 13 | 7 sem portas, `Parallel`, `Switch2`..`Switch6` |
| `Decorator` | 11 | 4 sem portas, 4 de parâmetro, 3 de *blackboard* |
| `SubTree` | 2 | `SubTree`, `SubTreePlus` |
| `Action` | 3 | `AlwaysSuccess`, `AlwaysFailure`, `SetBlackboard` |
| `Condition` | **0** | nenhuma condição vem de fábrica |
| **total** | **29** | |

## 27.6 IDs que não são o nome da classe

| Classe C++ | ID no XML | Observação |
|---|---|---|
| `RetryNode` | `RetryUntilSuccesful` | **grafia com um `s` a menos** |
| `SequenceStarNode` | `SequenceStar` | sufixo `Node` removido |
| `SwitchNode<2>`..`<6>` | `Switch2`..`Switch6` | instanciações do *template* |
| `BlackboardPreconditionNode<int>` | `BlackboardCheckInt` | nome inteiramente diferente |
| `BlackboardPreconditionNode<double>` | `BlackboardCheckDouble` | idem |
| `BlackboardPreconditionNode<std::string>` | `BlackboardCheckString` | idem |
| `TimeoutNode<>` | `Timeout` | *template* com parâmetros padrão |
| `SubtreeNode` | `SubTree` | `T` maiúsculo no XML |
| `SubtreePlusNode` | `SubTreePlus` | idem |
| `ManualSelectorNode` | `ManualSelector` | sufixo removido |
| `InverterNode` | `Inverter` | sufixo removido |
| `RepeatNode` | `Repeat` | sufixo removido |
| `DelayNode` | `Delay` | sufixo removido |
| `FallbackNode` | `Fallback` | sufixo removido |
| `SequenceNode` | `Sequence` | sufixo removido |
| `ParallelNode` | `Parallel` | sufixo removido |
| `IfThenElseNode` | `IfThenElse` | sufixo removido |
| `WhileDoElseNode` | `WhileDoElse` | sufixo removido |
| `ForceSuccessNode` | `ForceSuccess` | sufixo removido |
| `ForceFailureNode` | `ForceFailure` | sufixo removido |
| `KeepRunningUntilFailureNode` | `KeepRunningUntilFailure` | sufixo removido |
| `AlwaysSuccessNode` | `AlwaysSuccess` | sufixo removido |
| `AlwaysFailureNode` | `AlwaysFailure` | sufixo removido |
| `SetBlackboard` | `SetBlackboard` | igual |
| `ReactiveSequence` | `ReactiveSequence` | igual |
| `ReactiveFallback` | `ReactiveFallback` | igual |

## 27.7 O programa que extrai o catálogo

```cpp
#include "behaviortree_cpp_v3/bt_factory.h"
#include <map>
#include <iostream>

int main()
{
    BT::BehaviorTreeFactory factory;
    // std::map (e não unordered_map) só para a saída sair ordenada
    std::map<std::string, BT::TreeNodeManifest> ordenado;
    for (const auto& it : factory.manifests()) ordenado.insert(it);

    std::cout << "total de IDs registrados: " << ordenado.size() << "\n\n";
    for (const auto& it : ordenado)
    {
        const auto& m = it.second;
        std::cout << m.registration_ID << "  [" << BT::toStr(m.type) << "]"
                  << "  portas=" << m.ports.size() << "\n";

        std::map<std::string, BT::PortInfo> portas;
        for (const auto& p : m.ports) portas.insert(p);
        for (const auto& p : portas)
        {
            std::cout << "    - " << p.first
                      << "  dir="  << BT::toStr(p.second.direction())
                      << "  tipo=" << (p.second.type()
                                       ? BT::demangle(p.second.type())
                                       : std::string("(nenhum)"))
                      << "  default=" << (p.second.defaultValue().empty()
                                          ? std::string("(nenhum)")
                                          : p.second.defaultValue())
                      << "\n";
        }
    }
    return 0;
}
```

Saída de referência (trecho):

```
total de IDs registrados: 29

AlwaysFailure  [Action]  portas=0
AlwaysSuccess  [Action]  portas=0
BlackboardCheckDouble  [Decorator]  portas=3
    - return_on_mismatch  dir=Input  tipo=BT::NodeStatus  default=(nenhum)
    - value_A  dir=Input  tipo=(nenhum)  default=(nenhum)
    - value_B  dir=Input  tipo=(nenhum)  default=(nenhum)
...
Parallel  [Control]  portas=2
    - failure_threshold  dir=Input  tipo=unsigned int  default=1
    - success_threshold  dir=Input  tipo=unsigned int  default=(nenhum)
...
RetryUntilSuccesful  [Decorator]  portas=1
    - num_attempts  dir=Input  tipo=int  default=(nenhum)
...
SubTree  [SubTree]  portas=0
SubTreePlus  [SubTree]  portas=0
```

Para auditar um *plugin*, acrescente `factory.registerFromPlugin("./meu_plugin.so")` antes
do laço — ou use a ferramenta `bt3_plugin_manifest`, que faz isso e ainda subtrai os nós
embutidos.

---

# 28. MIGRAÇÃO E COMPATIBILIDADE

## 28.1 v2 → v3: a renomeação dos nós de controle

Do `docs/MigrationGuide.md`:

| Nome na v2 | Nome na v3 | É reativo? |
|---|---|---|
| `Sequence` | `ReactiveSequence` | sim |
| `SequenceStar (reset_on_failure=true)` | `Sequence` | não |
| `SequenceStar (reset_on_failure=false)` | `SequenceStar` | não |
| `Fallback` | `ReactiveFallback` | sim |
| `FallbackStar` | `Fallback` | não |
| `Parallel` | `Parallel` | sim (v2) / não (v3) |

> *"A reactive `ParallelNode` was very confusing and error prone; in most cases, what you
> really want is you want to use a `ReactiveSequence` instead."*

> *"In version `2.x` it was unclear what would happen if a 'reactive' node has more than a
> single asynchronous child. The new recommendation is: **Reactive nodes should NOT have
> more than a single asynchronous child**. This is a very opinionated decision and for this
> reason it is documented but not enforced by the implementation."*

**A incompatibilidade perigosa:** os nomes `Sequence` e `Fallback` **continuam válidos** e
**mudaram de semântica**. Um XML da v2 carrega e roda com comportamento diferente. Já
`FallbackStar` desapareceu, e dá `"Node not recognized"` — a falha benigna.

Outras mudanças v2 → v3:

- `NodeParameters` virou `NodeConfiguration` (o nome antigo sobrevive em duas mensagens de
  `static_assert`);
- o `<remap>` dentro de `<SubTree>` foi removido (`"<remap> was deprecated"`);
- o *namespace* de instalação passou a ser `behaviortree_cpp_v3/`.

## 28.2 v3 → v4: o que muda (para reconhecer material da v4)

**Nada neste documento vale para a v4.** Os sinais de que um material é da v4:

| Sinal na v4 | Equivalente na v3 |
|---|---|
| `BT::NodeConfig` | `BT::NodeConfiguration` |
| `tree.tickOnce()`, `tree.tickWhileRunning()`, `tree.tickExactlyOnce()` | `tree.tickRoot()` |
| `<SubTree>` aceitando literais e `_autoremap` | `<SubTreePlus>` com `__autoremap` |
| `RetryUntilSuccessful` (grafia correta) | `RetryUntilSuccesful` |
| `Script`, `Precondition`, `LoopDouble`, `Sleep` | não existem |
| *scripting* embutido (`_successIf`, `_while`) | não existe |
| `SUBTREE` removido de `NodeType` | existe |
| `BT::CoroActionNode` removido | existe (com Boost) |
| `include <behaviortree_cpp/...>` (sem `_v3`) | `behaviortree_cpp_v3/` |
| `enableAllPortsRemapping` | não existe |

**Regra prática para reconhecer:** se o cabeçalho incluído for
`behaviortree_cpp/bt_factory.h` (**sem `_v3`**), é v4.

## 28.3 Compatibilidade de plataforma

| Plataforma | Situação |
|---|---|
| Linux (GCC/Clang) | caminho principal, testado |
| macOS | suportado (`shared_library_UNIX.cpp`, sufixo `.dylib`) |
| Windows / MSVC | suportado, **sempre biblioteca estática**; `/W3 /WX` |
| Outras (nem UNIX nem WIN32) | **não compila** (nenhum `add_library`) |

---

# 29. APLICAÇÃO COMPLETA DE REFERÊNCIA

Esta seção traz um programa **completo, compilado e executado** contra a
BehaviorTree.CPP 3.5.6. Toda a saída transcrita adiante foi produzida por ele. É uma
versão elaborada do exemplo `t05_crossdoor` do repositório, com as três correções que as
seções anteriores recomendaram:

- o estado do mundo **não é global** — é injetado por *builder* (23.16);
- a ação longa **não bloqueia** — é um `StatefulActionNode` (12.6);
- o resultado **atravessa a fronteira da subárvore por remapeamento de porta**, e não por
  variável estática (16.1).

O cenário: *um robô precisa atravessar uma porta. Se ela já estiver aberta, ele passa. Se
não, tenta destrancá-la — a fechadura emperra e só cede na terceira tentativa — e então
passa. Se nada disso funcionar, passa pela janela. Ao final, fecha a porta.*

## 29.1 A árvore (XML)

```xml
<root main_tree_to_execute="MainTree">

    <BehaviorTree ID="PortaFechada">
        <Sequence name="destrancar_e_passar">
            <Inverter>
                <EstaAberta/>
            </Inverter>
            <RetryUntilSuccesful num_attempts="4">
                <Sequence name="uma_tentativa">
                    <Timeout msec="2000">
                        <Destrancar duracao_ms="200"/>
                    </Timeout>
                    <Abrir/>
                </Sequence>
            </RetryUntilSuccesful>
            <Atravessar via="porta" rota="{rota}"/>
        </Sequence>
    </BehaviorTree>

    <BehaviorTree ID="MainTree">
        <Sequence name="missao">
            <Aproximar goal="1.5;2.0;0" duracao_ms="300"/>
            <Fallback name="estrategias">
                <Sequence name="ja_estava_aberta">
                    <EstaAberta/>
                    <Atravessar via="porta" rota="{rota_final}"/>
                </Sequence>
                <SubTree ID="PortaFechada" rota="rota_final"/>
                <Atravessar via="janela" rota="{rota_final}"/>
            </Fallback>
            <Falar message="{rota_final}"/>
            <Fechar/>
        </Sequence>
    </BehaviorTree>

</root>
```

Três pontos a ler com o vocabulário das seções anteriores:

- `<RetryUntilSuccesful>` está com a grafia da biblioteca, **com um `s` a menos** — é
  obrigatório (11.6);
- `<SubTree ID="PortaFechada" rota="rota_final"/>` — o atributo liga a chave **interna**
  `rota` à chave **externa** `rota_final`, e por isso **não leva chaves** (16.1);
- dentro da subárvore, `rota="{rota}"` **leva** chaves: ali é um ponteiro para o
  *blackboard* local (7.7).

## 29.2 O tipo próprio e a sua conversão

```cpp
struct Pose2D { double x, y, theta; };

namespace BT
{
template <> inline Pose2D convertFromString(StringView str)
{
    auto partes = splitString(str, ';');
    if (partes.size() != 3) {
        throw RuntimeError("Pose2D espera tres numeros separados por ';'");
    }
    Pose2D p;
    p.x     = convertFromString<double>(partes[0]);
    p.y     = convertFromString<double>(partes[1]);
    p.theta = convertFromString<double>(partes[2]);
    return p;
}
}   // namespace BT
```

## 29.3 Uma ação assíncrona sem *thread*

```cpp
class Aproximar : public BT::StatefulActionNode
{
  public:
    Aproximar(const std::string& name, const BT::NodeConfiguration& config)
      : BT::StatefulActionNode(name, config) {}

    static BT::PortsList providedPorts()
    {
        return { BT::InputPort<Pose2D>("goal", "destino, no formato x;y;theta"),
                 BT::InputPort<unsigned>("duracao_ms", 300, "quanto o trajeto demora") };
    }

    BT::NodeStatus onStart() override
    {
        Pose2D goal;
        if (!getInput("goal", goal)) {
            throw BT::RuntimeError("[", name(), "] faltou a porta [goal]");
        }
        unsigned dur = 300;
        getInput("duracao_ms", dur);

        fim_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(dur);
        std::cout << "  [Aproximar] indo para " << goal.x << ";" << goal.y
                  << ";" << goal.theta << std::endl;
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override
    {
        if (std::chrono::steady_clock::now() < fim_) {
            return BT::NodeStatus::RUNNING;
        }
        std::cout << "  [Aproximar] cheguei" << std::endl;
        return BT::NodeStatus::SUCCESS;
    }

    void onHalted() override
    {
        std::cout << "  [Aproximar] cancelado" << std::endl;
    }

  private:
    std::chrono::steady_clock::time_point fim_;
};
```

## 29.4 Nós que precisam do "mundo"

```cpp
struct Porta
{
    bool aberta = false;
    bool trancada = true;
    int  tentativas = 0;      // a fechadura so cede na terceira tentativa
};

class Destrancar : public BT::StatefulActionNode
{
  public:
    Destrancar(const std::string& name, const BT::NodeConfiguration& config, Porta* porta)
      : BT::StatefulActionNode(name, config), porta_(porta) {}

    static BT::PortsList providedPorts()
    {
        return { BT::InputPort<unsigned>("duracao_ms", 200, "quanto a tentativa demora") };
    }

    BT::NodeStatus onStart() override
    {
        unsigned dur = 200;
        getInput("duracao_ms", dur);
        fim_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(dur);
        return BT::NodeStatus::RUNNING;
    }

    BT::NodeStatus onRunning() override
    {
        if (std::chrono::steady_clock::now() < fim_) {
            return BT::NodeStatus::RUNNING;
        }
        porta_->tentativas++;
        if (porta_->tentativas < 3) {
            std::cout << "  [Destrancar] emperrou (tentativa "
                      << porta_->tentativas << ")" << std::endl;
            return BT::NodeStatus::FAILURE;
        }
        porta_->trancada = false;
        std::cout << "  [Destrancar] destrancada na tentativa "
                  << porta_->tentativas << std::endl;
        return BT::NodeStatus::SUCCESS;
    }

    void onHalted() override {}

  private:
    Porta* porta_;
    std::chrono::steady_clock::time_point fim_;
};

class Abrir : public BT::SyncActionNode
{
  public:
    Abrir(const std::string& name, const BT::NodeConfiguration& config, Porta* porta)
      : BT::SyncActionNode(name, config), porta_(porta) {}

    static BT::PortsList providedPorts() { return {}; }

    BT::NodeStatus tick() override
    {
        if (porta_->trancada) {
            std::cout << "  [Abrir] esta trancada" << std::endl;
            return BT::NodeStatus::FAILURE;
        }
        porta_->aberta = true;
        std::cout << "  [Abrir] aberta" << std::endl;
        return BT::NodeStatus::SUCCESS;
    }

  private:
    Porta* porta_;
};
```

## 29.5 Uma ação com entrada e saída

```cpp
class Atravessar : public BT::SyncActionNode
{
  public:
    Atravessar(const std::string& name, const BT::NodeConfiguration& config)
      : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts()
    {
        return { BT::InputPort<std::string>("via",  "porta ou janela"),
                 BT::OutputPort<std::string>("rota", "por onde acabei passando") };
    }

    BT::NodeStatus tick() override
    {
        std::string via;
        if (!getInput("via", via)) {
            throw BT::RuntimeError("[", name(), "] faltou a porta [via]");
        }
        setOutput("rota", "atravessei pela " + via);
        return BT::NodeStatus::SUCCESS;
    }
};

class Falar : public BT::SyncActionNode
{
  public:
    Falar(const std::string& name, const BT::NodeConfiguration& config)
      : BT::SyncActionNode(name, config) {}

    static BT::PortsList providedPorts()
    {
        return { BT::InputPort<std::string>("message") };
    }

    BT::NodeStatus tick() override
    {
        auto msg = getInput<std::string>("message");
        if (!msg) {
            throw BT::RuntimeError("[", name(), "] ", msg.error());
        }
        std::cout << "  [Falar] " << msg.value() << std::endl;
        return BT::NodeStatus::SUCCESS;
    }
};
```

## 29.6 O `main()`

```cpp
#include <chrono>
#include <iostream>
#include <thread>

#include "behaviortree_cpp_v3/bt_factory.h"
#include "behaviortree_cpp_v3/loggers/bt_cout_logger.h"
#include "behaviortree_cpp_v3/loggers/bt_file_logger.h"

int main()
{
    Porta porta;
    BT::BehaviorTreeFactory factory;

    // 1a. condicao e acao triviais, sem escrever classe
    factory.registerSimpleCondition("EstaAberta", [&porta](BT::TreeNode&) {
        return porta.aberta ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });
    factory.registerSimpleAction("Fechar", [&porta](BT::TreeNode&) {
        porta.aberta = false;
        std::cout << "  [Fechar] porta fechada" << std::endl;
        return BT::NodeStatus::SUCCESS;
    });

    // 1b. nos com portas, registrados por tipo
    factory.registerNodeType<Aproximar>("Aproximar");
    factory.registerNodeType<Atravessar>("Atravessar");
    factory.registerNodeType<Falar>("Falar");

    // 1c. nos que precisam do mundo: builder proprio
    factory.registerBuilder<Destrancar>("Destrancar",
        [&porta](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<Destrancar>(name, config, &porta);
        });
    factory.registerBuilder<Abrir>("Abrir",
        [&porta](const std::string& name, const BT::NodeConfiguration& config) {
            return std::make_unique<Abrir>(name, config, &porta);
        });

    // 2. carga
    auto tree = factory.createTreeFromText(xml_text);
    BT::printTreeRecursively(tree.rootNode());

    // 3. observadores: DEPOIS da arvore, ANTES do primeiro tick
    BT::StdCoutLogger logger_terminal(tree);
    logger_terminal.enableTransitionToIdle(false);
    BT::FileLogger logger_arquivo(tree, "porta_trace.fbl", 0);

    // 4. o laco
    BT::NodeStatus status = BT::NodeStatus::RUNNING;
    while (status == BT::NodeStatus::RUNNING)
    {
        status = tree.tickRoot();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "\nresultado: " << status << std::endl;

    std::cout << "\n--- blackboard raiz ---" << std::endl;
    tree.blackboard_stack[0]->debugMessage();
    std::cout << "--- blackboard da subarvore ---" << std::endl;
    tree.blackboard_stack[1]->debugMessage();

    return 0;
}
```

## 29.7 Compilando

```bash
REPO=/caminho/para/BehaviorTree.CPP
g++ -std=c++14 -DBT_NO_COROUTINES \
    -I$REPO/include -I$REPO/3rdparty \
    porta.cpp -o porta \
    -L$REPO/build/Debug/lib -lbehaviortree_cpp_v3 -lpthread -ldl

LD_LIBRARY_PATH=$REPO/build/Debug/lib ./porta
```

Com CMake e Conan:

```cmake
cmake_minimum_required(VERSION 3.16)
project(porta CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(behaviortree.cpp.asa REQUIRED)
add_executable(porta porta.cpp)
target_link_libraries(porta PRIVATE behaviortree.cpp.asa::behaviortree.cpp.asa)
```

## 29.8 A saída: a árvore construída

```
----------------
missao
   Aproximar
   estrategias
      ja_estava_aberta
         EstaAberta
         Atravessar
      PortaFechada
         destrancar_e_passar
            Inverter
               EstaAberta
            RetryUntilSuccesful
               uma_tentativa
                  Timeout
                     Destrancar
                  Abrir
            Atravessar
      Atravessar
   Falar
   Fechar
----------------
```

Note que a subárvore aparece com o **nome do seu `ID`**, não com um `name` próprio (16.2).

## 29.9 A saída: o traço de execução

Carimbos abreviados, coloração removida, transições para `IDLE` suprimidas:

```
[..8.721]: missao                    IDLE    -> RUNNING
  [Aproximar] indo para 1.5;2;0
[..8.722]: Aproximar                 IDLE    -> RUNNING
  [Aproximar] cheguei
[..9.028]: Aproximar                 RUNNING -> SUCCESS
[..9.028]: estrategias               IDLE    -> RUNNING
[..9.028]: ja_estava_aberta          IDLE    -> RUNNING
[..9.028]: EstaAberta                IDLE    -> FAILURE
[..9.028]: ja_estava_aberta          RUNNING -> FAILURE
[..9.028]: PortaFechada              IDLE    -> RUNNING
[..9.028]: destrancar_e_passar       IDLE    -> RUNNING
[..9.028]: Inverter                  IDLE    -> RUNNING
[..9.028]: EstaAberta                IDLE    -> FAILURE
[..9.028]: Inverter                  RUNNING -> SUCCESS
[..9.028]: RetryUntilSuccesful       IDLE    -> RUNNING
[..9.028]: uma_tentativa             IDLE    -> RUNNING
[..9.028]: Timeout                   IDLE    -> RUNNING
[..9.028]: Destrancar                IDLE    -> RUNNING
  [Destrancar] emperrou (tentativa 1)
[..9.233]: Destrancar                RUNNING -> FAILURE
[..9.233]: Timeout                   RUNNING -> FAILURE
[..9.233]: uma_tentativa             RUNNING -> FAILURE
[..9.233]: uma_tentativa             IDLE    -> RUNNING
[..9.233]: Timeout                   IDLE    -> RUNNING
[..9.233]: Destrancar                IDLE    -> RUNNING
  [Destrancar] emperrou (tentativa 2)
[..9.439]: Destrancar                RUNNING -> FAILURE
[..9.439]: Timeout                   RUNNING -> FAILURE
[..9.439]: uma_tentativa             RUNNING -> FAILURE
[..9.439]: uma_tentativa             IDLE    -> RUNNING
[..9.439]: Timeout                   IDLE    -> RUNNING
[..9.439]: Destrancar                IDLE    -> RUNNING
  [Destrancar] destrancada na tentativa 3
[..9.644]: Destrancar                RUNNING -> SUCCESS
[..9.644]: Timeout                   RUNNING -> SUCCESS
  [Abrir] aberta
[..9.644]: Abrir                     IDLE    -> SUCCESS
[..9.644]: uma_tentativa             RUNNING -> SUCCESS
[..9.644]: RetryUntilSuccesful       RUNNING -> SUCCESS
[..9.644]: Atravessar                IDLE    -> SUCCESS
[..9.644]: destrancar_e_passar       RUNNING -> SUCCESS
[..9.644]: PortaFechada              RUNNING -> SUCCESS
[..9.644]: estrategias               RUNNING -> SUCCESS
  [Falar] atravessei pela porta
[..9.644]: Falar                     IDLE    -> SUCCESS
[..9.644]: Fechar                    IDLE    -> RUNNING
  [Fechar] porta fechada
[..9.644]: Fechar                    RUNNING -> SUCCESS
[..9.644]: missao                    RUNNING -> SUCCESS

resultado: SUCCESS

--- blackboard raiz ---
rota_final (std::string) -> full
--- blackboard da subarvore ---
rota (std::string) -> remapped to parent [rota_final]
```

## 29.10 Sete coisas para reparar nesse traço

O traço é a melhor revisão possível de todo este documento.

**1. Por que `Fechar` passa por `RUNNING` e `Abrir` não?**
Porque `Fechar` foi registrado com `registerSimpleAction()`, e `SimpleActionNode::tick()`
chama `setStatus(RUNNING)` antes de invocar o *functor* (12.5). `Abrir` é uma classe
derivada de `SyncActionNode`, cujo `tick()` vai direto ao resultado. Os dois são igualmente
síncronos; a diferença é só de **instrumentação**.

**2. Por que `EstaAberta` aparece duas vezes?**
São duas **instâncias distintas** do mesmo ID — uma na `Sequence` `ja_estava_aberta`, outra
sob o `Inverter` da subárvore. O `StdCoutLogger` imprime o `name()`, não o `UID()` (5.7), e
como nenhuma das duas recebeu atributo `name`, as duas herdaram o ID (5.3).

**3. Por que `uma_tentativa` vai de `FAILURE` para `IDLE` e de volta a `RUNNING` no mesmo
milissegundo?**
É o `haltChild()` do `RetryNode` entre tentativas (11.6): ele devolve o filho a `IDLE` para
que possa ser executado de novo. A transição **para** `IDLE` está suprimida na saída, mas a
volta a `RUNNING` denuncia que ela ocorreu.

**4. Por que as três tentativas levam 200 ms cada e não bloqueiam?**
Porque `Destrancar` é um `StatefulActionNode`: cada `onRunning()` custa uma comparação de
relógio, e o tempo passa **entre** *ticks*, no *sleep* do laço do `main()`. Com um
`SyncActionNode` que dormisse 200 ms, o traço seria idêntico — mas a árvore inteira ficaria
parada, e o `<Timeout msec="2000">` não teria como interromper nada (12.4).

**5. Por que o `Timeout` nunca disparou?**
Porque 200 ms < 2 000 ms. Ele está ali como rede de segurança, e o seu custo é **uma
*thread* viva durante toda a execução** (11.9).

**6. Por que `Falar` imprime o valor escrito *dentro* da subárvore?**
Porque `Atravessar` escreveu na chave `rota` do *blackboard* da subárvore, que o
`<SubTree rota="rota_final"/>` remapeou para `rota_final` no *blackboard* do pai. O
`debugMessage()` confirma os dois lados.

**7. Por que os carimbos de tempo são números de dez dígitos?**
Porque o padrão de `StatusChangeLogger` é `TimestampType::ABSOLUTE`, ou seja, segundos
desde a época (19.1). Para um traço legível, chame
`logger.seTimestampType(BT::TimestampType::RELATIVE)` — lembrando da grafia.

## 29.11 Três variações, com o resultado observado

As três foram **executadas**; o que se descreve é o resultado observado, não o esperado.

### Variação 1: `<Sequence name="missao">` → `<ReactiveSequence name="missao">`

**Resultado:** a saída é *praticamente idêntica*, e a missão termina normalmente.

**Por quê:** o `Aproximar` de fato passa a ser *ticado* a cada passagem, mas
`StatefulActionNode::tick()` só chama `onStart()` quando o estado é `IDLE` e só chama
`onRunning()` quando é `RUNNING`; em `SUCCESS`, devolve o próprio estado **sem executar
nada** (12.6). E a `ReactiveSequence` **não** repõe a `IDLE` os filhos *à esquerda* do que
está `RUNNING` — apenas os à direita (10.4).

O único efeito visível é a **ordem das duas primeiras linhas**: com `Sequence`, o `missao`
aparece como `RUNNING` *antes* do filho; com `ReactiveSequence`, *depois* — porque os nós
reativos são os únicos que não chamam `setStatus(RUNNING)` no início do `tick()` (10.1).

**Lição:** "reativo" não basta para tornar uma ação reexecutável; é preciso que alguém a
devolva a `IDLE`.

### Variação 2: `num_attempts="4"` → `num_attempts="2"`

**Resultado:** as duas tentativas falham, o `RetryUntilSuccesful` devolve `FAILURE`, a
subárvore falha e o `Fallback` cai na terceira estratégia. O traço termina com:

```
[..5.814]: RetryUntilSuccesful       RUNNING -> FAILURE
[..5.814]: destrancar_e_passar       RUNNING -> FAILURE
[..5.814]: PortaFechada              RUNNING -> FAILURE
[..5.814]: Atravessar                IDLE -> SUCCESS
[..5.814]: estrategias               RUNNING -> SUCCESS
  [Falar] atravessei pela janela
...
resultado: SUCCESS
```

A missão **ainda assim devolve `SUCCESS`** — o `Fallback` fez o seu trabalho (10.5).

### Variação 3: remover `rota="rota_final"` do `<SubTree>`

**Resultado:** a chave deixa de atravessar a fronteira. A árvore executa normalmente até o
`Falar`, e então **aborta com exceção não capturada**:

```
terminate called after throwing an instance of 'BT::RuntimeError'
  what():  [Falar] getInput() failed because it was unable to find the key
           [message] remapped to [rota_final]
```

**Nada disso é detectado na carga:** o remapeamento ausente não é erro de validação, e a
chave simplesmente não existe quando é procurada (16.1). Note também que este `main()` não
tem `try/catch` em volta do laço — num programa real, teria (6.7).

---

# 30. TESTANDO NÓS E ÁRVORES

## 30.1 Testar um nó isoladamente, sem XML

```cpp
#include "behaviortree_cpp_v3/behavior_tree.h"
#include <gtest/gtest.h>

TEST(Atravessar, EscreveARota)
{
    BT::NodeConfiguration config;
    config.blackboard = BT::Blackboard::create();
    BT::assignDefaultRemapping<Atravessar>(config);   // portas ↔ chaves homônimas

    config.blackboard->set("via", std::string("janela"));

    Atravessar no("teste", config);
    ASSERT_EQ(no.executeTick(), BT::NodeStatus::SUCCESS);

    std::string rota;
    ASSERT_TRUE(config.blackboard->get("rota", rota));
    EXPECT_EQ(rota, "atravessei pela janela");
}
```

`assignDefaultRemapping<T>()` é a peça que falta para usar um nó fora do *parser*: preenche
`config.input_ports`/`output_ports` com `"="`, o que faz cada porta apontar para a entrada
de mesmo nome (7.7). É o que os testes da própria biblioteca fazem
(`tests/gtest_blackboard.cpp`, `tests/gtest_coroutines.cpp`).

## 30.2 Testar uma máquina de estados (`StatefulActionNode`)

```cpp
TEST(Destrancar, CedeNaTerceiraTentativa)
{
    Porta porta;
    BT::NodeConfiguration config;
    config.blackboard = BT::Blackboard::create();
    BT::assignDefaultRemapping<Destrancar>(config);
    config.blackboard->set("duracao_ms", 0u);        // sem espera, no teste

    Destrancar no("teste", config, &porta);

    EXPECT_EQ(no.executeTick(), BT::NodeStatus::RUNNING);   // onStart()
    EXPECT_EQ(no.executeTick(), BT::NodeStatus::FAILURE);   // onRunning(), tentativa 1
    // o pai reporia a IDLE aqui; no teste, fazemos à mão:
    // (setStatus é protected — a alternativa é envolver numa árvore de verdade)
}
```

**ARMADILHA do teste unitário:** `setStatus()` é `protected`, então **não dá para repor um
nó a `IDLE` de fora** num teste. Duas saídas:

1. testar através de uma árvore de verdade (seção 30.3);
2. declarar a classe de teste `friend`, ou expor um método auxiliar só para testes.

## 30.3 Testar uma árvore inteira

```cpp
TEST(Missao, PassaPelaJanelaQuandoAFechaduraNaoCede)
{
    static const char* xml = R"(
      <root main_tree_to_execute="MainTree">
        <BehaviorTree ID="MainTree">
          <Fallback>
            <AlwaysFailure/>
            <Atravessar via="janela" rota="{rota}"/>
          </Fallback>
        </BehaviorTree>
      </root>)";

    BT::BehaviorTreeFactory factory;
    factory.registerNodeType<Atravessar>("Atravessar");

    auto tree = factory.createTreeFromText(xml);

    BT::NodeStatus st = BT::NodeStatus::RUNNING;
    while (st == BT::NodeStatus::RUNNING) { st = tree.tickRoot(); }

    EXPECT_EQ(st, BT::NodeStatus::SUCCESS);
    EXPECT_EQ(tree.rootBlackboard()->get<std::string>("rota"), "atravessei pela janela");
}
```

**Vantagens:** exercita a carga, a validação, os valores padrão e a declaração de tipos.
**Custo:** o teste passa a depender do XML.

## 30.4 Nós de mentira (*mocks*) para testes de árvore

O padrão mais simples é registrar ações e condições controladas por variáveis do teste:

```cpp
struct Cenario {
    BT::NodeStatus resultado_do_sensor = BT::NodeStatus::SUCCESS;
    int chamadas_da_acao = 0;
};

void registrarMocks(BT::BehaviorTreeFactory& f, Cenario& c)
{
    f.registerSimpleCondition("SensorOK", [&c](BT::TreeNode&) {
        return c.resultado_do_sensor;
    });
    f.registerSimpleAction("Trabalhar", [&c](BT::TreeNode&) {
        c.chamadas_da_acao++;
        return BT::NodeStatus::SUCCESS;
    });
}
```

Com isso é possível testar a **lógica da árvore** (que é o que muda com frequência) sem
tocar nas implementações reais.

## 30.5 Testar o comportamento reativo

Para verificar que uma guarda interrompe uma ação, é preciso uma ação assíncrona
controlada:

```cpp
class AcaoControlada : public BT::StatefulActionNode
{
  public:
    AcaoControlada(const std::string& n, const BT::NodeConfiguration& c, Cenario* cen)
      : BT::StatefulActionNode(n, c), cen_(cen) {}
    static BT::PortsList providedPorts() { return {}; }

    BT::NodeStatus onStart()   override { cen_->iniciada = true;  return BT::NodeStatus::RUNNING; }
    BT::NodeStatus onRunning() override { return cen_->concluir ? BT::NodeStatus::SUCCESS
                                                                : BT::NodeStatus::RUNNING; }
    void onHalted()            override { cen_->cancelada = true; }
  private:
    Cenario* cen_;
};

TEST(Guarda, InterrompeAAcaoQuandoACondicaoFalha)
{
    Cenario c;
    // ... montar a árvore com <ReactiveSequence><SensorOK/><AcaoControlada/></ReactiveSequence>
    tree.tickRoot();                       // ação começa
    EXPECT_TRUE(c.iniciada);

    c.resultado_do_sensor = BT::NodeStatus::FAILURE;
    tree.tickRoot();                       // guarda falha
    EXPECT_TRUE(c.cancelada);              // onHalted() foi chamado
}
```

## 30.6 Testes que a própria biblioteca traz

`tests/` é documentação executável do comportamento esperado. Os mais úteis para consulta:

| Arquivo | O que cobre |
|---|---|
| `gtest_sequence.cpp` | `Sequence`, `SequenceStar`, `ReactiveSequence` e as combinações de estado |
| `gtest_fallback.cpp` | `Fallback`, `ReactiveFallback` |
| `gtest_parallel.cpp` | limiares do `Parallel` |
| `gtest_decorator.cpp` | `Repeat`, `Retry`, `Timeout`, `Inverter` |
| `gtest_blackboard.cpp` | tipos, remapeamento, `assignDefaultRemapping` |
| `gtest_ports.cpp` | valores padrão, direções |
| `gtest_subtree.cpp` | isolamento e remapeamento de subárvore |
| `gtest_switch.cpp` | seleção e *halt* do ramo antigo |
| `gtest_factory.cpp` | registro, XML inválido, mensagens de erro |
| `navigation_test.cpp` | um cenário completo, mais próximo de uso real |
| `tests/include/action_test_node.h` | `AsyncActionTest`, o exemplo de `AsyncActionNode` que o cabeçalho da classe cita |

**Nota:** `tests/include/action_test_node.h` é o "complete example" mencionado no
comentário de `AsyncActionNode` — e é a única implementação **correta** de um
`AsyncActionNode` no repositório, já que o `movebase_node.cpp` omite a chamada à base no
`halt()`.

## 30.7 Verificação estática do XML antes de rodar

Um "teste" barato e muito eficaz é carregar todos os XML do projeto num teste que só
verifica que eles **constroem**:

```cpp
TEST(Arvores, TodasCarregam)
{
    BT::BehaviorTreeFactory factory;
    registrarTudo(factory);

    for (const auto& arquivo : {"missao.xml", "recuperacao.xml", "patrulha.xml"})
    {
        EXPECT_NO_THROW({
            auto tree = factory.createTreeFromFile(arquivo);
            BT::printTreeRecursively(tree.rootNode());
        }) << "falhou: " << arquivo;
    }
}
```

Isso pega, na CI: IDs errados, portas inexistentes, aridade errada nas formas explícitas,
conflitos de tipo entre portas e `<include>` quebrados — **tudo o que a validação da carga
detecta** (13.3, 15.4).

---

# 31. PERGUNTAS FREQUENTES

Formuladas como um usuário as faria, com a resposta e a seção que a detalha.

## 31.1 Começando

**P: Qual é o mínimo para rodar uma árvore?**
R: Registrar os nós numa `BehaviorTreeFactory`, chamar `createTreeFromText()` ou
`createTreeFromFile()`, e chamar `tree.tickRoot()` num laço com pausa. Ver 23.1.

**P: A biblioteca tem um laço principal?**
R: Não. Você escreve o laço. A biblioteca não tem relógio, agendador nem *thread* de
execução. Ver 2.4.

**P: Com que frequência devo *ticar*?**
R: É decisão do seu sistema. A frequência é a granularidade com que a árvore pode mudar de
ideia. 10–100 Hz é comum em robótica. **Nunca sem pausa.** Ver 4.6, 23.2.

**P: Preciso chamar `tickRoot()` mais de uma vez?**
R: Só se algum nó puder devolver `RUNNING`. Uma árvore inteiramente síncrona resolve num
único *tick* (é o caso do exemplo `t01`).

**P: Onde ponho o meu código?**
R: Nas folhas — ações e condições. Os nós de controle e decoradores embutidos cobrem quase
toda a lógica. Ver 12.

**P: Preciso de XML? Posso montar a árvore em C++?**
R: Pode, mas perde a validação, os valores padrão, a declaração de tipos e o objeto `Tree`
(e portanto os *loggers*). Ver 23.20.

## 31.2 Nós e classes base

**P: Qual classe base devo usar para a minha ação?**
R: `SyncActionNode` se terminar dentro do *tick*; `StatefulActionNode` se houver como
perguntar "já acabou?" sem bloquear; `AsyncActionNode` só se precisar bloquear de verdade.
Ver 12.1.

**P: Como faço uma ação que demora 5 segundos?**
R: `StatefulActionNode`. `onStart()` dispara e devolve `RUNNING`; `onRunning()` verifica se
acabou. Ver 12.6.

**P: Posso dormir dentro do `tick()`?**
R: Tecnicamente sim, e o exemplo `crossdoor` faz isso. Mas trava a árvore inteira: nenhuma
condição é reavaliada e nenhum `Timeout` consegue interromper. Ver 12.4.

**P: Por que meu nó lança `"SyncActionNode MUST never return RUNNING"`?**
R: Porque `SyncActionNode` proíbe `RUNNING`. Troque a classe base por
`StatefulActionNode`. Ver 12.4.

**P: Como escrevo uma condição?**
R: `registerSimpleCondition(id, functor)` para casos simples; herde de `ConditionNode` se
precisar de `providedPorts()` estático. Ver 12.3.

**P: Por que `std::bind(minhaFuncao)` e não uma lambda?**
R: `registerSimpleCondition` espera `std::function<NodeStatus(TreeNode&)>`. Uma função sem
argumentos só funciona via `std::bind`, que ignora argumentos extras. Uma lambda precisa
declarar o parâmetro: `[](BT::TreeNode&) { ... }`. Ver 12.3.

**P: Como passo um ponteiro para o meu robô ao nó?**
R: Pelo construtor, com um `NodeBuilder` próprio (`registerBuilder`). Ver 12.10, 23.16.

**P: `init()` depois de criar a árvore também serve?**
R: Serve, mas depende de você lembrar de chamar. Prefira o *builder*. Ver 12.10.

**P: Meu nó tem portas e o compilador reclama de `providedPorts()`. Por quê?**
R: Nós com portas precisam **também** do construtor `(const std::string&, const
NodeConfiguration&)` — e vice-versa. Ver 14.4.

**P: Declarei `providedPorts()` e o XML diz "Possible typo?". Por quê?**
R: Provavelmente `providedPorts()` está `private`. Precisa ser `public static`. Ver 7.4.

**P: Meu nó não recebe o *blackboard*. Por quê?**
R: Se a classe tem **os dois** construtores e o XML não deu nenhum atributo, a fábrica usa
o construtor de um argumento e descarta a configuração. Remova o construtor de um
argumento. Ver 14.5.

## 31.3 Portas e dados

**P: Qual a diferença entre `message="oi"` e `message="{oi}"`?**
R: O primeiro é um **literal** convertido do texto; o segundo é um **ponteiro** para a
entrada `oi` do *blackboard*. Ver 7.7.

**P: Como conecto a saída de um nó à entrada de outro?**
R: Aponte os dois para a **mesma chave**: `<A saida="{k}"/> <B entrada="{k}"/>`. Ver 22.2.

**P: Como uso um tipo próprio numa porta?**
R: Especialize `BT::convertFromString<T>` dentro do *namespace* `BT`. Ver 9.4.

**P: Onde ponho a especialização?**
R: Num cabeçalho, com `inline`, ou num único `.cpp`. Ver 9.4.

**P: Posso ter porta de tipo próprio sem `convertFromString`?**
R: Pode, desde que o valor **nunca** venha como texto — ou seja, só seja escrito por
`setOutput()` e lido por `getInput()` do mesmo tipo. Se alguém escrever no XML um literal,
a exceção aparece.

**P: Como dou um valor padrão a uma porta?**
R: `InputPort<T>("nome", valor_padrao, "descrição")`. Só funciona se `BT::toStr(valor)`
compilar (ou seja, tipos que `std::to_string` aceita). Ver 7.9, 9.5.

**P: Por que a porta com valor padrão falha quando construo o nó em C++?**
R: O padrão é aplicado pelo **parser**, não pelo nó. Ver 7.9.

**P: Como leio o resultado da árvore no C++?**
R: `tree.rootBlackboard()->get<T>("chave")`. Ver 23.21.

**P: Como escrevo um valor antes de *ticar*?**
R: `tree.rootBlackboard()->set("chave", valor)`. Ver 23.22.

**P: `getInput()` devolve "unable to find the key". O que houve?**
R: A chave existe na configuração mas a entrada do *blackboard* está vazia. Ninguém
escreveu, ou a chave não atravessou a fronteira de uma subárvore. Ver 25.4.

**P: Por que dá erro "the type of a port shall not change"?**
R: Duas portas tipadas apontam para a mesma chave com tipos diferentes. Ver 8.5.

**P: Posso guardar um `shared_ptr` no *blackboard*?**
R: Pode — qualquer tipo copiável cabe num `BT::Any`. Mas prefira o construtor para
infraestrutura; o *blackboard* é para dados que fluem. Ver 23.16.

## 31.4 Nós de controle

**P: Qual a diferença entre `Sequence` e `SequenceStar`?**
R: Ao falhar, a `Sequence` recomeça do primeiro filho; a `SequenceStar` retoma do filho que
falhou. Ver 10.2, 10.3.

**P: E `ReactiveSequence`?**
R: Reavalia **todos** os filhos a cada *tick*, mesmo os que já tiveram sucesso. Ver 10.4.

**P: Quando uso cada uma?**
R: `Sequence` por padrão; `SequenceStar` quando os passos têm efeito irreversível;
`ReactiveSequence` para o padrão guarda-e-trabalho. Ver 10.13.

**P: Por que meu XML da v2 se comporta diferente?**
R: `Sequence` e `Fallback` **trocaram de semântica** entre v2 e v3. Ver 10.7, 28.1.

**P: Existe `FallbackStar`?**
R: Não na v3. O `Fallback` da v3 **é** o antigo `FallbackStar`.

**P: `Parallel` executa em *threads*?**
R: **Não.** Os filhos são *ticados* sequencialmente dentro do mesmo *tick*. "Paralelo"
significa que vários podem estar `RUNNING` ao mesmo tempo. Ver 10.8.

**P: Por que `Parallel` lança "Missing parameter [success_threshold]"?**
R: Porque essa porta não tem valor padrão. Escreva sempre os dois limiares. Ver 10.8.

**P: `Switch3` com 3 filhos não funciona. Por quê?**
R: Precisa de **4**: três casos mais o *default*. Ver 10.11.

**P: Meu `Switch` sempre cai no *default*.**
R: Provavelmente a variável guarda um `double` (a comparação é textual e
`std::to_string(42.0)` é `"42.000000"`), ou `variable` não pôde ser lida. Ver 10.11.

**P: `WhileDoElse` com dois filhos lança. Mas a mensagem diz "2 ou 3"!**
R: A mensagem está errada; o código exige **exatamente 3**. Ver 10.10.

**P: Como faço "faça A e B ao mesmo tempo"?**
R: `<Parallel success_threshold="2" failure_threshold="1">` com **ações assíncronas**. Ver
23.14.

**P: Como interrompo uma ação quando uma condição muda?**
R: `ReactiveSequence` com a condição à esquerda e a ação assíncrona à direita. Ver 23.5.

## 31.5 Decoradores

**P: Como repito uma ação N vezes?**
R: `<Repeat num_cycles="N">` (repete enquanto tiver **sucesso**) ou
`<RetryUntilSuccesful num_attempts="N">` (repete enquanto **falhar**). Ver 11.5, 11.6.

**P: Escrevi `<RetryUntilSuccessful>` e dá "Node not recognized".**
R: O ID registrado tem **um `s` a menos**: `RetryUntilSuccesful`. Ver 11.6.

**P: `num_cycles="-1"` travou o programa.**
R: Com filho síncrono, o laço infinito acontece **dentro de um `tick()`**. Use `-1` só com
filhos assíncronos. Ver 11.6.

**P: Meu `<Timeout>` não interrompe nada.**
R: Três causas: o filho é síncrono (não pode ser interrompido); `msec="0"` desliga o
prazo; ou o filho é `AsyncActionNode` e não consulta `isHaltRequested()`. Ver 11.7, 12.7.

**P: Quantas *threads* meu processo tem?**
R: Uma por `<Timeout>` e uma por `<Delay>` na árvore (criadas na carga), mais uma por
execução de `AsyncActionNode`, mais uma do `PublisherZMQ`. Ver 11.9, 12.7.

**P: Minha árvore fica `RUNNING` para sempre.**
R: Suspeite de um filho devolvendo `IDLE` sob `ForceSuccess`, `ForceFailure` ou
`KeepRunningUntilFailure`, que engolem o erro. Ver 11.3.

**P: Como torno um passo opcional?**
R: `<ForceSuccess>` em volta dele. Ver 23.9.

**P: Meu `<Delay>` falha uma vez depois de ser interrompido.**
R: Defeito conhecido: `halt()` não repõe `delay_started_`. Ver 11.8.

**P: Como faço uma guarda que não executa o filho?**
R: `BlackboardCheck*` ou um decorador próprio. `registerSimpleDecorator` **não serve** — ele
*tica* o filho antes de chamar o *functor*. Ver 6.4, 11.10.

## 31.6 XML

**P: `main_tree_to_execute` é obrigatório?**
R: Só se houver mais de uma `<BehaviorTree>`. Mas escreva sempre. Ver 13.1.

**P: Qual a diferença entre `<SaySomething/>` e `<Action ID="SaySomething"/>`?**
R: Nenhuma para o executor. A forma explícita carrega o tipo, que o Groot precisa. Ver
13.2.

**P: Como divido a árvore em vários arquivos?**
R: `<include path="outro.xml"/>` no nível de `<root>`, **antes** das `<BehaviorTree>` que
os usam. Ver 13.5.

**P: O caminho do `<include>` é relativo a quê?**
R: Ao diretório do arquivo que inclui — **exceto** com `loadFromText()`, em que é relativo
ao diretório de trabalho. Ver 13.5.

**P: Posso incluir A em B e B em A?**
R: **Não.** Não há detecção de ciclo; o resultado é estouro de pilha. Ver 13.5.

**P: Duas `<BehaviorTree>` com o mesmo `ID`?**
R: Vale a **primeira lida**, sem aviso. Ver 15.6.

**P: Como o Groot vê meus nós customizados?**
R: Use a forma explícita, ou gere um `<TreeNodesModel>` com `writeTreeNodesModelXML()`. Ver
13.4.

**P: Escrevi `<TreeNodeModel>` e o Groot não vê nada.**
R: É `<TreeNodesModel>`, com `s`. A tag errada é silenciosamente ignorada. Ver 13.3.

## 31.7 Subárvores

**P: A subárvore não enxerga os dados do pai.**
R: É o comportamento correto: cada subárvore tem *blackboard* próprio. Declare o
remapeamento: `<SubTree ID="X" interna="externa"/>`. Ver 16.1.

**P: Dentro do `<SubTree>`, o valor leva chaves ou não?**
R: **Não leva.** Todo atributo de um `<SubTree>` é nome de chave externa. Dentro da
subárvore, sim: `<No porta="{interna}"/>`. Ver 16.1.

**P: E se eu quiser compartilhar tudo?**
R: `<SubTree ID="X" __shared_blackboard="1"/>`. Desfaz o isolamento — use com parcimônia.
Ver 16.1.

**P: E se eu quiser passar um literal para dentro?**
R: `<SubTreePlus ID="X" param="valor"/>`. Ver 16.2.

**P: O que é `__autoremap`?**
R: Remapeia automaticamente toda chave da subárvore para a de mesmo nome no pai, no
`SubTreePlus`. Só enxerga chaves criadas na carga. Ver 16.2.

**P: Duas instâncias da mesma subárvore aparecem com o mesmo nome no traço.**
R: O atributo `name` de um `<SubTree>` é **ignorado**; o *parser* usa o `ID`. Ver 16.2.

**P: Como acesso o *blackboard* de uma subárvore?**
R: `tree.blackboard_stack[i]`, sendo `i` a ordem de criação (0 é a raiz). Ver 16.3.

## 31.8 *Plugins*

**P: Como carrego nós de uma biblioteca dinâmica?**
R: `factory.registerFromPlugin("./libmeus_nos.so")`. Ver 17.4.

**P: O *plugin* carrega mas os nós não existem.**
R: Faltou `BT_PLUGIN_EXPORT` na compilação do *plugin*. A mensagem de erro sai em
`std::cout` e não lança. Ver 17.4.

**P: Posso descarregar um *plugin*?**
R: Não. O `SharedLibrary` local nunca chama `dlclose()`, e não há `unregister` em massa.
Ver 17.4.

**P: O *plugin* dá erro estranho de tipo.**
R: Provável ligação estática duplicada da biblioteca, ou comparação de `type_info` por
ponteiro em `setPortInfo`. Use a biblioteca **compartilhada**. Ver 8.5, 17.4.

## 31.9 Depuração

**P: Como vejo o que está acontecendo?**
R: `BT::StdCoutLogger logger(tree);` antes do laço. Ver 19.2.

**P: O traço tem números gigantes de carimbo de tempo.**
R: O padrão é absoluto. `logger.seTimestampType(BT::TimestampType::RELATIVE)` — note a
grafia. Ver 19.1.

**P: O traço tem linhas demais.**
R: `logger.enableTransitionToIdle(false)`. O padrão real é `true`, apesar do comentário.
Ver 19.1.

**P: Como sei se o XML virou a árvore que eu queria?**
R: `BT::printTreeRecursively(tree.rootNode())`. Ver 19.10.

**P: Como descubro por que uma porta está vazia?**
R: `tree.blackboard_stack[i]->debugMessage()`. Ver 8.6.

**P: Como meço o tempo de cada nó?**
R: `MinitraceLogger` e depois `chrome://tracing`. Ver 19.6.

**P: Como uso o Groot ao vivo?**
R: `PublisherZMQ` — **mas ele não é compilado** se ZeroMQ não estiver presente na
compilação. Ver 19.7.

**P: Criei dois `StdCoutLogger` e não deu erro.**
R: Alguém chamou `flush()`, que desarma o guarda. Ver 19.2.

## 31.10 *Build* e integração

**P: `cmake` falha pedindo GTest.**
R: `-DBUILD_UNIT_TESTS=OFF`. Ver 21.2.

**P: O *linker* reclama de `CoroActionNode`.**
R: A biblioteca foi compilada sem Boost (`BT_NO_COROUTINES`), mas o macro não chegou ao seu
código. Defina `-DBT_NO_COROUTINES` na compilação da sua aplicação. Ver 21.6.

**P: Como consumo o pacote Conan?**
R: `self.requires("behaviortree.cpp.asa/3.5.6")` e
`find_package(behaviortree.cpp.asa REQUIRED)`. Ver 21.11.

**P: Como compilo um único arquivo contra o repositório?**
R: Ver o comando completo em 21.12.

**P: `cmake` recusa o projeto por causa da política mínima.**
R: CMake 4 recusa `cmake_minimum_required(VERSION 3.5.1)`. Use
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`. Ver 21.7.

**P: Meu binário tem `CoroActionNode` e o do meu colega não.**
R: O *build* sonda Boost/ZMQ/ncurses na máquina. Fixe com `CMAKE_DISABLE_FIND_PACKAGE_*`.
Ver 21.3, 21.9.

**P: Estou no ROS 2 e `ros_pkg` não funciona.**
R: `USING_ROS` não é definido no caminho do ament. Use caminhos relativos. Ver 21.8.

## 31.11 Desempenho e recursos

**P: Quanto custa um *tick*?**
R: Uma travessia de ponteiros mais o `tick()` das folhas visitadas. O custo real está no
seu código e nas conversões de literais, que são refeitas a cada leitura. Ver 7.7.

**P: Posso *ticar* a 1 kHz?**
R: Tecnicamente sim, se as folhas forem baratas. Meça com o `MinitraceLogger`.

**P: A biblioteca aloca durante o *tick*?**
R: Sim, em vários pontos (conversões de *string*, `std::function`, `Any`). Não é adequada a
tempo real rígido sem análise. Ver 3.3.

**P: Posso limitar as *threads*?**
R: Só indiretamente: evitando `<Timeout>`/`<Delay>` (uma *thread* cada) e
`AsyncActionNode` (uma por execução). Prefira `StatefulActionNode` e um decorador de prazo
próprio. Ver 11.9.

## 31.12 Erros conceituais comuns

**P: Posso devolver `IDLE` do meu `tick()`?**
R: **Não.** Ver 4.3.

**P: Meu nó com `SUCCESS` não executa de novo.**
R: Correto: só volta a executar depois que o **pai** o repuser a `IDLE`. Ver 4.8.

**P: Troquei para `ReactiveSequence` e minha ação não reinicia.**
R: Nós reativos não repõem a `IDLE` os filhos **à esquerda** do `RUNNING`, e um
`StatefulActionNode` em `SUCCESS` devolve o estado sem executar. Ver 10.4.

**P: Onde guardo o estado da minha máquina de estados?**
R: No *blackboard* (para dados) ou no próprio nó (para estado de execução). Mas prefira
expressar a máquina **na estrutura da árvore**. Ver 23.12.

**P: A árvore é *thread-safe*?**
R: **Não.** *Tique* de uma única *thread*. As únicas partes com sincronização são
`TreeNode::setStatus()`/`status()` e o `Blackboard` — e nem o `Signal` nem `getKeys()` são
protegidos. Ver 5.6, 8.6.

---

# 32. GLOSSÁRIO

**Ação (*action*)** — folha que muda o mundo. Deriva de `ActionNodeBase`. Pode ser
síncrona ou assíncrona.

**Apagamento de tipo (*type erasure*)** — técnica pela qual o *blackboard* guarda valores
de qualquer tipo numa única estrutura (`BT::Any`), recuperando o tipo original apenas na
leitura.

**Aridade** — quantos filhos um nó pode ter: 0 (`LeafNode`), 1 (`DecoratorNode`) ou N
(`ControlNode`).

***Blackboard*** — dicionário chave → valor com tipo apagado pelo qual os nós trocam dados.
Cada subárvore isolada tem o seu, encadeado ao do pai por um mapa de remapeamentos.

**Condição (*condition*)** — folha que apenas responde `SUCCESS`/`FAILURE`, sem efeito
colateral. Deriva de `ConditionNode`.

**Decorador (*decorator*)** — nó de um único filho que modifica o resultado, a frequência
ou a duração da execução dele.

**FlatBuffers** — formato de serialização binária embutido, usado no cabeçalho dos
registros do `FileLogger` e nas mensagens do `PublisherZMQ`.

**Groot** — editor e monitor gráfico do projeto. Lê a árvore pelo `<TreeNodesModel>` e
acompanha a execução pelas mensagens do `PublisherZMQ`.

***Halt*** — cancelamento explícito de um nó que está `RUNNING`. Percorre a subárvore
devolvendo cada nó a `IDLE`.

**ID de registro** — a *string* pela qual um nó é instanciável a partir do XML. Não precisa
coincidir com o nome da classe C++.

**Manifesto (`TreeNodeManifest`)** — trio *tipo de nó*, *ID de registro* e *lista de
portas* que a fábrica guarda para cada nó registrado.

**Minitrace** — biblioteca embutida que grava eventos no formato de *trace* do Chrome.

**Nó com memória** — nó de controle que guarda em qual filho parou e retoma dali no *tick*
seguinte.

**Nó reativo** — nó de controle que reavalia **todos** os filhos a cada *tick*.

**`NodeStatus`** — valor de retorno de todo *tick*: `SUCCESS`, `FAILURE`, `RUNNING` ou
`IDLE`. `IDLE` é estado, não resultado.

***Plugin*** — biblioteca compartilhada que exporta `BT_RegisterNodesFromPlugin` e registra
os seus nós na fábrica em tempo de execução.

**Porta (*port*)** — declaração estática de uma entrada ou saída de um nó, devolvida pelo
método estático `providedPorts()`.

**Remapeamento** — ligação entre o nome da porta (fixo no código) e a chave do *blackboard*
(escolhida no XML). A sintaxe `{chave}` indica ponteiro; sem chaves, literal.

**Subárvore** — uma `<BehaviorTree>` inteira usada como se fosse um nó dentro de outra. Por
padrão recebe um *blackboard* próprio.

***Tick*** — a única forma de executar uma árvore. Uma chamada de `executeTick()` propagada
da raiz para baixo.

---

# 33. MAPA DE CONSULTA RÁPIDA

## 33.1 Cabeçalho por assunto

| Preciso de… | `#include "behaviortree_cpp_v3/…"` |
|---|---|
| tudo para uma aplicação | `bt_factory.h` |
| só os tipos de nó (para escrever um nó) | `behavior_tree.h` |
| escrever uma ação | `action_node.h` |
| escrever uma condição | `condition_node.h` |
| escrever um decorador | `decorator_node.h` |
| escrever um nó de controle | `control_node.h` |
| tipos básicos, portas, conversões | `basic_types.h` |
| *blackboard* direto | `blackboard.h` |
| *logger* de terminal | `loggers/bt_cout_logger.h` |
| *logger* de arquivo | `loggers/bt_file_logger.h` |
| *logger* de *trace* | `loggers/bt_minitrace_logger.h` |
| *logger* ZMQ / Groot | `loggers/bt_zmq_publisher.h` |
| gerar `<TreeNodesModel>` | `xml_parsing.h` |
| exceções | `exceptions.h` |

`bt_factory.h` inclui `behavior_tree.h`, que inclui **todos** os nós concretos. Na prática,
`#include "behaviortree_cpp_v3/bt_factory.h"` basta para quase tudo.

## 33.2 Símbolos mais usados

```cpp
BT::BehaviorTreeFactory        // fábrica
BT::Tree                       // árvore construída
BT::TreeNode                   // base de todos os nós
BT::NodeStatus                 // SUCCESS / FAILURE / RUNNING / IDLE
BT::NodeConfiguration          // blackboard + remapeamentos
BT::PortsList                  // unordered_map<string, PortInfo>
BT::InputPort<T>(nome, [padrão], [descrição])
BT::OutputPort<T>(nome, [descrição])
BT::BidirectionalPort<T>(nome, [padrão], [descrição])
BT::Blackboard::Ptr            // shared_ptr<Blackboard>
BT::Optional<T>                // nonstd::expected<T, std::string>
BT::Result                     // Optional<void>
BT::StringView                 // nonstd::string_view
BT::convertFromString<T>(str)  // especialize para tipos próprios
BT::toStr(valor)
BT::printTreeRecursively(no)
BT::applyRecursiveVisitor(no, visitor)
BT::writeTreeNodesModelXML(fabrica)
BT_REGISTER_NODES(factory)     // macro do plugin
```

## 33.3 Fluxo de decisão para escrever um nó

```
o nó tem filhos?
 ├─ nenhum
 │   ├─ só responde uma pergunta?      → ConditionNode / registerSimpleCondition
 │   └─ muda o mundo?
 │        ├─ termina no tick?          → SyncActionNode / registerSimpleAction
 │        ├─ dá para perguntar?        → StatefulActionNode
 │        ├─ Boost disponível?         → CoroActionNode
 │        └─ senão                     → AsyncActionNode
 ├─ exatamente um                      → DecoratorNode (ou registerSimpleDecorator,
 │                                        se só transformar o resultado)
 └─ vários                             → ControlNode
```

## 33.4 Comandos essenciais

```bash
# compilar o repositório
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DBUILD_UNIT_TESTS=OFF
cmake --build build -j

# compilar um programa contra a árvore de fontes
g++ -std=c++14 -DBT_NO_COROUTINES -I$REPO/include -I$REPO/3rdparty \
    prog.cpp -o prog -L$REPO/build/lib -lbehaviortree_cpp_v3 -lpthread -ldl

# pacote Conan (fork)
make create BUILD_TYPE=Release
make list
make path

# ferramentas
./build/bin/bt3_plugin_manifest ./libmeus_nos.so
./build/bin/bt3_log_cat bt_trace.fbl
```

---

# 34. RESUMO EXECUTIVO — OS DOZE PONTOS QUE MAIS IMPORTAM

1. **Uma operação, quatro valores.** Todo nó responde a `executeTick()` devolvendo
   `SUCCESS`, `FAILURE` ou `RUNNING`. `IDLE` é estado, nunca resultado.

2. **Estrutura em XML, comportamento em C++.** Reconfigurar o comportamento não exige
   recompilar; os três mecanismos que sustentam isso são a fábrica por nome, o sistema de
   portas e o protocolo do *tick*.

3. **O laço é seu, e precisa de pausa.** A biblioteca não tem relógio nem agendador.

4. **Memória versus reatividade é a decisão central de toda árvore.** `Sequence` retoma;
   `ReactiveSequence` reavalia. Na v2 os nomes significavam o oposto.

5. **Quem devolve um nó a `IDLE` é o pai.** Um nó em `SUCCESS`/`FAILURE` não é reexecutado
   até isso acontecer — e é por isso que `haltChildren()` aparece em toda parte.

6. **Escolha a classe de ação mais simples que resolva.** `SyncActionNode` →
   `StatefulActionNode` → `AsyncActionNode`. A última cria uma *thread* por execução e é
   difícil de acertar; o próprio cabeçalho avisa.

7. **Toda porta deveria ser tipada, e toda chave que atravessa uma subárvore deveria ser
   remapeada explicitamente.** O isolamento do *blackboard* é a proteção que torna
   subárvores reutilizáveis.

8. **`providedPorts()` precisa ser público.** Se for privado, é silenciosamente ignorado —
   e é exatamente o que acontece com `SubTree` e `SubTreePlus` na própria biblioteca.

9. **Quatro portas embutidas são obrigatórias e não têm padrão** (`success_threshold`,
   `num_cycles`, `num_attempts`, `msec`/`delay_msec`), e a falta delas só aparece no
   primeiro *tick* daquele nó.

10. **O ID de registro nem sempre é o nome da classe — e um deles está grafado errado.**
    É `RetryUntilSuccesful`, com um `s` a menos.

11. **O binário depende da máquina que compilou.** Boost, ZeroMQ, *ncurses* e ROS são
    sondados em silêncio e mudam o conteúdo e a ABI. No empacotamento documentado, os três
    primeiros estão desligados: **sem `CoroActionNode`, sem `PublisherZMQ`/Groot ao vivo,
    sem `ManualSelector`.**

12. **Nada disto vale para a v4.** Se o material que você encontrou fala em `NodeConfig`,
    `tickOnce()` ou `behaviortree_cpp/` sem `_v3`, é outra biblioteca.

---

*Fim do documento.*
