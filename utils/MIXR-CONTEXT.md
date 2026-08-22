---
titulo: "MIXR — Base de Conhecimento Técnica (contexto para RAG)"
projeto: "MIXR (Mixed Reality Simulation) — framework C++ de Modelagem & Simulação"
fork_documentado: "github.com/ASA-Simulation/mixr"
versao: "1.0.5 (meson.build da raiz); MIXR_VERSION = 170600 em include/mixr/config.hpp"
licenca: "LGPL-3.0"
fontes: "Manual técnico MIXR (report LaTeX, 13 capítulos) + árvore de fontes C++ do fork"
idioma: "português do Brasil; identificadores, nomes de slot e nomes de fábrica em inglês (originais)"
convencao_de_verdade: "toda afirmação foi conferida contra o código-fonte do fork; divergências entre comentário de cabeçalho e código são registradas explicitamente"
---

# COMO USAR ESTE DOCUMENTO

Este é um documento de contexto técnico sobre o **MIXR**, destinado a alimentar sistemas
de recuperação (RAG) que respondem perguntas sobre o framework. Cada seção é
autocontida: repete o nome da classe, do módulo e do mecanismo de que trata, de modo
que um trecho recuperado isoladamente continue interpretável.

Convenções deste documento:

- **REGRA** — norma do framework: algo que o desenvolvedor precisa fazer (ou não fazer)
  para que o mecanismo funcione.
- **ARMADILHA** — comportamento real do código que contraria a expectativa razoável:
  valor padrão surpreendente, nome que não é o que parece, comentário de cabeçalho
  desatualizado, funcionalidade existente pela metade. São resultado da conferência do
  texto contra o fonte.
- **POR QUÊ** — motivação de uma decisão de projeto.
- Caminhos como `src/base/Component.cpp` referem-se à árvore de fontes do MIXR.
- Blocos de código marcados com caminho de arquivo são transcrição do fonte
  (eventualmente condensada, com omissões marcadas por `// ...`), nunca reescrita.

---

# 1. IDENTIDADE DO PROJETO

## 1.1 O que é o MIXR

O **MIXR** (*Mixed Reality Simulation*) é um **framework em C++** para construção de
aplicações de modelagem e simulação (M&S) de sistemas — plataformas, sensores,
armamento e redes de interoperabilidade. **Não é um simulador pronto**: é um conjunto
de bibliotecas em camadas a partir das quais o desenvolvedor monta a simulação que
precisa.

A decisão de projeto que distingue o MIXR de uma biblioteca convencional, e que governa
praticamente todo o resto, é a **separação entre estrutura e comportamento**:

- o **comportamento** de cada peça é implementado em C++;
- a **estrutura** de uma simulação — quais peças existem, como se aninham e com que
  parâmetros — é descrita em arquivos de configuração na linguagem **EDL**
  (*English Description Language*).

Em tempo de carga, um *parser* lê o arquivo EDL e constrói a árvore de objetos
correspondente. Consequência prática direta: **reconfigurar uma simulação não exige
recompilar a aplicação** — basta editar o arquivo de configuração.

Essa escolha só funciona porque três mecanismos do núcleo a sustentam, e os três
reaparecem em todas as camadas:

1. **Fábrica por nome** (*factory*) — permite criar um objeto a partir de uma *string*
   de identificação lida do arquivo.
2. **Sistema de *slots*** — conecta os atributos nomeados do arquivo às funções C++ que
   os configuram.
3. **Contagem de referências** — gerencia automaticamente o ciclo de vida de uma árvore
   de objetos montada dinamicamente, sem `delete` explícito.

Toda simulação MIXR é uma **árvore de objetos cuja raiz é uma `Station`** — o componente
que organiza o *executive*, as redes de interoperabilidade, o hardware de
entrada/saída e o gravador de dados. A topologia completa dessa árvore é declarada em um
arquivo EDL; o *parser* a constrói em tempo de carga e a mantém viva pela contagem de
referências.

## 1.2 Público-alvo e escopo

O material aqui reunido é referência para desenvolvedores que pretendem **usar** o MIXR
para construir simulações ou **estender** o framework com novos modelos. Pressupõe
familiaridade com C++ moderno e conceitos gerais de M&S. Não é tutorial de usuário
final.

## 1.3 Características do fork documentado (importantes para planejar integração)

Este documento descreve o fork mantido em `github.com/ASA-Simulation/mixr`, versão
**1.0.5**, cuja diferença principal em relação ao MIXR original (`mixr.dev`) é a adoção
de **Meson** e **Conan 2** no lugar do sistema de compilação anterior.

Três características do fork decorrem do estado atual do *build* e afetam diretamente o
planejamento de uma integração:

1. **A compilação é apenas para Linux.** As fontes específicas de Windows estão
   presentes na árvore (`*_msvc.cpp`, `system_mingw.cpp`), mas **não são selecionadas
   pelo `meson.build`** — as fontes Linux são listadas incondicionalmente. Portar para
   Windows exigiria introduzir seleção condicional no `meson.build`, não apenas escrever
   código.
2. **HLA e RPR-FOM não são compilados.** Dos protocolos de interoperabilidade, apenas o
   **DIS** entra no build. Os diretórios `src/interop/hla` e `src/interop/rprfom`
   existem completos, mas estão comentados no `meson.build` do módulo.
3. **O padrão de linguagem é C++11**, com `-fpermissive`.

Além disso:

- O módulo **`linearsystem` não possui `factory.cpp`** e nenhum outro módulo depende
  dele — é uma caixa de ferramentas oferecida, não uma peça em uso interno.
- Não existe um **`IoHandler` concreto** no framework: a aplicação precisa fornecê-lo.

## 1.4 Números de referência do fork

| Grandeza | Valor |
|---|---|
| Nomes de fábrica declarados no fonte | 334 |
| Nomes de fábrica registrados em alguma `factory.cpp` | 224–227 (varia com a contagem de módulos) |
| Classes em que nome de fábrica ≠ nome de classe | 56 declaradas; 28 delas registradas |
| Arquivos `.cpp` de `models/` | ~107 |
| Classes do módulo `simulation` | 13 classes + `factory.cpp` (14 arquivos) |
| Arquivos de fonte de `linearsystem` | 13 |
| Bibliotecas produzidas | 9 componentes Conan |

Bibliotecas/componentes Conan produzidos: `base`, `simulation`, `terrain`, `models`,
`linearsystem`, `linkage`, `recorder`, `interop_common`, `interop_dis`.

---

# 2. PRINCÍPIOS DE ARQUITETURA

Quatro decisões arquiteturais permeiam o framework inteiro. Mantê-las em mente torna
todo o resto previsível.

1. **Composição dirigida por dados.** Estrutura em EDL, comportamento em C++. Favorece
   experimentação rápida e reaproveitamento, ao custo de um núcleo de *runtime* mais
   elaborado — preço que o framework paga uma vez para que toda aplicação se beneficie.

2. **Tudo é um `Component`.** Da estação no topo ao menor subsistema, todos compartilham
   a mesma maquinaria de atualização, contenção e eventos. Um mecanismo aprendido uma
   vez se aplica em toda a árvore.

3. **Dependências unidirecionais entre camadas.** Cada camada conhece apenas as
   inferiores. Isso fixa ordem de construção clara, isola mudanças e mantém as camadas
   baixas testáveis de forma independente.

4. **Atualização determinística e faseada.** O avanço do tempo é dividido em fases com
   papéis fixos, de modo que o resultado de um *frame* independa da ordem em que os
   participantes são processados — condição necessária para reprodutibilidade e para
   paralelizar com segurança.

---

# 3. ORGANIZAÇÃO EM CAMADAS

O MIXR empilha-se em camadas com dependência fluindo de cima para baixo:

```
                 interop      recorder     linkage        (camadas de topo)
                     \            |           /
                      \           |          /
   models  ────────────┴──────────┴─────────┘
     |  \        \
     |   \        terrain
     |    \
     |   simulation
     |      |
     └──── base                                            (fundação)

   linearsystem  ─── depende só de base; NINGUÉM depende dele
```

| Camada | Biblioteca | Depende de | Papel |
|---|---|---|---|
| `base` | `libmixr_base.so` | — | modelo de objetos, parser EDL, matemática, geodésia, threading, unidades, rede, cores, LFI, UBF, máquinas de estado |
| `simulation` | `libmixr_simulation.so` | `base` | *engine* de execução: `Station`, `Simulation`, threads, interfaces abstratas de infra |
| `terrain` | `libmixr_terrain.so` | `base` | dados de elevação (DTED, SRTM, DED) por interface uniforme |
| `models` | `libmixr_models.so` | `base`, `simulation`, `terrain`, `jsbsim` | o conteúdo simulado: players, dinâmica, sensores, armas, navegação, ambiente |
| `linearsystem` | `libmixr_linearsystem.so` | `base` | filtros e funções de transferência discretas (Tustin) |
| `linkage` | `libmixr_linkage.so` | `base` | E/S com hardware (joystick, painéis) |
| `recorder` | `libmixr_recorder.so` | `base`, `simulation`, `models`, `protobuf` | gravação/reprodução de dados com Protocol Buffers |
| `interop_common` | `libmixr_interop_common.so` | `base`, `simulation`, `models` | NIBs e *dead reckoning* comuns a DIS/HLA |
| `interop_dis` | `libmixr_interop_dis.so` | `interop_common` | implementação DIS (IEEE 1278) |

A direção única das dependências é o que permite compilar e validar `base` sem nenhuma
das camadas acima, e explica por que a integração e o estudo do framework seguem
naturalmente essa mesma ordem ascendente.

**Namespaces**: o código organiza-se em *namespaces* aninhados sob `mixr` —
`mixr::base`, `mixr::simulation`, `mixr::terrain`, `mixr::models`,
`mixr::linearsystem`, `mixr::linkage`, `mixr::recorder`, `mixr::interop`,
`mixr::dis`, `mixr::base::ubf`.

---

# 4. MODELO DE OBJETOS: A CADEIA `Referenced → Object → Component`

A composição dirigida por dados exige um modelo de objetos que saiba ser criado,
configurado e gerenciado a partir de texto. No MIXR isso se concretiza numa cadeia de
herança curta em que **cada nível acrescenta exatamente uma capacidade**:

| Classe | Acrescenta |
|---|---|
| `Referenced` | ciclo de vida por contagem de referências |
| `Object` | identidade de tipo, nome de fábrica, sistema de *slots*, categorias de mensagem |
| `Component` | contenção (árvore), tempo (`updateTC`/`updateData`), eventos, ciclo de vida (reset/freeze/shutdown) |

## 4.1 `Referenced` — contagem de referências

**POR QUÊ.** Uma árvore montada dinamicamente pelo *parser*, com objetos compartilhados
entre vários donos (um mesmo modelo de assinatura referido por diversos *players*, por
exemplo), torna o gerenciamento manual de memória frágil. A decisão do MIXR é que
`Referenced` controle o ciclo de vida por contagem: cada objeto nasce com contador 1,
cada novo dono chama `ref()`, cada dono que se desliga chama `unref()`, e o objeto se
destrói sozinho ao chegar a zero.

**REGRA — nunca usar `delete`, apenas `unref()`.** É a primeira regra que todo
desenvolvedor do framework internaliza.

`Referenced` é o topo abstrato e deliberadamente magro: não conhece nada do framework
além do próprio ciclo de vida. Internamente guarda dois campos privados:

- `refCount` — contador, nasce em 1;
- `semaphore` — usado apenas para travar o acesso a esse contador.

Expõe três operações: `ref()` incrementa; `unref()` decrementa e, ao chegar a zero,
executa `delete this`; `getRefCount()` expõe o valor para diagnóstico.

O travamento (funções `lock()`/`unlock()` de `atomics.hpp`) **não usa mutex do sistema
operacional**: o `semaphore` é manipulado por instruções atômicas do processador, sem
chamada de sistema. A razão é de frequência — `ref()` e `unref()` disparam a cada
atribuição de ponteiro em toda a árvore, potencialmente a partir de *threads*
diferentes, e um mutex de sistema seria custoso demais. É essa primitiva atômica que
torna a contagem de referências segura entre *threads*.

Dois detalhes de C++ reforçam a disciplina:

- O construtor de cópia e o `operator=` de `Referenced` são explicitamente removidos
  (`= delete`): copiar um `Referenced` duplicaria um contador que descreve donos
  compartilhados, o que não tem sentido semântico. Cada subclasse define sua própria
  noção de cópia por meio de `copyData()`, que clona dados, não identidade referenciada.
- `Referenced` não é instanciável diretamente — toda instância real pertence a uma
  subclasse com identidade própria obtida pelo par
  `DECLARE_SUBCLASS`/`IMPLEMENT_SUBCLASS`.

## 4.2 `Object` — fábrica por nome, *slots*, tipo

**POR QUÊ fábrica por nome.** Para o *parser* traduzir o texto `( Aircraft ... )` em um
objeto, é preciso criar uma instância da classe certa a partir apenas do nome. `Object`
atribui a cada classe um **nome de fábrica**, e a aplicação fornece uma função que mapeia
nome em objeto. Como a aplicação encadeia as fábricas das bibliotecas que usa, ela
controla a precedência: um modelo específico pode sobrepor-se a um genérico de mesmo
nome.

**POR QUÊ *slots*.** Configurar um objeto a partir de texto requer mapear atributos
nomeados (`type: "F-16"`) às funções C++ que os recebem. O sistema de *slots* é esse
mapeamento; é herdado pela cadeia de classes, de modo que atributos comuns são definidos
uma vez na base. Ao terminar de preencher os *slots*, o *parser* pergunta ao objeto se
ele está íntegro (`isValid()`), dando-lhe a chance de recusar configurações incompletas.

Dois métodos são a ponte entre o *parser* e o sistema de *slots*:

```cpp
// chamado pelo parser: recebe o nome textual do slot e o objeto de valor
bool setSlotByName(const char* slotname, Object* obj);

// virtual, sobrescrito por cada subclasse via BEGIN_SLOT_MAP
bool setSlotByIndex(int slotindex, Object* obj);
```

A separação é deliberada: `setSlotByName()` resolve o nome para um índice global (via
`SlotTable::index()`), e `setSlotByIndex()` despacha para a função de configuração
correta. **O *parser* conhece apenas `setSlotByName()`; as subclasses conhecem apenas
`setSlotByIndex()`.**

Identificação de tipo **sem `dynamic_cast`**: cada subclasse sobrescreve `isClassType()`
e `isFactoryName()`, testa o próprio tipo/nome e, se não corresponder, repassa para
`BaseClass`. A cadeia sobe até `Object`, que é a raiz. Resultado prático:
`obj->isFactoryName("Aircraft")` retorna `true` tanto para `Aircraft` quanto para
qualquer classe que herde de `Aircraft`.

Categorias de mensagem, definidas em `Object`: `MSG_ERROR`, `MSG_WARNING`, `MSG_INFO`,
`MSG_DEBUG`, `MSG_DATA`, `MSG_USER`. O padrão habilita apenas as duas primeiras.

## 4.3 As macros do modelo de classes

Para que cada objeto participe da composição dirigida por dados ele precisa satisfazer um
contrato: ser contado por referência, clonar-se corretamente, identificar seu tipo em
tempo de execução e expor sua tabela de *slots*. Implementar isso à mão em cada classe
seria repetitivo e frágil — um `clone()` mal escrito corrompe a contagem de referências
de uma subárvore inteira; um `isFactoryName()` que esquece de chamar a base quebra
silenciosamente a resolução de tipos.

**A decisão do MIXR é gerar essa maquinaria por macros**, tornando a adesão correta ao
contrato automática em vez de opcional.

### 4.3.1 `DECLARE_SUBCLASS` / `IMPLEMENT_SUBCLASS`

```cpp
// --- Foo.hpp ---
class Foo : public Component {
    DECLARE_SUBCLASS(Foo, Component)
    // gera as declaracoes de:
    //   clone(), copyData(), deleteData(),
    //   isClassType(), isFactoryName(),
    //   setSlotByIndex(), SlotTable, MetaObject
    // e define o alias:  typedef Component BaseClass;
public:
    Foo();
    // ... resto da interface propria de Foo
};

// --- Foo.cpp ---
IMPLEMENT_SUBCLASS(Foo, "Foo")
// implementa os metodos declarados acima
// e fixa o NOME DE FABRICA como a string "Foo"
```

O alias `BaseClass` merece atenção: todo código interno de `Foo` deve usar
`BaseClass::método()` em vez de `Component::método()`. Se a hierarquia mudar, somente a
linha da macro muda, não o corpo dos métodos.

Expansão simplificada de `DECLARE_SUBCLASS`:

```cpp
typedef Component BaseClass;

public:
    Foo(const Foo& org);                // construtor de copia
    virtual ~Foo();                     // destrutor virtual
    Foo* clone() const override;        // clonagem profunda
    Foo& operator=(const Foo& org);     // atribuicao
protected:
    void copyData(const Foo& org, const bool cc = false);
    void deleteData();
public:
    bool isClassType(const std::type_info&) const override;
    bool isFactoryName(const char[]) const override;
    static const MetaObject* getMetaObject();
    static const char* getFactoryName();
    static const SlotTable& getSlotTable();
protected:
    bool setSlotByIndex(const int, Object* const) override;
    static const SlotTable slottable;
private:
    static MetaObject metaObject;  // UM por classe, nao por instancia
    static const char* slotnames[];
    static const int   nslots;
private:                           // <-- a macro TERMINA em 'private:'
```

**ARMADILHA — `DECLARE_SUBCLASS` termina em `private:`.** Tudo escrito logo abaixo da
macro no corpo da classe é privado por padrão. É por isso que o exemplo reabre
explicitamente com `public:` antes de declarar o construtor. Omitir esse `public:` torna
a classe não instanciável de fora, com um erro de acesso que costuma desconcertar quem vê
a macro pela primeira vez.

### 4.3.2 Variantes das macros

| Macro | O que faz |
|---|---|
| `IMPLEMENT_SUBCLASS(C, "nome")` | o par completo |
| `IMPLEMENT_ABSTRACT_SUBCLASS(C, "nome")` | tudo, exceto que `clone()` retorna `nullptr` (classes com métodos virtuais puros) |
| `IMPLEMENT_PARTIAL_SUBCLASS(C, "nome")` | gera **apenas** identidade de tipo (`metaObject`, `isFactoryName()`, `isClassType()`, `getSlotTable()`); deixa a cargo do desenvolvedor o construtor de cópia, o destrutor, o `clone()` **e** o `operator=` — quatro membros, não três |
| `EMPTY_COPYDATA(C)` | classe sem dados próprios a copiar |
| `EMPTY_DELETEDATA(C)` | classe sem recursos próprios a liberar |
| `EMPTY_SLOTTABLE(C)` | classe sem *slots* próprios: declara `SlotTable` vazia encadeada à da base e um `setSlotByIndex()` que apenas repassa |
| `IMPLEMENT_EMPTY_SLOTTABLE_SUBCLASS(C, "nome")` | atalho para a combinação frequente |

### 4.3.3 `STANDARD_CONSTRUCTOR` / `STANDARD_DESTRUCTOR`

Devem ser chamadas **dentro de cada construtor e destrutor** de uma classe MIXR:

```cpp
Foo::Foo() : Component()
{
    STANDARD_CONSTRUCTOR()
    // O que a macro faz:
    //   slotTable = &Foo::slottable;  // aponta para a SlotTable estatica da classe
    //   metaObject.count++;           // registra mais uma instancia viva
    //   if (count > mc) mc = count;   // atualiza pico historico
    //   metaObject.tc++;              // incrementa total historico
    // ... inicializacao propria de Foo
}

Foo::~Foo()
{
    STANDARD_DESTRUCTOR()
    // O que a macro faz:
    //   deleteData();                 // libera recursos proprios
    //   metaObject.count--;           // desregistra a instancia
}
```

### 4.3.4 `copyData()` e `deleteData()`

A motivação é o `clone()`: o *parser* e vários modelos clonam protótipos para gerar
instâncias novas, e o framework exige que cada clone seja **completamente independente**
do original — sem ponteiros compartilhados disfarçados de cópia.

O parâmetro `cc` (*copy constructor*) distingue dois contextos de chamada: construção por
cópia, onde os membros ainda estão "virgens", e atribuição, onde dados antigos precisam
ser liberados antes de substituídos.

```cpp
void Foo::copyData(const Foo& org, const bool cc)
{
    // REGRA: sempre chama a base ANTES de copiar os proprios dados
    BaseClass::copyData(org, cc);

    if (!cc) {
        // contexto de atribuicao: libera recursos antigos antes de copiar
        if (mySubObj != nullptr) { mySubObj->unref(); mySubObj = nullptr; }
    }

    speed    = org.speed;        // valores escalares normalmente
    altitude = org.altitude;

    if (org.mySubObj != nullptr) {      // subobjetos: CLONAR, nunca compartilhar
        mySubObj = org.mySubObj->clone();
        mySubObj->ref();
    }
}
```

`deleteData()` deve ser **idempotente** (pode ser chamada mais de uma vez sem efeito
colateral) e libera recursos na ordem **inversa** à construção — próprios primeiro, base
depois:

```cpp
void Foo::deleteData()
{
    if (mySubObj != nullptr) { mySubObj->unref(); mySubObj = nullptr; }
    BaseClass::deleteData();
}
```

### 4.3.5 `MetaObject` — o registro estático de uma classe

Cada classe participante possui, como membro estático, um único `MetaObject` que existe
**uma vez por tipo** — independentemente de quantas instâncias existam em memória. Ele
não herda de `Object`, não participa da contagem de referências e vive em memória
estática durante toda a execução.

```cpp
// include/mixr/base/MetaObject.hpp
class MetaObject {
public:
    const char* getClassName()   const;  // nome C++ (de typeid), util para diagnostico
    const char* getFactoryName() const;  // nome EDL, ex.: "Aircraft"

    const SlotTable*  slottable {};       // tabela de slots desta classe
    const MetaObject* baseMetaObject {};  // MetaObject da classe base (forma a cadeia)

    int count {};  // instancias vivas agora
    int mc    {};  // pico historico de instancias simultaneas
    int tc    {};  // total de instancias criadas desde o inicio
};
```

O encadeamento via `baseMetaObject` espelha em dados a mesma hierarquia que
`isFactoryName()` percorre recursivamente em código. A redundância é proposital: a cadeia
pode ser percorrida em depuração para inspecionar a hierarquia sem executar nenhuma
chamada virtual.

Os três contadores são ferramenta de diagnóstico integrada e sem custo adicional (são
atualizados pelas macros `STANDARD_*`):

- `count` nunca retornando a zero ao final de uma simulação indica instâncias vazando;
- `mc` ajuda a dimensionar pré-alocação de memória;
- `tc` elevado em uma classe simples pode indicar criação/destruição excessivas —
  candidata a *object pooling*.

`Object::metaObject` é o único cujo `baseMetaObject` é `nullptr` — é a raiz da cadeia,
fechando a recursão de `isClassType()`/`isFactoryName()`.

### 4.3.6 `SlotTable` — indexação cumulativa

Cada classe que aceita *slots* declara, via `BEGIN_SLOTTABLE`/`END_SLOTTABLE`, um array
local de nomes. Em tempo de inicialização estática, a macro instancia uma `SlotTable` que
guarda esse array e um ponteiro para a `SlotTable` da classe base — formando uma cadeia
que espelha a hierarquia.

**A decisão de projeto crítica é que a indexação seja global e cumulativa**: se a classe
base já define N *slots*, os *slots* declarados pela subclasse começam no índice N+1.
Isso garante que um índice passado de uma subclasse para a base signifique o mesmo *slot*
em qualquer nível da cadeia.

```cpp
// --- Foo.cpp ---
BEGIN_SLOTTABLE(Foo)
    "speed",        // indice local 1 -> indice global N+1
    "altitude",     // indice local 2 -> indice global N+2
END_SLOTTABLE(Foo)

BEGIN_SLOT_MAP(Foo)
    ON_SLOT(1, setSlotSpeed,    Number)
    ON_SLOT(2, setSlotAltitude, Number)
END_SLOT_MAP()

bool Foo::setSlotSpeed(const Number* const n)
{
    speed = n->getReal();
    return true;
}
```

Expansão real das macros de *slot map* (resumo de `include/mixr/base/macros.hpp`). **O
repasse à base é a *primeira* decisão, e é por comparação de índice, não por falha de
despacho:**

```cpp
bool Foo::setSlotByIndex(const int slotindex, Object* const obj)
{
    // 1. Quantos slots existem ATE a classe base?
    const int _n {BaseClass::getSlotTable().n()};

    // 2. Indice dentro da faixa da base -> repassa IMEDIATAMENTE
    if (slotindex <= _n) {
        return BaseClass::setSlotByIndex(slotindex, obj);
    }

    // 3. Caso contrario, o slot e nosso: converte global -> local
    bool _ok {};
    int _n1 {slotindex - _n};

    if ( !_ok ) {                                   // ON_SLOT(1, setSlotSpeed, Number)
        const auto _msg = dynamic_cast<Number*>(obj);
        if (1 == _n1 && _msg != nullptr) {
            _ok = setSlotSpeed(_msg);
        }
    }
    // ...
    return _ok;   // 4. Nada casou -> false. NAO ha novo repasse a base aqui.
}
```

Duas consequências práticas:

1. A subtração `_n1 = slotindex - _n` é a contrapartida exata da indexação cumulativa —
   é ela que traduz o índice global de volta ao índice local que o `ON_SLOT` conhece.
2. **Um índice que pertence a esta classe mas cujo `dynamic_cast` falha — *slot* correto,
   tipo errado — retorna `false` sem tentar a base.** É esse `false` que sobe até
   `setSlotByName()` e vira a mensagem
   `"error while setting slot name: ..."` do *parser*. A recursão que fecha a cadeia é a
   do passo 2, e sua base é `Object::setSlotByIndex()`, que retorna `false`.

---

# 5. `Component`: A ÁRVORE, O TEMPO E OS EVENTOS

`Component` é o elo mais consequente da cadeia — é o que transforma uma coleção de
objetos configuráveis em uma *simulação*. Acrescenta quatro capacidades:

1. **Contenção**: um componente contém outros e conhece o seu continente, formando a
   árvore que o arquivo EDL descreve.
2. **Tempo**: `updateTC()` e `updateData()` descem automaticamente por essa árvore, sem
   que nenhum componente precise saber quem são os seus filhos.
3. **Eventos**: mensagens nomeadas trafegam pela árvore — para baixo por chamada direta,
   para cima por um mecanismo de propagação restrito a teclas.
4. **Ciclo de vida**: `reset()`, *freeze* e *shutdown* são coordenados ao longo de toda a
   árvore.

**ARMADILHA — `Component` não é instanciável em EDL.** Apesar de ser a base de tudo,
`Component` **não** está registrada em `base/factory.cpp`. Escrever `( Component ... )`
num arquivo EDL produz `undefined factory name: Component`. A classe existe para ser
herdada, não instanciada.

## 5.1 A árvore de contenção

Todo o estado da contenção cabe em dois campos:

```cpp
// resumo de include/mixr/base/Component.hpp
private:
    safe_ptr<PairStream> components;   // filhos  -- CONTADO por referencia
    Component*           containerPtr; // pai     -- ponteiro CRU, sem ref()
```

A assimetria é deliberada e é o detalhe mais importante do mecanismo. O ponteiro para o
pai é **cru, não contado**. Se um filho mantivesse referência contada para o continente,
toda árvore do MIXR seria um ciclo de referências — pai referencia filho, filho referencia
pai — e nenhuma seria destruída. Contando apenas para baixo, a posse flui numa direção só:
**destruir a raiz destrói a árvore inteira**.

**POR QUÊ o pai é um ponteiro cru.** Contagem de referências resolve posse, não
navegação. O pai *possui* o filho; o filho apenas *sabe* quem é o seu pai. Marcar essa
distinção no tipo do ponteiro é o que impede o ciclo. A contrapartida é que `container()`
pode devolver ponteiro para um objeto em vias de ser destruído — e é por isso que existe
`shutdownNotification()`, que dá a cada componente a chance de largar ponteiros crus antes
que a destruição comece.

Coerentemente, `copyData()` **anula** o ponteiro de continente ao copiar — o comentário no
fonte é direto: *"Copied doesn't mean contained in the same container!"*. Um clone nasce
órfão, e será o código que o clonou a decidir onde inseri-lo.

### Como a árvore é montada

O *slot* que constrói a árvore é `components`, e aceita duas formas:

```
// Forma 1 -- varios filhos nomeados
components: {
    radar1:  ( Radar    )
    nav1:    ( Gps      )
    piloto:  ( Autopilot )
}

// Forma 2 -- um filho unico, sem nome explicito
components: ( Radar )
// o framework o embrulha num Pair de nome "1", como se fosse posicional
```

Toda mutação da lista de filhos — pelo *slot*, por `addComponent()` em tempo de execução,
ou por remoção — passa por um único método, `processComponents()`:

```cpp
// src/base/Component.cpp (essencia)
void Component::processComponents(
        PairStream* const list,        // lista nova
        const std::type_info& filter,  // tipo exigido dos filhos
        Pair* const add,               // um filho a acrescentar (opcional)
        Component* const remove)       // um filho a remover     (opcional)
{
    PairStream* oldList {components.getRefPtr()};

    // 1. Uma lista NOVA e construida do zero -- a antiga nunca e editada
    const auto newList = new PairStream();

    for (/* cada par da lista recebida */) {
        const auto cp = dynamic_cast<Component*>(pair->object());

        // 2. Nao-Components sao descartados SILENCIOSAMENTE (porta de tipo)
        if (cp != nullptr && cp != remove && (skipFilter || cp->isClassType(filter))) {
            newList->put(pair);
            cp->container(this);        // 3. adotado: passa a apontar para nos
        } else if (cp == remove) {
            cp->container(nullptr);     //    removido: fica orfao
        }
    }

    if (add != nullptr) { /* ... mesmo filtro ... */ }  // 4. acrescentado no FIM

    components = newList;               // 5. troca ATOMICA via safe_ptr
    newList->unref();

    // 6. a selecao e reaplicada sobre a lista nova
    oldList->unref();
}
```

Seis consequências, na ordem dos passos:

1. A lista antiga nunca é modificada no lugar. Quem estiver percorrendo-a — a *thread* de
   tempo crítico, por exemplo — continua vendo uma lista íntegra até o fim da travessia.
2. O `dynamic_cast` é a **porta de tipo** do framework: um objeto que não seja `Component`
   é descartado *sem erro*.
3. Adotar um filho é o que estabelece `container()`. Antes de entrar numa lista de
   componentes, um objeto não tem pai.
4. O parâmetro `filter` é o ponto de extensão: uma classe que só aceite certos filhos
   sobrescreve `processComponents()` e passa um `typeid` mais estreito. É assim que o
   `OutputHandler` do gravador garante que todo filho seja outro `OutputHandler`.
5. A troca é uma única atribuição a um `safe_ptr`, e é isso que torna a mutação segura
   enquanto outra *thread* lê.
6. A seleção sobrevive à mutação, porque é reaplicada por nome ou índice sobre a lista
   nova.

**ARMADILHA — filho de tipo errado desaparece sem aviso.** Se um *slot* `components:`
receber um objeto que não deriva de `Component` — um `Table1`, digamos — o `dynamic_cast`
falha e o objeto é simplesmente **omitido da árvore**. Não há mensagem de erro, o *parser*
não acusa nada, e `isValid()` continua verdadeiro. O sintoma aparece muito depois, como um
subsistema que "não faz nada". O mesmo vale para o filtro de tipo das classes que o
estreitam.

## 5.2 Navegando a árvore: a família `find`

Quatro métodos localizam objetos. Todos devolvem ponteiros **emprestados** — não contados,
não precisam de `unref()` — o que os torna baratos e é a razão de aparecerem dentro de
laços de tempo real:

```cpp
// DESCEM na arvore:
Pair* findByName (const char* nome);        // por nome, com caminho pontilhado
Pair* findByIndex(int indice);              // por posicao, 1-based, so filhos diretos
Pair* findByType (const std::type_info&);   // por tipo, recursivo, primeiro que casar

// SOBE na arvore:
Component* findContainerByType(const std::type_info&);  // primeiro ANCESTRAL do tipo
```

`findContainerByType()` é o que sustenta todo o resto do framework: é assim que um radar
aninhado a três níveis encontra o *player* que o carrega, e como esse *player* encontra a
simulação em que vive. Note que ele **não** considera o próprio objeto — começa pelo pai.

### O caminho pontilhado de `findByName()`

```cpp
// (a) nome simples -> busca em PROFUNDIDADE na subarvore inteira
findByName("radar1");        // acha mesmo que esteja varios niveis abaixo

// (b) nome com ponto inicial -> APENAS filhos diretos ("nome duro")
findByName(".radar1");       // nao desce

// (c) caminho pontilhado -> o PRIMEIRO segmento so busca filhos diretos,
//     e todos os seguintes tambem
findByName("sms.estacao1.missil");
```

A assimetria entre (a) e (c) é a parte não óbvia: um nome simples faz busca profunda, mas
assim que existe um ponto, *todos* os segmentos passam a ser buscas rasas. Internamente, a
recursão repassa a cauda do caminho já começando com o ponto, o que reativa a regra do
"nome duro" em cada nível.

**ARMADILHA — caminho pontilhado não tem retrocesso.** Se o primeiro segmento de `"a.b.c"`
não estiver entre os filhos diretos, a busca falha imediatamente — ela **não** tenta então
procurar `"a.b.c"` em profundidade. E o *buffer* interno que copia o primeiro segmento tem
**128 bytes, sem verificação de limite**.

**REGRA — quem devolve emprestado e quem devolve contado.** Na árvore de componentes,
`findByName()`, `findByIndex()`, `findByType()` e `findContainerByType()` devolvem
ponteiros **emprestados** — não chame `unref()`. Já `getComponents()` e
`findNameOfComponent()` devolvem objetos **contados** — `unref()` é obrigatório.

## 5.3 A propagação do tempo e o papel de `tcFrame()`

```cpp
// src/base/Component.cpp
void Component::updateTC(const double dt)
{
    PairStream* subcomponents {getComponents()};   // contado: precisa unref()
    if (subcomponents != nullptr) {
        if (selection != nullptr) {
            // Ha uma selecao: SO o filho selecionado e atualizado
            if (selected != nullptr) selected->tcFrame(dt);
        } else {
            List::Item* item{subcomponents->getFirstItem()};
            while (item != nullptr) {
                const auto pair = static_cast<Pair*>(item->getValue());
                const auto obj  = static_cast<Component*>( pair->object() );
                obj->tcFrame(dt);        // <-- tcFrame(), NAO updateTC()
                item = item->getNext();
            }
        }
        subcomponents->unref();
    }
}
```

Três pontos:

1. A lista é obtida com `getComponents()`, que a devolve **contada** — a lista fica viva
   durante toda a travessia mesmo que outra *thread* a substitua no meio dela. É a
   contrapartida direta da troca atômica de `processComponents()`.
2. O `dt` atravessa **sem modificação**. `Component` **não zera o `dt` quando congelado**;
   quem faz isso é cada subclasse — por isso o congelamento não é automático.
3. A chamada aos filhos é `tcFrame(dt)`, e não `updateTC(dt)`.

```cpp
// src/base/Component.cpp (condensado)
void Component::tcFrame(const double dt)          // NAO e virtual
{
    double tcStartTime {};
    if (isTimingStatsEnabled()) tcStartTime = getComputerTime();

    this->updateTC(dt);                            // o trabalho de fato

    if (isTimingStatsEnabled()) {
        double dtime {(getComputerTime() - tcStartTime) * 1000.0};  // em MS
        timingStats->sigma(dtime);
        if (isTimingStatsPrintEnabled()) printTimingStats();
    }
}
```

`tcFrame()` é um invólucro **não virtual** de medição em torno de `updateTC()`. Com as
estatísticas desligadas — o padrão — é repasse puro, custo zero. Ligadas, cronometra a
chamada e alimenta um `Statistic`. Como a propagação passa por `tcFrame()` em todos os
níveis, **qualquer nó da árvore pode ser instrumentado isoladamente** ligando um único
*slot* nele — e o número que sai já inclui toda a sua subárvore.

**ARMADILHA — só o caminho de tempo crítico é instrumentado.** Não existe um
`dataFrame()`. `updateData()` chama `updateData()` nos filhos diretamente, sem invólucro e
sem medição.

## 5.4 Eventos: mensagens nomeadas na árvore

Componentes trocam *eventos*: mensagens identificadas por um inteiro simbólico,
opcionalmente carregando um objeto.

```cpp
BEGIN_EVENT_HANDLER(MeuSistema)
    ON_EVENT_OBJ( RESET_EVENT,  aoResetarCom, Number )  // com objeto tipado
    ON_EVENT    ( RESET_EVENT,  aoResetar            )  // sem objeto
    ON_EVENT_OBJ( DATALINK_MESSAGE, aoReceber, Message )
END_EVENT_HANDLER()
```

A expansão segue exatamente o padrão do `BEGIN_SLOT_MAP`: cadeia de testes sobre uma
variável `_used`, em que **o primeiro que casar vence**, e `END_EVENT_HANDLER()` repassa a
`BaseClass::event()` o que ninguém tratou.

**REGRA — a forma com objeto vem antes da forma sem.** Para um mesmo *token*,
`ON_EVENT_OBJ` deve ser escrito **antes** de `ON_EVENT`. Como o primeiro teste que casar
encerra a cadeia, a ordem inversa faria a versão sem objeto capturar também os eventos que
trazem um — e o objeto seria descartado silenciosamente.

Os *tokens* vêm de `eventTokens.hpp`, que é incluído *dentro* do corpo da classe
`Component`. Consequência: todo componente herda os nomes e pode escrevê-los sem
qualificação.

| Faixa | Grupo | Exemplos |
|---|---|---|
| 1–127 | teclas ASCII | `ENTER_KEY` (13), `ESC_KEY` (27), `DELETE_KEY` (127) |
| 128–152 | mouse e edição | `ON_SINGLE_CLICK`, `ON_DOUBLE_CLICK`, `F1_KEY`…`F12_KEY` |
| 200–239 | teclas de MFD | `OSB_T1`…`OSB_L10` (botões da moldura) |
| **999** | **`MAX_KEY_EVENT`** | **a fronteira que decide a propagação** |
| 1001 | sistema | `SHUTDOWN_EVENT` |
| 1201–1235 | gráficos | `ON_ENTRY`, `ON_EXIT`, `ON_RETURN`, `SELECT` |
| 1301–1323 | simulação | `RESET_EVENT`, `FREEZE_EVENT`, `KILL_EVENT`, `CRASH_EVENT`, `RF_EMISSION`, `IR_QUERY`, `DATALINK_MESSAGE` |
| 1400–1423 | HOTAS | `TRIGGER_SW_EVENT`, `WPN_REL_EVENT`, `TMS_*`, `DMS_*` |
| ≥ 2000 | da aplicação | `USER_EVENTS` |

O `event()` do próprio `Component` é escrito à mão, e não com as macros — justamente
porque precisa de um final não padrão:

```cpp
// src/base/Component.cpp
bool Component::event(const int _event, Object* const _obj)
{
    bool _used {};

    ON_EVENT_OBJ(SELECT,       select, Number)
    ON_EVENT_OBJ(SELECT,       select, String)
    ON_EVENT(RESET_EVENT,      onEventReset)
    ON_EVENT(SHUTDOWN_EVENT,   shutdownNotification)
    ON_EVENT_OBJ(FREEZE_EVENT, setSlotFreeze, Number)

    // *** Special handling of the end of the EVENT table ***
    // Pass only key events up to our container
    if (_event <= MAX_KEY_EVENT && container() != nullptr) {
        _used = container()->event(_event,_obj);
    }
    return _used;
}
```

**REGRA — eventos de tecla sobem; os demais morrem.** Um evento não tratado só é repassado
ao continente se o seu *token* for ≤ `MAX_KEY_EVENT` (999) — ou seja, se for uma tecla.
Eventos de simulação como `RESET_EVENT` ou `RF_EMISSION` **não sobem**: se ninguém os
tratou naquele nível, perdem-se ali. A razão é de interface: uma tecla pressionada tem que
encontrar quem a queira, e quem a queira pode estar acima na hierarquia de telas; já um
evento de simulação é endereçado — quem o envia sabe a quem.

### `send()` e o `SendData`

```cpp
// Forma simples: localiza por nome e dispara
send("radar1", RESET_EVENT);

// Forma com valor: exige um SendData, que deve ser MEMBRO da classe
class Painel : public Component {
    SendData sdAltitude;     // um por destino/valor -- persiste entre frames
};

void Painel::updateData(const double dt)
{
    send("mostradorAlt", UPDATE_VALUE, getAltitudeFt(), sdAltitude);
}
```

O `SendData` guarda duas coisas:

1. **O destinatário já resolvido.** `findByName()` é busca recursiva com comparação de
   *strings*; refazê-la a cada quadro seria proibitivo. O `SendData` guarda o ponteiro e
   só busca na primeira vez.
2. **O último valor enviado.** O evento só é disparado se o valor **mudou**. Um mostrador
   acionado a 50 Hz com um número parado não gera evento nenhum.

**POR QUÊ — `send()` é um filtro de mudança.** A segunda função é a mais valiosa: a
família `send()` não é apenas atalho para `findByName() + event()`, é um **detector de
mudança**. É o que permite escrever a atualização de um painel inteiro como lista de
`send()` incondicionais, chamada a cada quadro, sem inundar a árvore de eventos
redundantes.

**ARMADILHA — `SendData` guarda um ponteiro cru.** O destinatário em *cache* não é
contado. Se a árvore mudar, o ponteiro fica pendurado. Quem altera a árvore em tempo de
execução precisa chamar `empty()` nos `SendData` afetados. Por isso eles devem ser membros
da classe, não variáveis locais: uma variável local perderia o *cache* a cada quadro.

## 5.5 Ciclo de vida: *reset*, *freeze* e *shutdown*

As três operações têm comportamentos de propagação **diferentes** — fonte recorrente de
engano.

### `reset()` propaga, mas não reinicia nada por conta própria

`Component::reset()` percorre os filhos e chama `reset()` em cada um (respeitando a
seleção, se houver). Ele **não toca em nenhum campo do próprio `Component`** — nem
*freeze*, nem estatísticas, nem *shutdown*. Restaurar condições iniciais é
responsabilidade de cada subclasse, que faz o seu trabalho e chama `BaseClass::reset()`
para que a propagação continue. O evento `RESET_EVENT` chega aqui pela ponte
`onEventReset()`, que apenas chama `reset()`.

### `freeze()` **não** propaga

```cpp
void Component::freeze(const bool f)  { frz = f;    }
bool Component::isFrozen() const      { return frz; }
```

**ARMADILHA — congelar um componente não congela os filhos.** `freeze()` escreve um `bool`
e nada mais. Não desce pela árvore, e `updateTC()` não zera o `dt` dos filhos por causa
dele. Cada subclasse é que precisa consultar `isFrozen()` e agir — o padrão do framework é
a linha `double dt{dt0}; if (isFrozen()) dt = 0.0;` que aparece em `Simulation`, `Player`
e `System`. Um componente próprio que se esqueça dessa linha continuará rodando com a
simulação congelada.

### `shutdownNotification()` propaga e deve ser idempotente

O encerramento é o oposto do *reset*: desce pela árvore por meio de **eventos**, não por
chamada direta.

```cpp
// src/base/Component.cpp
bool Component::shutdownNotification()
{
   PairStream* subcomponents {getComponents()};
   if (subcomponents != nullptr) {
      List::Item* item {subcomponents->getFirstItem()};
      while (item != nullptr) {
         const auto pair = static_cast<Pair*>(item->getValue());
         const auto p = static_cast<Component*>(pair->object());
         p->event(SHUTDOWN_EVENT);      // vira shutdownNotification() no filho
         item = item->getNext();
      }
      subcomponents->unref();
   }
   shutdown = true;
   return shutdown;
}
```

**REGRA — `shutdownNotification()` tem duas obrigações.** Toda sobrescrita precisa
(a) chamar `BaseClass::shutdownNotification()`, sob pena de a subárvore inteira nunca ser
notificada, e (b) ser **idempotente**: `deleteData()` a chama de novo "por via das
dúvidas", de modo que ela roda ao menos duas vezes em toda destruição normal.

É aqui que um componente larga os ponteiros crus que mantém para os seus pares —
justamente aqueles que a contagem de referências não protege, como o `container()`.

## 5.6 Seleção, estatísticas e mensagens

**Seleção: um filho de cada vez.** Os *slots* `select` e o par `selection`/`selected`
permitem eleger **um único** filho para receber as atualizações. Todos os demais deixam de
custar qualquer coisa. É o mecanismo por trás da troca de páginas de um MFD.

**ARMADILHA — seleção que não resolve silencia a árvore.** Se o nome ou índice selecionado
não for encontrado, o framework imprime `"Component::select: name not found!"` **mas
mantém a seleção ativa**. Como `updateTC()` pergunta "há seleção?" antes de perguntar "ela
resolveu?", **nenhum** filho passa a ser atualizado — a subárvore inteira congela em
silêncio. `isComponentSelected()` continua devolvendo `true`.

**Estatísticas de tempo.** Ligadas pelos *slots* `enableTimingStats` e `printTimingStats`,
alimentam o `Statistic`. "Habilitado" é literalmente "o ponteiro para o `Statistic` não é
nulo" — não há *flag* separado. As amostras são em milissegundos.

**Tipos de mensagem e herança pela árvore.**

```cpp
// src/base/Component.cpp
bool Component::isMessageEnabled(const unsigned short msgType) const
{
   bool enabled {BaseClass::isMessageEnabled(msgType)};

   // Se nao estava habilitada aqui, e nao foi explicitamente DESABILITADA aqui,
   // e temos um continente: pergunte a ele.
   if ( !enabled && !isMessageDisabled(msgType) && containerPtr != nullptr) {
      enabled = containerPtr->isMessageEnabled(msgType);
   }
   return enabled;
}
```

A política de mensagens é **herdada para baixo pela árvore de contenção**, e qualquer
componente pode vetá-la localmente. Ligar `MSG_DEBUG` na `Station` torna a simulação
inteira verbosa; ligar num único radar, apenas aquele ramo. `MSG_ERROR` **não pode ser
desabilitada** — o *slot* `disableMessageType` aceita apenas `WARNING`, `INFO`, `DEBUG`,
`USER` e `DATA`.

## 5.7 Os sete *slots* de `Component` (índices globais 1 a 7 de TODA classe do MIXR)

| # | Slot | Tipo | Efeito |
|---|---|---|---|
| 1 | `components` | `PairStream` \| `Component` | monta a árvore de filhos |
| 2 | `select` | `String` \| `Number` | elege um único filho a atualizar |
| 3 | `enableTimingStats` | `Number` | liga a cronometragem de `tcFrame()` |
| 4 | `printTimingStats` | `Number` | imprime as estatísticas a cada quadro |
| 5 | `freeze` | `Number` | estado inicial do *flag* de congelamento |
| 6 | `enableMessageType` | `Identifier` \| `Number` | habilita categorias de mensagem |
| 7 | `disableMessageType` | `Identifier` \| `Number` | desabilita (exceto `ERROR`) |

Como `Component` está na base de tudo, esses sete são os primeiros índices globais de toda
`SlotTable` do framework — o que explica por que os *slots* próprios de uma classe como
`Radar` começam num índice bem acima de 1.

## 5.8 Exemplo consolidado

```
( Aircraft
    type: "F-16C"

    // Liga a cronometragem DESTE no e de toda a sua subarvore
    enableTimingStats: 1
    printTimingStats:  1

    // Deixa a subarvore inteira verbosa
    enableMessageType: DEBUG

    components: {
        sensores: ( Rwr
            components: {
                antena1: ( Antenna )
            }
        )
        piloto:   ( Autopilot )
    }
)
```

```cpp
// Busca PROFUNDA: acha "antena1" mesmo dois niveis abaixo
base::Pair* p { aircraft->findByName("antena1") };

// Caminho pontilhado: so filhos diretos em cada segmento
base::Pair* q { aircraft->findByName("sensores.antena1") };   // mesma antena

// Busca RASA: nao desce, entao NAO acha
base::Pair* r { aircraft->findByName(".antena1") };           // nullptr

// Por tipo, recursivo -- o idioma usado por Player::updateSystemPointers()
base::Pair* s { aircraft->findByType(typeid(Antenna)) };

// Subindo: de dentro da antena, achar o Player que a carrega
Player* dono { static_cast<Player*>(
    antena->findContainerByType(typeid(Player))) };

// Nenhum dos ponteiros acima precisa de unref() -- todos sao emprestados.
```

O idioma de subir com `findContainerByType()` até encontrar o *player* dono reaparece
dezenas de vezes na camada `models`. É o que permite que um subsistema seja escrito sem
nunca receber um ponteiro para o seu contexto: ele o encontra.
---

# 6. EDL: A LINGUAGEM DE CONFIGURAÇÃO

## 6.1 O *parser* e a tríade fábrica / *slots* / validação

O *parser* EDL é o ponto onde todos os mecanismos anteriores convergem: lê o texto, chama
a fábrica, configura os *slots* e organiza os objetos em estruturas de contenção. **Seu
acoplamento com o domínio é zero** — toda ligação com classes concretas chega por injeção
de uma única função:

```cpp
typedef Object* (*factory_func)(const std::string& name);

Object* edl_parser(const std::string& filename,
                   factory_func       f,
                   int*               num_errors = nullptr);
```

A função interna `parse()` — chamada a cada forma `(Nome ...)` reconhecida:

```cpp
// src/base/edl_parser/edl_parser.y
static Object* parse(const std::string& name, PairStream* arg_list)
{
    Object* obj {};

    if (factory != nullptr) {          // 'factory' e o ponteiro injetado

        // 1. FABRICA: instancia o objeto pelo nome
        obj = factory(name);

        // 2. SLOTS: configura cada atributo pelo nome
        if (obj != nullptr && arg_list != nullptr) {
            List::Item* item {arg_list->getFirstItem()};
            while (item != nullptr) {
                Pair* p {static_cast<Pair*>(item->getValue())};
                bool ok {obj->setSlotByName(*p->slot(), p->object())};
                if (!ok) {
                    yyerror(("error while setting slot name: "
                             + std::string(*p->slot())).c_str());
                }
                item = item->getNext();
            }

            // 3. VALIDACAO: o objeto tem a palavra final sobre sua integridade
            if (!obj->isValid()) {
                yyerror(("error: invalid object: " + name).c_str());
            }
        }
        else if (obj == nullptr) {
            yyerror(("undefined factory name: " + name).c_str());
        }
    }
    return obj;
}
```

**Falhas em qualquer etapa não interrompem a análise.** `yyerror()` apenas imprime a
mensagem (com arquivo e número de linha) e incrementa um contador; `parse()` segue
configurando os *slots* restantes e devolve o objeto de qualquer forma.

**ARMADILHA — ignorar `num_errors` é o engano mais comum ao integrar o *parser*.**

```cpp
int nErrs {};
Object* raiz { base::edl_parser("cenario.edl", minhaFactory, &nErrs) };

// Um ponteiro nao-nulo NAO significa que o arquivo estava correto:
// pode haver dezenas de slots mal configurados. Verifique sempre:
if (raiz == nullptr || nErrs > 0) {
    std::cerr << "configuracao invalida (" << nErrs << " erros)\n";
    return EXIT_FAILURE;
}
```

A simulação sobe, a árvore existe, e apenas os parâmetros mal escritos ficaram
silenciosamente com seus valores padrão.

## 6.2 A gramática do EDL

O *scanner* (gerado por **flex**) categoriza o texto em: identificadores (`IDENT`),
*slot-ids* (identificador seguido de `:` — `SLOT_ID`), *strings* entre aspas ou chevrons
(`STRING_LITERAL`), constantes inteiras/flutuantes/booleanas (`INTEGERconstant`,
`FLOATINGconstant`, `BOOLconstant`) e os delimitadores literais `(`, `)`, `{`, `}`, `[`,
`]`.

A gramática que o *parser* (gerado por **bison**) reconhece, transcrita de
`edl_parser.y` em notação BNF:

```
file       ::= form
             | SLOT_ID form

arglist    ::= /* vazio */             // -> PairStream() vazia
             | arglist form            // item POSICIONAL (objeto sem nome)
             | arglist prim            // item POSICIONAL (literal sem nome)
             | arglist slot_value      // item NOMEADO

form       ::= '(' IDENT arglist ')'   // objeto: ( Tipo ... )
             | '{' arglist '}'         // lista de itens (vira uma PairStream)

slot_value ::= SLOT_ID prim            // nome: literal
             | SLOT_ID form            // nome: ( Tipo ... )

prim       ::= STRING_LITERAL          // "texto" ou <texto>  -> String
             | IDENT                   // palavra nua         -> Identifier
             | BOOLconstant            // true / false        -> Boolean
             | '[' numlist ']'         // lista de numeros    -> List
             | number                  // Integer ou Float

numlist    ::= number | numlist number
number     ::= INTEGERconstant | FLOATINGconstant
```

Três pontos merecem comentário:

1. Um bloco entre chaves — `'{' arglist '}'` — é construído como uma `PairStream` na ordem
   em que os itens aparecem. É esse mecanismo que torna possível a sintaxe do *slot*
   `players:` da `Simulation`: um único *slot* recebe uma lista completa de *players*
   nomeados.
2. Um identificador nu, sem aspas, é um **valor válido** — vira um `Identifier`, não um
   erro de sintaxe. É assim que `earthModel: wgs84` funciona sem aspas.
3. `arglist` aceita `form` e `prim` **sem** um `SLOT_ID` à frente — a base dos *slots*
   posicionais.

**Comentários em EDL:** o texto do manual usa `--` e `//` em blocos EDL de exemplo.

## 6.3 *Slots* posicionais: por que `( Feet 10000 )` funciona

O mecanismo opera em dois tempos.

**Primeiro**, o *parser* dá nome a cada item sem nome: a sua posição ordinal, como texto.
A ação semântica da produção `arglist prim` é literalmente esta:

```cpp
// src/base/edl_parser/edl_parser.y
| arglist prim   {
                   int i = $1->entries();       // quantos itens ja ha
                   char cbuf[20] {};
                   std::sprintf(cbuf, "%i", i+1);  // "1", "2", "3", ...
                   Pair* p {new Pair(cbuf, $2)};   // nome = a posicao
                   $2->unref();
                   $1->put(p);
                   p->unref();
                   $$ = $1;
                 }
```

**Segundo**, `Object::slotName2Index()` reconhece nomes puramente numéricos e os interpreta
como **índices globais diretos**, saltando a busca por nome na `SlotTable`:

```cpp
// src/base/Object.cpp
int Object::slotName2Index(const char* const slotname) const
{
   int slotindex {};
   if (slotname == nullptr) return slotindex;

   const int n {slotTable->n()};

   // a) o nome e composto so de digitos? (ex.: "1", "12")
   bool isNum {true};
   for (int i = 0; isNum && slotname[i] != '\0'; i++) {
      if ( !std::isdigit(slotname[i]) ) isNum = false;
   }

   if (isNum) {
      // b) SIM: o proprio numero E o indice global do slot
      int j {std::atoi(slotname)};
      if (j > 0 && j <= n) slotindex = j;
   } else {
      // c) NAO: resolucao normal por nome
      slotindex = slotTable->index(slotname);
      if (slotindex <= 0)
         std::cerr << "slot not found: " << slotname << std::endl;
   }
   return slotindex;
}
```

Juntando as pontas: em `( Feet 10000 )`, o literal `10000` é o primeiro item anônimo,
recebe o nome `"1"`, e `"1"` resolve para o *slot* global de índice 1. Na cadeia
`Object → Number → Distance → Feet`, o *slot* de índice 1 é `"value"`, declarado por
`Number` — precisamente onde o valor numérico deve ir. Portanto:

```
altitude: ( Feet 10000 )              -- item anonimo -> Pair("1", Integer)
altitude: ( Feet  value: 10000 )      -- identico, com o slot nomeado
```

O preço é uma dependência silenciosa da **ordem de declaração** dos *slots* na hierarquia:
uma classe que insira um *slot* novo antes dos existentes muda o significado de todo EDL
posicional que a referencie. Na prática, a forma posicional é usada quase exclusivamente
para o *slot* 1 de `Number` e suas famílias de unidades, onde essa ordem é estável por
construção.

## 6.4 Arquivos gerados do *parser*

`EdlScanner.cpp` e `EdlParser.cpp` são distribuídos **já gerados** no repositório, ao lado
das fontes `edl_scanner.l` e `edl_parser.y` que os originaram — **flex e bison não são
necessários para compilar o MIXR**, mas as fontes estão presentes para quem precisar
alterar a gramática. Os cabeçalhos `EdlParser.hpp` e `EdlScanner.hpp` completam o
conjunto; sem eles o *scanner* não compila.

## 6.5 Percurso completo de uma linha EDL

Para `ownship: ( Aircraft  type: "F-16" )`:

1. **Scanner** produz: `SLOT_ID("ownship")`, `'('`, `IDENT("Aircraft")`,
   `SLOT_ID("type")`, `STRING("F-16")`, `')'`.
2. **Parser** reconhece a forma e chama `parse("Aircraft", {type: String("F-16")})`.
3. **`parse()`** executa:
   a. `factory("Aircraft")` retorna `new Aircraft()`, cujo `STANDARD_CONSTRUCTOR()`
      registra a instância no `MetaObject` e aponta `slotTable` para a tabela de *slots*
      de `Aircraft`.
   b. `aircraft->setSlotByName("type", String("F-16"))`: `Object` consulta a `SlotTable`
      via `index("type")` → índice global → `Aircraft::setSlotByIndex()` → `ON_SLOT`
      despacha para `setSlotType()` → armazena `"F-16"`.
   c. `aircraft->isValid()` retorna `true`.
4. O objeto é embrulhado em `Pair("ownship", aircraft)` e inserido na `PairStream` de
   filhos da `Simulation`.

Todo esse percurso ocorre **sem que o *parser* conheça a classe `Aircraft`**. Ele conhece
apenas `Object`; as classes concretas chegam exclusivamente pela fábrica injetada.

---

# 7. NOME DE FÁBRICA ≠ NOME DE CLASSE

## 7.1 A regra

**REGRA — o nome de fábrica é uma *string* independente.** O nome de fábrica é o
**segundo argumento** de `IMPLEMENT_SUBCLASS` — uma *string* escolhida livremente.
Coincide com o nome da classe C++ na maioria dos casos, mas isso é **convenção**, não
regra do framework. Em **56 classes** do MIXR os dois divergem.

**E o comentário `// Factory name: ...` no topo dos cabeçalhos não é confiável**: vários
estão desatualizados em relação à macro logo abaixo. **A fonte de verdade é sempre o
`IMPLEMENT_*SUBCLASS` no `.cpp`.**

## 7.2 Os cinco padrões de divergência

| Padrão | Exemplos | Razão |
|---|---|---|
| **Símbolo** | `Add`→`"+"`, `Subtract`→`"-"`, `Multiply`→`"*"`, `Divide`→`"/"` | operadores ficam legíveis em notação prefixada |
| **Minúscula** | `Rgb`→`"rgb"`, `Hsva`→`"hsva"`, `Integer`→`"int"`, `Float`→`"float"`, `Boolean`→`"boolean"`, `Decibel`→`"dB"` | tipos "de valor" parecem palavras-chave, não classes |
| **Prefixo de módulo** | `FileWriter`→`"RecorderFileWriter"`, `NetIO`→`"DisNetIO"`, `Ntm`→`"DisNtm"` | desambiguar nomes que se repetem entre módulos |
| **Prefixo `Abstract`/`Base`** | `Distance`→`"AbstractDistance"`, `IoHandler`→`"BaseIoHandler"`, `StoresMgr`→`"BaseStoresMgr"`, `Terrain`→`"AbstractTerrain"`, `StateMachine`→`"AbstractStateMachine"` | liberar o nome "bonito" para a classe concreta de uso comum |
| **Renomeação** | `Aam`→`"AamMissile"`, `Agm`→`"AgmMissile"`, `Image`→`"SarImage"`, `Tdb`→`"Gimbal_Tdb"`, `TdbIr`→`"Seeker_TdbIr"`, `RfSignature`→`"Signature"`, `MonitorMetrics`→`"monitorMetrics"`, `IoDevice`→`"BaseIoDevice"` | o nome EDL é mais descritivo (ou mais curto) que o nome C++ |

**ARMADILHA — o quinto caso do quarto padrão é o perigoso.** Os quatro primeiros padrões
produzem erro imediato e claro: `( Decibel 7.0 )` falha com
`undefined factory name: Decibel`, e o autor corrige para `( dB 7.0 )`.

Já `StoresMgr`→`"BaseStoresMgr"` **liberou** o nome `"StoresMgr"`, que foi então tomado
por `SimpleStoresMgr`. Escrever `( StoresMgr ... )` num arquivo EDL **funciona** — e
constrói silenciosamente um objeto de **outra classe**, com outro comportamento. Um erro
assim não aparece no carregamento; aparece como comportamento inesperado horas depois.

## 7.3 Registrado ≠ existir

Segunda condição, independente da primeira: mesmo com o nome certo, a classe só é
construtível se alguma `factory.cpp` a registrar. Dos **334** nomes de fábrica declarados
no fonte, **224–227 estão registrados**. Os demais são classes abstratas, classes
internas — ou omissões:

- `UbfAgentTC` — a classe existe, o nome existe, e `base/factory.cpp` registra apenas
  `UbfAgent` e `UbfArbiter`.
- `BaseIoHandler` — além de não registrado, é abstrato e **não tem implementação concreta
  em lugar nenhum**.
- **Todo o módulo `linearsystem`** — não possui `factory.cpp`.

**REGRA — como descobrir o nome de fábrica correto (leva dez segundos):**

1. Abra o `.cpp` da classe e leia a *string* do `IMPLEMENT_*SUBCLASS` — **esse** é o nome.
2. Confirme que ele aparece na `factory.cpp` do módulo.

Não confie no nome da classe, nem no nome do arquivo, nem no comentário do cabeçalho.

Comandos para regenerar o catálogo contra outra versão:

```bash
# O nome de fabrica de uma classe especifica:
grep -rn "IMPLEMENT_.*SUBCLASS(NomeDaClasse" src/

# Todas as divergencias de uma vez:
grep -rhno 'IMPLEMENT_[A-Z_]*SUBCLASS([A-Za-z0-9_]*, *"[^"]*")' src/ \
  | sed 's/.*SUBCLASS(//; s/, *"/ -> /; s/")//' \
  | awk -F' -> ' '$1 != $2'
```

## 7.4 Catálogo das divergências REGISTRADAS (instanciáveis a partir de EDL)

Módulos omitidos (`simulation`, `terrain`, `linkage`) não têm nenhuma divergência
registrada.

### `base`

| Nome de fábrica (EDL) | Classe C++ |
|---|---|
| `*` | `Multiply` |
| `+` | `Add` |
| `-` | `Subtract` |
| `/` | `Divide` |
| `UbfAgent` | `Agent` |
| `UbfArbiter` | `Arbiter` |
| `boolean` | `Boolean` |
| `cie` | `Cie` |
| `cmy` | `Cmy` |
| `dB` | `Decibel` |
| `float` | `Float` |
| `hls` | `Hls` |
| `hsv` | `Hsv` |
| `hsva` | `Hsva` |
| `int` | `Integer` |
| `rgb` | `Rgb` |
| `rgba` | `Rgba` |
| `yiq` | `Yiq` |

### `models`

| Nome de fábrica (EDL) | Classe C++ |
|---|---|
| `AamMissile` | `Aam` |
| `AgmMissile` | `Agm` |
| `StoresMgr` | `SimpleStoresMgr` |

### `recorder`

| Nome de fábrica (EDL) | Classe C++ |
|---|---|
| `RecorderFileReader` | `FileReader` |
| `RecorderFileWriter` | `FileWriter` |
| `RecorderNetInput` | `NetInput` |
| `RecorderNetOutput` | `NetOutput` |
| `RecorderOutputHandler` | `OutputHandler` |

### `interop`

| Nome de fábrica (EDL) | Classe C++ |
|---|---|
| `DisNetIO` | `NetIO` |
| `DisNtm` | `Ntm` |

## 7.5 Catálogo de TODAS as divergências declaradas no fonte (registradas ou não)

Extraído do fonte com `IMPLEMENT_*SUBCLASS`. Formato `ClasseC++ -> "nomeDeFabrica"`:

```
Aam            -> AamMissile          Agm            -> AgmMissile
Add            -> +                   Subtract       -> -
Multiply       -> *                   Divide         -> /
Agent          -> UbfAgent            AgentTC        -> UbfAgentTC
Arbiter        -> UbfArbiter          Angle          -> AbstractAngle
Area           -> AbstractArea        Distance       -> AbstractDistance
Energy         -> AbstractEnergy      Force          -> AbstractForce
Frequency      -> AbstractFrequency   Mass           -> AbstractMass
Time           -> AbstractTime        Volume         -> AbstractVolume
Terrain        -> AbstractTerrain     StateMachine   -> AbstractStateMachine
Boolean        -> boolean             Float          -> float
Integer        -> int                 Decibel        -> dB
Cie -> cie     Cmy -> cmy             Hls -> hls
Hsv -> hsv     Hsva -> hsva           Rgb -> rgb
Rgba -> rgba   Yiq -> yiq             MonitorMetrics -> monitorMetrics
FileReader     -> RecorderFileReader  FileWriter     -> RecorderFileWriter
NetInput       -> RecorderNetInput    NetOutput      -> RecorderNetOutput
OutputHandler  -> RecorderOutputHandler
InputHandler   -> RecorderInputHandler
Image          -> SarImage            RfSignature    -> Signature
SimpleStoresMgr-> StoresMgr           StoresMgr      -> BaseStoresMgr
IoHandler      -> BaseIoHandler       IoDevice       -> BaseIoDevice
Tdb            -> Gimbal_Tdb          TdbIr          -> Seeker_TdbIr
NetIO          -> DisNetIO / HlaNetIO / RprFomNetIO
Nib            -> DisNib / HlaNib
Ntm            -> DisNtm / HlaNtm
```

Observação: as classes com nome de fábrica `Abstract*` são as bases abstratas das
famílias de unidades e de terreno — elas existem para dar identidade de tipo, e **não são
instanciáveis** em EDL (não estão registradas). Quem escreve EDL usa as concretas
(`Feet`, `Meters`, `Degrees`, `DtedFile`, …).

## 7.6 Classes REGISTRADAS por módulo (o que pode ser escrito em EDL)

### `src/base/factory.cpp` (≈102 nomes)

Números e valores: `Number`, `Complex`, `Integer` (`int`), `Float` (`float`),
`Boolean` (`boolean`), `Decibel` (`dB`), `LatLon`.
Operadores: `Add` (`+`), `Subtract` (`-`), `Multiply` (`*`), `Divide` (`/`).
Diversos: `FileReader`, `Statistic`, `EarthModel`.
Transformações: `Translation`, `Rotation`, `Scale`.
Funções/tabelas: `Func1`…`Func5`, `Polynomial`, `Table1`…`Table5`.
Timers: `UpTimer`, `DownTimer`.
Ângulos: `Degrees`, `Radians`, `Semicircles`.
Área: `SquareMeters`, `SquareFeet`, `SquareInches`, `SquareYards`, `SquareMiles`,
`SquareCentiMeters`, `SquareMilliMeters`, `SquareKiloMeters`, `DecibelSquareMeters`.
Distância: `Meters`, `CentiMeters`, `MicroMeters`, `Microns`, `KiloMeters`, `Inches`,
`Feet`, `NauticalMiles`, `StatuteMiles`.
Energia: `KiloWattHours`, `BTUs`, `Calories`, `FootPounds`, `Joules`.
Força: `Newtons`, `KiloNewtons`, `Poundals`, `PoundForces`.
Frequência: `Hertz`, `KiloHertz`, `MegaHertz`, `GigaHertz`, `TeraHertz`.
Massa: `Grams`, `KiloGrams`, `Slugs`.
Potência: `KiloWatts`, `Watts`, `MilliWatts`, `Horsepower`, `DecibelWatts`,
`DecibelMilliWatts`.
Tempo: `Seconds`, `MilliSeconds`, `MicroSeconds`, `NanoSeconds`, `Minutes`, `Hours`,
`Days`.
Compostas: `AngularVelocity`, `LinearVelocity`.
Cores: `Color`, `Cie` (`cie`), `Cmy` (`cmy`), `Hls` (`hls`), `Hsv` (`hsv`),
`Hsva` (`hsva`), `Rgb` (`rgb`), `Rgba` (`rgba`), `Yiq` (`yiq`).
Rede: `TcpClient`, `TcpServerSingle`, `TcpServerMultiple`, `UdpBroadcastHandler`,
`UdpMulticastHandler`, `UdpUnicastHandler`.
UBF: `ubf::Agent` (`UbfAgent`), `ubf::Arbiter` (`UbfArbiter`).

### `src/simulation/factory.cpp` (2 nomes)

`Simulation`, `Station`. As demais classes do módulo são abstratas.

### `src/terrain/factory.cpp` (4 nomes)

`QuadMap`, `DedFile`, `DtedFile`, `SrtmHgtFile`.

### `src/linkage/factory.cpp` (11 nomes)

`IoData`, `DiscreteInput`, `DiscreteOutput`, `AnalogInput`, `AnalogOutput`,
`Ai2DiSwitch`, `AnalogInputFixed`, `AnalogSignalGen`, `DiscreteInputFixed`,
`MockDevice`, `UsbJoystick`.
**Não registra `IoHandler`** (nome de fábrica `BaseIoHandler`).

### `src/recorder/factory.cpp` (9 nomes)

`FileWriter` (`RecorderFileWriter`), `FileReader` (`RecorderFileReader`),
`NetInput` (`RecorderNetInput`), `NetOutput` (`RecorderNetOutput`),
`OutputHandler` (`RecorderOutputHandler`), `TabPrinter`, `PrintPlayer`,
`DataRecorder`, `PrintSelected`.

### `src/interop/dis/factory.cpp` (3 nomes)

`NetIO` (`DisNetIO`), `Ntm` (`DisNtm`), `EmissionPduHandler`.

### `src/models/factory.cpp` (≈96 nomes)

Dinâmica: `RacModel`, `JSBSimModel`, `LaeroModel`.
Ambiente: `IrAtmosphere`, `IrAtmosphere1`.
Sensores de radar: `Gmti`, `Stt`, `Tws`, `Radar`, `Rwr`, `Sar`, `Jammer`, `RfSensor`,
`SensorMgr`, `IrSensor`, `MergingIrSensor`, `IrSeeker`.
Núcleo: `WorldModel`, `Player`, `System`.
Players aéreos: `AirVehicle`, `Aircraft`, `Helicopter`, `UnmannedAirVehicle`.
Players terrestres: `GroundVehicle`, `Tank`, `ArmoredVehicle`, `WheeledVehicle`,
`Artillery`, `SamVehicle`, `GroundStation`, `GroundStationRadar`, `GroundStationUav`.
Outros players: `Building`, `LifeForm`, `Ship`, `SpaceVehicle`, `MannedSpaceVehicle`,
`UnmannedSpaceVehicle`, `BoosterSpaceVehicle`.
Subsistemas: `AvionicsPod`, `Pilot`, `Autopilot`, `Navigation`, `Ins`, `Gps`, `Route`,
`Steerpoint`, `Bullseye`, `OnboardComputer`, `Radio`, `CommRadio`, `Iff`, `Datalink`,
`CollisionDetect`.
*Gimbals*/antena: `Gimbal`, `ScanGimbal`, `StabilizingGimbal`, `Antenna`.
Armas: `Bomb`, `Missile`, `Aam` (`AamMissile`), `Agm` (`AgmMissile`), `Sam`, `Chaff`,
`Decoy`, `Flare`, `Bullet`, `Gun`, `Stores`, `SimpleStoresMgr` (`StoresMgr`), `FuelTank`.
Assinaturas: `SigConstant`, `SigSphere`, `SigPlate`, `SigDihedralCR`, `SigTrihedralCR`,
`SigSwitch`, `SigAzEl`, `IrSignature`, `AircraftIrSignature`, `IrShape`, `IrSphere`,
`IrBox`.
Rastreio: `Track`, `GmtiTrkMgr`, `AirTrkMgr`, `RwrTrkMgr`, `AirAngleOnlyTrkMgr`.
Ações: `ActionImagingSar`, `ActionWeaponRelease`, `ActionDecoyRelease`,
`ActionCamouflageType`.
Dados/agentes: `TargetData`, `SimAgent`, `MultiActorAgent`.

## 7.7 Não há registro global de fábricas

**Não existe no framework nenhum registro global de fábricas nem função de encadeamento.**
Não há `setFactory()`, não há lista de fábricas instaladas, e a ordem dos arquivos no
`meson.build` é irrelevante. Cada módulo expõe apenas uma **função livre**
`factory(const std::string&)`, e o único ponto do MIXR que consome uma fábrica é o
parâmetro `factory_func` de `edl_parser()` — um ponteiro de função, um só.

O encadeamento é **responsabilidade da aplicação**, escrito à mão:

```cpp
// A fabrica da aplicacao -- a ordem AQUI define a precedencia
base::Object* minhaFactory(const std::string& name)
{
    base::Object* obj {};

    // 1. Classes proprias da aplicacao vem primeiro:
    //    podem sobrepor um nome de fabrica dos modulos abaixo
    obj = minhaApp::factory(name);

    // 2. Depois as camadas do MIXR, da mais especifica para a mais geral
    if (obj == nullptr) obj = models::factory(name);
    if (obj == nullptr) obj = simulation::factory(name);
    if (obj == nullptr) obj = terrain::factory(name);
    if (obj == nullptr) obj = base::factory(name);

    return obj;   // nullptr -> o parser reporta "undefined factory name"
}
```

É essa ordem escrita à mão — e nada mais — que concretiza a promessa de que "um modelo
específico pode sobrepor-se a um genérico de mesmo nome".

---

# 8. CAMADA `base` — ESTRUTURAS DE CONTENÇÃO

O *parser* produz, a partir do texto EDL, uma árvore de objetos de tipos heterogêneos.
O framework precisa de um mecanismo capaz de carregar qualquer combinação desses tipos sem
conhecê-los de antemão. A solução é deliberadamente simples: como todo objeto deriva de
`Object`, basta uma **lista homogênea de `Object*`**.

Sobre essa lista constrói-se uma segunda decisão: **nomes como cidadãos de primeira
classe**. Cada entrada pode ser um par `(nome, objeto)` — exatamente a estrutura que o EDL
expressa com `nome: ( Tipo ... )`.

## 8.1 `List` — a lista e sua semântica de posse

`List` é lista duplamente encadeada de `Object*`. Cada nó é um `List::Item` que guarda o
ponteiro e os *links* `next`/`previous`. O que a distingue de um contêiner genérico é a
**semântica de posse**:

```cpp
// Inserir: a lista torna-se coproprietaria -- chama ref()
void List::addHead(Object* const obj)
{
    auto* d = new Item;
    d->value = obj;
    obj->ref();      // lista assume co-posse
    addHead(d);
}

// Remover por valor: a lista libera a posse -- chama unref()
bool List::remove(const Object* const obj)
{
    // ... localiza o Item ...
    remove(d);
    obj->unref();    // lista libera sua co-posse
    return true;
}

// Remover da cabeca: TRANSFERE a posse para o chamador -- NAO chama unref()
Object* List::removeHead()
{
    // ... remove o Item ...
    return obj;      // chamador recebe o objeto JA possuido; nao deve ref()
}
```

**REGRA — a assimetria de posse em `List`:**

- `addHead()`/`addTail()` chamam `ref()` — a lista torna-se co-proprietária.
- `remove(obj)` chama `unref()` — a lista abre mão da co-posse.
- `removeHead()`/`removeTail()`/`get()` **não** chamam `unref()` — **transferem** a posse
  ao chamador. O chamador *não deve* chamar `ref()` adicionalmente.

Padrão canônico de travessia:

```cpp
List::Item* item = minhaLista->getFirstItem();
while (item != nullptr) {
    Object* obj = item->getValue();
    // ... processa obj ...
    item = item->getNext();
}
```

Outras operações: `getPosition(n)` para acesso por índice (custo O(n)), `getIndex(obj)`
para localizar por ponteiro, `insert(newItem, refItem)` para inserção antes de um item de
referência, e `getNumberList(values[], max)` para extrair valores numéricos de uma lista
de `Number` para um array C:

```cpp
// EDL:  gains: [ 1.0  0.5  0.25 ]
// O parser produz uma List de Float; recuperamos assim:
double gains[8]{};
unsigned int n = slotList->getNumberList(gains, 8);
// gains[0]=1.0, gains[1]=0.5, gains[2]=0.25, n=3
```

## 8.2 `Pair` e `PairStream`

`Pair` associa um `Identifier*` (nome do *slot*) a um `Object*` (o valor), e corresponde
diretamente a uma entrada `nome: valor` no arquivo EDL.

O construtor exige ambos os campos e chama `ref()` no objeto recebido. `deleteData()`
chama `unref()` em ambos e os anula — idempotente. Em `copyData()`, `Pair` **não
compartilha ponteiros**: tanto o `Identifier` quanto o `Object` são clonados via
`clone()`, tornando cada `Pair` proprietário exclusivo de cópias próprias.

`PairStream` herda de `List` e a especializa para `Pair*`. É a estrutura que representa,
na árvore, qualquer bloco delimitado por chaves no EDL. Acrescenta três buscas:

```cpp
// Localiza o primeiro Pair cujo slot() == "ownship"
Pair* p = players->findByName("ownship");
if (p != nullptr) {
    auto* ac = dynamic_cast<Aircraft*>(p->object());
}

// Localiza o primeiro Pair cujo object() e do tipo Aircraft
Pair* p2 = players->findByType(typeid(Aircraft));

// Dado um ponteiro de objeto ja na lista, devolve o nome associado
// ATENCAO: retorna CLONE -- o chamador deve chamar unref()
const Identifier* nome = players->findName(ac);
if (nome != nullptr) {
    std::cout << *nome << std::endl;  // imprime "ownship"
    nome->unref();                    // OBRIGATORIO
}
```

**ARMADILHA — `PairStream::findName()` devolve um clone**, não o ponteiro original
guardado no `Pair`. O chamador é responsável por `unref()`. Mesmo padrão de transferência
de posse de `List::removeHead()`.

Idioma canônico de travessia de `PairStream` (presente em todo o framework):

```cpp
List::Item* item = players->getFirstItem();
while (item != nullptr) {
    const auto* pair = static_cast<const Pair*>(item->getValue());
    const auto* ac   = dynamic_cast<const Aircraft*>(pair->object());
    if (ac == nullptr) {
        std::cerr << "slot " << *pair->slot() << " nao e Aircraft\n";
    }
    item = item->getNext();
}
```

Reconhecer esse idioma — `getFirstItem()`, `getValue()`, `static_cast<Pair*>`,
`pair->object()`, `getNext()` — é essencial para ler qualquer trecho de código do framework
que processe configuração ou listas de *players*.

---

# 9. CAMADA `base` — SISTEMA DE UNIDADES

A primeira decisão da camada matemática é tratar grandezas físicas como **objetos
tipados**, não como `double`s nus. Motivação dupla: elimina uma classe inteira de erros
(não se passa um valor em pés onde se esperam metros, porque o tipo carrega a unidade) e
serve à composição dirigida por dados, já que cada grandeza é um `Object` instanciável
pelo *parser*.

## 9.1 O padrão de família

Cada grandeza é uma **família**: uma classe base abstrata que fixa uma **unidade de
referência**, e várias classes concretas — uma por unidade — que implementam dois métodos
virtuais puros:

```cpp
// include/mixr/base/units/Distances.hpp
class Distance : public Number {
    DECLARE_SUBCLASS(Distance, Number)
public:
    // converte o valor armazenado PARA metros (unidade de referencia)
    virtual double toDistance() const = 0;

    // converte um valor EM metros DE VOLTA para esta unidade
    virtual double fromDistance(const double) const = 0;

    // conveniencia: converte qualquer Distance para esta unidade
    double convert(const Distance& n) { return fromDistance(n.toDistance()); }
};
```

```cpp
class Meters : public Distance {
public:
    double toDistance() const override   { return val; }     // e a referencia
    double fromDistance(const double a) const override { return a; }
    static double convertStatic(const Distance& n) { return n.toDistance(); }
};

class Feet : public Distance {
public:
    double toDistance() const override   { return val * distance::FT2M; }  // 0.3048
    double fromDistance(const double a) const override { return a * distance::M2FT; }
    static double convertStatic(const Distance& n) {
        return n.toDistance() * distance::M2FT;
    }
};

class NauticalMiles : public Distance {
public:
    double toDistance() const override   { return val * distance::NM2M; }  // 1852.0
    double fromDistance(const double a) const override { return a * distance::M2NM; }
    static double convertStatic(const Distance& n) {
        return n.toDistance() * distance::M2NM;
    }
};
```

**Toda conversão se reduz a ir à referência e voltar.** Não há conversão direta Pés→NM — o
caminho sempre passa por metros, de forma transparente. Acrescentar uma unidade nova exige
apenas implementar esse par de funções; a conversão para todas as demais vem de graça.

## 9.2 `convertStatic` — conversão sem instanciar o tipo de destino

Cada subclasse concreta oferece `convertStatic(const Grandeza&)`, que recebe *qualquer*
instância da grandeza e devolve o valor na unidade da classe chamada. É o padrão mais
frequente no código do MIXR:

```cpp
// src/models/dynamics/RacModel.cpp
bool RacModel::setSlotCmdAltitude(const base::Distance* const msg)
{
    if (msg != nullptr) {
        // aceita Feet, Meters, NauticalMiles, etc. -- converte para metros
        double value = base::Meters::convertStatic(*msg);
        cmdAltitude = value;   // armazenado internamente em metros
        return true;
    }
    return false;
}

bool RacModel::setSlotCmdHeading(const base::Angle* const msg)
{
    if (msg != nullptr) {
        // aceita Degrees, Radians, Semicircles -- converte para graus
        double value = base::Degrees::convertStatic(*msg);
        cmdHeading = value;    // armazenado internamente em graus
        return true;
    }
    return false;
}
```

O *slot* declara apenas que espera um `base::Distance*` — qualquer subclasse serve:

```
( RacModel
    cmdAltitude: ( Feet 10000 )     // convertStatic -> 3048.0 metros
    cmdHeading:  ( Degrees 270 )    // convertStatic -> 270.0 graus
)
```

O mesmo *slot* aceitaria `( Meters 3048 )` ou `( NauticalMiles 1.646 )` sem qualquer
alteração no código C++.

## 9.3 Grandezas compostas

Algumas grandezas são razões entre duas unidades. O MIXR as representa com classes
dedicadas que guardam internamente dois valores normalizados e aceitam *slots* separados
para cada componente:

```
velocidade:
   ( LinearVelocity
      distance: ( Feet 90 )         -- setSlotDistance converte para metros
      time:     ( MilliSeconds 1 )  -- setSlotTime converte para segundos
   )
   -- armazenado internamente como 90*0.3048 / 0.001 = 27432 m/s
```

```cpp
// Consulta em m/s (unidade interna)
double mps = vel->getMetersPerSecond();

// Converte para pes por segundo
Feet   d;  Seconds t;
double fps = vel->convert(&d, &t);

// Converte para nos (milhas nauticas por hora)
NauticalMiles d2;  Hours h;
double kts = vel->convert(&d2, &h);
```

O mesmo padrão se aplica a `AngularVelocity`: armazena em rad/s e converte para qualquer
par `Angle`/`Time`.

## 9.4 Catálogo de famílias

| Grandeza (base abstrata) | Referência interna | Subclasses concretas | Domínio típico |
|---|---|---|---|
| `Time` | `Seconds` | `MilliSeconds`, `MicroSeconds`, `NanoSeconds`, `Minutes`, `Hours`, `Days` | temporização geral |
| `Angle` | **`Semicircles`** | `Degrees`, `Radians` | navegação, dinâmica |
| `Distance` | `Meters` | `CentiMeters`, `MicroMeters`, `Microns`, `KiloMeters`, `Inches`, `Feet`, `NauticalMiles`, `StatuteMiles` | altitude, alcance |
| `Mass` | `KiloGrams` | `Grams`, `Slugs` | dinâmica FPS |
| `Force` | `Newtons` | `KiloNewtons`, `PoundForces`, `Poundals` | empuxo |
| `Power` | `Watts` | `MilliWatts`, `KiloWatts`, `Horsepower`, `DecibelWatts`, `DecibelMilliWatts` | radar, RF |
| `Frequency` | `Hertz` | `KiloHertz`, `MegaHertz`, `GigaHertz`, `TeraHertz` | frequência de RF |
| `Energy` | `Joules` | `KiloWattHours`, `BTUs`, `Calories`, `FootPounds` | termodinâmica |
| `Area` | `SquareMeters` | `SquareFeet`, `SquareInches`, `SquareYards`, `SquareMiles`, `SquareCentiMeters`, `SquareMilliMeters`, `SquareKiloMeters`, `DecibelSquareMeters` | RCS em dBsm |
| `Volume` | `CubicMeters` | `CubicFeet`, `CubicInches`, `Liters` | tanques |

Sem famílias — classes isoladas derivadas diretamente de `Number`:

| Classe | Unidade interna | Forma |
|---|---|---|
| `LinearVelocity` | m/s | composta: *slots* `distance` + `time` |
| `AngularVelocity` | rad/s | composta: *slots* `angle` + `time` |
| `Density` | kg/m³ | composta |
| `FlowRate` | m³/s | composta |

**Nota sobre `Angle`:** sua referência interna são **Semicircles** (não radianos), que é o
formato nativo de hardware GPS e INS; `Radians` e `Degrees` convertem para e de
semicírculos. Isso é invisível ao código que consome a família — basta
`Degrees::convertStatic(angulo)`.

**ARMADILHA — não existem classes `Knots`, `Mach` ou `RPM`.** Velocidade não é uma família
de unidades no MIXR, e sim a grandeza composta `LinearVelocity`. Nós e Mach aparecem no
framework como **métodos** sobre valores em m/s — `Player::getTotalVelocityKts()`,
`Player::getMach()` — e não como tipos instanciáveis em EDL. O mesmo vale para
`AngularVelocity` e RPM.

**`Decibel` é um caso à parte:** deriva de `Number` sem pertencer a nenhuma família, e
representa uma *escala* logarítmica em vez de uma grandeza. É usada onde um valor em dB
precisa ser aceito diretamente — por exemplo, o *slot* `rcs` de `SigConstant`, que aceita
indiferentemente um `Number` em m², uma `Area` em qualquer unidade, ou um `Decibel` em
dBsm. As classes `DecibelWatts`, `DecibelMilliWatts` e `DecibelSquareMeters` são
independentes dela: pertencem, respectivamente, às famílias `Power` e `Area`, e apenas
aplicam a conversão logarítmica nos seus `toPower()`/`toArea()`.

A profusão de famílias reflete a natureza do MIXR como framework aeroespacial/militar que
precisa interoperar com hardware e modelos legados: radar em dBm e dBsm, motores em lbf e
lbs/hora, navegação em pés e nós, dinâmica inteiramente em *slugs* e pés.

---

# 10. CAMADA `base` — ÁLGEBRA (OSG) E OS DOIS MUNDOS DE TIPOS

Para geometria e cinemática, o MIXR incorpora os tipos algébricos do **OpenSceneGraph**
(OSG, licença OSGPL) em vez de reimplementá-los.

**O MIXR convive com dois mundos de tipos completamente distintos:**

| Mundo | Tipos | Características |
|---|---|---|
| `Object`s reflexivos | `Component`, `Player`, `Number`, … | contados por referência, clonáveis, visíveis ao *parser*, compartilháveis |
| Tipos algébricos | `Vec2d/f`, `Vec3d/f`, `Vec4d/f`, `Matrixd/f`, `Quat` | **valores leves**: sem herança de `Object`, sem `ref()`/`unref()`, manipulados por valor e operadores como qualquer `double` |

**POR QUÊ.** Vetores e matrizes nascem e morrem aos milhares dentro de laços numéricos a
cada *frame* e por *player*; impor alocação em *heap* e contagem de referências seria
proibitivo.

## 10.1 Vetores

Existem em dimensões 2, 3 e 4, em precisão simples (`f`) e dupla (`d`). Para dinâmica de
voo e simulação, `Vec3d` é o mais usado: posições e velocidades em referenciais locais
(NED) e globais (ECEF).

```cpp
#include "mixr/base/osg/Vec3d"

base::Vec3d pos(1000.0, 2000.0, -500.0);   // x, y, z em metros
base::Vec3d vel(100.0, 0.0, 0.0);          // m/s

base::Vec3d pos2 = pos + vel * dt;         // integracao Euler simples

double dp  = pos * vel;                    // dot product
base::Vec3d cross = pos ^ vel;             // cross product

double len = pos.length();
base::Vec3d unit = pos;
unit.normalize();                          // modifica in-place

double x = pos[0];    // equivalente a pos.x()
double y = pos.y();
double z = pos.z();
```

## 10.2 Matrizes

As matrizes 4×4 (`Matrixf`/`Matrixd`) cuidam de transformações de coordenadas e rotações.
A convenção é **vetor-linha à esquerda** — V_novo = V_antigo · M — herdada do OSG.

```cpp
#include "mixr/base/osg/Matrixd"

base::Matrixd rot;
rot.makeRotate(base::PI/4.0, 0.0, 0.0, 1.0);  // angulo, x, y, z

base::Vec3d v(1.0, 0.0, 0.0);
base::Vec3d vRot = v * rot;    // vetor-linha

base::Matrixd trans;
trans.makeTranslate(100.0, 200.0, 0.0);
base::Matrixd combined = rot * trans;   // ordem: rot primeiro, trans depois

base::Vec3d t;  base::Quat q;  base::Vec3d scale;  base::Quat so;
combined.decompose(t, q, scale, so);   // decomposicao TRS
```

## 10.3 Quaternions

Preferidos às matrizes de rotação por duas razões: **sem gimbal lock** e **interpolação
suave** (`slerp()`). Quaternion identidade tem w = 1, x = y = z = 0.

```cpp
#include "mixr/base/osg/Quat"

base::Quat q;
q.makeRotate(base::D2RCC * 30.0, 0.0, 1.0, 0.0);  // angulo (rad), eixo

base::Quat q2;
q2.makeRotate(base::D2RCC * 45.0, 0.0, 0.0, 1.0);
base::Quat qTotal = q * q2;   // primeiro q, depois q2

base::Matrixd m;
qTotal.get(m);                // Quat -> Matrixd
base::Quat q3(m);             // Matrixd -> Quat

base::Quat qa, qb, qInterp;
qInterp.slerp(0.5, qa, qb);  // orientacao a meio caminho

base::Vec3d v(1.0, 0.0, 0.0);
base::Vec3d vRot = qTotal * v;
```

## 10.4 Padrão de uso

No código do framework, os tipos algébricos aparecem tipicamente como variáveis locais em
métodos de dinâmica, nunca como membros gerenciados por `ref()`/`unref()`:

```cpp
// src/base/util/navDR_utils.hpp -- dead reckoning
bool deadReckoning(
    const double      dT,      // incremento de tempo (s)
    const unsigned int drNum,  // codigo de modelo DR
    const Vec3d& p0,           // posicao inicial (metros, ECEF)
    const Vec3d& v0,           // velocidade inicial (m/s)
    const Vec3d& a0,           // aceleracao inicial (m/s^2)
    const Vec3d& rpy0,         // angulos de Euler iniciais (rad)
    const Vec3d& av0,          // velocidade angular inicial (rad/s)
    Vec3d& pN,                 // OUT: posicao final
    Vec3d& vN,                 // OUT: velocidade final
    Vec3d& rpyN                // OUT: Euler final
);
```

Nenhum `new`, nenhum `ref()`, nenhuma herança de `Object` — apenas aritmética de ponto
flutuante sobre valores na *stack*.

## 10.5 `Transforms` — matrizes declaradas em EDL

Os tipos algébricos vivem no lado C++ da fronteira e não são visíveis ao *parser*. A
família `Transform` é a ponte: `Translation`, `Rotation` e `Scale` são `Object`s com
*slots* — declaráveis em EDL — cujo produto é uma `Matrixd`.

```
// Rotacao: os slots x/y/z aceitam Number (radianos) ou Angle (qualquer unidade)
( Rotation  z: ( Degrees 45 ) )

// Translacao em metros
( Translation  x: 100.0  y: 200.0 )

// Escala uniforme ou por eixo
( Scale  x: 2.0  y: 2.0  z: 1.0 )
```

Os *slots* aceitam tanto um `Number` cru quanto uma `Angle`: `z: 0.785` e
`z: ( Degrees 45 )` produzem a mesma matriz.

## 10.6 `base::Matrix` — a OUTRA álgebra

**ARMADILHA — existem duas classes chamadas "matriz" no MIXR.**

- **OSG**: `Matrixd`, `Matrixf` — **fixas em 4×4**, para transformações geométricas.
- **`base::Matrix`** (`include/mixr/base/Matrix.hpp`) — matriz numérica **geral N×M**, com
  decomposição LU, inversão e autovalores. É um `Object`, tem nome de fábrica `"Matrix"`, e
  acompanha-se de `RVector` e `CVector` (vetores linha e coluna).

Critério para distinguir ao ler código: se é geometria, é OSG e tem tamanho fixo; se é
álgebra linear numérica, é `base::Matrix`.
---

# 11. CAMADA `base` — GEODÉSIA (`EarthModel` e `nav_utils`)

A geodésia ilustra uma decisão de estilo recorrente no MIXR: como as conversões entre
referenciais são **transformações matemáticas sem estado**, elas são oferecidas como
**funções livres** no namespace `base::nav`, e não como hierarquia de classes. O único
estado relevante — a forma da Terra — é isolado em `EarthModel` e injetado como parâmetro
opcional, com o WGS-84 como padrão.

## 11.1 `EarthModel`

```cpp
// include/mixr/base/EarthModel.hpp
class EarthModel : public Object {
    DECLARE_SUBCLASS(EarthModel, Object)
public:
    double getA()  const;  // semieixo maior  a (metros)    -- WGS84: 6 378 137.0
    double getB()  const;  // semieixo menor  b (metros)    -- WGS84: 6 356 752.3142
    double getF()  const;  // achatamento  f = (a-b)/a      -- WGS84: 1/298.257
    double getE2() const;  // excentricidade^2  e^2 = 1-b^2/a^2

    static const EarthModel wgs84;  // instancia estatica -- passada como padrao
};
```

Passar `nullptr` equivale a usar `EarthModel::wgs84`. Para simulações que exigem outro
datum (Clarke 1866, GRS 80 etc.), basta passar a instância correspondente.
`EarthModel::getEarthModel(String)` resolve um nome textual para a constante estática.

## 11.2 Os três referenciais

| Referencial | Significado | Uso |
|---|---|---|
| **LLA** | geodésico: latitude/longitude em graus, altitude em metros acima do elipsoide | representação natural de posições geográficas; arquivos EDL e interfaces de usuário |
| **ECEF** | *Earth-Centered, Earth-Fixed*: cartesiano; eixo X aponta para Greenwich no equador, Z para o polo norte | referencial universal; independe de ponto local; é o que o GPS produz internamente e o que trafega na rede |
| **NED** | *North-East-Down*: plano tangente local (Down positivo para baixo), ancorado na *gaming area origin* | referencial de trabalho de sensores, radar e dinâmica de voo |

## 11.3 LLA ↔ ECEF

Cada função tem três sobrecargas: escalares, `Vec3d` e array C.

```cpp
#include "mixr/base/util/nav_utils.hpp"

// LLA -> ECEF (Vec3d)
base::Vec3d lla{36.12, -86.67, 3000.0};   // lat(deg), lon(deg), alt(m)
base::Vec3d ecef;
base::nav::convertGeod2Ecef(lla, &ecef);  // WGS-84 implicito
// resultado: ecef ~ (729762, -5498014, 3725022) metros

// ECEF -> LLA (escalares)
double lat{}, lon{}, alt{};
base::nav::convertEcef2Geod(
    ecef[0], ecef[1], ecef[2],   // X, Y, Z em metros
    &lat, &lon, &alt             // lat/lon em graus, alt em metros
);

// Com modelo de Terra alternativo
base::nav::convertGeod2Ecef(lla, &ecef, &myEarthModel);
```

O algoritmo iterativo de `convertEcef2Geod()` é o de **Bowring**, com tratamento especial
para pontos polares. `convertGeod2Ecef()` é não-iterativo, usando diretamente o raio de
curvatura da normal N(φ) = a / √(1 − e² sin²φ).

## 11.4 LLA ↔ NED — variante elíptica (E) vs. esférica (S)

A projeção ancora-se num ponto de referência (*gaming area origin*) e tem duas variantes
que expõem o compromisso entre precisão e custo:

```cpp
const double refLat{36.12};
const double refLon{-86.67};
const double cosRlat{std::cos(base::angle::D2RCC * refLat)};
const double sinRlat{std::sin(base::angle::D2RCC * refLat)};

base::Vec3d posNED;

// VARIANTE E -- elipsoide WGS-84: mais precisa, recomendada para areas grandes
base::nav::convertLL2PosVecE(
    refLat, refLon, sinRlat, cosRlat,   // referencia (com sin/cos pre-calculados)
    tgtLat, tgtLon, tgtAlt,             // destino
    &posNED                             // OUT: NED em metros (N, E, D=-alt)
);

// VARIANTE S -- Terra esferica: mais barata, suficiente para areas ate ~100 km
base::nav::convertLL2PosVecS(
    refLat, refLon, cosRlat,
    tgtLat, tgtLon, tgtAlt,
    &posNED
);
```

A variante esférica (`S`) usa aproximação de plano tangente puro
(ΔN ≈ Δφ × 60 × NM2M, ΔE ≈ Δλ × 60 × NM2M × cos φ), linear e muito barata. A elíptica
(`E`) corrige a curvatura do elipsoide e é necessária para áreas de jogo grandes.

`Player.cpp` usa exatamente esse par, escolhendo por um *flag* da `Simulation`:

```cpp
// src/models/player/Player.cpp
if (s->isGamingAreaUsingEarthModel()) {
    base::nav::convertLL2PosVecE(refLat, refLon, sinRlat, cosRlat,
                                  latitude, longitude, altitude,
                                  &posVecNED, em);
} else {
    base::nav::convertLL2PosVecS(refLat, refLon, cosRlat,
                                  latitude, longitude, altitude,
                                  &posVecNED);
}
```

## 11.5 ECEF ↔ NED — a matriz mundo

```cpp
// Computa M = Ry[-(90+lat)] * Rz[lon]
// tal que:  V_NED = V_ECEF * M   e   V_ECEF = V_NED * M^T
base::Matrixd wm;
base::nav::computeWorldMatrix(latitude, longitude, &wm);

base::Vec3d velNED{100.0, 0.0, 0.0};  // 100 m/s para o Norte
base::Vec3d velECEF = velNED * wm;    // vetor-linha: V * M

base::Matrixd wmT;
wmT.transpose(wm);
base::Vec3d velNED2 = velECEF * wmT;
```

`Player` mantém `wm` e a matriz combinada `rmW2B = rm * wm` (NED→corpo), recomputada
sempre que a posição ou a atitude mudam.

## 11.6 Círculo máximo

```cpp
double brg{};   // azimute verdadeiro (graus)
double dist{};  // distancia de superficie (milhas nauticas)

// Variante eliptica (precisa)
base::nav::gll2bd(36.12, -86.67, 48.85, 2.35, &brg, &dist);
// brg ~ 47.2 deg  dist ~ 3860 NM (Nashville -> Paris)

// Variante esferica (mais rapida, sufixo S)
base::nav::gll2bdS(36.12, -86.67, 48.85, 2.35, &brg, &dist);

// Dado ponto de origem, azimute e distancia: qual o ponto de destino?
double dlat{}, dlon{};
base::nav::gbd2ll(36.12, -86.67, 47.2, 500.0, &dlat, &dlon);

// Com altitude: inclui alcance inclinado e angulo de elevacao
double slantRng{}, elev{};
base::nav::glla2bd(36.12, -86.67, 0.0,
                   36.50, -86.00, 5000.0,
                   &brg, &slantRng, &dist, &elev);
```

Existe também `fll2bd()`, usada pelo `Autopilot`.

## 11.7 Matrizes de orientação e utilitários angulares

```cpp
// angulos em radianos
const double phi  {base::angle::D2RCC *   0.0};  // roll
const double theta{base::angle::D2RCC *  10.0};  // pitch
const double psi  {base::angle::D2RCC * 270.0};  // yaw

base::Matrixd rm;
base::nav::computeRotationalMatrix(phi, theta, psi, &rm);
// rm converte vetores NED para o referencial de corpo (body)

// versao em graus, com cache opcional de sin/cos
base::Vec2d scPhi, scTht, scPsi;
base::nav::computeRotationalMatrixDeg(0.0, 10.0, 270.0, &rm,
                                       &scPhi, &scTht, &scPsi);
```

```cpp
const double D2RCC{base::angle::D2RCC};   // pi/180
const double R2DCC{base::angle::R2DCC};   // 180/pi

// normalizacao de angulo para [-180, +180] graus
double hdg  = base::angle::aepcdDeg(370.0);   // -> 10.0
double hdg2 = base::angle::aepcdDeg(-200.0);  // -> 160.0

// em radianos: [-pi, +pi]
double ang = base::angle::aepcdRad(4.0);      // -> 4.0 - 2*pi ~ -2.28 rad
```

`aepcdDeg()`/`aepcdRad()` é usada em toda operação que manipula diferenças de longitude ou
azimutes, evitando descontinuidades na transição ±180°.

## 11.8 Fluxo completo de atualização de posição de um `Player`

```cpp
// 1. Armazena a posicao geodesica
latitude  = newLat;  longitude = newLon;  altitude  = newAlt;

// 2. LLA -> ECEF (para interoperabilidade e dead reckoning)
double lla[3]{ latitude, longitude, altitude };
double ecef[3]{};
base::nav::convertGeod2Ecef(lla, ecef, em);
posVecECEF.set(ecef[0], ecef[1], ecef[2]);

// 3. Computa a matriz mundo (orientacao do plano NED no ECEF)
base::nav::computeWorldMatrix(latitude, longitude, &wm);

// 4. LLA -> NED (relativo ao ponto de referencia da gaming area)
if (s->isGamingAreaUsingEarthModel())
    base::nav::convertLL2PosVecE(refLat, refLon, sinRlat, cosRlat,
                                  latitude, longitude, altitude, &posVecNED, em);
else
    base::nav::convertLL2PosVecS(refLat, refLon, cosRlat,
                                  latitude, longitude, altitude, &posVecNED);

// 5. Matriz combinada NED->corpo (para transformar vetores de sensores)
rmW2B = rm * wm;
```

---

# 12. CAMADA `base` — *THREADING*

Há **dois regimes de trabalho** fundamentalmente diferentes numa simulação em tempo real,
e `base` oferece uma abstração para cada:

| Regime | Abstração | Uso |
|---|---|---|
| **Cadência própria** — taxa fixa e autônoma, independente do ciclo da simulação | `PeriodicThread` | tratador de E/S, processamento de rede, laço de tempo crítico da `Station` |
| **Sincronização com fases** — deve marchar em sincronia com ciclo/*frame*/fase | `SyncThread` | processamento paralelo de *players* |

Uma terceira especialização, `OneShotThread`, executa `userFunc()` exatamente uma vez e
encerra — adequada para inicializações caras que não devem bloquear o laço de tempo real
(carregar um banco de terreno, abrir uma conexão). Ao contrário das duas anteriores, **não
tem arquivo de plataforma próprio**: todo o seu corpo cabe na classe genérica.

## 12.1 `AbstractThread` — a base comum

Herda de **`Referenced`**, não de `Object`: uma *thread* é infraestrutura, não um
componente configurável via EDL.

```cpp
// include/mixr/base/threads/AbstractThread.hpp
class AbstractThread : public Referenced   // nao herda de Object!
{
public:
    AbstractThread(Component* const parent);// parent: ponteiro FRACO (nao ref())

    bool start(const double priority);     // [0.0, 1.0] -> agendador do SO
    virtual bool terminate();
    bool isTerminated() const;
    bool setStackSize(const std::size_t);  // deve ser chamado ANTES de start()
    static int getNumProcessors();

private:
    virtual unsigned long mainThreadFunc() = 0;  // laco real (subclasse)
    static void* staticThreadFunc(void* lpParam); // ponto de entrada do SO

    Component* parent {};     // ponteiro FRACO -- nao ref()'ado no construtor
    double     priority {};   // prioridade portavel [0..1]
    bool       killed   {};   // flag de encerramento
    void*      theThread {};  // pthread_t* (Linux) ou HANDLE (Windows)
};
```

### O ponteiro fraco e o truque do `ref()` estático

O `parent` é guardado como ponteiro **fraco** — não `ref()`-eado no construtor. Criar uma
*thread* não deveria, por si só, manter o componente dono vivo artificialmente. A *thread*
é um servo do componente; se o componente decidir morrer, a *thread* deve poder morrer com
ele.

O `ref()` acontece apenas no instante em que a *thread* efetivamente começa a executar:

```cpp
// src/base/threads/platform/AbstractThread_linux.cpp
void* AbstractThread::staticThreadFunc(void* lpParam)
{
    const auto thread = static_cast<AbstractThread*>(lpParam);
    Component* parent { thread->getParent() };

    // Garante que nenhum dos dois seja destruido enquanto a thread roda
    thread->ref();
    parent->ref();

    unsigned long rtn { thread->mainThreadFunc() };

    thread->setTerminated();

    // Libera a posse -- se o contador chegar a zero, o objeto se destroi aqui
    parent->unref();
    thread->unref();

    return reinterpret_cast<void*>(rtn);
}
```

**POR QUÊ uma função estática.** Toda API de *threading* de SO (`pthread_create`,
`CreateThread`) exige ponto de entrada estático — não pode ser método virtual, pois a
*vtable* do objeto pode não estar estabelecida no instante da criação em todas as
plataformas. O MIXR contorna passando `this` como `void*` e recuperando dentro da função
estática. O método virtual `mainThreadFunc()` só é chamado **depois** dos dois `ref()`.

### Prioridade portável [0, 1]

| `priority` | Linux | Windows (HIGH_PRIORITY_CLASS) |
|---|---|---|
| 1.0 | `SCHED_FIFO` (max FIFO) | `TIME_CRITICAL` (15) |
| [0.9, 1.0) | `SCHED_FIFO` (90% FIFO) | `HIGHEST` (2) |
| [0.5, 0.6) | `SCHED_FIFO` (50% FIFO) | `NORMAL` (0) |
| 0.0 | `SCHED_OTHER` (normal) | `IDLE` (−15) |

**No Linux, qualquer prioridade > 0 usa `SCHED_FIFO` (tempo real)** — a *thread* não será
preemptada por *threads* de prioridade menor.

## 12.2 `PeriodicThread` — agendamento por tempo absoluto

```cpp
// include/mixr/base/threads/PeriodicThread.hpp
class PeriodicThread : public AbstractThread
{
public:
    PeriodicThread(Component* const parent, const double rate);

    double getRate() const;            // taxa em Hz
    int    getTotalFrameCount() const; // total de frames executados

    // Estatisticas de overrun (apenas Windows)
    const Statistic& getBustedFrameStats() const;

    // Se true, dt e ajustado para compensar overruns (apenas Windows)
    bool setVariableDeltaTimeFlag(const bool enable);

private:
    unsigned long mainThreadFunc() final; // implementado pela plataforma

    // O DESENVOLVEDOR IMPLEMENTA APENAS ISSO:
    virtual unsigned long userFunc(const double dt) = 0;

    double rate    {};   // taxa em Hz
    int    tcnt    {};   // contador de frames
    bool   vdtFlg  {};   // variable delta time flag
    Statistic bfStats{}; // estatisticas de overrun
};
```

Exemplo real de subclasse (a mais simples possível — delegar e sair):

```cpp
// src/simulation/StationBgPeriodicThread.cpp
unsigned long StationBgPeriodicThread::userFunc(const double dt)
{
    Station* station { static_cast<Station*>(getParent()) };
    station->processBackgroundTasks(dt);
    return 0;
}
```

### Como evitar *drift*: tempo absoluto vs. relativo

```cpp
// src/base/threads/platform/PeriodicThread_linux.cpp
unsigned long PeriodicThread::mainThreadFunc()
{
    const double dt { 1.0 / getRate() };
    int sec0  { static_cast<int>(dt) };
    int nsec0 { static_cast<int>((dt - sec0) * 1e9) };

    // tp: instante absoluto de referencia (CLOCK_REALTIME)
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);

    pthread_cond_t  cond  = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&mutex);

    while (!getParent()->isShutdown()) {

        this->userFunc(dt);   // trabalho do frame
        tcnt++;

        // Avanca tp por EXATAMENTE um periodo (absoluto)
        // Frame n sempre agendado para t0 + n*periodo
        tp.tv_nsec += nsec0;
        if (tp.tv_nsec >= 1000000000) {
            tp.tv_sec++;
            tp.tv_nsec -= 1000000000;
        }

        // Dorme ate o instante absoluto -- nao ate "daqui a dt segundos"
        pthread_cond_timedwait(&cond, &mutex, &tp);
    }

    pthread_mutex_unlock(&mutex);
    return 0;
}
```

Consequência: o *frame* n está sempre agendado para t₀ + n × período, independentemente de
quanto o *frame* anterior demorou. **No Windows a estratégia é diferente**: calcula-se
`refTime - now` e chama-se `Sleep()`. Se o resultado for negativo, o *frame* atrasou
(*overrun*), o excesso é registrado em `bfStats` e, se `vdtFlg` estiver ativo, o próximo
`dt` é aumentado para compensar.

## 12.3 `SyncThread` — sinalização por mutex pré-bloqueado

```cpp
// resumo de include/mixr/base/threads/SyncThread.hpp
class SyncThread : public AbstractThread
{
public:
    void signalStart();                                      // gerente -> thread
    void waitForCompleted();                                 // gerente espera
    static void waitForAllCompleted(SyncThread**, int num);  // espera N threads
    static int  waitForAnyCompleted(SyncThread**, int num);  // espera qualquer

    bool terminate() override;  // chama signalCompleted() ANTES de terminar!

protected:
    void waitForStart();     // thread espera o gerente
    void signalCompleted();  // thread avisa que terminou

private:
    virtual unsigned long userFunc() = 0;  // SEM dt -- sincrono por natureza

    void* startSig     {};  // mutex/semaforo de inicio   (criado JA BLOQUEADO)
    void* completedSig {};  // mutex/semaforo de conclusao (criado JA BLOQUEADO)
};
```

```cpp
// src/base/threads/SyncThread.cpp
unsigned long SyncThread::mainThreadFunc()
{
   unsigned long rtn{};

   bool ok{configThread()};

   while ( ok && getParent()->isNotShutdown() ) {

      waitForStart();                       // Wait for the start signal

      // Just in case we've been shutdown while we were waiting
      if (getParent()->isShutdown()) {
         signalCompleted();
         break;
      }

      this->userFunc();                     // User defined tasks

      signalCompleted();                    // Signal that we've completed
   }

   return rtn;
}
```

O `signalCompleted()` dentro do `if` de encerramento não é redundância: sem ele, um gerente
parado em `waitForCompleted()` no instante do desligamento esperaria para sempre.

### O mecanismo de sinalização no Linux

Implementação elegante e incomum: usa `pthread_mutex_t` **não como exclusão mútua, mas
como mecanismo de sinalização**. Os dois mutexes são criados **já bloqueados**:

```cpp
// src/base/threads/platform/SyncThread_linux.cpp
bool SyncThread::createSignals()
{
    pthread_mutex_t* m1 { new pthread_mutex_t };
    pthread_mutex_init(m1, nullptr);
    pthread_mutex_lock(m1);      // <-- bloqueia imediatamente
    startSig = m1;

    pthread_mutex_t* m2 { new pthread_mutex_t };
    pthread_mutex_init(m2, nullptr);
    pthread_mutex_lock(m2);      // <-- bloqueia imediatamente
    completedSig = m2;
    return true;
}

void SyncThread::signalStart()     { pthread_mutex_unlock((pthread_mutex_t*)startSig); }
void SyncThread::waitForStart()    { pthread_mutex_lock  ((pthread_mutex_t*)startSig); }
void SyncThread::signalCompleted() { pthread_mutex_unlock((pthread_mutex_t*)completedSig); }
void SyncThread::waitForCompleted(){ pthread_mutex_lock  ((pthread_mutex_t*)completedSig); }
```

O raciocínio: `lock` num mutex já bloqueado bloqueia o chamador até outro chamar `unlock`.
Esse uso **inverte a semântica habitual** de exclusão mútua — o mutex vira canal de
sinalização de custo mínimo. No Windows, o mesmo protocolo usa semáforos
(`CreateSemaphore`/`ReleaseSemaphore`/`WaitForSingleObject`).

### Padrão de uso: *pool* com barreira por fase

```cpp
// Criar e iniciar os workers (tipicamente em reset())
SyncThread* workers[3];
for (int i = 0; i < 3; i++) {
    workers[i] = new MyWorker(parent, i);  // MyWorker subclasse de SyncThread
    workers[i]->ref();
    workers[i]->start(0.9);  // prioridade alta -- SCHED_FIFO no Linux
    // o worker entra imediatamente em waitForStart() e dorme
}

// A cada fase (tipicamente em updateTC()):
for (int i = 0; i < 3; i++) workers[i]->signalStart();

doMyShare();                                     // fatia da thread principal

SyncThread::waitForAllCompleted(workers, 3);     // barreira da fase
// so aqui a proxima fase comeca
```

**Detalhe de encerramento:** `SyncThread::terminate()` chama `signalCompleted()` antes de
delegar a `AbstractThread::terminate()`. Sem isso, qualquer chamador bloqueado em
`waitForCompleted()` ficaria bloqueado para sempre — *deadlock* silencioso.

## 12.4 Arquivos pareados por plataforma

| Arquivo genérico | Linux (compilado) | Windows (NÃO compilado neste fork) |
|---|---|---|
| `AbstractThread.cpp` | `AbstractThread_linux.cpp` | `AbstractThread_msvc.cpp` |
| `PeriodicThread.cpp` | `PeriodicThread_linux.cpp` | `PeriodicThread_msvc.cpp` |
| `SyncThread.cpp` | `SyncThread_linux.cpp` | `SyncThread_msvc.cpp` |
| `util/system_utils.cpp` | `util/platform/system_linux.cpp` | `system_msvc.cpp`, `system_mingw.cpp` |
| `linkage/IoDevice.cpp` | `linkage/platform/UsbJoystick_linux.cpp` | `UsbJoystick_msvc.cpp` |

O genérico declara a classe e a interface; o de plataforma implementa os métodos que
dependem do SO. **Omitir o arquivo de plataforma causa erros de ligação.**

**ARMADILHA — a seleção NÃO é condicional neste fork.** Seria natural esperar que o
`meson.build` escolhesse as fontes por `host_machine.system()`. Não é o que acontece:

```python
# src/base/meson.build (trecho real)
source_files = [
    # ...
    './threads/AbstractThread.cpp',
    './threads/PeriodicThread.cpp',
    './threads/OneShotThread.cpp',
    './threads/SyncThread.cpp',
    './threads/platform/AbstractThread_linux.cpp',   # <-- sempre Linux
    './threads/platform/PeriodicThread_linux.cpp',
    './threads/platform/SyncThread_linux.cpp',
    # ...
    './util/platform/system_linux.cpp',              # <-- idem
]
```

Os cabeçalhos correspondentes continuam sendo selecionados por pré-processador —
`atomics.hpp` escolhe entre `atomics_linux.hpp`, `atomics_mingw.hpp` e `atomics_msvc.hpp`
conforme a plataforma — mas **nenhuma fonte Windows está ligada ao build**. Na prática,
**este fork compila apenas em Linux**.

---

# 13. CAMADA `base` — MÁQUINAS DE ESTADO (`StateMachine`)

`StateMachine` (nome de fábrica `"AbstractStateMachine"`) oferece máquinas de estado
finitas como `Component`s do framework. Três pilares:

- estados são **funções membro** mapeadas por número;
- transições usam um conjunto de **funções com pilha de retorno** (não um `switch`
  simples);
- a máquina admite **hierarquia** — estados podem ser implementados por outras
  `StateMachine`s filhas, definidas em EDL e ativadas pelo nome.

Como herda de `Component`, recebe `updateTC()`/`updateData()` de graça, participa da árvore
de contenção e responde a eventos (`ON_ENTRY`, `ON_EXIT`, `ON_RETURN`).

## 13.1 O ciclo de execução por *frame*

A cada `updateTC(dt)`, a máquina executa `step(dt)`, que percorre **cinco** etapas fixas:

1. **Efetivação da transição pedida no quadro anterior.** `goTo()`, `next()`, `call()` e
   `rtn()` **não mudam o estado**. Elas escrevem em campos sombra — `nState`, `nSubstate`,
   `nArg`, `nMode` — consumidos aqui, no topo do quadro **seguinte**.
2. **`preStateProc(dt)`** — função comum a todos os estados. Coleta entradas, atualiza
   sensores, qualquer preparação que toda iteração exige.
3. **`stateTable(state, CURR_STATE, dt)`** — despacha para a função ou submáquina do estado
   atual.
4. **função de estado ou submáquina** — executa a lógica, verifica condições, dispara
   transições.
5. **`postStateProc(dt)`** — função comum pós-estado, para envio de saídas e contabilidade.

**REGRA — transições são diferidas em um quadro.** Uma função de estado que chame `goTo(3)`
**continua sendo a função do estado atual até o fim daquele quadro** — e continua
executando as linhas seguintes. A troca só acontece na próxima passagem por `step()`. Três
consequências:

- `getMode()` devolve `NEW_STATE` no quadro **posterior** à transição, não naquele em que
  ela foi pedida. Daí o idioma
  `if (getMode() == Mode::NEW_STATE) { ... }` dentro da função do estado *novo*.
- Duas transições no mesmo quadro não se acumulam: a última sobrescreve a anterior, e
  nenhuma gera erro.
- `getArgument()` só tem valor útil nos modos `NEW_STATE` e `RTN_STATE`; num quadro de
  `HOLD_STATE` o argumento é zerado por `step()`.

**POR QUÊ diferir a transição.** Executar a troca no meio de uma função de estado
significaria que o resto daquela função rodaria num estado que já não é o dela — com
`getMode()`, `getArgument()` e a submáquina ativa já trocados sob os seus pés. Diferir
garante que cada quadro execute inteiramente num único estado (mesma disciplina de "estado
estável durante o quadro" das fases da simulação).

## 13.2 Definindo estados

```cpp
BEGIN_STATE_TABLE(MeuComportamento)
    STATE_FUNC( INIT_STATE, stateInicio  )  // estado inicial (obrigatorio)
    STATE_FUNC( 1,          stateIdle    )  // estado 1 -> chama stateIdle(dt)
    STATE_MACH( 2,          "sm01"       )  // estado 2 -> delega a "sm01"
    STATE_FUNC( 3,          stateAtaque  )
    STATE_FUNC( 9,          stateFinal   )  // estado terminal
END_STATE_TABLE()
```

- `STATE_FUNC` mapeia um número para método membro de assinatura
  `void f(const double dt)`.
- `STATE_MACH` mapeia um número para o nome de uma `StateMachine` filha declarada no
  *slot* `stateMachines:` em EDL.
- `ANY_STATE_FUNC` não recebe número: casa com *qualquer* estado que nenhuma linha anterior
  tenha reconhecido — o *default* do `switch`.

Dois números reservados (constantes estáticas de `StateMachine`):

| Constante | Valor | Significado |
|---|---|---|
| `INIT_STATE` | `0` | estado para o qual `reset()` leva a máquina |
| `INVALID_STATE` | `0xFFFF` | "nenhum estado" — valor de partida e de falha |

Uma máquina recém-construída está em `INVALID_STATE`: não executa nada até o primeiro
`reset()`, que a leva a `INIT_STATE`. **`STATE_FUNC(INIT_STATE, ...)` é obrigatório** — sem
ele a máquina sai do *reset* para um estado que a tabela não reconhece.

**POR QUÊ a "tabela de estados" não é uma tabela.** `BEGIN_STATE_TABLE` não constrói
estrutura de dados nenhuma — abre a definição do método virtual
`stateTable(cstate, code, dt)`, e cada `STATE_FUNC`/`STATE_MACH` expande para um
`if`/`else if` dentro dele. O segundo parâmetro é um `StateTableCode` com três valores,
porque a mesma cadeia de `if` responde a três perguntas:

| Código | Pergunta que a chamada faz |
|---|---|
| `CURR_STATE` | "execute o estado `cstate`" — chama a função membro |
| `TEST_STATE` | "o estado `cstate` existe?" — devolve-o, ou `INVALID_STATE` |
| `FIND_NEXT_STATE` | "qual o estado seguinte a `cstate` na tabela?" |

Isso explica os comportamentos: `goTo(n)` chama a tabela com `TEST_STATE` antes de mudar —
daí falhar silenciosamente para estado inexistente. `next()` chama com `FIND_NEXT_STATE`,
que devolve a **próxima linha escrita na tabela**, não `state + 1`: numa tabela que salta
de 3 para 9, `next()` a partir do estado 3 vai para 9. E `ANY_STATE_FUNC`, sob
`FIND_NEXT_STATE`, devolve literalmente `cstate + 1`, porque não há "próxima linha".

## 13.3 Funções de transição

```cpp
// --- transicoes de estado ---
goTo(n);         // vai direto ao estado n (sem retorno)
next();          // vai para o proximo estado em ordem na tabela
call(n);         // vai ao estado n e EMPILHA o estado atual para retorno
rtn();           // retorna ao estado que chamou call() -- desempilha

// --- transicoes de sub-estado (granularidade dentro de um estado) ---
nextSubstate();  // avanca substate++
goToSubstate(n); // vai ao sub-estado n

// --- transicoes para o estado pai (quando rodando como submaquina) ---
nextState();     // chama next() na maquina PAI
goToState(n);    // chama goTo(n) na maquina PAI
callState(n);    // chama call(n) na maquina PAI
rtnState();      // chama rtn() na maquina PAI
```

O par `call()`/`rtn()` permite que um estado "chame" uma sub-rotina de comportamento e
retorne ao ponto exato de onde saiu — ao longo de múltiplos *frames*. Esse "ponto exato" é
uma **pilha de dez posições**, com entradas paralelas para estado e sub-estado;
`callState()` empilha tanto na pilha da máquina pai quanto na da filha.

**ARMADILHA — todas as transições devolvem `bool`, e podem falhar em silêncio.**
`goTo(n)` falha se o estado `n` não existir na tabela; `next()` falha se não houver estado
seguinte; `call()` falha se a pilha de dez já estiver cheia; `rtn()` falha se a pilha
estiver vazia. **Em todos os casos o pedido é silenciosamente descartado** e a máquina
permanece no estado atual. Um `goTo()` para um número de estado inexistente não gera erro
nem trava: apenas não acontece.

## 13.4 Composição hierárquica via EDL

```cpp
BEGIN_STATE_TABLE(PlayerAutonomo)
    STATE_FUNC( INIT_STATE, stateInit    )
    STATE_FUNC( 1,          stateAguarda )  // aguarda ordem de inicio
    STATE_MACH( 2,          "busca"      )  // delega a SM nomeada "busca"
    STATE_FUNC( 3,          stateReporta )  // reporta resultado e encerra
END_STATE_TABLE()

void PlayerAutonomo::stateInit(const double dt) { goTo(1); }

void PlayerAutonomo::stateAguarda(const double dt)
{
    if (getMode() == Mode::NEW_STATE) timer = 0.0;
    timer += dt;
    if (ordemRecebida()) next();  // avanca para o estado 2 ("busca")
}

void PlayerAutonomo::stateReporta(const double dt)
{
    if (getMode() == Mode::NEW_STATE) {
        // getArgument() recebe o objeto passado por rtnState() da submaquina
        const auto* r = dynamic_cast<const Identifier*>(getArgument());
        if (r != nullptr) enviarRelatorio(*r);
    }
}
```

Duas submáquinas intercambiáveis:

```cpp
// BuscaLinear.cpp
BEGIN_STATE_TABLE(BuscaLinear)
    STATE_FUNC( INIT_STATE, stateInit     )
    STATE_FUNC( 1,          stateVarrer   )
    STATE_FUNC( 2,          stateConcluir )
END_STATE_TABLE()

void BuscaLinear::stateConcluir(const double dt)
{
    rtnState();   // sinaliza conclusao ao pai; pai avanca para stateReporta
}
```

A escolha é feita inteiramente em EDL, sem recompilar:

```
( PlayerAutonomo
    stateMachines: {
        busca: ( BuscaLinear )   -- estado 2 usa BuscaLinear
    }
)
```

```
( PlayerAutonomo
    stateMachines: {
        busca: ( BuscaEspiral )  -- estado 2 agora usa BuscaEspiral
    }
)
```

## 13.5 Eventos de submáquina

```cpp
BEGIN_EVENT_HANDLER(BuscaLinear)
    ON_EVENT(ON_ENTRY,  onEntry )  // estado 2 foi ativado
    ON_EVENT(ON_EXIT,   onExit  )  // estado 2 foi desativado
    ON_EVENT(ON_RETURN, onReturn)  // retorno de um callState() interno
END_EVENT_HANDLER()

bool BuscaLinear::onEntry(Object* const)
{
    // chamado TODA VEZ que o estado 2 e ativado -- nao apenas na primeira
    goTo(INIT_STATE);
    distanciaPercorrida = 0.0;
    return true;
}
```

`ON_ENTRY` é enviado **toda vez** que o estado é ativado. A submáquina **não é destruída**
ao ser desativada — é apenas pausada, pronta para ser reusada.

---

# 14. CAMADA `base` — UBF (*Unified Behavior Framework*)

O UBF (`base::ubf`) é uma arquitetura de controle baseada em comportamentos cuja decisão
central é **separar percepção, decisão e atuação** em três abstrações desacopladas. Um
comportamento não sabe *como* uma ação se executa; uma ação não sabe *quem* a decidiu.

## 14.1 As três abstrações

```cpp
// AbstractState: le o ator e constroi a visao do mundo.
// NAO e virtual pura -- tem corpo, e o corpo e a parte interessante.
class AbstractState : public Component {
public:
    // Chamado pelo Agent antes de genAction. A implementacao base percorre
    // os componentes filhos, faz dynamic_cast para AbstractState e recorre --
    // honrando isComponentSelected(). E isso que permite COMPOR estados:
    // um AbstractState pode ser a raiz de uma arvore de sub-estados.
    virtual void updateState(const base::Component* const actor);

    // Segunda travessia, independente de ator (contexto global da simulacao)
    virtual void updateGlobalState();

    const AbstractState* getUbfStateByType(const std::type_info&) const;
};

// AbstractBehavior: decide, a partir do estado, qual acao tomar
class AbstractBehavior : public Component {
public:
    // retorna uma Action pre-ref'd, ou nullptr se nao ha acao
    virtual AbstractAction* genAction(const AbstractState* const state,
                                      const double dt) = 0;
protected:
    int getVote() const;              // PROTEGIDO em AbstractBehavior
    virtual void setVote(const int);  // (em AbstractAction, os dois sao publicos)
};

// AbstractAction: sabe executar-se sobre o ator
class AbstractAction : public Object {
public:
    virtual bool execute(base::Component* actor) = 0;   // sem o 'const' do ponteiro
    int  getVote() const;         // voto herdado do behavior que a gerou
    void setVote(const int);
};
```

`AbstractAction::execute()` recebe o ator como parâmetro — a ação **não guarda referência
ao ator**, não o conhece em tempo de construção e não depende de estado global. É um objeto
de valor transitório: nasce em `genAction()`, executa em `execute()` e é destruída (via
`unref()`) em seguida.

## 14.2 O `Agent` (nome de fábrica `"UbfAgent"`)

```cpp
// src/base/ubf/Agent.cpp
void Agent::controller(const double dt)
{
    base::Component* actor { getActor() };

    if (actor != nullptr && getState() != nullptr && getBehavior() != nullptr) {

        // 1. PERCEPCAO: atualiza o estado a partir do ator
        getState()->updateState(actor);

        // 2. DECISAO: pede ao behavior uma acao
        AbstractAction* action { getBehavior()->genAction(state, dt) };

        // 3. ATUACAO: executa a acao sobre o ator (se houver)
        if (action != nullptr) {
            action->execute(actor);
            action->unref();   // Action e transitoria -- destruida aqui
        }
    }
}
```

`genAction()` pode retornar `nullptr` — o comportamento tem o direito de não recomendar
ação alguma num dado instante, sem que isso seja erro.

`AgentTC` (nome de fábrica `"UbfAgentTC"`) é subclasse trivial que sobrescreve `updateTC()`
em vez de `updateData()`, movendo o ciclo de controle para a *thread* crítica em tempo:

```cpp
class AgentTC : public Agent {
    DECLARE_SUBCLASS(AgentTC, Agent)
public:
    AgentTC();
    void updateTC(const double dt = 0.0) override;  // chama controller(dt)
    // updateData() NAO e sobrescrito -- nao roda na thread de fundo
};
```

## 14.3 O `Arbiter` (nome de fábrica `"UbfArbiter"`) — composição por votação

O `Arbiter` é, **ele próprio, um `AbstractBehavior`** — e contém uma lista de comportamentos
filhos. Quando `genAction()` é chamado nele:

1. Chama `genAction()` em cada comportamento filho, coletando as ações recomendadas numa
   lista (*action set*).
2. Seleciona a ação de maior `vote` como resultante (`genComplexAction()`).
3. Retorna essa ação ao chamador — que pode ser o `Agent` ou **outro `Arbiter`**.

```cpp
// src/base/ubf/Arbiter.cpp
AbstractAction* Arbiter::genAction(const AbstractState* const state, const double dt)
{
    // 1. coleta acoes de todos os behaviors filhos
    const auto actionSet = new base::List();
    base::List::Item* item { behaviors->getFirstItem() };
    while (item != nullptr) {
        const auto behavior = static_cast<AbstractBehavior*>(item->getValue());
        AbstractAction* action { behavior->genAction(state, dt) };
        if (action != nullptr) {
            actionSet->addTail(action);
            action->unref();
        }
        item = item->getNext();
    }

    // 2. arbitragem: seleciona a acao de maior voto
    AbstractAction* complexAction { genComplexAction(actionSet) };
    actionSet->unref();

    return complexAction;  // nullptr se nenhum behavior gerou acao
}

AbstractAction* Arbiter::genComplexAction(base::List* const actionSet)
{
    AbstractAction* complexAction {};
    int maxVote {};
    base::List::Item* item { actionSet->getFirstItem() };
    while (item != nullptr) {
        const auto action = static_cast<AbstractAction*>(item->getValue());
        if (maxVote == 0 || action->getVote() > maxVote) {
            if (complexAction != nullptr) complexAction->unref();
            complexAction = action;
            complexAction->ref();
            maxVote = action->getVote();
        }
        item = item->getNext();
    }

    // *** A LINHA QUE FAZ O ANINHAMENTO FUNCIONAR ***
    // Se ESTE arbitro tem um voto proprio, ele o CARIMBA na acao vencedora.
    if (getVote() > 0 && complexAction != nullptr)
        complexAction->setVote(getVote());

    return complexAction;   // pre-ref'd para o chamador
}
```

**POR QUÊ o árbitro carimba o próprio voto.** Sem esse carimbo, um `Arbiter` aninhado
devolveria ao árbitro pai uma ação com o voto do *comportamento neto* que a produziu — e o
pai compararia maçãs com laranjas, já que os votos dos netos foram calibrados numa escala
interna àquele subgrupo. Com o carimbo, cada árbitro apresenta-se ao nível de cima com *o
seu próprio peso*, e a escala de cada nível é independente das demais. É o mesmo princípio
de encapsulamento dos *slots*.

**ARMADILHA — o primeiro comportamento vence o desempate com voto zero.** O teste é
`maxVote == 0 || action->getVote() > maxVote`. Na primeira iteração `maxVote` vale zero, de
modo que a **primeira** ação da lista é sempre adotada como incumbente — mesmo com voto
zero. Se depois nenhum outro superar zero, ela vence. Um comportamento sem `vote:`
declarado, colocado no topo da lista, torna-se assim o padrão efetivo do árbitro.

## 14.4 Configuração em EDL

```
agente: ( UbfAgent
    state:    ( MeuEstado )        -- AbstractState concreto
    behavior:
        ( UbfArbiter               -- Arbiter e um Behavior
            behaviors: {
                b1: ( EvitarColisao   vote: 90 )  -- alta prioridade
                b2: ( BuscarAlvo      vote: 40 )  -- prioridade baixa
                b3: ( PadraoDeVoo     vote: 10 )  -- comportamento de fundo
            }
        )
)
```

A cada `updateData(dt)`, o `Agent` atualiza `MeuEstado` com o ator, passa o estado ao
`UbfArbiter`, que consulta os três comportamentos. Se `EvitarColisao` gera ação (vote 90) e
`BuscarAlvo` também (vote 40), o árbitro retorna a de `EvitarColisao`. Se `EvitarColisao`
não gera ação (`nullptr`), `BuscarAlvo` vence automaticamente.

**ARMADILHA — `UbfAgentTC` não é registrado pelo framework.** `base/factory.cpp` registra
`"UbfAgent"` e `"UbfArbiter"` — e mais nada do UBF. `AgentTC` existe como classe, tem nome
de fábrica declarado, mas **nenhuma fábrica do MIXR o constrói**. Escrever
`( UbfAgentTC ... )` em EDL falha, a menos que a aplicação o registre na sua própria
fábrica.

**ARMADILHA — um `Agent` não propaga atualizações aos filhos.** Nem `Agent::updateData()`
nem `AgentTC::updateTC()` chamam `BaseClass::update*()`: os dois chamam **apenas**
`controller(dt)`. Como `setState()` insere o estado na lista de componentes do agente, o
efeito é que **o `state` e qualquer outro filho do agente nunca recebem `updateTC()` nem
`updateData()`**. O próprio cabeçalho de `Agent.hpp` registra a intenção: as chamadas "não
são repassadas ao restante do framework de comportamento". Quem escreve um
`AbstractState` que dependa de ser atualizado pelo ciclo normal de componentes não será
servido — o estado só é atualizado por `updateState(actor)`, dentro de `controller()`.

A camada `models` estende o UBF com agentes cientes do contexto da simulação (`SimAgent`,
`MultiActorAgent`).

---

# 15. CAMADA `base` — INTERPOLADORES (LFI) E FUNÇÕES TABELADAS

Modelos de engenharia dependem de funções tabeladas — coeficientes aerodinâmicos (C_L,
C_D), curvas de motor, perfis de atmosfera. O MIXR oferece interpolação linear
multidimensional (**LFI**, *Linear Function Interpolation*) em **dois níveis**, por uma
razão que ecoa a dos "dois mundos" de tipos: velocidade e configurabilidade exigem
estratégias diferentes.

| Nível | Interface | Quando usar | Custo por chamada |
|---|---|---|---|
| Baixo | `lfi_1D` … `lfi_5D` (funções livres) | laços numéricos, sem EDL | O(1) com cache, O(n) sem |
| Alto | `Table1` … `Table5` (`Object`s com *slots*) | configuração via EDL | idem + *overhead* de `Object` |

## 15.1 A fórmula fundamental (1D)

Dados dois *breakpoints* consecutivos x₁ < x₂ com valores tabelados a₁ e a₂, e um ponto de
consulta x ∈ [x₁, x₂]:

```
m = (x - x1) / (x2 - x1)
f(x) = m * (a2 - a1) + a1
```

m ∈ [0,1] é a fração de interpolação. Código real:

```cpp
// src/base/util/lfi.cpp
// Localiza x2: primeiro breakpoint ACIMA de x  (versao sem cache -- busca linear)
unsigned int x2 { low + delta };
while (x > x_data[x2]) { x2 += delta; }

// Aplica a formula de interpolacao linear
const unsigned int x1 { x2 - delta };
const double m { (x - x_data[x1]) / (x_data[x2] - x_data[x1]) };
return m * (a_data[x2] - a_data[x1]) + a_data[x1];
```

Comportamento nas bordas, controlado pela *flag* `eFlg`:

- **`eFlg = false`** (padrão): retorna o valor do extremo mais próximo (*clamping*).
  f(x < x₀) = a₀ e f(x > x_{n−1}) = a_{n−1}.
- **`eFlg = true`**: extrapola linearmente além dos *breakpoints*, usando a reta definida
  pelo primeiro ou último par de pontos.

## 15.2 Redução dimensional (ND)

O salto de 1D para ND é feito por **redução dimensional**. Para f(x, y) em 2D:

1. Encontra os dois *breakpoints* consecutivos de y que encaixotam o ponto: y₁ ≤ y < y₂.
2. a₁ = `lfi_1D`(x, linha y₁) — a linha inteira de valores para y₁ é uma subtabela 1D em x.
3. a₂ = `lfi_1D`(x, linha y₂).
4. Interpola linearmente: m_y = (y − y₁)/(y₂ − y₁); f(x,y) = m_y (a₂ − a₁) + a₁.

```cpp
// src/base/util/lfi.cpp -- lfi_2D
const unsigned int y1  { y2 - delta };
const unsigned int ax1 { nx * y1 };
const double a1 { lfi_1D(x, x_data, nx, &a_data[ax1], eFlg, xbp) };

const unsigned int ax2 { nx * y2 };
const double a2 { lfi_1D(x, x_data, nx, &a_data[ax2], eFlg, xbp) };

const double m { (y - y_data[y1]) / (y_data[y2] - y_data[y1]) };
return m * (a2 - a1) + a1;
```

O padrão é estritamente recursivo: `lfi_3D` chama `lfi_2D` duas vezes e interpola em z;
`lfi_4D` chama `lfi_3D` duas vezes e interpola em w; e assim até `lfi_5D`.

## 15.3 Ordem dos *breakpoints* e o cache `xbp`

**Os *breakpoints* precisam ser ordenados — mas em QUALQUER das duas ordens.** Os arrays
devem ser estritamente monótonos (crescentes *ou* decrescentes), e o código detecta a ordem
automaticamente comparando os dois primeiros elementos:

```cpp
// src/base/util/lfi.cpp
unsigned int low{};
unsigned int high{ nx - 1 };
int delta{ 1 };                         // crescente por padrao

if (x_data[1] < x_data[0]) {           // decrescente?
    low   = nx - 1;                     // inverte: busca comeca no fim
    high  = 0;
    delta = -1;                         // passo da busca: x2--
}
```

A fórmula fica invariante porque `y1 = y2 - delta` produz sempre o índice *inferior* na
ordem lógica, independentemente da direção física do array.

**O cache `xbp`: de O(n) para O(1).** O parâmetro opcional `xbp` armazena o índice da
última consulta; a busca parte dele:

```cpp
x2 = *xbp;
if (x2 >= nx) x2 = 0;                           // seguranca

while (x > x_data[x2])        { x2 += delta; }  // sobe se passou
while (x < x_data[x2 - delta]) { x2 -= delta; } // desce se voltou

*xbp = x2;   // persiste para a proxima chamada
```

Numa simulação em tempo real, altitude, Mach e ângulo de ataque evoluem continuamente — o
*breakpoint* correto raramente muda entre *frames*. A busca termina em uma ou duas
comparações, tornando-se efetivamente O(1).

`TableStorage` encapsula os índices de cache para todas as dimensões:

```cpp
// include/mixr/base/functors/TableStorage.hpp
class TableStorage : public FStorage {
    DECLARE_SUBCLASS(TableStorage, FStorage)
public:
    TableStorage();
    unsigned int xbp {};  // cache x (dimensao 1)
    unsigned int ybp {};  // cache y (dimensao 2)
    unsigned int zbp {};  // cache z (dimensao 3)
    unsigned int wbp {};  // cache w (dimensao 4)
    unsigned int vbp {};  // cache v (dimensao 5)
};
```

## 15.4 A família `Table1`–`Table5`

Encapsula dados e *breakpoints* como `Object`s com *slots* configuráveis em EDL, e delega o
cálculo às funções livres. A hierarquia é estritamente aditiva: cada classe acrescenta um
eixo à anterior (`Table2` herda `Table1`, etc.).

```cpp
// src/base/functors/Table1.cpp
double Table1::lfi(const double iv1, FStorage* const f) const
{
    if (!valid) throw new ExpInvalidTable();   // tabela invalida

    if (f != nullptr) {
        const auto s = dynamic_cast<TableStorage*>(f);
        if (s == nullptr) throw new ExpInvalidFStorage();

        // COM cache: passa &s->xbp para lfi_1D
        return lfi_1D(iv1, getXData(), getNumXPoints(),
                      getDataTable(), isExtrapolationEnabled(),
                      &s->xbp);
    } else {
        // SEM cache: busca linear simples
        return lfi_1D(iv1, getXData(), getNumXPoints(),
                      getDataTable(), isExtrapolationEnabled());
    }
}
```

## 15.5 Exemplo completo: C_L(α, M)

Dados armazenados em **row-major**: cada linha de `data:` corresponde a um valor de `y`
(Mach), e as colunas a valores de `x` (α).

```
clTable: ( Table2
    data: [
       -- alpha:  -2      4      10     16   (graus)
          0.10   0.28   0.52   0.80        -- Mach 0.3
          0.11   0.30   0.55   0.85        -- Mach 0.6
          0.13   0.34   0.60   0.90        -- Mach 0.9
    ]
    x: [ -2.0   4.0   10.0   16.0 ]       -- breakpoints alpha (graus)
    y: [  0.3   0.6    0.9        ]       -- breakpoints Mach
)
```

```cpp
// Em reset(): cria o storage UMA VEZ por instancia de ator
base::TableStorage* fs { new base::TableStorage() };
fs->ref();

// Em updateTC() ou userFunc() -- a cada frame:
const double alpha_deg { getAlphaDegs() };
const double mach      { getMach() };
double cl { clTable->lfi(alpha_deg, mach, fs) };   // xbp/ybp quase nao mudam

// Ao destruir o ator (em deleteData()):
fs->unref();
fs = nullptr;
```

Para laços internos onde `Object` é peso excessivo — integração numérica de trajetórias —
as funções livres são a escolha correta, com cache gerenciado manualmente:

```cpp
static const double alpha_bp[] { -2.0, 4.0, 10.0, 16.0 };
static const double mach_bp[]  {  0.3, 0.6,  0.9 };
static const double cl_data[]  {
    0.10, 0.28, 0.52, 0.80,    // Mach 0.3
    0.11, 0.30, 0.55, 0.85,    // Mach 0.6
    0.13, 0.34, 0.60, 0.90,    // Mach 0.9
};

unsigned int xbp {};  // cache de alpha -- persiste entre iteracoes
unsigned int ybp {};  // cache de Mach

for (int i = 0; i < N; i++) {
    cl[i] = base::lfi_2D(alpha[i], mach[i],
                          alpha_bp, 4, mach_bp, 3, cl_data,
                          false,       // sem extrapolacao: clamp nas extremidades
                          &xbp, &ybp);
}
```

## 15.6 `Func1`–`Func5` e `Polynomial` — a função como *slot*

Um terceiro nível resolve um problema diferente: **como declarar que um *slot* aceita "uma
função de duas variáveis"**, sem fixar se ela será uma tabela ou uma fórmula.

```cpp
// resumo de include/mixr/base/functors/Func2.hpp
class Func2 : public Function {
public:
    virtual double f(const double iv1, const double iv2,
                     FStorage* const = nullptr) const;
};
```

A implementação da base é um **adaptador de tabela**: se nenhuma subclasse tratar a
chamada, delega à `Table` anexada pelo *slot* `table`, verificando a aridade (um `Func2`
rejeita qualquer tabela que não seja `Table2`).

**POR QUÊ um *slot* `Func2` aceita tabela *ou* fórmula.** Um sistema que precise de um
padrão de ganho de antena pode declarar seu *slot* como `Func2` — e o autor do cenário
decide se fornece uma `Table2` medida em câmara anecoica ou uma classe C++ com a fórmula
analítica. `Antenna` faz exatamente isso. Nenhum dos dois lados precisa negociar: o
framework entrega uma interface chamável.

`Polynomial` deriva de `Func1` e avalia f(x) = a₀ + a₁x + … + a_N x^N, com o grau inferido
do tamanho da lista (**máximo 32**):

```
// f(x) = 0.02 - 0.15x + 1.1x^2
( Polynomial  coefficients: [ 0.02  -0.15  1.1 ] )
```

Onde a curva é suave e conhecida, um polinômio evita dezenas de *breakpoints* — e, ao
contrário da tabela, não usa `FStorage` nenhum, porque não há intervalo a localizar.

**REGRA — obtenha o *storage* pela função, não à mão.** O caminho previsto é
`Function::storageFactory()`, que devolve o *storage* apropriado ao objeto concreto —
`TableStorage` para tabelas, `FStorage` simples para o resto, nada para `Polynomial`.
Usá-lo mantém o código correto se a fórmula for depois trocada por uma tabela, ou
vice-versa.

**ARMADILHA — as exceções de tabela são lançadas por PONTEIRO.** `Table1::lfi()` e
companhia lançam `throw new ExpInvalidTable()` — um **ponteiro**, não um objeto. A cláusula
de captura tem de ser `catch (ExpInvalidTable* e)`, e quem captura fica dono da exceção.
São **três**: `ExpInvalidTable`, `ExpInvalidFStorage` e `ExpInvalidVector` (esta última
lançada ao carregar uma lista de *breakpoints* malformada).
---

# 16. CAMADA `base` — CORES E ESPAÇOS DE COR

O sistema de cores é baseado em `Object`s configuráveis via EDL. `Color` armazena a cor
internamente como vetor RGBA (`Vec4d color`); `Hsv` e `Hsva` adicionam um segundo vetor
HSVA (`Vec4d hsv`) e mantêm os dois sincronizados via conversão automática nos *setters*.

| Classe | Espaço | Nome de fábrica | Uso típico |
|---|---|---|---|
| `Rgb` / `Rgba` | RGB(A) | `rgb` / `rgba` | componentes diretos; `Rgba` acrescenta *alpha* |
| `Hsv` / `Hsva` | HSV(A) | `hsv` / `hsva` | escolha perceptual de matiz; mapas de cor |
| `Hls` | HLS | `hls` | variante de HSV com *lightness* |
| `Cmy` | CMY | `cmy` | subtrativo, orientado a impressão |
| `Cie` | CIE | `cie` | colorimetria absoluta, com *white point* |
| `Yiq` | YIQ | `yiq` | luminância/crominância (vídeo NTSC) |

**ARMADILHA — todas as cores concretas registram-se em MINÚSCULAS.** A única em maiúscula
é `"Color"`, a classe base — que tem `EMPTY_SLOTTABLE` e por isso **não aceita nenhum
componente de cor**. Escrever `( Color red: 0.2 ... )` carrega, mas o `red:` é rejeitado.

## 16.1 A hierarquia `Color → Hsv → Hsva`

```cpp
// include/mixr/base/colors/Color.hpp
class Color : public Object {
public:
    enum { RED, GREEN, BLUE, ALPHA };  // indices do vetor

    double red()   const;  // [0.0, 1.0]
    double green() const;
    double blue()  const;
    double alpha() const;

    operator const Vec3d*() const;   // retorna RGB (sem alpha)
    operator const Vec4d*() const;   // retorna RGBA

    const Vec3d* getRGB()  const;
    const Vec4d* getRGBA() const;

protected:
    Vec4d color {};   // [RED, GREEN, BLUE, ALPHA] -- formato canonico
};
```

```cpp
// include/mixr/base/colors/Hsv.hpp
class Hsv : public Color {
public:
    enum { HUE, SATURATION, VALUE };  // indices do vetor hsv

    double hue()        const;   // [0.0, 360.0]  graus na roda de cores
    double saturation() const;   // [0.0, 1.0]  0 = branco, 1 = cor pura
    double value()      const;   // [0.0, 1.0]  0 = preto, 1 = maximo

    static void hsv2rgb(Vec4d& rgb, const Vec4d& hsv);
    static void rgb2hsv(Vec4d& hsv, const Vec4d& rgb);

    // os operadores Vec3d*() e Vec4d*() HERDAM de Color
    // e retornam 'color' (RGBA), NAO 'hsv'

protected:
    Vec4d hsv {};   // [HUE, SATURATION, VALUE, ALPHA]
};
```

**Decisão arquitetural central:** os operadores `Vec3d*()` e `Vec4d*()` herdados
**continuam retornando o vetor RGBA**, não o HSVA. Quem recebe um `Hsv*` e o converte com
`static_cast<const Vec4d*>(...)` obtém RGBA — o mesmo vetor de qualquer outro `Color`.

`Hsva` estende `Hsv` com um *slot* `alpha:` adicional e o método `colorInterpolate()`.

## 16.2 Configuração em EDL

```
-- cor RGB: componentes diretos em [0.0, 1.0]
corFundo: ( rgb   red: 0.2   green: 0.4   blue: 0.8  )

-- cor HSV: hue em graus, saturation e value em [0.0, 1.0]
corAlerta: ( hsv  hue: 0.0   saturation: 1.0   value: 1.0  )  -- vermelho puro

-- cor HSVA: como HSV com canal alpha
corOverlay: ( hsva  hue: 120.0  saturation: 0.8  value: 0.9  alpha: 0.6 )
```

Cada *setter* de `Hsv` chama `hsv2rgb(color, hsv)` automaticamente, mantendo o vetor RGBA
sempre sincronizado:

```cpp
// src/base/colors/Hsv.cpp
bool Hsv::setSlotHue(const Number* const msg)
{
    if (msg == nullptr) return false;
    double value { msg->getReal() };
    // nao ha restricao de intervalo para hue -- aepcdDeg() normaliza
    hsv[HUE] = value;
    hsv2rgb(color, hsv);   // atualiza o vetor RGBA canonico
    return true;
}
```

O mesmo padrão vale para `setSlotSaturation()`, `setSlotValue()` e `setSlotAlpha()`.

## 16.3 Conversão HSV ↔ RGB

Divide o círculo de matiz em seis setores de 60°:

```cpp
// src/base/colors/Hsv.cpp
void Hsv::hsv2rgb(Vec3d& rgb, const Vec3d& hsv)
{
    double h { angle::aepcdDeg(hsv[HUE]) };   // normaliza para [0, 360)
    if (h < 0.0) h += 360.0;
    const double s { hsv[SATURATION] };
    const double v { hsv[VALUE]      };

    if (s == 0.0) {
        rgb[RED] = rgb[GREEN] = rgb[BLUE] = v;   // saturacao zero: cinza
        return;
    }

    h /= 60.0;                        // setor [0..6)
    const auto i { static_cast<int>(h) };
    const double f { h - i };         // fracao dentro do setor
    const double p { v * (1.0 - s)           };   // projecao minima
    const double q { v * (1.0 - s * f)       };   // descida suave
    const double t { v * (1.0 - s * (1.0-f)) };   // subida suave

    // i=0: vermelho->amarelo   i=1: amarelo->verde   i=2: verde->ciano
    // i=3: ciano->azul         i=4: azul->magenta    i=5: magenta->vermelho
    switch (i) {
        case 0: rgb[RED]=v; rgb[GREEN]=t; rgb[BLUE]=p; break;
        case 1: rgb[RED]=q; rgb[GREEN]=v; rgb[BLUE]=p; break;
        case 2: rgb[RED]=p; rgb[GREEN]=v; rgb[BLUE]=t; break;
        case 3: rgb[RED]=p; rgb[GREEN]=q; rgb[BLUE]=v; break;
        case 4: rgb[RED]=t; rgb[GREEN]=p; rgb[BLUE]=v; break;
        case 5: rgb[RED]=v; rgb[GREEN]=p; rgb[BLUE]=q; break;
    }
}
```

## 16.4 `Hsva::colorInterpolate()` — mapas de cor

```cpp
// src/base/colors/Hsva.cpp
bool Hsva::colorInterpolate(
    const double value,
    const double minValue,
    const double maxValue,
    const Hsva&  minColor,  // cor no limite inferior
    const Hsva&  maxColor   // cor no limite superior
)
{
    // fracao de interpolacao: 0.0 = minColor, 1.0 = maxColor
    const double p { (value - minValue) / (maxValue - minValue) };

    Vec4d deltaColor { maxColor.hsv - minColor.hsv };

    // normaliza a diferenca de hue para [-180, +180]
    // evita o caminho longo ao cruzar 0/360 graus
    deltaColor[Hsv::HUE] = angle::aepcdDeg(deltaColor[Hsv::HUE]);

    Vec4d newColor { minColor.hsv + deltaColor * p };
    newColor[Hsv::HUE] = angle::aepcdDeg(newColor[Hsv::HUE]);
    if (newColor[Hsv::HUE] < 0.0) newColor[Hsv::HUE] += 360.0;

    setHSVA(newColor);   // atualiza hsv e dispara hsv2rgb()
    return true;
}
```

**O detalhe crítico é a normalização do delta de hue com `aepcdDeg()`**: sem ela, uma
interpolação de 350° (vermelho escuro) para 10° (vermelho claro) percorreria 340° pelo
caminho longo (passando por azul, verde e amarelo) em vez dos 20° pelo caminho curto.

```cpp
// Mapa de cor altitude -> HSV: azul (baixo) a vermelho (alto)
const Hsva corMinima { 240.0, 1.0, 1.0, 1.0 };  // hue=240 (azul)
const Hsva corMaxima {   0.0, 1.0, 1.0, 1.0 };  // hue=0   (vermelho)

Hsva corAtual;
corAtual.colorInterpolate(getAltitude(), 0.0, 10000.0, corMinima, corMaxima);

const Vec4d* rgba { static_cast<const Vec4d*>(&corAtual) };
// rgba[0]=R, rgba[1]=G, rgba[2]=B, rgba[3]=A
```

---

# 17. CAMADA `base` — UTILITÁRIOS (`base/util`)

## 17.1 `safe_ptr<T>` — referências duráveis sem vazamento

O idioma de referência manual (`ref()`/`unref()`) funciona bem em *slots* e passagens de
parâmetro, mas é frágil para **referências duráveis** — membros de classe que apontam para
outros objetos. `safe_ptr<T>` automatiza o ciclo de vida e usa um *spin-lock* para
segurança em ambiente *multithreaded*:

```cpp
// include/mixr/base/safe_ptr.hpp
template <class T>
class safe_ptr {
public:
    // construtor: chama ref() por padrao (refThis=true)
    safe_ptr(T* x, const bool refThis = true) : ptr(x) {
        if (ptr != nullptr && refThis) ptr->ref();
    }

    // destrutor: chama unref() automaticamente
    ~safe_ptr() {
        if (ptr != nullptr) ptr->unref();
    }

    // operador=: unref() no antigo, ref() no novo, com spin-lock
    safe_ptr<T>& operator=(T* x) {
        if (ptr != x) {
            lock();
            if (ptr != nullptr) ptr->unref();
            ptr = x;
            if (ptr != nullptr) ptr->ref();
            unlock();
        }
        return *this;
    }

    T* operator->()             { return ptr; }
    operator T*()               { return ptr; }
    bool operator==(const T* x) { return (ptr == x); }

private:
    T*   ptr       {};   // ponteiro gerenciado
    mutable long semaphore {};   // spin-lock para thread-safety
};
```

`getRefPtr()` retorna uma cópia **já `ref()`-eada** do ponteiro, para passagens entre
*threads*: quem recebe torna-se temporariamente co-proprietário, evitando que o objeto seja
destruído enquanto a outra *thread* ainda o usa.

Três padrões de uso:

```cpp
// Padrao 1: membro de classe -- substitui T* + ref()/unref() manual
class MeuComponente : public base::Component {
    base::safe_ptr<base::Table2> tabela;  // gerenciado automaticamente
};

// Padrao 2: new sem ref() extra (refThis=false -- ref cnt permanece 1)
base::safe_ptr<base::Object> sp;
sp.set(new base::Object(), false);   // nao chama ref(); sp assume a posse

// Padrao 3: passagem thread-safe
base::safe_ptr<Sensor> sensor { getSensor() };  // ref() no construtor
// destrutor chama unref() automaticamente ao sair do escopo
```

Existe também `safe_queue<T>` e `safe_stack<T>` (pilha/fila com trava para uso entre
*threads*).

## 17.2 `Timer`, `UpTimer`, `DownTimer` — cronômetros registrados globalmente

`Timer` auto-registra-se num array **estático global** ao ser construído e remove-se ao ser
destruído. Uma única chamada estática `Timer::updateTimers(dt)` percorre o array e avança
todos os cronômetros ativos — **nenhum componente precisa chamar `update()` no seu próprio
cronômetro**.

```cpp
// resumo de include/mixr/base/Timers.hpp
class Timer : public Object {
public:
    void   start();                    // inicia a contagem
    void   stop();                     // pausa sem resetar
    void   reset();                    // volta ao valor inicial
    void   restart();                  // reset() + start()

    double getCurrentTime() const;
    double getTimerValue()  const;     // intervalo configurado
    bool   isRunning()      const;
    bool   isExpired()      const;     // atingiu o limite?

    bool   alarm(const double atime);  // dispara em atime
    bool   alarm() const;

    // CHAMADA GLOBAL: avanca todos os timers de uma vez
    static void updateTimers(const double dt);

private:
    double ctime {};          // tempo corrente
    double timerValue {};     // intervalo (reset value)
    bool   active {};
    Type   dir { Type::DOWN }; // UP ou DOWN

    static Timer* timers[];   // array estatico (max 500)
    static unsigned int nTimers;
    static long semaphore;    // protege o array
};
```

| Classe | Direção | `isExpired()` | Pergunta que responde |
|---|---|---|---|
| `UpTimer` | sobe de 0 até o intervalo | `time >= timerValue` | "quanto tempo desde X?" |
| `DownTimer` | desce do intervalo até 0 | `time <= 0` | "quanto falta para X?" |

**Integração com o laço de simulação:** `updateTimers(dt)` ocorre **antes** de
`root->updateTC(dt)`, que por sua vez antecede `root->updateData(dt)`. Quando o
`updateTC()` de qualquer componente executa, todos os `Timer`s já refletem o estado do
*frame* atual:

```cpp
// Em initData() ou reset():
reloadTimer = new base::DownTimer(2.5);   // 2.5 segundos
reloadTimer->ref();
reloadTimer->start();

// Em updateTC():  -- updateTimers(dt) ja foi chamado antes
if (reloadTimer->isExpired()) {
    executarRecarga();
    reloadTimer->restart();   // reset + start
}
```

A escolha de array estático (em vez de lista dinâmica) elimina alocação durante o laço de
tempo real.

## 17.3 `include/mixr/config.hpp` — os oito parâmetros globais

Todos seguem o padrão `#ifndef` + `#define`, de modo que qualquer um pode ser redefinido
pela linha de comando do compilador (`-DMIXR_CONFIG_MAX_TRACKS=400`) sem editar o arquivo.
Como todos dimensionam arrays estáticos, aumentá-los é trocar memória por capacidade; a
alocação continua fora do laço de tempo real.

| Constante (prefixo `MIXR_CONFIG_`) | Padrão | Dimensiona |
|---|---:|---|
| `MAX_INTERVAL_TIMERS` | 500 | array estático de `Timer` |
| `MAX_PLAYERS_OF_INTEREST` | 4000 | `Gimbal::MAX_PLAYERS` e os arrays do `Tdb` |
| `RF_MAX_EMISSIONS` | 800 | filas de `Emission` em `RfSystem` |
| `MAX_TRACKS` | 200 | lista de `Track` do `TrackManager` |
| `MAX_REPORTS` | 200 | relatórios de detecção por quadro |
| `MAX_NETIO_ENTITIES` | 5000 | entidades de rede em `NetIO` |
| `MAX_NETIO_ENTITY_TYPES` | 1000 | tabela de tipos do `EntityMapper` |
| `MAX_NETIO_NEW_OUTGOING` | 150 | novos *players* publicados por quadro |

Além dessas, o arquivo define `MIXR_VERSION 170600`.

Não confundir os dois maiores: `MAX_PLAYERS_OF_INTEREST` (4000) é o teto de alvos que um
*gimbal* pode considerar num quadro; `RF_MAX_EMISSIONS` (800) limita as emissões RF em
trânsito.

## 17.4 Mapa dos cabeçalhos de `include/mixr/base/util/`

| Cabeçalho | O que traz |
|---|---|
| `constants.hpp` | `PI`, `LIGHTSPEED`, `BOLTZMANN`, `ETHG`, `UNDEFINED_VALUE` |
| `math_utils.hpp` | `alim()`, `sign()`, `lcm()`, funções trigonométricas auxiliares, `pow10Array()`, `multArrayConst()` |
| `nav_utils.hpp` | geodésia: `fll2bd()`, `convertLL2PosVec*()`, `gll2bd()`, `computeWorldMatrix()`, `computeRotationalMatrix()`, `computeEulerAngles()` |
| `navDR_utils.hpp` | *dead reckoning*: as nove funções `deadReckoning()` |
| `str_utils.hpp` | `lcStrcpy()`, `lcStrcat()` — envoltórios com limite de tamanho |
| `lfi.hpp` | interpolação linear de 1 a 5 dimensões |
| `atomics.hpp` | incremento/decremento atômicos — a base do `refCount`; seleciona `atomics_linux.hpp` / `atomics_mingw.hpp` / `atomics_msvc.hpp` por pré-processador |
| `system_utils.hpp` | `getComputerTime()`, `msleep()`, prioridades de *thread* |

**Estes são cabeçalhos de funções livres em *namespaces***, não classes: nada aqui herda de
`Object`, nada tem *slots*, nada aparece em EDL. As unidades de medida, ao contrário, são
classes — e é por isso que `base::angle::D2RCC` (uma constante) e `Degrees` (uma classe)
coexistem sem conflito.

## 17.5 Classes de `base` sem consumidores internos

Seis classes de `base` que nenhuma outra parte do MIXR usa (interessam apenas a quem for
escrever código novo):

- **`Statistic`** — calculadora estatística incremental: some pontos com `sigma()` e leia
  média, variância, desvio padrão, RMS, máximo e mínimo. Útil para instrumentar um modelo
  sem guardar histórico. (É consumida internamente pelas estatísticas de `tcFrame()`.)
- **`Locus`** — uma linha de pontos igualmente espaçados a partir de uma referência, numa
  dada direção e alcance. É a forma que um *radial* de terreno teria se alguém a usasse;
  **nenhum consumidor existe no fonte**.
- **`Stack` e `safe_stack`** — pilhas, a segunda com trava para uso entre *threads*. Esta
  última **tem** consumidores: `Antenna` e `IrSeeker` a usam como reserva de objetos de
  emissão livres, evitando alocação no laço de tempo crítico.
- **`MonitorMetrics`** (nome de fábrica `monitorMetrics`) — caracterização colorimétrica de
  um monitor, com curvas de luminância por canal e coordenadas CIE dos fósforos, para
  correção de cor fisicamente correta em cabines simuladas.
- **`FileReader`** — leitor de arquivos de **registro de comprimento fixo**, com *slots*
  `pathname`, `filename` e `recordLength`, e acesso posicional por número de registro.

**ARMADILHA — existem dois `FileReader`, e é o nome de fábrica que os separa.**
`base::FileReader` (acima) e `recorder::FileReader` são classes **sem parentesco**: uma lê
registros de tamanho fixo de um arquivo qualquer; a outra lê mensagens *protobuf* de um
arquivo de gravação. Em C++ os *namespaces* bastam. Em EDL não há *namespaces* — a colisão
foi resolvida pelo nome de fábrica: a de `base` registra-se como `"FileReader"`, e a de
`recorder` como `"RecorderFileReader"`. **Escrever `( FileReader ... )` num arquivo de
configuração cria a de `base`, mesmo dentro de um bloco de gravação.**

---

# 18. CAMADA `base` — TIPOS DE VALOR

Os literais que aparecem num arquivo EDL — uma *string* entre aspas, um número inteiro, um
booleano — **não são tipos primitivos de C++**: cada um se torna uma instância de uma
subclasse de `Object`, com a mesma cidadania das demais (contada por referência, clonável,
configurável por *slot*). Isso é necessário para que o despacho de `setSlotByIndex()`
receba literais e objetos de forma uniforme.

## 18.1 `String` e `Identifier`

```cpp
// resumo de include/mixr/base/String.hpp
class String : public Object {
public:
    // Manipulacao
    virtual void setStr(const char* s);       // copia s para o buffer interno
    virtual void catStr(const char* s);       // concatena s ao final
    std::size_t  len() const;
    bool         isEmpty() const;

    // Integracao com C++
    operator char*();
    operator const char*() const;

    // Integracao numerica
    bool isInteger() const;                   // "42" -> true
    int  getInteger() const;
    bool isNumber() const;                    // "3.14" -> true
    double getNumber() const;

    // Copia com largura e justificacao
    void setString(const String& str,
                   const std::size_t width,
                   const Justify j = Justify::NONE);  // LEFT RIGHT CENTER NONE

private:
    char*       str {};  // buffer
    std::size_t n   {};  // tamanho em uso
    std::size_t nn  {};  // tamanho alocado
};
```

`Identifier` herda de `String` e acrescenta **uma única regra**: toda atribuição substitui
espaços por *underscores*, tornando a *string* um identificador válido sem validação
externa.

```cpp
// src/base/Identifier.cpp
void Identifier::setStr(const char* string)
{
    if (string == nullptr || std::strlen(string) == 0) {
        BaseClass::setStr(string);
        return;
    }

    // copia substituindo espacos por underscores
    const std::size_t len { std::strlen(string) };
    const auto newStr { new char[len + 1] };
    for (unsigned int i = 0; i < len; i++) {
        newStr[i] = (string[i] == ' ') ? '_' : string[i];
    }
    newStr[len] = '\0';

    BaseClass::setStr(newStr);    // delega ao buffer de String
    delete[] newStr;
}
```

A mesma normalização é aplicada em `catStr()`. É por isso que `Identifier` é o tipo das
chaves de `Pair` e dos nomes de *slot*.

## 18.2 `Number` e as especializações do *parser*

```cpp
// resumo de include/mixr/base/numeric/Number.hpp
class Number : public Object {
public:
    // Constructores: aceitam qualquer tipo numerico, convertem para double
    Number(const double  value) { val = value;                      }
    Number(const float   value) { val = static_cast<double>(value); }
    Number(const int     value) { val = static_cast<double>(value); }
    Number(const int64_t value) { val = static_cast<double>(value); }
    Number(const bool    value) { val = (value ? 1.0 : 0.0);       }

    // Leitores uniformes -- funcionam em QUALQUER subclasse
    double  getReal()    const { return val;                         }
    float   getFloat()   const { return static_cast<float>(val);    }
    int     getInt()     const { return static_cast<int>(val);      }
    int64_t getInt64()   const { return static_cast<int64_t>(val);  }
    bool    getBoolean() const { return (val != 0.0);               }

    virtual void setValue(const double nv) { val = nv; }

protected:
    double val {};   // valor canonico
};
```

O *parser* instancia automaticamente a subclasse correta conforme o literal:

```
-- inteiro         -> Integer  (nome de fabrica "int")
altitude: 10000

-- ponto flutuante -> Float    (nome de fabrica "float")
ganho: 3.14

-- booleano        -> Boolean  (nome de fabrica "boolean")
ativo: true

-- qualquer slot "aceita Number" recebe qualquer dos tres acima
```

`Integer` e `Float` oferecem `+=`, `-=`, `*=` e `/=` (com proteção contra divisão por
zero). `Boolean` oferece `!`, `&&` e `||`.

O *slot* de índice **global 1** de `Number` é `"value"` — é ele que a forma posicional
`( Feet 10000 )` preenche.

## 18.3 `Complex`

```cpp
// include/mixr/base/numeric/Complex.hpp
class Complex : public Number {
    // slot: "imag"
public:
    Complex(const double r = 0.0, const double i = 0.0);

    double getImag() const;             // parte imaginaria
    double getMag()  const;             // modulo: sqrt(r^2 + i^2)
    double getArg()  const;             // argumento: atan2(imag, real)

    const Complex& operator+=(const Complex& z);
    const Complex& operator-=(const Complex& z);
    const Complex& operator*=(const Complex& z);
    const Complex& operator/=(const Complex& z);

    friend Complex operator+(const Complex& z1, const Complex& z2);
    friend Complex operator*(const Complex& z1, const Complex& z2);
    // ...
};
```

```
impedancia: ( Complex  value: 50.0   imag: 25.0 )   -- 50 + j25
```

## 18.4 `LatLon` — coordenadas como se escrevem numa carta

Uma latitude tem duas representações naturais: a que o código quer (−23,1667) e a que a
carta imprime (23° 10' S). `LatLon` deriva de `Number` e tem quatro *slots*:

```
// A forma legivel: 23 graus, 10 minutos, 12 segundos, Sul
latitude:  ( LatLon direction: s  degrees: 23  minutes: 10  seconds: 12.0 )
longitude: ( LatLon direction: w  degrees: 46  minutes: 20 )

// Equivale exatamente a escrever, em graus decimais com sinal:
latitude:  -23.17
longitude: -46.3333
```

Cada *setter* (`direction`, `degrees`, `minutes`, `seconds`) chama `computeVal()`, que
consolida grau/minuto/segundo mais o hemisfério no campo `val` herdado de `Number` — em
graus decimais, com sinal. A direção é um dos identificadores `n`, `s`, `e` ou `w`, e é ela
que decide o sinal.

**POR QUÊ `LatLon` deriva de `Number`.** Porque assim ela é aceita em **qualquer** *slot*
que espere um `Number`. `Player::setSlotInitLatitude()` recebe um `base::Number*` e chama
`getReal()` — não sabe, nem precisa saber, se o autor do cenário escreveu −23,17 ou a forma
sexagesimal.

## 18.5 `Operators` — aritmética declarativa em EDL

As classes `Add`, `Subtract`, `Multiply` e `Divide` (declaradas em
`numeric/Operators.hpp`) tomam direção diferente: em vez de uma função C++ que calcula um
valor, cada uma é um `Number` configurável por *slots*. Os operandos vêm do EDL; o
resultado — também um `Number` — pode alimentar o *slot* de outro objeto.

Duas particularidades:

```cpp
// src/base/numeric/Operators.cpp
IMPLEMENT_SUBCLASS(Add,      "+")
IMPLEMENT_SUBCLASS(Subtract, "-")
IMPLEMENT_SUBCLASS(Multiply, "*")
IMPLEMENT_SUBCLASS(Divide,   "/")
```

Escrever `( Divide ... )` em EDL produz `undefined factory name: Divide` — a forma correta
é `( / ... )`. Note também que `Subtract`, `Multiply` e `Divide` **herdam de `Add`** e
apenas sobrescrevem `operation()`; toda a maquinaria de operandos vive em `Add`.

A segunda é a forma dos operandos. `Add` declara os *slots* `n2`…`n10` — o **primeiro**
operando não é um deles: é o *slot* `value` herdado de `Number`. Um operador aceita, assim,
de **dois a dez** operandos:

```cpp
BEGIN_SLOTTABLE(Add)
    "n2",       //  2nd number (first number is from Number)
    "n3",       //  3rd number
    // ... ate ...
    "n10",      // 10th number
END_SLOTTABLE(Add)
```

```
-- velocidade = distancia / tempo, calculada em tempo de montagem da arvore
--   600.0 -> slot 1 ("value", de Number);  3.6 -> slot 2 ("n2", de Add)
velocidade: ( / 600.0 3.6 )
-- o slot "velocidade" recebe um Number com valor 600/3.6 = 166.67

-- a forma nomeada equivalente, mais verbosa:
velocidade: ( / value: 600.0  n2: 3.6 )

-- operadores aninham, e aceitam ate dez operandos
resultado: ( + ( * 2.0 3.0 )   -- 6.0
              4.0 )            -- + 4.0
-- resultado = 10.0

-- soma de cinco parcelas numa unica forma
total: ( + 1.0  2.0  3.0  4.0  5.0 )   -- 15.0
```

O efeito é uma forma limitada de expressão aritmética declarativa: um parâmetro pode ser
definido em função de outros, calculado no momento em que a árvore é montada, sem que
nenhuma linha de C++ precise ser escrita ou recompilada.

---

# 19. CAMADA `base` — COMUNICAÇÃO EM REDE (`base/network`)

Fornece a infraestrutura usada pela interoperabilidade (DIS, HLA): *sockets* UDP (unicast,
*broadcast*, *multicast*) e TCP (cliente e servidor). O padrão de projeto repete o já
familiar: uma interface abstrata (`NetHandler`) define o contrato, uma implementação
concreta (`PosixHandler`) o cumpre sobre *sockets* POSIX/Berkeley, e subclasses
especializam o tipo de *socket*.

## 19.1 `NetHandler` — o contrato

```cpp
// resumo de include/mixr/base/network/NetHandler.hpp
class NetHandler : public Component {
public:
    // Inicializa o socket (noWaitFlag = modo nao-bloqueante)
    virtual bool initNetwork(const bool noWaitFlag) = 0;

    virtual bool isConnected() const = 0;
    virtual bool closeConnection() = 0;

    virtual bool         sendData(const char* const packet, const int size) = 0;
    virtual unsigned int recvData(char* const packet, const int maxSize) = 0;

    virtual bool setBlocked() = 0;
    virtual bool setNoWait()  = 0;

    // Conversao de byte order em BLOCO (host <-> network, big-endian).
    // ATENCAO ao contrato: o buffer tem de ser 'nl' palavras de 4 bytes
    // SEGUIDAS de 'ns' palavras de 2 bytes. Devolvem void, nao bool.
    static void toNet (const void* const hostData, void* const netData,
                       const int nl, const int ns);
    static void toHost(const void* const netData, void* const hostData,
                       const int nl, const int ns);

    // Conversao campo a campo -- 16 sobrecargas (int16..uint64, float, double).
    // Sao ESTAS que o DIS usa ao montar um PDU.
    static void toNetOrder(int16_t* const, const int16_t);
    // ... e as demais ...
};
```

Duas formas de conversão de ordem de *bytes*, com propósitos distintos:

- **`toNet()`/`toHost()`** convertem um **bloco** de uma vez, e por isso impõem leiaute
  rígido: n_l palavras de 4 bytes seguidas de n_s palavras de 2.
- **`toNetOrder()`/`fromNetOrder()`**, com dezesseis sobrecargas, convertem **campo a
  campo** — e é essa que o `interop/` de fato usa ao montar um PDU, porque um *Entity
  State* mistura `double`s, `float`s e inteiros de vários tamanhos numa ordem que nenhum
  leiaute em bloco descreveria.

**POR QUÊ a conversão é um não-fazer-nada em máquinas *big-endian*.** Todas essas funções
consultam um *flag* estático, inicializado uma vez por `checkByteOrder()` — que simplesmente
testa se `htons(1) == 1`. Num *host* que já seja *big-endian*, converter seria **inverter
indevidamente**, e por isso as funções devolvem o valor intacto. É a razão de o código de
serialização do DIS chamar `swapBytes()` sob a condição `isNotNetworkByteOrder()`, e não
incondicionalmente.

## 19.2 `PosixHandler` — *slots* compartilhados e fluxo de inicialização

```
( UdpMulticastHandler          -- qualquer subclasse herda estes slots
    port:          2010         -- porta de destino
    localPort:     2011         -- porta local (necessaria para receber)
    shared:        1            -- SO_REUSEADDR
    sendBuffSizeKb: 32          -- buffer de envio (KB, padrao 32)
    recvBuffSizeKb: 128         -- buffer de recepcao (KB, padrao 128)
    localIpAddress: "192.168.1.10"  -- interface especifica (opcional)
    ignoreSourcePort: 2011      -- descarta o que vier desta porta de origem
)
```

**ARMADILHA — `port` e `localPort` NÃO são "destino" e "origem".** Os dois *slots* mudam de
papel conforme o *handler* esteja enviando ou recebendo:

| | Enviando | Recebendo |
|---|---|---|
| `port` | porta de **destino** | porta em que se escuta |
| `localPort` | porta de **origem** | porta em que se escuta |

E há o *fallback*: se `localPort` não for declarado, o *socket* liga-se a `port` —
`if (getLocalPort() != 0) addr.sin_port = htons(getLocalPort()); else addr.sin_port = htons(getPort());`,
no `bindSocket()` de cada subclasse UDP.

**Só `port` é obrigatório.** Declarar apenas ele dá um *handler* que envia e recebe na mesma
porta — o que costuma ser o que se quer em *multicast*, e quase nunca o que se quer em
*unicast* entre dois processos na mesma máquina.

`ignoreSourcePort` existe por causa desse último caso. Em *multicast* ou *broadcast*, tudo
o que o processo envia volta para ele próprio. O tratamento está dentro do laço de recepção
do `PosixHandler`: se a porta de origem do datagrama recebido for igual a
`ignoreSourcePort`, ele não é entregue — e o laço **tenta de novo**, em vez de devolver
zero *bytes*. Descartar não é o mesmo que "nada chegou".

Depois de cada recepção bem-sucedida, `getLastFromAddr()` e `getLastFromPort()` devolvem
quem enviou o último pacote. É como uma aplicação distingue vários emissores no mesmo grupo
*multicast* sem que o protocolo carregue essa informação.

**REGRA — `backlog` existe em uma única subclasse.** `backlog` não pertence ao
`PosixHandler` — é declarado só em `TcpServerMultiple`, e é o segundo argumento de
`listen()`: o número máximo de conexões pendentes na fila de aceitação. **O padrão é 1**, o
que significa que um servidor TCP MIXR recusa a segunda conexão simultânea a menos que se
declare outra coisa.

```cpp
// src/base/network/PosixHandler.cpp
bool PosixHandler::initNetwork(const bool noWaitFlag)
{
    // Ponto de extensao 1: cria o socket e configura opcoes especificas
    bool ok { init() };

    if (ok) {
        // Ponto de extensao 2: bind() + configura buffers de SO
        ok = bindSocket();

        if (ok) {
            if (noWaitFlag) ok = setNoWait();
            else            ok = setBlocked();
        }
    }

    initialized = ok;
    return ok;
}
```

Cada subclasse concreta sobrescreve `init()` e `bindSocket()`; o restante do fluxo é
herdado sem modificação.

## 19.3 Os tipos de *socket* concretos

### UDP

```
-- Unicast: envia para um destino especifico
rede: ( UdpUnicastHandler
    ipAddress: "192.168.1.20"   -- destino
    port: 3000
)

-- Broadcast: envia para toda a sub-rede.
-- O SO_BROADCAST e ligado sempre, em init() -- nao ha slot para ele.
-- O UNICO slot proprio da classe e networkMask, e ele e OBRIGATORIO:
-- e dele que sai o endereco de broadcast.
rede: ( UdpBroadcastHandler
    localIpAddress: "192.168.1.10"
    networkMask:    "255.255.255.0"
    port: 3001
    shared: 1
)

-- Multicast: modo preferido pelo DIS (grupos 224.x.x.x - 239.x.x.x)
rede: ( UdpMulticastHandler
    multicastGroup: "224.0.0.251"  -- grupo
    port:  2010
    ttl:   4           -- max 4 roteadores
    loopback: 1        -- receber o proprio envio (testes)
)
```

`UdpMulticastHandler` acrescenta `joinTheGroup()` ao fluxo de `initNetwork()`: após a
inicialização base, o *socket* inscreve-se no grupo via `IP_ADD_MEMBERSHIP`. O TTL controla
quantos roteadores o pacote pode atravessar — valor 1 limita à sub-rede local.

### TCP

`TcpHandler` é a base TCP e tem um construtor especial que recebe um *socket* já conectado
(`TcpHandler(LcSocket sn)`): é assim que um servidor cria um *handler* para cada cliente
aceito sem precisar inicializar do zero.

```cpp
// CLIENTE: inicia a conexao no initNetwork()
//   slot ipAddress = IP do servidor
class TcpClient : public TcpHandler {
    bool initNetwork(const bool noWait) override {
        // ... BaseClass::initNetwork() ...
        ::connect(socketNum, ...);   // <-- diferencial do cliente
    }
};

// SERVIDOR SIMPLES: aceita uma conexao, socket de escuta e substituido
class TcpServerSingle : public TcpHandler {
    bool initNetwork(const bool noWait) override {
        // ... listen() + accept() ...
    }
};

// SERVIDOR MULTIPLO: mantem socket de escuta ativo, cria filho por cliente
class TcpServerMultiple : public TcpHandler {
public:
    TcpHandler* acceptConnection();   // retorna TcpHandler pre-conectado
};
```

```cpp
// setup: inicia o socket de escuta
server->initNetwork(true);   // noWait = true

// a cada frame ou em thread dedicada:
TcpHandler* client { server->acceptConnection() };
if (client != nullptr) {
    // NAO chame ref() aqui: acceptConnection() faz 'new TcpHandler(...)',
    // portanto o objeto ja vem com contador 1 e a posse e sua.
    // Um ref() extra tornaria o unref() abaixo inocuo e vazaria o handler.

    // usa o cliente -- sendData()/recvData()
    client->unref();   // unico unref(): destroi quando terminar
}
```

Esses *handlers* são consumidos diretamente pela camada `interop`: um `NetIO` de DIS recebe
um `UdpMulticastHandler` configurado via EDL e o usa para `recvData()`/`sendData()` de
*Entity State PDUs* a cada *frame*.

---

# 20. CAMADA `base` — INTERFACES ABSTRATAS DE E/S (`base/concepts/linkage`)

Três classes **puramente abstratas** definem o contrato de entrada/saída entre a simulação
e dispositivos de hardware. A decisão de projeto é separar **interface** de
**implementação**: `base/concepts/linkage` define apenas o contrato; as implementações
concretas residem no módulo `linkage`.

## 20.1 `AbstractIoDevice` — interface com o hardware

```cpp
// resumo de include/mixr/base/concepts/linkage/AbstractIoDevice.hpp
class AbstractIoDevice : public Object {
public:
    virtual void reset() = 0;   // abre o dispositivo

    // Entradas discretas (bool): botoes, switches
    virtual int  getNumDiscreteInputChannels() const = 0;
    virtual int  getNumDiscreteInputPorts()    const = 0;
    virtual bool getDiscreteInput(bool* const value,
                                  const int channel, const int port) const = 0;

    virtual bool setDiscreteOutput(const bool value,
                                   const int channel, const int port) = 0;

    // Entradas analogicas (double): eixos, pedais
    virtual int  getNumAnalogInputs() const = 0;
    virtual bool getAnalogInput(double* const value, const int channel) const = 0;

    virtual int  getNumAnalogOutputs() const = 0;
    virtual bool setAnalogOutput(const double value, const int channel) = 0;

    // Leitura/escrita em batch (chamados pelo IoHandler)
    void processInputs(const double dt, AbstractIoData* const inData);
    void processOutputs(const double dt, const AbstractIoData* const outData);
};
```

**Canal e porta são endereçados por índice a partir de ZERO** neste nível.

## 20.2 `AbstractIoData` — o *buffer* de valores

```cpp
// resumo de include/mixr/base/concepts/linkage/AbstractIoData.hpp
class AbstractIoData : public Object {
public:
    virtual int getNumAnalogInputChannels()   const = 0;
    virtual int getNumDiscreteInputChannels() const = 0;
    virtual int getNumAnalogOutputChannels()  const = 0;
    virtual int getNumDiscreteOutputChannels()const = 0;

    virtual bool getAnalogInput(  const int ch, double* const v) const = 0;
    virtual bool getDiscreteInput(const int ch, bool*   const v) const = 0;
    virtual bool setAnalogInput(  const int ch, const double v)        = 0;
    virtual bool setDiscreteInput(const int ch, const bool   v)        = 0;

    virtual bool getAnalogOutput(  const int ch, double* const v) const = 0;
    virtual bool getDiscreteOutput(const int ch, bool*   const v) const = 0;
    virtual bool setAnalogOutput(  const int ch, const double v)        = 0;
    virtual bool setDiscreteOutput(const int ch, const bool   v)        = 0;

    virtual void clear() = 0;  // zera todos os canais
};
```

**Aqui os canais são 1-based** — ver a armadilha da seção do módulo `linkage`.

## 20.3 `AbstractIoHandler` — o orquestrador

```cpp
// resumo de include/mixr/base/concepts/linkage/AbstractIoHandler.hpp
class AbstractIoHandler : public Component {
public:
    // Leitura de todos os dispositivos registrados
    // (ignorado se processamento assincrono estiver ativo)
    void inputDevices(const double dt);
    void outputDevices(const double dt);

    AbstractIoData*       getInputData();
    const AbstractIoData* getInputData()  const;
    AbstractIoData*       getOutputData();
    const AbstractIoData* getOutputData() const;

    // Inicia processamento assincrono (opcional)
    // Cria uma PeriodicThread interna -- I/O roda desacoplado do frame
    void startAsyncProcessing();

private:
    virtual bool async() = 0;
    virtual void inputDevicesImpl(const double dt)  = 0;
    virtual void outputDevicesImpl(const double dt) = 0;
    virtual AbstractIoData* getInputDataImpl()      = 0;
    virtual AbstractIoData* getOutputDataImpl()     = 0;
    virtual void startAsyncProcessingImpl()         = 0;
};
```

O padrão **Non-Virtual Interface (NVI)** é visível: os métodos públicos verificam o estado
assíncrono antes de delegar para os métodos virtuais privados. No modo assíncrono, chamadas
síncronas vindas do laço principal são silenciosamente ignoradas — evitando condição de
corrida sem que o chamador precise saber do modo.

**Modo síncrono vs. assíncrono.** No síncrono (padrão), `Station` chama `inputDevices(dt)`
e `outputDevices(dt)` diretamente no seu `updateTC()`, dentro do *frame* de tempo crítico.
No assíncrono, `startAsyncProcessing()` cria uma `PeriodicThread` interna que corre a uma
taxa configurável, independente do *frame* de simulação.

**POR QUÊ as interfaces ficam em `base/`.** `Station` declara o *slot* `ioHandler:` do tipo
`AbstractIoHandler*`. Para que `Station` compile, o compilador precisa apenas **ver** essa
declaração de interface — a implementação concreta (`IoHandler`, `IoData`, `UsbJoystick`)
pode residir no módulo `linkage` e ser adicionada depois, sem alterar nem recompilar
`Station`. É a mesma separação que mantém a camada `base` compilável de forma autônoma.

---

# 21. CAMADA `simulation` — A ESPINHA DORSAL DE EXECUÇÃO

`base` fornece as ferramentas; `models` define **o quê** está sendo simulado. Entre as
duas, `simulation` define **como** a simulação é executada: o laço de tempo, a
sincronização entre *threads*, os pontos de extensão para rede, gravação de dados e geração
de imagem.

É uma biblioteca autônoma (`libmixr_simulation.so`) que depende apenas de `mixr_base` —
**nenhuma das 13 classes deste módulo precisa saber o que é um `Player`, um `Radar` ou um
`Aircraft`**.

Os 14 arquivos de fonte organizam-se em cinco grupos:

1. **Interfaces abstratas de infraestrutura** — `AbstractRecorderComponent`,
   `AbstractDataRecorder`, `AbstractIgHost`, `AbstractNetIO` e `AbstractNib`: os contratos
   que `Station` usa para conversar com gravadores, geradores de imagem e redes sem
   conhecer suas implementações concretas.
2. **Entidade simulada** — `AbstractPlayer`.
3. **O par *executive*** — `Simulation` e `Station`.
4. ***Threads* concretas** — `StationTcPeriodicThread`, `StationBgPeriodicThread`,
   `StationNetPeriodicThread` (criadas por `Station`) e `SimulationTcSyncThread[]`,
   `SimulationBgSyncThread[]` (criadas por `Simulation`).
5. **Fábrica** — `factory.cpp`.

| Thread | Criada por | Chama |
|---|---|---|
| `StationTcPeriodicThread` | `Station` | `processTimeCriticalTasks(dt)` |
| `StationBgPeriodicThread` | `Station` | `processBackgroundTasks(dt)` |
| `StationNetPeriodicThread` | `Station` | `processNetworkTasks(dt)` |
| `SimulationTcSyncThread[]` | `Simulation` | `updateTC(dt)` (fatia de *players*) |
| `SimulationBgSyncThread[]` | `Simulation` | `updateData(dt)` (fatia de *players*) |

Relação entre os dois componentes centrais: **`Station` contém `Simulation`** no *slot*
`simulation:` e é o *container* raiz de toda a árvore. `Simulation`, por sua vez, contém a
lista de *players* (acessível via `getPlayers()`) e o estado global de execução (tempo,
*freeze*, área de jogo).

Fluxo de controle a cada *frame*: começa na `StationTcPeriodicThread`, que acorda na taxa
configurada em `tcRate:` (padrão 50 Hz) e chama `Station::processTimeCriticalTasks(dt)`.
Este método itera `fastForwardRate` vezes sobre `tcFrame(dt)`, que aciona o laço de tempo
crítico de toda a árvore.

## 21.1 O ciclo/*frame*/fase — o contrato de tempo

Documentação interna de `Simulation.hpp`:

```cpp
// Cycles, frames and phases:
//
//   Each call to updateTC() by its manager (e.g., a Station class) is one
//   frame, and there are 16 frames, which are numbered 0 to 15, to one cycle.
//
//   Each frame is broken into 4 phases, which are numbered 0 to 3, and are
//   used to process dynamics, transmit sensor queries (e.g., emissions),
//   receive sensor queries, and information or control processing,
//   respectively. These phases allow for the synchronized flow of data
//   between players within a given frame. The player list is traversed for
//   each phase, therefore the player list is traversed 4 times the core
//   frame rate (e.g., 50 Hz frame rate is 200 Hz phase rate).
//
//   Use cycle(), frame() and phase() to get the current values, and use
//   getExecCounter() to get the total number of phases since the start
//   of the exec.
```

| Fase | Papel | Motivação |
|---|---|---|
| 0 | Dinâmica | move os *players* antes de qualquer interação |
| 1 | Sensores transmitem | todas as emissões do *frame* são geradas |
| 2 | Sensores recebem | só então cada *player* processa o que recebeu |
| 3 | Lógica e controle | decisões sobre um estado já estabilizado |

O ganho é o **determinismo**: ao garantir que *toda* emissão (fase 1) ocorra antes de
*qualquer* recepção (fase 2), o resultado de um *frame* deixa de depender da ordem em que
os *players* aparecem na lista. Sem isso, um *player* processado antes de outro "veria" um
mundo diferente, e a simulação não seria reproduzível nem segura para paralelizar. Pela
mesma lógica, **valores de tempo e data são fixados apenas no início de cada *frame***.

A estrutura em ciclo/*frame*/fase também serve de relógio para agendamento em sub-taxas:
lógica pode rodar a cada *frame*, a cada N *frames* do ciclo, ou a cada ciclo inteiro
(idioma `frame() % N == 0`).

### O Δt das fases: dividido na descida, recomposto na chegada

**Este é o ponto do modelo mais frequentemente mal interpretado.** O Δt que `Simulation`
passa à lista de *players* em cada fase é de fato Δt_frame/4 — a chamada é
`updateTcPlayerList(lista, dt0/4.0, ...)`, repetida quatro vezes por *frame*. A leitura
ingênua concluiria que a dinâmica de voo integra quatro vezes por *frame*, com um quarto do
passo. **Não é o que acontece.**

A razão é que **cada método de fase roda em UMA única das quatro fases, não em todas**. Um
subsistema que só age na fase 0 é chamado uma vez por *frame*; se recebesse Δt/4, integraria
com um quarto do tempo que de fato passou. Por isso `Player::updateTC()` e
`System::updateTC()` — os dois pontos onde o despacho por fase realmente ocorre — começam
recompondo o intervalo do *frame*:

```cpp
// src/models/system/System.cpp (o mesmo padrao aparece em Player.cpp)
// Delta time for methods that are running every fourth phase
double dt4{dt * 4.0};

switch (sim->phase()) {
   case 0 : dynamics(dt4); break;   // Dinamica
   case 1 : transmit(dt4); break;   // Sensores transmitem
   case 2 : receive(dt4);  break;   // Sensores recebem
   case 3 : process(dt4);  break;   // Logica e controle
}

// A propagacao aos subcomponentes usa 'dt' (o dt/4 original), porque
// cada subcomponente refaz este mesmo raciocinio para a sua propria fase.
BaseClass::updateTC(dt);
```

**Enunciado correto (as duas metades):** a travessia da lista de *players* ocorre a quatro
vezes a taxa do *frame*, **mas cada método de fase recebe o Δt INTEGRAL do *frame* e é
executado uma única vez por *frame***. A divisão por quatro na descida e a multiplicação
por quatro na chegada se cancelam. O que a subdivisão compra não é um passo de integração
menor, e sim a **ordenação**.

## 21.2 Modelo de *threading* da simulação

O par `updateTC()`/`updateData()` é também a fronteira do paralelismo. A *engine* pode
distribuir a travessia da lista de *players* por várias *threads* (configuráveis pelos
*slots* `numTcThreads` e `numBgThreads`), cada uma cuidando de um subconjunto.

A decisão que mantém o paralelismo correto é o **reagrupamento por fase**: as *threads*
críticas em tempo se sincronizam ao final de cada fase antes de seguir para a próxima.
Nenhuma *thread* entra na fase de recepção enquanto outra ainda transmite. **O paralelismo
não relaxa as garantias do modelo de fases; apenas as executa em paralelo dentro de cada
fase.**

Como há custo em gerenciar *threads*, o ganho só aparece com muitos *players*, e o
framework adota a regra: **não solicitar mais *threads* do que o número de CPUs menos um**,
preservando um núcleo para o SO e demais tarefas.
## 21.3 Interfaces abstratas de infraestrutura

`Station` precisa de protocolo fixo para conversar com subsistemas que não conhece
concretamente. A decisão é definir a interface aqui e deixar a implementação para módulos
posteriores (`recorder`, `interop`, e a aplicação do usuário no caso de `AbstractIgHost`).

### `AbstractRecorderComponent` — os dois filtros de evento

Em uma simulação com centenas de eventos por segundo, gravar tudo é proibitivo. Dois
filtros **mutuamente exclusivos**, configuráveis por *slot*:

```cpp
// resumo de include/mixr/simulation/AbstractRecorderComponent.hpp
class AbstractRecorderComponent : public base::Component {
    // slot "enabledList":  grava APENAS os IDs listados
    // slot "disabledList": grava TUDO EXCETO os IDs listados
    //   (os dois sao mutuamente exclusivos; enabledList tem prioridade)

    bool isDataEnabled(const unsigned int id) const;

private:
    unsigned int* enabledList  {};   // IDs habilitados (nullptr = todos)
    unsigned int  numEnabled   {};
    unsigned int* disabledList {};   // IDs desabilitados (nullptr = nenhum)
    unsigned int  numDisabled  {};
};

// Implementacao inline -- zero overhead quando evento esta filtrado
inline bool AbstractRecorderComponent::isDataEnabled(const unsigned int id) const
{
    bool ok { true };   // padrao: habilitado
    if (id != REID_END_OF_DATA) {
        if (numEnabled > 0 && enabledList != nullptr) {
            ok = false;
            for (unsigned int i = 0; !ok && i < numEnabled; i++)
                ok = (id == enabledList[i]);
        } else if (numDisabled > 0 && disabledList != nullptr) {
            for (unsigned int i = 0; ok && i < numDisabled; i++)
                ok = (id != disabledList[i]);
        }
    }
    return ok;
}
```

Os identificadores de evento são constantes `REID_*` definidas em
`dataRecorderTokens.hpp` — o vocabulário que é contrato entre `models`, `simulation` e
`recorder`. Note que `REID_END_OF_DATA` **nunca pode ser filtrado**.

### `AbstractDataRecorder`

```cpp
// include/mixr/simulation/AbstractDataRecorder.hpp
class AbstractDataRecorder : public AbstractRecorderComponent {
public:
    Station*    getStation();     // container Station (nao ref()'d)
    Simulation* getSimulation();  // Simulation aninhada (nao ref()'d)

    // Ponto de entrada de gravacao (chamado pelas macros)
    bool recordData(
        const unsigned int     id,          // REID_*
        const base::Object*    pObjects[4], // players/objetos associados
        const double           values[4]    // valores numericos associados
    );

    // Descarga de registros acumulados -- chamado no background por Station
    virtual void processRecords();

protected:
    virtual bool recordDataImp(const unsigned int id,
                               const base::Object* pObjects[4],
                               const double values[4]);
    virtual bool processUnhandledId(const unsigned int id) = 0;
};

// recordData() aplica o filtro antes de chamar a implementacao
inline bool AbstractDataRecorder::recordData(
    const unsigned int id, const base::Object* pObjects[4], const double values[4])
{
    bool recorded {};
    if (isDataEnabled(id)) {
        recorded = recordDataImp(id, pObjects, values);
        if (!recorded) processUnhandledId(id);
    }
    return recorded;
}
```

Uso típico via macros:

```cpp
BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_PLAYER_DATA )
    SAMPLE_1_OBJECT( this )
END_RECORD_DATA_SAMPLE()
```

### `AbstractIgHost` — interface para gerador de imagem

```cpp
// include/mixr/simulation/AbstractIgHost.hpp
class AbstractIgHost : public base::Component {
public:
    // Chamados por Station::updateTC() antes de tcFrame()
    virtual void setOwnship(AbstractPlayer* const)      = 0;
    virtual void setPlayerList(base::PairStream* const) = 0;

    // updateTC() e FINAL: sempre chama BaseClass::updateTC(dt) ANTES de updateIg()
    void updateTC(const double dt = 0.0) final {
        BaseClass::updateTC(dt);   // inicializacao obrigatoria da base
        updateIg(dt);              // ponto de extensao da subclasse
    }

private:
    virtual void updateIg(const double dt = 0.0) = 0;  // unico ponto de extensao
};
```

Marcar `updateTC()` como `final` é a garantia oferecida: **não há como uma subclasse pular
`BaseClass::updateTC(dt)` por engano** — o compilador rejeita.

### `AbstractNetIO`

```cpp
// include/mixr/simulation/AbstractNetIO.hpp
class AbstractNetIO : public base::Component {
public:
    // No maximo 2 redes simultaneas (dimensiona nibList em AbstractPlayer)
    static const unsigned int MAX_NETWORD_ID { 2 };

    // Chamados por StationNetPeriodicThread a cada frame de rede
    virtual void inputFrame(const double dt)  = 0;  // processa entidades recebidas
    virtual void outputFrame(const double dt) = 0;  // envia estado dos players locais

    virtual unsigned short getNetworkID() const = 0;
};
```

A separação entre `inputFrame` e `outputFrame` — em vez de um único `updateTC` — permite
que cada protocolo escolha sua estratégia de *threading*: **DIS pode processar entrada e
saída em *threads* distintas; HLA exige que ambas corram na mesma** (restrição da API RTI).

`MAX_NETWORD_ID = 2` não é apenas limite de `Station`: **dimensiona o array `nibList` de
cada `AbstractPlayer`**.

### `AbstractNib` — o bloco de interface de rede

NIB (*Network Interface Block*) representa a relação entre um *player* local e sua
contraparte na rede.

```cpp
// include/mixr/simulation/AbstractNib.hpp
class AbstractNib : public base::Component {
public:
    virtual unsigned short  getPlayerID()      const = 0;
    virtual AbstractNetIO*  getNetIO()               = 0;

    // Dead reckoning: extrapola posicao e orientacao entre updates de rede
    virtual bool updateDeadReckoning(
        const double   dt,              // delta de tempo (s)
        base::Vec3d*   const pNewPos,   // posicao extrapolada (saida)
        base::Vec3d*   const pNewAngles // orientacao extrapolada (saida)
    ) = 0;

    // Vetores de estado no instante T0 (base do dead reckoning)
    virtual const base::Vec3d& getDrVelocity()          const = 0;
    virtual const base::Vec3d& getDrAcceleration()      const = 0;
    virtual const base::Vec3d& getDrAngularVelocities() const = 0;
};
```

- **NIB de entrada** (`INPUT_NIB`): representa uma entidade recebida da rede — o
  `AbstractNetIO` cria o NIB e um *player networked* correspondente, **sem física local**.
- **NIB de saída** (`OUTPUT_NIB`): representa um *player* local sendo anunciado para a rede.

`updateDeadReckoning()` é virtual puro: cada protocolo implementa o algoritmo que melhor
lhe convém sobre as funções de `nav_utils`. *Dead reckoning* é o que permite a uma entidade
remota continuar se movendo de forma plausível entre dois *updates* de rede — evitando que
cada *player* precise transmitir seu estado a 50 Hz.

## 21.4 `AbstractPlayer` — a interface de toda entidade simulada

Contrato mínimo que `Simulation` exige de qualquer entidade. **Não é instanciada
diretamente**: `Player`, em `models`, é a implementação concreta.

```cpp
// resumo de include/mixr/simulation/AbstractPlayer.hpp
class AbstractPlayer : public base::Component {
    // Slots proprios (apenas dois)
    // slot "id":   identificador unico [1..65535]
    // slot "mode": modo inicial como string ("INACTIVE", "ACTIVE", "DEAD")
public:
    // enum SIMPLES (nao 'enum class'): os valores sao usados sem qualificacao,
    // como em  isMode(ACTIVE)  ou  setMode(DELETE_REQUEST).
    enum Mode {
        INACTIVE,       // existe na arvore, mas nao e atualizado nem transmitido
        ACTIVE,         // participa normalmente da simulacao
        KILLED,         // destruido            (condicao de "morto")
        CRASHED,        // colidiu com o solo   (condicao de "morto")
        DETONATED,      // arma detonou         (condicao de "morto")
        PRE_RELEASE,    // arma criada mas ainda nao liberada (apenas flyouts)
        LAUNCHED,       // arma inicial ja lancada (apenas a arma "original")
        DELETE_REQUEST  // sinaliza intencao de sair da lista ativa
    };

    bool isLocalPlayer()     const { return (nib == nullptr); }
    bool isNetworkedPlayer() const { return (nib != nullptr); }

    // NIBs de saida: um por rede (maximo MAX_NETWORD_ID = 2)
    AbstractNib* getLocalNib(const unsigned int netId);

    // NIB de entrada: no maximo um (identifica player networked)
    AbstractNib* getNib() { return nib; }

    void reset() override;  // local: restaura initMode; networked: aguarda rede

private:
    Mode mode     {ACTIVE};   // ATENCAO: o padrao e ACTIVE, nao INACTIVE
    Mode initMode {ACTIVE};

    AbstractNib** nibList {};          // nibList[MAX_NETWORD_ID]
    AbstractNib*  nib     {};          // NIB de entrada (nullptr = player local)
    unsigned int  netID   {};          // ID da rede do nib de entrada
};
```

**ARMADILHA — `Mode` é `enum` simples, não `enum class`**: no código do framework os
valores aparecem sem qualificação (`isMode(PRE_RELEASE)`, `setMode(DETONATED)`).

**ARMADILHA — o modo inicial padrão é `ACTIVE`**, não `INACTIVE`. Um *player* declarado em
EDL sem o *slot* `mode` **entra participando da simulação**. Quem quiser um *player*
dormente precisa declará-lo explicitamente. (Exceção: `AbstractWeapon` sobrescreve o padrão
para `INACTIVE` no construtor.)

### O ciclo de vida — dois caminhos distintos

```
Entidade comum:
  INACTIVE / ACTIVE  --(dano)-->  KILLED / CRASHED  -->  DELETE_REQUEST

Arma original:
  ACTIVE  --(release())-->  LAUNCHED

Flyout:
  PRE_RELEASE  --(fase 0)-->  ACTIVE  --(impacto)-->  DETONATED  -->  DELETE_REQUEST
```

A distinção entre `LAUNCHED` e `PRE_RELEASE` é exatamente a distinção entre a arma
*original* (a que fica presa ao cabide) e o *flyout* (o clone que voa).

`DELETE_REQUEST` é a **única saída**: um *player* nunca é destruído imediatamente (contagem
de referências). Ele sinaliza a intenção, e `Simulation::updatePlayerList()` o remove da
lista ativa no próximo ciclo — garantindo que nenhuma referência pendente de outro *player*
seja invalidada no meio de um quadro.

### NIBs e a distinção local vs. *networked*

O array `nibList` é alocado no construtor com `MAX_NETWORD_ID = 2` posições e liberado em
`deleteData()`. **`copyData()` deliberadamente NÃO copia esses ponteiros**: NIBs são
recriados pelo `AbstractNetIO` a cada `reset()`, pois representam relações de rede
transitórias.

A pergunta `isLocalPlayer()` — decidida pela presença de um NIB de entrada — governa
**quem** atualiza posição e orientação a cada quadro: um *player* local é movido pelo seu
`DynamicsModel`; um *player networked* é movido pelo *dead reckoning* do seu NIB. O mesmo
*flag* governa `reset()`: locais reiniciam para `initMode`; *networked* aguardam o próximo
*update* de rede.

## 21.5 `Simulation` — o *executive*

### Dois relógios independentes

```cpp
// src/simulation/Simulation.cpp
// Tempo do computador (UTC real -- nao e congelado)
unsigned long pcTvSec  {};  // segundos desde 1970-01-01
unsigned long pcTvUSec {};  // microssegundos fracionarios

// Tempo simulado (pode ser congelado)
unsigned long simTvSec  {};
unsigned long simTvUSec {};

// Inicio de updateTC():
double dt0 { dt };
if (isFrozen()) dt0 = 0.0;   // dt0 = 0 congela a simulacao...

// ...mas o tempo do computador avanca normalmente:
// pcTvSec e pcTvUSec sao atualizados independentemente de isFrozen()

// O tempo simulado e avancado com dt0 (pode ser zero):
unsigned long newSimTvUSec {
    simTvUSec + static_cast<unsigned long>(dt0 * 1000000.0 + 0.5)
};
while (newSimTvUSec >= 1000000) {
    newSimTvUSec -= 1000000;
    simTvSec++;
}
simTvUSec = newSimTvUSec;
```

Quando `isFrozen()` está ativo, `dt0 = 0` e nenhum *player* recebe avanço de tempo — a
física para. Mas o relógio do computador e as *threads* de sincronização continuam correndo
normalmente. A simulação pode ser pausada e retomada sem que os mecanismos de `SyncThread`
precisem saber do congelamento.

`simTimeSlaved` é uma variante: quando ativo, o tempo simulado avança com o `dt` real mesmo
quando congelado — útil para sincronizar com relógio externo de rede (HLA).

Acessores de tempo: `getExecTimeSec()`, `getSimTimeOfDay()`, `getSysTimeOfDay()`,
`getExecCounter()`, `cycle()`, `frame()`, `phase()`.

### O laço de quatro fases

```cpp
// src/simulation/Simulation.cpp
// Captura a lista de players para este frame (ref() implicito pelo safe_ptr)
base::safe_ptr<base::PairStream> currentPlayerList = players;

for (unsigned int f = 0; f < 4; f++) {

    setPhase(f);   // marca a fase -- players consultam phase() em tcFrame()

    if (reqTcThreads == 1) {
        // Thread unica: processa todos os players em sequencia
        updateTcPlayerList(currentPlayerList, (dt0 / 4.0), 1, 1);

    } else if (numTcThreads > 0) {
        // Pool de threads: distribui round-robin
        for (unsigned short i = 0; i < numTcThreads; i++) {
            unsigned int idx { static_cast<unsigned int>(i + 1) };
            tcThreads[i]->start0(currentPlayerList, (dt0/4.0), idx, reqTcThreads);
        }
        // Thread principal processa o ultimo subconjunto
        updateTcPlayerList(currentPlayerList, (dt0/4.0), reqTcThreads, reqTcThreads);

        // Barreira: espera todas as threads concluirem antes da proxima fase
        base::SyncThread** pp { reinterpret_cast<base::SyncThread**>(&tcThreads[0]) };
        base::SyncThread::waitForAllCompleted(pp, numTcThreads);
    }
}

// Atualiza contadores de ciclo/frame
int cframe { static_cast<int>(frame() + 1) };
if (cframe >= 16) { incCycle(); cframe = 0; }
setFrame(cframe);
setPhase(0);   // fase volta a 0 apos o frame
```

Frequência efetiva de `tcFrame()`: **quatro vezes a taxa do *frame*** — a 50 Hz, cada
*player* recebe 200 chamadas de `tcFrame(dt/4)` por segundo.

Os quatro nomes `dynamics()`, `transmit()`, `receive()` e `process()` são o vocabulário que
se reencontra em toda a camada `models`: um radar implementa `transmit()` e `receive()`, um
autopiloto implementa `process()`.

### Duas listas de *players*

```cpp
// resumo de include/mixr/simulation/Simulation.hpp
// Lista original: declarada no EDL, nunca modificada durante a execucao
base::safe_ptr<base::PairStream> origPlayers;

// Lista ativa: copia sincronizada, modificada no background
base::safe_ptr<base::PairStream> players;
```

`origPlayers` é populada no `reset()` a partir do *slot* `players:` do EDL e **nunca é
modificada durante a execução**.

**POR QUÊ duas listas em vez de uma lista com histórico.** A separação resolve o problema
do *reset* de graça. Reiniciar uma simulação significa desfazer tudo o que aconteceu: armas
lançadas, entidades vindas da rede, *players* destruídos. Com uma lista só, isso exigiria
registrar cada alteração para poder revertê-la. Com duas, o *reset* é uma reconstrução:
`players` é montada de novo a partir de `origPlayers`. Um míssil lançado no minuto anterior
nunca esteve em `origPlayers` — e portanto **desaparece sem que exista nenhuma linha de
código para removê-lo**.

```cpp
// src/simulation/Simulation.cpp
void Simulation::updatePlayerList()
{
    // Verifica se ha motivo para reconstruir a lista
    bool yes { newPlayerQueue.isNotEmpty() };  // novos players?

    if (!yes) {
        // Verifica DELETE_REQUEST
        base::safe_ptr<base::PairStream> pl = players;
        base::List::Item* item { pl->getFirstItem() };
        while (!yes && item != nullptr) {
            const auto pair = static_cast<base::Pair*>(item->getValue());
            const auto p    = static_cast<AbstractPlayer*>(pair->object());
            yes = p->isMode(AbstractPlayer::DELETE_REQUEST);
            item = item->getNext();
        }
    }

    if (yes) {
        // Reconstroi: copia origPlayers + networked players, descarta DELETE_REQUEST
        // e insere novos players da fila -- em ordem crescente de ID
        base::safe_ptr<base::PairStream> newList( new base::PairStream() );
        // ... copia origPlayers ...
        // ... copia players networked ativos ...
        // ... insere newPlayerQueue ...
        players = newList;   // swap atomico via safe_ptr
    }
}
```

**REGRA — a lista é substituída, nunca editada.** Duas metades que precisam existir juntas:

1. `updatePlayerList()` constrói uma **lista nova** e a instala com uma única atribuição a
   um `safe_ptr`. A lista antiga nunca é alterada no lugar.
2. `updateTC()` captura a lista **uma vez**, no início do *frame*
   (`safe_ptr<> currentPlayerList = players`), e segura essa referência pelas quatro fases.

Resultado: uma troca ocorrida no meio do *frame* não é vista por ele. **Um *player* nunca
aparece ou some entre a fase 1 e a fase 2.** É o mesmo mecanismo de
`Component::processComponents()`, aplicado à lista de *players*.

### Adição de *players* em tempo de execução

```cpp
// src/simulation/Simulation.cpp
// Sobrecarga 1: recebe o Pair ja montado -- e a que enfileira de fato
bool Simulation::addNewPlayer(base::Pair* const player)
{
    if (player == nullptr) return false;
    player->ref();              // a fila torna-se co-proprietaria
    newPlayerQueue.put(player); // safe_queue -- thread-safe
    return true;
}

// Sobrecarga 2: conveniencia -- monta o Pair a partir de um nome C
bool Simulation::addNewPlayer(const char* const playerName,
                              AbstractPlayer* const player)
{
    if (playerName == nullptr || player == nullptr) return false;
    const auto pair = new base::Pair(playerName, player);
    bool ok{addNewPlayer(pair)};
    pair->unref();              // a fila ja tem a sua referencia
    return ok;
}
```

### Identificadores únicos

`Simulation` é a fonte de IDs únicos para correlação entre módulos:

```cpp
unsigned short getNewEventID()          { return ++eventID; }    // explosoes, impactos
unsigned short getNewWeaponEventID()    { return ++eventWpnID; } // release <-> detonation
unsigned short getNewReleasedWeaponID() { return ++relWpnId;  }  // armas lancadas
```

Esses IDs aparecem nos *players* de `models`, nos registros do `recorder` e nos PDUs do
`interop/dis` — garantindo correlação consistente entre os três módulos sem que nenhum
precise coordenar com os outros.

### Distribuição *round-robin* entre *threads*

Algoritmo: **round-robin por índice**. A *thread* de índice `idx` (1-based) processa o 1º,
(1+n)º, (1+2n)º… *players* — onde `n` é o número total de *threads*. Com 2 *threads* e 6
*players*: thread 1 → P1, P3, P5; thread 2 → P2, P4, P6.

```cpp
// src/simulation/Simulation.cpp
void Simulation::updateTcPlayerList(
    base::PairStream* const playerList,
    const double dt,
    const unsigned int idx,   // indice desta thread (1-based)
    const unsigned int n)     // total de threads
{
    unsigned int index { idx };   // proximo player a processar
    unsigned int count {};

    base::List::Item* item { playerList->getFirstItem() };
    while (item != nullptr) {
        count++;
        if (count == index) {
            base::Pair* pair { static_cast<base::Pair*>(item->getValue()) };
            AbstractPlayer* ip { static_cast<AbstractPlayer*>(pair->object()) };
            ip->tcFrame(dt);   // processa este player
            index += n;        // pula para o proximo da fatia
        }
        item = item->getNext();
    }
}
```

Sem coordenação adicional, sem filas, sem alocação durante o quadro. O custo de despacho é
um `signalStart()` e um `waitForCompleted()`, ambos operações de mutex pré-bloqueado, com
custo de microssegundos. **Os *workers* nunca são destruídos e recriados entre quadros**:
são criados no `reset()` de `Simulation` e permanecem em `waitForStart()` entre fases,
consumindo zero CPU enquanto dormem.

### Limite de *threads*

```cpp
// src/simulation/Simulation.cpp -- setSlotNumTcThreads()
      // Max threads is the number of processors assigned to this
      // process minus one, or minimum of one.
      const int np{base::AbstractThread::getNumProcessors()};
      int maxT{1};
      if (np > 1) maxT = np - 1;

      const int v{msg->getInt()};
      if (v >= 1 && v <= maxT) {
         reqTcThreads = v;
         ok = true;
      } else {
         std::cerr << "Simulation::setSlotNumTcThreads(): invalid number of threads: " << v;
         std::cerr << "; number of processors = " << np;
         std::cerr << "; use [ 1 ... " << maxT << " ];" << std::endl;
      }
```

**ARMADILHA — o número de *threads* depende da MÁQUINA QUE CARREGA O ARQUIVO.** O teto não
é constante: é `getNumProcessors() - 1`, avaliado no momento em que o *slot* é preenchido.
Um cenário com `numTcThreads: 8`, escrito e testado numa estação de 16 núcleos, **falha ao
carregar** numa máquina de 4 — e a falha é a de qualquer *slot* rejeitado: o *parser* conta
mais um erro, o valor fica no padrão de 1, e a simulação sobe monothread. É por isso que
vale conferir `num_errors` depois de carregar.

**POR QUÊ reservar um núcleo.** As *threads* de tempo crítico rodam em `SCHED_FIFO` quando
a prioridade é maior que zero, e uma *thread* `SCHED_FIFO` não é preemptada por nada de
prioridade menor. Ocupar *todos* os núcleos com elas deixaria o SO — e as próprias
*threads* de fundo e de rede do MIXR — sem onde rodar. O núcleo reservado não é folga: é o
que mantém a máquina responsiva.

## 21.6 `Station` — a raiz da simulação

`Station` é o `Component` de nível mais alto de toda simulação MIXR — a raiz da árvore que
o EDL constrói. **Não simula nada por si só**; organiza e dá ritmo aos subsistemas: o
*executive* (`Simulation`), as redes (`AbstractNetIO`), os geradores de imagem
(`AbstractIgHost`), o hardware de E/S (`AbstractIoHandler`) e o gravador
(`AbstractDataRecorder`).

### Configuração via EDL

```
( Station
    simulation:       ( Simulation ... )     -- executive obrigatorio
    networks:         { net1: ( DisNetIO ... ) } -- lista de redes (opcional)
    igHosts:          { ig1: ( MyIgHost ) }  -- geradores de imagem (opcional)
    ioHandler:        ( MeuIoHandler ... )   -- handler de E/S da aplicacao (opcional)
    dataRecorder:     ( DataRecorder ... )   -- gravador (opcional)

    -- taxas das threads (Hz); 0 = sem thread, roda sincronamente
    tcRate:           50                     -- thread TC a 50 Hz (padrao)
    tcPriority:       0.9                    -- SCHED_FIFO no Linux
    bgRate:           20                     -- thread background a 20 Hz
    netRate:          50                     -- thread de rede a 50 Hz

    fastForwardRate:  1                      -- 1 = tempo real; 4 = 4x mais rapido
    ownship:          "ownship"              -- nome do player principal

    enableUpdateTimers: true                 -- chama Timer::updateTimers() no TC
    startupResetTimer: ( Seconds 0.1 )       -- envia RESET_EVENT apos 0.1s
)
```

Uma taxa igual a zero (`bgRate: 0`) significa que o processamento correspondente roda
sincronamente na *thread* que chama `updateData()` — útil para depuração ou quando não há
CPUs extras disponíveis.

### `Station::updateTC(dt)` — sete passos em ordem fixa

```cpp
// src/simulation/Station.cpp
void Station::updateTC(const double dt)
{
    // (1) Avanca todos os Timers antes de qualquer logica
    //     (garante que isExpired() retorna valores corretos no frame atual)
    if (isUpdateTimersEnabled()) {
        base::Timer::updateTimers(dt);
    }

    // (2) Le o hardware de E/S (joystick, pedais, paineis)
    if (ioHandler != nullptr) {
        ioHandler->tcFrame(dt);
    }

    // (3) Copia os valores lidos para os canais de entrada
    inputDevices(dt);

    // (4) FISICA: percorre todos os players em 4 fases (o passo mais custoso)
    if (sim != nullptr) sim->tcFrame(dt);

    // (5) Copia os valores de saida para o hardware
    outputDevices(dt);

    // (6) Atualiza os geradores de imagem (IG hosts)
    //     Passa ownship e lista de players para cada IG antes de tcFrame
    if (sim != nullptr && igHosts != nullptr) {
        base::PairStream* playerList { sim->getPlayers() };
        base::List::Item* item { igHosts->getFirstItem() };
        while (item != nullptr) {
            const auto pair = static_cast<base::Pair*>(item->getValue());
            const auto p    = static_cast<AbstractIgHost*>(pair->object());
            p->setOwnship(ownship);
            p->setPlayerList(playerList);
            p->tcFrame(dt);
            item = item->getNext();
        }
        if (playerList != nullptr) playerList->unref();
    }

    // (7) Startup RESET: envia RESET_EVENT apos startupResetTime segundos
    if (startupResetTimer >= 0) {
        startupResetTimer -= dt;
        if (startupResetTimer < 0) this->event(RESET_EVENT);
    }

    BaseClass::updateTC(dt);
}
```

A ordem 2-3 (leitura de hardware) **antes** de 4 (física) e 5 (escrita) **depois** de 4 é o
mesmo princípio de consistência por fase, estendido ao mundo externo.

O passo 6 merece atenção: `setOwnship()` e `setPlayerList()` são chamados **a cada
*frame*** — não apenas no `reset()`. Isso permite que o *ownship* seja trocado
dinamicamente (o jogador "salta" para outro *player*) sem que o IG precise saber da troca.

### `Station::updateData(dt)` — ramos condicionais e gravador

```cpp
// src/simulation/Station.cpp
void Station::updateData(const double dt)
{
   // Ramo 1: cria a thread de background, se pedida e ainda inexistente
   if (getBackgroundRate() > 0 && !doWeHaveTheBgThread()) {
      createBackgroundProcess();
   }

   // Ramo 2: sem thread de background -> roda sincronamente aqui
   if (getBackgroundRate() == 0 && !doWeHaveTheBgThread()) {
      processBackgroundTasks(dt);
   }

   // Ramo 3: cria a thread de rede -- note a guarda 'networks != nullptr':
   //         sem redes configuradas, nenhuma thread de rede e criada
   if (getNetworkRate() > 0 && networks != nullptr && !doWeHaveTheNetThread()) {
      createNetworkProcess();
   }

   // Ramo 4: sem thread de rede -> roda sincronamente aqui
   if (getNetworkRate() == 0 && networks != nullptr && !doWeHaveTheNetThread()) {
      processNetworkInputTasks(dt);
      processNetworkOutputTasks(dt);
   }

   // Incondicional: descarga do gravador de dados (sempre no background)
   if (dataRecorder != nullptr) dataRecorder->processRecords();

   BaseClass::updateData(dt);
}
```

Duas *threads* — a de *background* e a de rede — são criadas **preguiçosamente**, na
primeira chamada de `updateData()` em que a taxa correspondente seja maior que zero. A
guarda `networks != nullptr` é significativa: uma `Station` sem o *slot* `networks` **não
paga por nenhuma *thread* de rede**.

**A *thread* de tempo crítico é a exceção.** `StationTcPeriodicThread` **não** é criada
aqui. Quem decide se existe uma *thread* de tempo crítico é a **aplicação**, chamando
`createTimeCriticalProcess()` explicitamente durante a inicialização. A alternativa — não
chamá-la — deixa a aplicação livre para acionar `station->updateTC(dt)` a partir do seu
próprio laço, de um *timer* do SO ou de uma interrupção de hardware externo (caso de uso de
simuladores acoplados a bancadas reais). As três funções
(`createTimeCriticalProcess()`, `createNetworkProcess()`, `createBackgroundProcess()`) são
**virtuais**, previstas para serem sobrescritas por uma `Station` derivada.

```cpp
// src/simulation/Station.cpp
void Station::processBackgroundTasks(const double dt)
{
    // E/S em background (sem bloqueio)
    if (ioHandler != nullptr) ioHandler->updateData(dt);

    // Simulation executive: updatePlayerList() e updateData() dos players
    if (sim != nullptr) sim->updateData(dt);

    // IG hosts (background do gerador de imagem)
    if (igHosts != nullptr) {
        base::List::Item* item { igHosts->getFirstItem() };
        while (item != nullptr) {
            const auto pair = static_cast<base::Pair*>(item->getValue());
            const auto p    = static_cast<AbstractIgHost*>(pair->object());
            p->updateData(dt);
            item = item->getNext();
        }
    }
}
```

### *Fast-forward*

```cpp
void Station::processTimeCriticalTasks(const double dt)
{
    // fastForwardRate = 1 : tempo real (padrao)
    // fastForwardRate = 4 : 4x mais rapido que o tempo real
    for (unsigned int jj = 0; jj < getFastForwardRate(); jj++) {
        tcFrame(dt);   // cada chamada avanca dt segundos de fisica
    }
    // O dt NAO e dividido: cada iteracao e um frame completo de dt segundos.
    // A thread corre a tcRate Hz, entao o tempo simulado avanca a
    // fastForwardRate * tcRate Hz.
}
```

Com `tcRate = 50 Hz` e `fastForwardRate = 4`, a simulação avança a 200 Hz de tempo simulado
por segundo real. Útil para missões longas ou *burn-in* de modelos de IA.

### *Ownship*

```cpp
// src/simulation/Station.cpp
bool Station::setOwnshipByName(const char* const newOS)
{
    base::PairStream* pl { (sim != nullptr) ? sim->getPlayers() : nullptr };
    if (pl == nullptr) return false;

    base::Pair* p { pl->findByName(newOS) };
    if (p != nullptr) {
        const auto newOwnship = static_cast<AbstractPlayer*>(p->object());
        if (newOwnship != ownship) {
            setOwnshipPlayer(newOwnship);
        }
    }
    pl->unref();
    return (ownship != nullptr);
}

bool Station::setOwnshipPlayer(AbstractPlayer* const newOS)
{
    if (ownship != nullptr) ownship->event(ON_OWNSHIP_DISCONNECT);
    ownship = newOS;
    if (ownship != nullptr) ownship->event(ON_OWNSHIP_CONNECT);
    return true;
}
```

Os eventos `ON_OWNSHIP_CONNECT` e `ON_OWNSHIP_DISCONNECT` permitem que *displays* e
subsistemas de controle e visualização (C&D) saibam quando o foco mudou. A troca é segura
porque `ownship` é um `safe_ptr`.

## 21.7 As *threads* concretas de `simulation`

As três `PeriodicThread` de `Station` são subclasses triviais — implementam apenas
`userFunc(dt)`:

```cpp
// src/simulation/StationTcPeriodicThread.cpp
unsigned long StationTcPeriodicThread::userFunc(const double dt)
{
    Station* station { static_cast<Station*>(getParent()) };
    station->processTimeCriticalTasks(dt);
    return 0;
}

// src/simulation/StationBgPeriodicThread.cpp
unsigned long StationBgPeriodicThread::userFunc(const double dt)
{
    Station* station { static_cast<Station*>(getParent()) };
    station->processBackgroundTasks(dt);
    return 0;
}

// StationNetPeriodicThread
unsigned long StationNetPeriodicThread::userFunc(const double dt)
{
    Station* station { static_cast<Station*>(getParent()) };
    station->processNetworkInputTasks(dt);
    station->processNetworkOutputTasks(dt);
    return 0;
}
```

```cpp
// src/simulation/Station.cpp
void Station::createTimeCriticalProcess()
{
    if (tcThread == nullptr) {
        tcThread = new StationTcPeriodicThread(this, getTimeCriticalRate());
        tcThread->unref();  // safe_ptr<> tem a posse

        if (tcStackSize > 0) tcThread->setStackSize(tcStackSize);

        bool ok { tcThread->start(getTimeCriticalPriority()) };
        if (!ok) {
            tcThread = nullptr;
            // registra erro -- nao lanca excecao
        }
    }
}
```

O *pool* `SyncThread` de `Simulation`:

```cpp
// src/simulation/SimulationTcSyncThread.cpp
// start0(): carrega parametros e acorda o worker (signalStart)
void SimulationTcSyncThread::start0(
    base::PairStream* const pl1,   // lista de players (capturada no frame)
    const double            dt1,   // dt/4 para esta fase
    const unsigned int      idx1,  // indice desta thread (1-based)
    const unsigned int      n1)    // total de threads no pool
{
    pl0  = pl1;   dt0  = dt1;   idx0 = idx1;   n0   = n1;
    signalStart();   // acorda o worker que dorme em waitForStart()
}

unsigned long SimulationTcSyncThread::userFunc()
{
    if (pl0 != nullptr && idx0 > 0 && idx0 <= n0) {
        Simulation* sim { static_cast<Simulation*>(getParent()) };
        sim->updateTcPlayerList(pl0, dt0, idx0, n0);
        // ao retornar, SyncThread chama signalCompleted() automaticamente
    }
    return 0;
}
```

`SimulationBgSyncThread` é idêntica, exceto que chama `updateBgPlayerList()`.

## 21.8 Fábrica do módulo `simulation`

```cpp
base::Object* factory(const std::string& name)
{
    base::Object* obj {};

    if      ( name == Simulation::getFactoryName() ) { obj = new Simulation(); }
    else if ( name == Station::getFactoryName()    ) { obj = new Station();    }
    // Apenas duas classes: as demais do modulo sao abstratas.

    return obj;
}
```

---

# 22. CAMADA `terrain`

`terrain` resolve um problema único: dar acesso a dados de elevação por uma **interface
uniforme**, independentemente do formato do arquivo. Separa o **contrato** (`Terrain`) das
**implementações concretas** (`DataFile` e suas subclasses por formato) e acrescenta um
**agregador** (`QuadMap`).

Depende apenas de `mixr_base` — mas é dependência de `models`: `WorldModel` tem um *slot*
`terrain`, e `Player::updateElevation()` consulta a elevação sob cada entidade a cada
quadro. A mesma interface é consultada por sensores — um radar precisa saber se uma
montanha bloqueia a linha de visão.

## 22.1 `Terrain` — a interface

**ARMADILHA — os *slots* são `path` e `file`, não `pathname` e `filename`.** `Terrain` tem
exatamente dois *slots*, e os nomes **não acompanham os dos acessores**. Os métodos
chamam-se `setPathname()` e `setFilename()`, mas em EDL escreve-se `path` e `file`. Um
`pathname:` num arquivo de configuração produz `slot not found` e o *tile* simplesmente não
carrega.

O estado comum a qualquer banco de dados de elevação é a sua **célula de cobertura**: os
quatro cantos geográficos (`latitudeSW`/`longitudeSW`/`latitudeNE`/`longitudeNE`) e os
extremos de elevação. É a partir desses cantos que `QuadMap` decide qual sub-banco contém
um ponto.

```cpp
// resumo de include/mixr/terrain/Terrain.hpp
class Terrain : public base::Component {
public:
    // Consulta por UM ponto: devolve true se dentro da cobertura,
    // preenchendo 'elev' em metros.
    // Chamado por Player::updateElevation() a cada fase 0.
    virtual bool getElevation(
        double* const elev,
        const double lat, const double lon,
        const bool interp = false) const = 0;

    // Consulta por RADIAL: preenche 'n' elevacoes ao longo de uma linha reta
    // a partir de (lat,lon), na 'direction' (graus verdadeiros), ate 'maxRng' m.
    // Retorna quantos pontos foram encontrados dentro da cobertura.
    // Usado por sensores para calcular linha de visao e mascaramento.
    virtual unsigned int getElevations(
        double* const elevations, bool* const validFlags,
        const unsigned int n,
        const double lat, const double lon,
        const double direction, const double maxRng,
        const bool interp = false) const = 0;

    // --- Utilitarios ESTATICOS de pos-processamento ---

    // vbwShadowChecker(): determina quais pontos estao em sombra de terreno.
    static bool vbwShadowChecker(
        bool*         const maskFlags,
        const double* const elevations,
        const bool*   const validFlags,
        const unsigned int  n,
        const double range, const double refAlt,
        const double beamAngle = 0,    // centro do feixe (graus)
        const double beamWidth = 180); // largura total (graus)

    // aac(): cosseno do angulo de aspecto do terreno em cada ponto.
    // Vetor m1 = direcao do feixe ao ponto; m2 = normal a inclinacao.
    // cos(aspecto) = m1 . m2  -- usado por modelos de clutter radar.
    static bool aac(
        double*       const aacData,
        const double* const elevData,
        const bool*   const maskFlags,
        const unsigned int  n,
        const double range, const double refAlt);

    // targetOcculting(): high-level que chama getElevations + occultCheck
    bool targetOcculting(
        const double refLat, const double refLon, const double refAlt,
        const double tgtLat, const double tgtLon, const double tgtAlt) const;
};
```

### `vbwShadowChecker()` — horizonte crescente + recorte pelo feixe

Combina dois testes numa passagem:

1. **Horizonte crescente**: percorre o radial do observador ao alvo acumulando o maior
   ângulo de visada encontrado (`tanLower`); qualquer ponto com ângulo abaixo do máximo
   histórico está em sombra — mesmo algoritmo de *shadow ray* de *ray casting*, aplicado ao
   perfil 2D de elevação.
2. **Recorte pelo feixe**: pontos acima da borda superior (`tanUpper`) também são
   mascarados, por estarem fora do campo de visada mesmo sem obstrução.

```cpp
// src/terrain/Terrain.cpp (esquematico -- o fonte repete o laco em dois ramos)
// Bordas do feixe, saturadas para evitar a singularidade de tan(90 graus)
double beamUpper { beamAngle + beamWidth/2.0 };
if (beamUpper >  89.9999) beamUpper =  89.9999;
double beamLower { beamAngle - beamWidth/2.0 };
if (beamLower < -89.9999) beamLower = -89.9999;

const double tanUpper { std::tan(beamUpper * base::angle::D2RCC) };
double       tanLower { std::tan(beamLower * base::angle::D2RCC) };

const double deltaRng { range / (n - 1) };
double currentRange {};

for (unsigned int i = 0; i < n; i++) {
    if (validFlags == nullptr || validFlags[i]) {
        double tanLookAngle {};
        if (currentRange > 0)
            tanLookAngle = (elevations[i] - refAlt) / currentRange;
        else
            tanLookAngle = (elevations[i] - refAlt) / 1.0;

        // Visivel exige AS DUAS condicoes:
        //   >= tanLower : acima do horizonte acumulado E da borda inferior
        //   <= tanUpper : abaixo da borda superior do feixe
        if (tanLookAngle >= tanLower && tanLookAngle <= tanUpper) {
            maskFlags[i] = false;    // visivel
            tanLower = tanLookAngle; // eleva o horizonte
        } else {
            maskFlags[i] = true;     // em sombra ou fora do feixe
        }
    } else {
        maskFlags[i] = true;         // dado invalido -> mascarado
    }
    currentRange += deltaRng;
}
```

`tanLower` acumula duas responsabilidades: nasce como borda inferior do feixe e passa a
representar o horizonte crescente. `tanUpper`, ao contrário, é constante. A saturação em
±89,9999° evita a singularidade de tan(90°) para feixes muito largos, e é o motivo de o
valor padrão `beamWidth = 180` não produzir infinito.

Outros utilitários estáticos de `Terrain`: `occultCheck()` e `occultCheck2()` (oclusão
ponto a ponto, base de `targetOcculting()`/`targetOcculting2()`), `cLight()` (iluminação
Lambertiana do perfil) e `getElevationColor()` (mapeamento de elevação para cor via tabela
de `base::Color`). Todas operam sobre arrays já preenchidos por `getElevations()`, sem
estado próprio e sem tocar no disco.

## 22.2 `DataFile` — um arquivo de elevação em grade regular

```cpp
// resumo de include/mixr/terrain/DataFile.hpp
class DataFile : public Terrain {
protected:
    short**      columns   {};          // [lon_idx][lat_idx] -- em metros
    double       latSpacing {};         // espacamento entre posts em lat (graus)
    double       lonSpacing {};         // espacamento entre posts em lon (graus)
    unsigned int nptlat    {};          // numero de posts em latitude (linhas)
    unsigned int nptlong   {};          // numero de posts em longitude (colunas)
    short        voidValue {-32767};    // valor de post invalido/vazio
};

// Porque short e nao double?
// DTED-2: 3601 x 3601 posts por tile de 1deg x 1deg
//   short:  3601 * 3601 * 2 bytes  =  ~25 MB por tile
//   double: 3601 * 3601 * 8 bytes  = ~100 MB por tile
// Carregando 4 tiles no QuadMap: 100 MB vs 400 MB
// Elevacoes em metros raramente excedem +/-32767 m (short e suficiente)
```

### Carregamento preguiçoso — disparado por `Terrain::reset()`

```cpp
// src/terrain/Terrain.cpp -- na integra
void Terrain::reset()
{
   if ( !isDataLoaded() ) {
      loadData();
   }

   BaseClass::reset();
}
```

São seis linhas, e cada detalhe conta:

- `loadData()` é **virtual puro** — o único método que cada formato precisa implementar.
- A guarda `isDataLoaded()` torna a operação **idempotente**: um segundo *reset* não relê
  gigabytes de disco.
- `BaseClass::reset()` vem **depois** do carregamento, de modo que, quando os componentes
  filhos forem reiniciados, os dados já estão em memória.

Essa última ordem é o que faz o `QuadMap` funcionar:

```cpp
// src/terrain/QuadMap.cpp
void QuadMap::reset()
{
   // Resetting our base class will reset our components,
   // which will load the elevation data files
   BaseClass::reset();

   // Now find our files
   findDataFiles();
}
```

**POR QUÊ o agregador só pode se montar depois dos agregados.** `findDataFiles()` varre os
componentes filhos e aceita apenas os que **já têm dados carregados**. Se rodasse antes de
`BaseClass::reset()`, encontraria quatro *tiles* vazios e montaria um `QuadMap` sem nenhum
arquivo — **silenciosamente**, porque um `QuadMap` com zero *tiles* é um objeto válido que
apenas responde `false` a toda consulta.

### `DataFile::getElevation()` — os dois modos

```cpp
// src/terrain/DataFile.cpp (corpo integral; cabecalho da funcao omitido)
   double value{};            // the elevation (meters)

   // Early out tests
   if ( !isDataLoaded() ||          // Not loaded or
        (lat < getLatitudeSW()  ||
         lat > getLatitudeNE()) ||  // wrong latitude or
        (lon < getLongitudeSW() ||
         lon > getLongitudeNE())    // wrong longitude
        ) return false;

   // Compute the lat and lon points
   double pointsLat {(lat - getLatitudeSW()) / latSpacing};
   if (pointsLat < 0) pointsLat = 0;

   double pointsLon {(lon - getLongitudeSW()) / lonSpacing};
   if (pointsLon < 0) pointsLon = 0;

   if (interp) {
      // South-west corner post is [icol][irow]
      unsigned int irow {static_cast<unsigned int>(pointsLat)};
      unsigned int icol {static_cast<unsigned int>(pointsLon)};
      if (irow > (nptlat-2)) irow = (nptlat-2);
      if (icol > (nptlong-2)) icol = (nptlong-2);

      // delta from s-w corner post
      double deltaLat {static_cast<double>(pointsLat - static_cast<double>(irow))};
      double deltaLon {static_cast<double>(pointsLon - static_cast<double>(icol))};

      // Get the elevations at each corner
      double elevSW {static_cast<double>(columns[icol][irow])};
      double elevNW {static_cast<double>(columns[icol][irow+1])};
      double elevSE {static_cast<double>(columns[icol+1][irow])};
      double elevNE {static_cast<double>(columns[icol+1][irow+1])};

      // Interpolate the west point, then the east point
      double westPoint {elevSW + (elevNW - elevSW) * deltaLat};
      double eastPoint {elevSE + (elevNE - elevSE) * deltaLat};

      // Interpolate between the west and east points
      value = westPoint + (eastPoint - westPoint) * deltaLon;
   }
   else {
      // No -- just use the nearest post
      unsigned int irow {static_cast<unsigned int>(pointsLat + 0.5f)};
      unsigned int icol {static_cast<unsigned int>(pointsLon + 0.5f)};
      if (irow >= nptlat) irow = (nptlat-1);
      if (icol >= nptlong) icol = (nptlong-1);

      value = static_cast<double>(columns[icol][irow]);
   }

   *elev = value;
   return true;
}
```

Aritmética de índice, decisão a decisão:

- `pointsLat`/`pointsLon` são posições **fracionárias** na grade: quantos espaçamentos
  cabem entre o canto sudoeste e o ponto pedido. A parte inteira é o índice do *post*; a
  fracionária, onde se está entre dois deles.
- Na interpolação, o truncamento para `unsigned int` dá o canto sudoeste do quadrado, e a
  saturação em `nptlat-2` garante que `irow+1` ainda exista — é isso que impede o acesso
  fora do array exatamente na borda norte ou leste do *tile*.
- No modo do *post* mais próximo, o `+0.5` antes do truncamento é o arredondamento, e a
  saturação é em `nptlat-1`, porque aí não há vizinho a consultar.

**ARMADILHA — a ordem dos índices é `columns[lon][lat]`.** O array é indexado primeiro por
**longitude** e só depois por latitude — `columns[icol][irow]`, e não o contrário. O nome
ajuda a lembrar: o dado é organizado em *colunas* de longitude. **Trocar os dois índices
não gera erro de compilação e devolve elevações de outro lugar do mundo.**

A escolha entre os dois modos não é estilística: para a dinâmica de voo o *post* mais
próximo é suficiente (custo mínimo a 50 Hz); para um sensor que precisa de um perfil
contínuo ao longo de um radial, a interpolação evita descontinuidades artificiais na linha
de visão.

## 22.3 Formatos: DTED, SRTM e DED

Três subclasses de `DataFile` implementam `loadData()`. A interface de consulta é idêntica
para as três; o que muda é apenas como `columns[][]` é preenchido.

### DTED (*Digital Terrain Elevation Data*) — `DtedFile`

Padrão militar (MIL-PRF-89020B), em três níveis:

| Nível | Espaçamento | *Posts*/célula |
|---|---|---|
| DTED-0 | 30 arc-sec (~900 m) | 121 × 121 |
| DTED-1 | 3 arc-sec (~90 m) | 1201 × 1201 |
| DTED-2 | 1 arc-sec (~30 m) | 3601 × 3601 |

O arquivo contém três cabeçalhos binários fixos (**UHL, DSI, ACC**) seguidos pelos dados de
elevação em colunas de longitude. Detalhe de codificação crítico: os valores são
armazenados em *big-endian* e **sinal-magnitude**, não em complemento de dois:

```cpp
// src/terrain/dted/DtedFile.cpp
short DtedFile::readValue(const unsigned char hbyte, const unsigned char lbyte)
{
    // The data is stored as 2 byte characters (sign and magnitude)
    // with high byte first.  The high bit is the sign bit.  Check for
    // sign bit and then turn it off and set SIGN_VAL accordingly.
    short height{};
    short sign_val{1};
    unsigned char nhbyte{hbyte};

    if (hbyte & ~0177) {
        // sign bit set
        nhbyte   = hbyte & 0177;
        sign_val = -1;
    }
    height = (256 * static_cast<short>(nhbyte) + static_cast<short>(lbyte)) * sign_val;

    return height;
}
```

As constantes estão em **octal**: `0177` é `0x7F`, os sete bits baixos; `~0177` é a máscara
do bit de sinal. A multiplicação por 256 é o deslocamento de oito bits, escrito como
aritmética porque o resultado precisa ser `short` com sinal.

**POR QUÊ sinal-magnitude.** O DTED é formato dos anos 1970, e sinal-magnitude era a
convenção de troca de dados científicos da época — independente da arquitetura da máquina,
que naquele momento nem sempre usava complemento de dois. Consequência para quem lê hoje:
**não se pode simplesmente reinterpretar dois *bytes* como um `int16_t`** — uma elevação de
−100 m ficaria com valor errado.

Há uma segunda sobrecarga de quatro *bytes*, com a mesma lógica, usada para ler a soma de
verificação do rodapé de cada coluna — conferida apenas quando o *slot* de verificação está
ligado.

### SRTM (*Shuttle Radar Topography Mission*) — `SrtmHgtFile`

Dados gratuitos da NASA/USGS cobrindo ~80% da superfície terrestre. **Não há cabeçalhos** —
todo o metadado vem do nome do arquivo e do tamanho:

```cpp
// src/terrain/srtm/SrtmHgtFile.cpp
bool SrtmHgtFile::determineSrtmInfo(const std::string& srtmFilename,
                                    std::streamoff size)
{
    unsigned int num_lat{};
    unsigned int num_lon{};
    switch (size) {
       case 2884802:                    // SRTM3: 3 arc-sec
         latSpacing = 3.0 / 3600.0;
         lonSpacing = 3.0 / 3600.0;
         num_lat = 1201;
         num_lon = 1201;
         break;
      case 25934402:                    // SRTM1: 1 arc-sec
         latSpacing = 1.0 / 3600.0;
         lonSpacing = 1.0 / 3600.0;
         num_lat = 3601;
         num_lon = 3601;
         break;
      default:
         return false;
    }

    // valid SRTM file extensions
    if (srtmFilename.substr(7, 4) != ".hgt" && srtmFilename.substr(7, 4) != ".HGT") {
        return false;
    }
    // ... daqui em diante, o nome fornece hemisferios e canto sudoeste ...
}
```

O nome do arquivo é lido por **posição fixa**: o método recebe os últimos onze caracteres
(`N38W077.hgt`) e indexa diretamente — os caracteres 7 a 10 são a extensão, e os anteriores
dão hemisfério e coordenada do canto sudoeste.

**ARMADILHA — um único *byte* a mais e o arquivo é rejeitado.** O `switch` é sobre o
tamanho **exato** em *bytes*. Não há tolerância, e o `default` devolve `false` sem
diagnóstico específico: a mensagem que o usuário vê é apenas *"ERROR in determining SRTM
type"*. Um arquivo truncado no *download*, descompactado com conversão de fim de linha, ou
de uma variante de resolução diferente das duas previstas, falha exatamente da mesma forma
— e a causa mais provável, na prática, é a primeira.

A codificação dos dados é a mesma do DTED — *big-endian*, sinal-magnitude — portanto o
mesmo `readValue()` se aplica.

### DED (*Digital Elevation Data*) — `DedFile`

Formato proprietário do MIXR — mais simples que os dois anteriores, sem cabeçalhos
complexos, usado tipicamente para dados sintéticos ou convertidos de outras fontes durante
o desenvolvimento.

## 22.4 `QuadMap` — agregação em grade 2×2

```
terrain: ( QuadMap
    components: {
        sw: ( DtedFile  path: "/data/terrain"  file: "n38w077.dt2" )
        se: ( DtedFile  path: "/data/terrain"  file: "n38w076.dt2" )
        nw: ( DtedFile  path: "/data/terrain"  file: "n39w077.dt2" )
        ne: ( DtedFile  path: "/data/terrain"  file: "n39w076.dt2" )
    }
)
```

**ARMADILHA — quatro é limite rígido, e o excedente some em silêncio.** `MAX_DATA_FILES`
vale exatamente **4** — o comentário no cabeçalho é lacônico: *"Only 4 files (as in
Quad!)"*. O laço de descoberta para em `count < MAX_DATA_FILES`, de modo que um quinto
*tile* declarado em `components` é **ignorado sem aviso**: o EDL carrega, o `QuadMap`
funciona, e há um buraco no mapa exatamente onde estava aquele arquivo. **Cobrir uma área
maior exige aninhar `QuadMap`s** — um `QuadMap` é um `Terrain` como qualquer outro, e
portanto pode ser filho de outro `QuadMap`.

```cpp
// src/terrain/QuadMap.cpp
void QuadMap::findDataFiles()
{
    clearData();
    base::PairStream* sub { getComponents() };
    if (sub == nullptr) return;

    unsigned int count {};
    base::List::Item* item { sub->getFirstItem() };
    while (item != nullptr && count < MAX_DATA_FILES) {
        const auto pair = static_cast<base::Pair*>(item->getValue());
        const auto df   = dynamic_cast<Terrain*>(pair->object());
        // Aceita qualquer Terrain JA CARREGADO (DtedFile, SrtmHgtFile, DedFile)
        if (df != nullptr && df->isDataLoaded()) {
            df->ref();
            dataFiles[count++] = df;
        }
        item = item->getNext();
    }
    numDataFiles = count;
    sub->unref();

    // Calcula o envelope de cobertura como uniao dos quatro cantos
    if (numDataFiles > 0) {
        double elevMin{999999.0}, elevMax{-999999.0};
        double lowerLat{90.0}, lowerLon{180.0};
        double upperLat{-90.0}, upperLon{-180.0};
        for (unsigned int i = 0; i < numDataFiles; i++) {
            elevMin  = std::min(elevMin,  dataFiles[i]->getMinElevation());
            elevMax  = std::max(elevMax,  dataFiles[i]->getMaxElevation());
            lowerLat = std::min(lowerLat, dataFiles[i]->getLatitudeSW());
            lowerLon = std::min(lowerLon, dataFiles[i]->getLongitudeSW());
            upperLat = std::max(upperLat, dataFiles[i]->getLatitudeNE());
            upperLon = std::max(upperLon, dataFiles[i]->getLongitudeNE());
        }
        setMinElevation(elevMin);  setMaxElevation(elevMax);
        setLatitudeSW(lowerLat);   setLongitudeSW(lowerLon);
        setLatitudeNE(upperLat);   setLongitudeNE(upperLon);
    }
}
```

**As duas consultas do contrato resolvem a agregação de maneiras diferentes:**

- `getElevation()` — um ponto — é uma **disjunção**: tenta cada *tile* em sequência e para
  no primeiro que responda. Faz sentido, porque um ponto pertence a um *tile* só.
- `getElevations()` — um radial — é uma **colaboração**: todos os quatro recebem os
  *mesmos* arrays `elevations[]` e `validFlags[]`, e cada um preenche apenas as posições
  que caem na sua cobertura, marcando o *flag* correspondente. Um radial de 80 km que
  atravesse a fronteira entre dois *tiles* sai com a primeira metade escrita por um e a
  segunda pelo outro — sem que o chamador saiba que havia uma fronteira.

**POR QUÊ o array de *flags* torna a colaboração possível.** Sem `validFlags[]`, um *tile*
não teria como dizer "estas posições não são minhas" sem escrever um valor — e qualquer
valor escolhido (zero, −32767) seria indistinguível de uma elevação legítima. O array de
*flags* separa "elevação zero" de "sem dado". É também por isso que `vbwShadowChecker()` e
`aac()` recebem esse mesmo array: um ponto sem dado é tratado como mascarado, e não como um
vale.

```cpp
// src/terrain/QuadMap.cpp
bool QuadMap::getElevation(double* const elev,
                             const double lat, const double lon,
                             const bool interp) const
{
    if (!isDataLoaded() ||
        lat < getLatitudeSW() || lat > getLatitudeNE() ||
        lon < getLongitudeSW()|| lon > getLongitudeNE()) return false;

    bool found {}; double value {};
    for (unsigned int i = 0; i < numDataFiles && !found; i++)
        found = dataFiles[i]->getElevation(&value, lat, lon, interp);
    if (found) *elev = value;
    return found;
}

unsigned int QuadMap::getElevations(
    double* const elevations, bool* const validFlags,
    const unsigned int n, const double lat, const double lon,
    const double direction, const double maxRng, const bool interp) const
{
    unsigned int num {};
    // Radial pode cruzar fronteiras: cada tile contribui com seus pontos
    for (unsigned int i = 0; i < numDataFiles && num < n; i++)
        num += dataFiles[i]->getElevations(
            elevations, validFlags, n, lat, lon, direction, maxRng, interp);
    return num;
}
```

## 22.5 Fábrica do módulo `terrain`

```cpp
base::Object* factory(const std::string& name)
{
    base::Object* obj {};
    if      (name == QuadMap::getFactoryName())      obj = new QuadMap();
    else if (name == DedFile::getFactoryName())      obj = new DedFile();
    else if (name == DtedFile::getFactoryName())     obj = new DtedFile();
    else if (name == SrtmHgtFile::getFactoryName())  obj = new SrtmHgtFile();
    return obj;
}
```
---

# 23. CAMADA `models` — O QUE ESTÁ SENDO SIMULADO

Se `simulation` define **como** a simulação executa, `models` define **o quê** está sendo
simulado: aeronaves, mísseis, radares, sistemas de armas, navegação, ambiente. É de longe a
maior biblioteca do MIXR — ~107 arquivos de implementação — e depende de `mixr_base`,
`mixr_simulation`, `mixr_terrain` e `jsbsim`. É cerca de sete vezes o tamanho de
`simulation` e a única camada que depende de todas as três abaixo dela.

| Domínio | Conteúdo |
|---|---|
| `WorldModel` | simulação situada no globo terrestre (estende `Simulation`) |
| `Player` | entidade simulada concreta |
| `dynamics/` | física de voo (`RacModel`, `LaeroModel`, `JSBSimModel`, `SpaceDynamicsModel`) |
| `system/` | subsistemas embarcados (radar, piloto, armas, contramedidas) |
| `navigation/` | rotas, *waypoints*, INS, GPS |
| `environment/` | atmosfera IR (`AbstractAtmosphere`, `IrAtmosphere`, `IrAtmosphere1`) |
| `player/` | tipos concretos (aeronaves, armas, veículos, efeitos, espaço) |
| dados e mensagens | `Emission`, `Track`, `SensorMsg`, `Tdb`, `Image`, `Designator`, … |

Três classes de dados transversais — `SynchronizedState`, `Message` e `TargetData` —
completam o módulo, ao lado de `SimAgent`/`MultiActorAgent` (extensões do UBF) e da fábrica.

## 23.1 `WorldModel` — a simulação situada no mundo

`WorldModel` é a subclasse concreta de `Simulation` que **qualquer simulação real
instancia** — não `Simulation` diretamente. Acrescenta ao *executive* tudo que é necessário
para **situar a simulação no globo terrestre**: o ponto de referência geodésico, o modelo
do elipsoide, o banco de dados de terreno e o modelo atmosférico.

### O ponto de referência e a *world matrix*

```cpp
// src/models/WorldModel.cpp
bool WorldModel::setRefLatitude(const double v)
{
    bool ok { v <= 90.0 && v >= -90.0 };
    if (ok) {
        refLat  = v;
        const double r { base::angle::D2RCC * refLat };
        sinRlat = std::sin(r);   // pre-calculado para uso por todos os players
        cosRlat = std::cos(r);   // idem
        base::nav::computeWorldMatrix(refLat, refLon, &wm);  // Rz[-lon] * Ry[90+lat]
    }
    return ok;
}

bool WorldModel::setRefLongitude(const double v)
{
    bool ok { v <= 180.0 && v >= -180.0 };
    if (ok) {
        refLon = v;
        base::nav::computeWorldMatrix(refLat, refLon, &wm);  // recalcula wm
    }
    return ok;
}
```

A *world matrix* `wm` converte vetores entre NED e ECEF:

```
wm = Rz(-lon_ref) * Ry(90 + lat_ref)
V_ned  = wm * V_ecef
V_ecef = V_ned * wm
```

O `cosRlat` é o fator de correção das projeções *flat-earth* que todos os *players* usam ao
converter deslocamentos angulares em metros no plano local. Como a largura de um grau de
longitude em metros varia com a latitude — de ≈111 km no equador até zero no polo —
`cosRlat` corrige essa diferença:

```
ΔE_metros = Δλ_rad × R_Terra × cos(φ_ref)
```

```cpp
// src/models/player/Player.cpp
bool Player::setPositionLLA(const double lat, const double lon,
                            const double alt, const bool slaved)
{
    WorldModel* s { getWorldModel() };
    const double refLat  { s->getRefLatitude()  };
    const double refLon  { s->getRefLongitude() };
    const double cosRlat { s->getCosRefLat() };

    if (s->isGamingAreaUsingEarthModel()) {
        const double sinRlat { s->getSinRefLat() };
        base::nav::convertLL2PosVecE(refLat, refLon, sinRlat, cosRlat,
                                      lat, lon, alt, &posVecNED, em);
    } else {
        base::nav::convertLL2PosVecS(refLat, refLon, cosRlat,
                                      lat, lon, alt, &posVecNED);
    }

    // Verifica limite da gaming area
    posVecValid = (maxRefRange <= 0.0) ||
                  (posVecNED.length2() <= maxRefRange * maxRefRange);
    // ...
}
```

### Configuração via EDL

```
( WorldModel
    -- Ponto de referência: centro da gaming area
    latitude:  ( LatLon 35.0 )        -- 35°N
    longitude: ( LatLon -117.5 )      -- 117.5°O

    -- Raio da gaming area (0 = ilimitado)
    gamingAreaRange: ( KiloMeters 500.0 )

    -- Modelo de Terra: WGS-84 (padrão se omitido)
    earthModel: "wgs84"

    -- Usar projecoes elipsoidais (true) ou esferico simplificado (false)
    gamingAreaUseEarthModel: true

    -- Banco de dados de elevacao de terreno
    terrain: ( QuadMap ... )

    -- Modelo atmosferico (para simulacao IR)
    atmosphere: ( IrAtmosphere ... )
)
```

O *slot* `earthModel` aceita tanto um objeto `EarthModel` completo quanto uma *string* de
nome:

```cpp
// src/models/WorldModel.cpp
bool WorldModel::setSlotEarthModel(const base::String* const msg)
{
    bool ok {};
    if (msg != nullptr && msg->len() > 0) {
        const base::EarthModel* p { base::EarthModel::getEarthModel(*msg) };
        if (p != nullptr) {
            ok = setEarthModel(p);  // clona o modelo
        } else {
            std::cerr << "WorldModel: earth model not found: " << *msg << std::endl;
        }
    } else {
        ok = setEarthModel(nullptr);  // volta ao WGS-84 default
    }
    return ok;
}
```

### `WorldModel::reset()` — a sequência de inicialização

```cpp
// src/models/WorldModel.cpp
void WorldModel::reset()
{
    // 1. Inicializa o executive (Simulation::reset()): reconstroi
    //    a lista de players, cria o pool de threads
    BaseClass::reset();

    // 2. Inicializa o banco de dados de terreno
    //    (primeiro reset: carrega os arquivos de elevacao do disco)
    if (terrain != nullptr) {
        std::cout << "Loading Terrain Data..." << std::endl;
        terrain->reset();
        std::cout << "Finished!" << std::endl;
    }

    // 3. Inicializa o modelo atmosferico
    if (atmosphere != nullptr) atmosphere->reset();
}
```

A ordem importa: `BaseClass::reset()` precisa ocorrer primeiro porque reconstrói a lista de
*players* e cria o *pool* de *threads*. O carregamento do terreno é sinalizado ao operador
pelas mensagens `"Loading Terrain Data..."`/`"Finished!"`, pois pode levar segundos. O
modelo atmosférico é inicializado por último, pois pode depender dos limites de altitude
definidos pelo terreno.

### Acesso ao `WorldModel` de dentro de um `Player`

```cpp
// Em qualquer Player ou System:
WorldModel* wm { static_cast<WorldModel*>(
    findContainerByType( typeid(WorldModel) )
) };
if (wm != nullptr) {
    double lat  { wm->getRefLatitude()  };   // graus
    double lon  { wm->getRefLongitude() };
    double cos  { wm->getCosRefLat()    };   // para projecoes flat-earth
    terrain::Terrain* t { wm->getTerrain() }; // elevacao do terreno
}

// Atalho disponivel em Player:
WorldModel* s { getWorldModel() };  // equivalente ao findContainerByType acima
```

## 23.2 `Player` — a entidade simulada

`Player` é a classe mais importante de `models` e a mais referenciada. Implementa
`AbstractPlayer` e é a raiz da hierarquia de entidades simuladas.

### Estado cinemático: três representações simultâneas

```cpp
// resumo de include/mixr/models/player/Player.hpp
// Posicao em tres sistemas
base::Vec3d  posVecNED;     // [Norte, Leste, Baixo] metros -- plano local tangente
base::Vec3d  posVecECEF;    // [x, y, z] metros geocentricos
double       latitude  {};  // graus
double       longitude {};
double       altitude  {};  // metros HAE

// Orientacao
base::Matrixd rm;           // rotacao corpo -> NED
base::Matrixd rmW2B;        // rotacao corpo -> ECEF (wm * rm)
base::Quat    q;            // quaternion de orientacao (Quat, nao "Quatd")

// Velocidades
base::Vec3d velVecNED;      // velocidade em NED (m/s)
base::Vec3d velVecECEF;     // velocidade em ECEF (m/s) -- enviada pela rede
base::Vec3d angularVel;     // velocidade angular no eixo corpo (rad/s)
```

Manter as três representações simultaneamente **não é redundância** — é decisão de
desempenho e coerência arquitetural. Cada subsistema usa o sistema que lhe é natural:

- `DynamicsModel` integra equações de movimento em NED (`posVecNED`, `velVecNED`);
- `AbstractNib` monta PDUs para a rede em ECEF (`posVecECEF`, `velVecECEF`);
- O EDL configura posição inicial em geodésicos (`initLatitude`, `initLongitude`,
  `initAlt`) — ou, alternativamente, em NED (`initXPos`, `initYPos`) ou em ECEF
  (`initGeocentric`).

A responsabilidade é dividida em dois métodos, e **confundir os dois é fácil**:

- `positionUpdate(dt)` **integra** a posição a partir da velocidade;
- `setPosition()` / `setPositionLLA()` **sincronizam** as três representações entre si.

O primeiro chama o segundo. É no segundo que vivem as conversões `convertPosVec2ll*`, o
`computeWorldMatrix()` e o `convertGeod2Ecef()` — e não em `positionUpdate()`.

```cpp
// src/models/player/Player.cpp -- o ramo CS_LOCAL
void Player::positionUpdate(const double dt)
{
   if ( !isLocalPlayer() ) return;    // networked: quem move e o dead reckoning

   const bool pfrz{(isPositionFrozen() || isPositionSlaved())};
   const bool afrz{(isAltitudeFrozen() || isAltitudeSlaved())};

   // Clamping ao solo: so para tipos que andam no chao
   const bool gcEnabled{tElevValid &&
       isMajorType(GROUND_VEHICLE | SHIP | BUILDING | LIFE_FORM)};

   const bool enabled{( vp > 0 && dt != 0 && (!pfrz || !afrz) )};
   if ( enabled && useCoordSys == CS_LOCAL ) {

      base::Vec3d newPosVecNED = posVecNED;

      const double ue{velVecNED.x()},  ve{velVecNED.y()},  we{velVecNED.z()};
      const double ue0{velVecN1.x()}, ve0{velVecN1.y()}, we0{velVecN1.z()};

      // INTEGRACAO TRAPEZOIDAL: media entre a velocidade atual e a anterior
      if (!pfrz) {
         newPosVecNED[INORTH] += (ue + ue0) * 0.5 * dt;
         newPosVecNED[IEAST]  += (ve + ve0) * 0.5 * dt;
      }
      if (!afrz) {
         if (gcEnabled) newPosVecNED[IDOWN] = -(tElev + tOffset);  // colado ao solo
         else           newPosVecNED[IDOWN] += (we + we0) * 0.5 * dt;
      }

      setPosition(newPosVecNED);   // <- e AQUI que as tres representacoes casam
      velVecN1 = velVecNED;        // guarda para o trapezio do proximo quadro
   }
}
```

Três coisas que só se veem lendo o código:

1. A integração é **trapezoidal**, não de Euler: usa a média entre a velocidade atual e a
   do quadro anterior, guardada em `velVecN1`.
2. O *ground clamping* **não integra** a altitude — ele a **substitui** pela elevação do
   terreno mais um deslocamento, e só para veículos terrestres, navios, edificações e
   `LifeForm`.
3. Há congelamento **por eixo**: posição e altitude podem ser congeladas
   independentemente.

**POR QUÊ três sistemas de coordenadas para integrar.** O campo `useCoordSys` escolhe em
qual referencial a integração acontece: `CS_LOCAL` (NED da *gaming area*), `CS_GEOD`
(latitude/longitude/altitude) ou `CS_WORLD` (ECEF). Integrar em NED é barato e preciso
perto da origem, mas acumula erro de projeção longe dela; integrar em ECEF é uniforme no
globo inteiro, ao custo de números grandes. **Trocar de sistema em voo é permitido** — e
quando isso acontece, a velocidade anterior é zerada, porque o trapézio não faria sentido
entre referenciais diferentes.

### Descoberta automática de sistemas

`Player` **não declara *slots* diretos para seus subsistemas** — radar, piloto, navegação
etc. são **componentes filhos** no `PairStream`, descobertos automaticamente **por tipo** no
`reset()`:

```cpp
// src/models/player/Player.cpp
void Player::updateSystemPointers()
{
    loadSysPtrs = false;
    setDynamicsModel(  findByType(typeid(DynamicsModel))    );
    setDatalink(       findByType(typeid(Datalink))          );
    setGimbal(         findByType(typeid(Gimbal))            );
    setIrSystem(       findByType(typeid(IrSystem))          );
    setNavigation(     findByType(typeid(Navigation))        );
    setOnboardComputer(findByType(typeid(OnboardComputer))   );
    setPilot(          findByType(typeid(Pilot))             );
    setRadio(          findByType(typeid(Radio))             );
    setSensor(         findByType(typeid(RfSensor))          );
    setStoresMgr(      findByType(typeid(StoresMgr))         );
}
```

Cada `set*()` confirma o tipo antes de aceitar:

```cpp
// src/models/player/Player.cpp
bool Player::setNavigation(base::Pair* const sys)
{
    bool ok {};
    if (sys == nullptr) {
        if (nav != nullptr) nav->unref();
        nav = nullptr;
        ok = true;
    } else if (sys->object()->isClassType(typeid(Navigation))) {
        if (nav != nullptr) nav->unref();
        nav = sys;
        nav->ref();
        ok = true;
    }
    return ok;
}
```

O sinalizador `loadSysPtrs` é usado por `processComponents()` para forçar nova varredura
quando a lista de componentes é modificada em tempo de execução:

```cpp
void Player::processComponents(base::PairStream* const list,
    const std::type_info& filter,
    base::Pair* const add, base::Component* const remove)
{
    base::Component::processComponents(list, filter, add, remove);
    loadSysPtrs = true;   // re-varre os ponteiros no proximo updateTC()
}
```

Consequência: **a ordem dos filhos no EDL é irrelevante**, e um `Player` sem
`DynamicsModel` simplesmente não tem física — não é erro fatal; cada chamada a
`getDynamicsModel()` testa o ponteiro antes de usar.

### O ciclo de quatro fases em `Player`

```cpp
// src/models/player/Player.cpp
void Player::updateTC(const double dt0)
{
    // Garante que os ponteiros de sistema estao atualizados
    if (loadSysPtrs) {
        updateSystemPointers();
        loadSysPtrs = false;
    }

    if (mode == ACTIVE || mode == PRE_RELEASE) {
        double dt { isFrozen() ? 0.0 : dt0 };
        double dt4 { dt * 4.0 };   // dt total do frame (= dt/4 * 4)

        switch (getWorldModel()->phase()) {

        case 0:   // Fase 0: DINAMICA
            dynamics(dt4);   // delega para DynamicsModel

            // Logging de dados (gravador)
            if (dataLogTime > 0.0) {
                dataLogTimer -= dt4;
                if (dataLogTimer <= 0.0) {
                    BEGIN_RECORD_DATA_SAMPLE(
                        getWorldModel()->getDataRecorder(), REID_PLAYER_DATA)
                        SAMPLE_1_OBJECT(this)
                    END_RECORD_DATA_SAMPLE()
                    dataLogTimer = dataLogTime;
                }
            }

            // Atualiza assinaturas RF e IR apos mover
            if (signature   != nullptr) signature->updateTC(dt4);
            if (irSignature != nullptr) irSignature->updateTC(dt4);
            break;

        case 1:   // Fase 1: sistemas TRANSMITEM
            break;  // tratado pelos subsistemas em BaseClass::updateTC()

        case 2:   // Fase 2: sensores RECEBEM
            break;

        case 3:   // Fase 3: LOGICA de bordo
            break;
        }

        // BaseClass::updateTC() atualiza todos os subsistemas filhos
        // (Pilot, Navigation, RfSensor, etc.) com a mesma fase
    }
}
```

**As fases 1, 2 e 3 têm `case` vazios no `Player`** — os subsistemas filhos recebem
`tcFrame(dt)` via `BaseClass::updateTC()` e cada um implementa seu comportamento de fase
internamente.

### `SynchronizedState` — o *snapshot* sem *lock* entre *threads*

O `AbstractNetIO` lê posição e velocidade de cada *player* local na **sua própria
*thread*** — concorrentemente com a *thread* TC que atualiza esses valores. Sem proteção, a
*thread* de rede poderia ler um estado parcialmente escrito. `SynchronizedState` resolve
com **duplo buffer sem lock**: dois *snapshots* alternados, escritos ao final da fase 0.

```cpp
// src/models/player/Player.cpp -- ao final da fase 0, apos positionUpdate()
if (!syncState1Ready) {
    // Escreve no buffer 1
    syncState1.setGeocPosition(getGeocPosition());
    syncState1.setGeocVelocity(getGeocVelocity());
    syncState1.setGeocAcceleration(getGeocAcceleration());
    syncState1.setGeocEulerAngles(getGeocEulerAngles());
    syncState1.setAngularVelocities(getAngularVelocities());
    syncState1.setTimeExec(getWorldModel()->getExecTimeSec());
    syncState1.setTimeUtc(getWorldModel()->getSysTimeOfDay());
    syncState1.setValid(true);
    syncState1Ready = true;    // buffer 1 pronto para leitura
    syncState2Ready = false;   // buffer 2 sera o proximo a ser escrito
} else {
    syncState2.setGeocPosition(getGeocPosition());
    // ... idem ...
    syncState2Ready = true;
    syncState1Ready = false;
}
```

```cpp
// include/mixr/models/player/Player.inl
inline const SynchronizedState& Player::getSynchronizedState() const
{
    if (syncState1Ready)
        return syncState1;
    else
        return syncState2;
}
```

O mecanismo funciona porque **apenas a *thread* TC escreve** (alternando entre os buffers) e
**apenas a *thread* de rede lê** (o buffer marcado como pronto). Não há condição de corrida:
a *thread* TC nunca escreve no buffer que a *thread* de rede está lendo.

`SynchronizedState` carrega, **em ECEF**: posição, velocidade, aceleração, ângulos de Euler
geocêntricos e velocidade angular — exatamente o conjunto que um PDU DIS necessita para o
campo de *dead reckoning*.

## 23.3 *Dynamics* — modelos de física de voo

O `DynamicsModel` integra as equações de movimento de um `Player` a cada fase 0.
**`DynamicsModel` deriva diretamente de `Component`** (não de `System`) — o mesmo vale para
`Route` e `Steerpoint`; nenhum dos três participa do despacho por fase de `System`.

### O contrato

```cpp
// resumo de include/mixr/models/dynamics/DynamicsModel.hpp
class DynamicsModel : public base::Component {
public:
    // GRUPO 1: integracao (chamado pelo Player na fase 0)
    virtual void dynamics(const double dt);     // default: nao faz nada
    virtual void atReleaseInit();               // arma: init no lancamento

    // GRUPO 2: comandos do Autopilot
    // Todos retornam false por default.
    virtual bool setHeadingHoldOn(const bool);
    virtual bool setCommandedHeadingD(const double h,
                                      const double hDps    = 0,
                                      const double maxBank = 0);
    virtual bool setVelocityHoldOn(const bool);
    virtual bool setCommandedVelocityKts(const double v, const double vNps = 0);
    virtual bool setAltitudeHoldOn(const bool);
    virtual bool setCommandedAltitude(const double a,
                                      const double aMps    = 0,
                                      const double maxPitch = 0);

    // GRUPO 3: entradas manuais (joystick)
    // roll: -1.0 (max esq) -> 0.0 (centro) -> +1.0 (max dir)
    virtual void setControlStickRollInput(const double roll);
    // pitch: -1.0 (nariz desce) -> 0.0 (centro) -> +1.0 (nariz sobe)
    virtual void setControlStickPitchInput(const double pitch);
    // throttle: <0=corte, 0=idle, 1=MIL, 2=afterburner
    virtual int  setThrottles(const double* const pos, const int num);
    virtual void setBrakes(const double left, const double right);
};
```

**ARMADILHA — o valor de retorno dos comandos de autopiloto é DESCARTADO.**
`Autopilot::headingController()`, `altitudeController()` e `velocityController()` chamam
`setCommandedHeadingD()`, `setHeadingHoldOn()` e congêneres **ignorando o `bool`
devolvido**. A escolha entre modo *hold* e comando de manche é feita por
`isHeadingHoldOn() || isNavModeOn()` — estado do próprio autopiloto — e não pela resposta do
modelo de dinâmica. Uma busca em toda a árvore não encontra um único ponto que teste esses
retornos.

Na prática: acoplar um `Autopilot` a um `DynamicsModel` que não implemente *heading hold*
**não produz recuo automático para o manche**. Produz uma aeronave que não vira, sem nenhum
diagnóstico. Os `false` existem como valor de retorno plausível, não como protocolo em uso.

```cpp
// resumo de include/mixr/models/dynamics/AerodynamicsModel.hpp
class AerodynamicsModel : public DynamicsModel {
public:
    virtual double getGload()              const;  // G's (+up, 1.0 em voo nivel)
    virtual double getMach()               const;
    virtual double getAngleOfAttack()      const;  // rad (getAngleOfAttackD() em graus)
    virtual double getSideSlip()           const;  // rad
    virtual double getFlightPath()         const;  // rad
    virtual double getCalibratedAirspeed() const;  // kts
    virtual double getAmbientPressureRatio()const;
    virtual double getWingSweepAngle()     const;
    // entradas de trim (alem do stick)
    virtual void setTrimSwitchRollInput(const double rollTrim);
    virtual void setTrimSwitchPitchInput(const double pitchTrim);
    virtual void setRudderPedalInput(const double pedal);
};
```

Todas as métricas têm *default* retornando 0.0.

### `RacModel` — comportamento sem aerodinâmica

*Rate-and-Control* simula o movimento como **resposta de primeira ordem** aos comandos —
sem equações de sustentação, arrasto ou momento. Adequado para alvos, ameaças simuladas e
*wingmen* de baixa prioridade.

```cpp
// resumo de include/mixr/models/dynamics/RacModel.hpp
class RacModel final : public AerodynamicsModel {
    // Slots:
    //   minSpeed   <Number>   -- velocidade minima (kts)
    //   speedMaxG  <Number>   -- velocidade para Gmax (kts)
    //   maxg       <Number>   -- G's maximos (default: 4.0)
    //   maxAccel   <Number>   -- aceleracao maxima (m/s/s, default: 10.0)
    //   cmdAltitude <Distance>
    //   cmdHeading  <Angle>
    //   cmdSpeed    <Number>  -- velocidade comandada (kts)

private:
    double vpMin    {};         // velocidade minima (m/s)
    double vpMaxG   {250.0};    // velocidade para G maximo (m/s)
    double gMax     {4.0};      // G maximo
    double maxAccel {10.0};     // aceleracao maxima longitudinal (m/s/s)
    double cmdAltitude {-9999.0};
    double cmdHeading  {-9999.0};
    double cmdVelocity {-9999.0};

    void updateRAC(const double dt);  // logica interna de convergencia
};
```

A cada `dynamics(dt)`, `updateRAC()` calcula o erro entre rumo atual e comandado, vira a uma
taxa proporcional limitada por G_max e v_pMaxG; sobe ou desce em direção à altitude
comandada limitado por `maxAccel`; e acelera em direção à velocidade comandada dentro do
mesmo limite. **Não há matriz de rotação nem quaternion** — o modelo opera diretamente em
ângulos de Euler e velocidade escalar.

### `LaeroModel` — quatro graus de liberdade

Modelo linear 4DOF: translação (x, y, z) mais *yaw* (ψ). *Roll* e *pitch* são determinados
pelos comandos, não por equações de momento. A integração usa **Adams-Bashforth de segunda
ordem**:

```
u_k = u_{k-1} + (1/2)(3 u̇_{k-1} - u̇_{k-2}) Δt
```

Mais estável que Euler simples com o mesmo custo, pois usa dois passos anteriores para
extrapolação. As velocidades no referencial do corpo são convertidas para NED pela matriz
de cossenos diretores calculada dos ângulos de Euler atuais via
`base::nav::computeRotationalMatrix()`.

### `JSBSimModel` — fidelidade completa (6DOF)

Integra a biblioteca JSBSim, carregando um arquivo XML de definição de aeronave:

```
dynamicsModel: ( JSBSimModel
    model:   "f16"          -- carrega <rootDir>/aircraft/f16/f16.xml
    rootDir: "/data/jsbsim"
)
```

```cpp
// ciclo de JSBSimModel::dynamics() (simplificado)
void JSBSimModel::dynamics(const double dt)
{
    // 1. Autopilot escreveu comandos via setCommandedHeadingD() etc.
    //    -- JSBSimModel os traduz para posicoes de superficie FCS
    //    (aileron, elevator, rudder, throttle) via PropertyManager

    // 2. Executa um passo do simulador
    fdm->Run();

    // 3. Extrai resultados e escreve no Player
    Player* p { static_cast<Player*>(findContainerByType(typeid(Player))) };

    p->setPositionLLA(
        fdm->GetPropagate()->GetLatitudeDeg(),
        fdm->GetPropagate()->GetLongitudeDeg(),
        fdm->GetPropagate()->GetAltitudeASLmeters());

    const double vNorth { fdm->GetPropagate()->GetVel(JSBSim::FGJSBBase::eNorth) };
    // ...

    p->setEulerAngles(
        fdm->GetPropagate()->GetEuler(JSBSim::FGJSBBase::ePhi),
        fdm->GetPropagate()->GetEuler(JSBSim::FGJSBBase::eTht),
        fdm->GetPropagate()->GetEuler(JSBSim::FGJSBBase::ePsi));
}
```

`atReleaseInit()` garante que uma arma inicializa seu `JSBSimModel` com as condições do
lançador no instante do lançamento — posição geodésica, velocidade NED e ângulos de Euler
são copiados do `Player` pai antes de `fdm->Run()` ser chamado pela primeira vez.

| Modelo | Fidelidade | CPU | DOF | Uso típico |
|---|---|---|---|---|
| `RacModel` | Baixa | Mínimo | Cinemático | alvos, ameaças |
| `LaeroModel` | Média | Baixo | 4DOF | *wingmen*, UAVs |
| `JSBSimModel` | Alta | Significativo | 6DOF | *ownship* |

## 23.4 `System` — a base dos subsistemas embarcados

Quase todo subsistema embarcado de um `Player` herda de `System`, que herda de `Component`.
A extensão central de `System` é o **acesso preguiçoso (*lazy*) ao `Player` dono e ao
`WorldModel`** — via `findContainerByType()`, na primeira chamada, com resultado em *cache*:

```cpp
// src/models/system/System.cpp
Player* System::getOwnship()
{
   // Primeira chamada: findOwnship() sobe a arvore de containers ate
   // encontrar o Player dono e guarda o ponteiro em 'ownship'.
   // Chamadas seguintes: o teste contra nullptr e todo o custo.
   if (ownship == nullptr) findOwnship();
   return ownship;
}
// findOwnship():
//    ownship = static_cast<Player*>(findContainerByType(typeid(Player)));
// idem para getWorldModel()
```

A segunda extensão é o **despacho por fase**:

| Fase | Método de `System` | Quem o implementa na cadeia RF |
|---|---|---|
| 0 | `dynamics(dt4)` | `Stt` (aponta a antena para a pista) |
| 1 | `transmit(dt4)` | `RfSensor` → `Antenna::rfTransmit()` |
| 2 | `receive(dt4)` | `Radar` (equação do radar, limiar) |
| 3 | `process(dt4)` | `Radar` (consolida e alimenta o `TrackManager`) |

## 23.5 A cadeia RF — percurso completo de uma detecção

O caminho inteiro de uma detecção atravessa **duas *threads* e três fases**:

```
Thread de FUNDO  -- RfSystem::updateData()
  └─ Antenna::processPlayersOfInterest()
        cria o Tdb; processPlayers() FILTRA por alcance, angulo, tipo, terreno

Fase 1  -- transmit()
  └─ Antenna::rfTransmit()
        computeBoresightData() MEDE a geometria; ganho da antena; ERP
  └─ Emission -> alvo->event(RF_EMISSION, em)
        leva potencia, frequencia e a perda de percurso JA CALCULADA
  └─ alvo reflete: RfSignature::getRCS() -> volta ao emissor
        RfSystem::rfReceivedEmission() calcula o sinal de IDA

Fase 2  -- receive()
  └─ Radar::receive()
        aplica RCS e a SEGUNDA perda; S/I em dB >= limiar -> rptQueue

Fase 3  -- process()
  └─ Radar::process() -> TrackManager::newReport()
        ao fim da varredura: melhor S/N por alvo; filtro alfa-beta mantem a pista
```

Três propriedades que contrariam suposições naturais:

1. **A filtragem e a medição estão em lugares diferentes.** Escolher *quais* alvos
   considerar é caro e tolera defasagem, então roda na *thread* de fundo. Medir a geometria
   *daqueles* alvos é barato e precisa ser atual, então roda a cada transmissão.
2. **A entrega é por evento, não por chamada.** A `Emission` chega ao alvo como
   `RF_EMISSION` — o transmissor não conhece a interface do receptor.
3. **A detecção não vira pista na mesma fase.** Entre o eco (fase 2) e a pista (fase 3) há
   uma fila, e a consolidação por alvo acontece apenas ao fim de uma varredura completa da
   antena.

### `Gimbal` e `Tdb` — apontamento e preenchimento

`Gimbal` é a base de tudo que aponta — antenas, óticas, buscadores IR. Monta, uma vez por
passagem, a lista de alvos que vale a pena considerar: o **Tdb**.

**ARMADILHA — o `Tdb` é montado na *thread* de fundo, não na fase 1.** Quem chama
`processPlayersOfInterest()` é `RfSystem::updateData()` — e `updateData()` roda na *thread*
de fundo, a uma taxa própria e **não sincronizada** com o quadro de tempo crítico (o
caminho IR é análogo, via `IrSystem::updateData()`). **`Gimbal` sequer sobrescreve
`updateTC()`.** A consequência de projeto é boa: a filtragem de centenas de *players* — cara
e tolerante a alguns milissegundos de defasagem — sai do caminho crítico. A consequência
prática é que **o `Tdb` que a fase 1 encontra pode ter sido montado há um ou dois quadros**.

Dimensionamento: o *slot* `maxPlayersOfInterest` tem padrão **200**. A constante
`Gimbal::MAX_PLAYERS` — *alias* de `MIXR_CONFIG_MAX_PLAYERS_OF_INTEREST`, padrão **4000** —
é outra coisa: dimensiona os arrays temporários dentro de `Antenna::rfTransmit()` e do
`IrSeeker`. E **ela não limita o *slot***: `setMaxPlayersOfInterest()` atribui sem
verificar, de modo que declarar `maxPlayersOfInterest` acima de 4000 é possível — e
transborda os arrays de `rfTransmit()`.

```cpp
// src/models/system/Gimbal.cpp
unsigned int Gimbal::processPlayersOfInterest(base::PairStream* const poi)
{
    // Cria novo Tdb para este frame
    const auto tdb0 = new Tdb(maxPlayers, this);

    // Preenche: LOS, ranges, rngRates, vetores az/el em coord. gimbal
    unsigned int ntgts { tdb0->processPlayers(poi) };

    setCurrentTdb(tdb0);   // armazena para uso por Antenna e RfSensor
    tdb0->unref();
    return ntgts;
}
```

**Segundo mal-entendido a desfazer:** `Tdb::processPlayers()` **não calcula geometria**. Ele
apenas *filtra* a lista de *players* — por alcance máximo, ângulo, máscara de tipo, oclusão
de terreno e pelo *flag* de "apenas locais". O resultado é uma lista de candidatos.

A geometria propriamente dita — LOS, alcances, taxas de aproximação, transformação para o
referencial do *gimbal*, ângulos fora do *boresight* — é calculada por
`Tdb::computeBoresightData()`, chamada **a cada transmissão**, de dentro de
`Antenna::rfTransmit()` e do `IrSeeker`. **A divisão de custo é "filtrar raramente, medir
sempre".**

```cpp
// src/models/Tdb.cpp -- computeBoresightData()
      for (unsigned int i = 0; i < numTgts; i++) {

         base::Vec3d pt;  // Position Vector
         base::Vec3d vt;  // Velocity vector [ x y z ] (meters/second)
         // ... escolha entre ECEF e NED conforme 'usingEcefFlg' ...

         base::Vec3d los{pt - p0};                    // Target LOS vector

         ranges[i] = los.normalize();                 // unit vector e range (m)

         rngRates[i] = ( (vt - v0) * los );            // range rate (m/s)

         if (usingEcefFlg) {
            // Rotate the LOS vectors into their local tangent planes
            losO2T[i] = wm * los;
            losT2O[i] = targets[i]->getWorldMat() * (-los);
         } else {
            losO2T[i] =  los;
            losT2O[i] = -los;
         }
      }

      // Start with the body to gimbal matrix
      base::Matrixd mm{gimbal->getRotMat()};

      // Post multi by the inertial (NED) to body matrix
      if (ownHdgOnly) {
         // Using heading only, ignore ownship roll and pitch
         base::Matrixd rr;
         rr.makeRotate( ownship->getHeading(), 0, 0, 1);
         mm *= rr;
      } else {
         mm *= ownship->getRotMat();
      }
      // ... losG = mm * losO2T, e dai os angulos fora do boresight ...
```

O *flag* `ownHdgOnly` vem de `Gimbal::isUsingHeadingOnly()` e é **ligado por padrão**: com
ele, apenas a proa do *ownship* entra na matriz de transformação, e rolagem e arfagem são
ignoradas. O efeito é **estabilizar o *gimbal* em relação ao horizonte** — o que se quer de
um radar de busca, que não deve varrer céu e chão alternadamente quando a aeronave inclina.

### `ScanGimbal` — os sete modos de varredura

| Modo | Padrão |
|---|---|
| `MANUAL_SCAN` | sem varredura: o apontamento é comandado de fora |
| `HORIZONTAL_BAR_SCAN` | varredura em barras horizontais (busca típica de radar) |
| `VERTICAL_BAR_SCAN` | idem, em barras verticais |
| `CONICAL_SCAN` | cone em torno do eixo — usado no rastreio de alvo único |
| `CIRCULAR_SCAN` | círculo completo (vigilância) |
| `PSEUDO_RANDOM_SCAN` | sequência pseudoaleatória de posições |
| `SPIRAL_SCAN` | espiral, do centro para fora |

**Não existem `BAR_SCAN` nem `SECTOR_SCAN`.**

`StabilizingGimbal` adiciona estabilização inercial que desacopla o movimento da antena do
movimento da aeronave, mantendo o feixe no alvo durante manobras.

### `Antenna` — transmissão e cálculo de ERP

`Antenna` herda de **`ScanGimbal`** — e não de `Gimbal` diretamente, o que importa porque
`Stt` e `Sar` comandam-na pela API de varredura.

```cpp
// src/models/system/Antenna.cpp -- rfTransmit()
// A geometria e (re)calculada AQUI, a cada transmissao
unsigned int ntgts{tdb->computeBoresightData()};
if (ntgts > MAX_PLAYERS) ntgts = MAX_PLAYERS;

// Consulta o padrao de ganho: tabela 2D (az x el) em DECIBEIS
double gainTgt0[MAX_PLAYERS]{};
if (gainPatternDeg) {
   for (unsigned int i1 = 0; i1 < ntgts; i1++) {
      gainTgt0[i1] = gainFunc2->f( (aazr[i1] * base::angle::R2DCC),
                                   (aelr[i1] * base::angle::R2DCC) )/10.0;
   }
} else {
   for (unsigned int i1 = 0; i1 < ntgts; i1++) {
      gainTgt0[i1] = gainFunc2->f( aazr[i1], aelr[i1] )/10.0f;
   }
}
base::pow10Array(gainTgt0, gainTgt, ntgts);      // 10^(dB/10) -> razao de POTENCIA

// Ganho efetivo = padrao x ganho nominal da antena
double aeGain[MAX_PLAYERS]{};
base::multArrayConst(gainTgt, getGain(), aeGain, ntgts);

// Compute Effective Radiated Power (watts) (Equation 2-1)
double erp[MAX_PLAYERS]{};
base::multArrayConst(aeGain, xmit->getPower(), erp, ntgts);

for (unsigned int i = 0; i < ntgts; i++) {
   // Only of power exceeds an optional threshold
   if (erp[i] > threshold) {
      // ... obtem uma Emission (reciclada da pilha, ou clonada) ...
      em->setRange( ranges[i] );          // <- e setRange() que calcula lossRng
      em->setPower( erp[i] );
      em->setGain( aeGain[i] );
      em->setTarget( targets[i] );
      // ...
      targets[i]->event(RF_EMISSION, em);  // entrega POR EVENTO
   }
}
```

**ARMADILHA — o ganho é dividido por dez ANTES da exponenciação.** A tabela de padrão de
antena guarda **decibéis**, e a conversão para razão linear é 10^(dB/10) — daí o `/10.0`
antes do `pow10Array()`. Esquecê-lo, e calcular 10^dB, produz ganhos com ordens de grandeza
a mais. O expoente dividido por dez é o que faz da razão uma razão de **potência**, e não de
amplitude.

E é **`Emission::setRange()`** — não o `Tdb` — que calcula e guarda a perda de percurso
1/(4πr²). O receptor nunca precisa conhecer a distância: multiplica pelo campo já pronto.

### `RfSystem`, `RfSensor` e `Radar` — o cálculo do sinal em DUAS METADES

**Primeira metade: o trecho de IDA, em `RfSystem`.** Todo sistema RF que recebe uma emissão
calcula primeiro o sinal de *um caminho só*, antes de saber se é eco ou emissão direta:

```cpp
// src/models/system/RfSystem.cpp -- rfReceivedEmission()
double losses{getRfSignalProcessLoss() * em->getAtmosphericAttenuationLoss()
                                       * em->getTransmitLoss()};
if (losses < 1.0) losses = 1.0;

const double rl{em->getRangeLoss()};          // 1/(4*pi*R^2), ja pronto

// Sinal de UM caminho
const double signal{em->getPower() * rl * raGain / losses};
```

**Segunda metade: o trecho de VOLTA, em `Radar`.** Só quem sabe que aquilo é eco de radar
aplica a RCS e a *segunda* perda de percurso:

```cpp
// src/models/system/Radar.cpp -- receive() (essencia)
// Interferencia = ruido termico + jamming; ruido = so o termico.
// (equacao S/I de Hannen: I = N + J)
const double interference{(getRfRecvNoise() + jamSignal) * getRfReceiveLoss()};
const double noise{getRfRecvNoise() * getRfReceiveLoss()};

// Para cada Emission da fila (protegida por packetLock):
    double rcs{em->getRCS()};
    double rl {em->getRangeLoss()};

    signal *= (rcs * rl);     // <- RCS e a SEGUNDA perda de percurso
    signal *= rfIGain;        // ganho de integracao (slot 'igain')

    // Razao sinal/interferencia, em dB
    // ... comparada contra o limiar, tambem em dB:
    if (signalToInterferenceRatioDbl >= getRfThreshold()
        && em->getRange() <= (maxRng * 1.25)
        && rptQueue.isNotFull()) {
        // enfileira o relatorio -- NAO chama o TrackManager aqui
    }
```

**ARMADILHA — a equação do radar do MIXR NÃO é a do livro-texto.** A forma clássica
`Pr = Pt Gt Gr λ² σ / ((4π)³ R⁴)` tem um λ² e um (4π)³. **Nenhum dos dois aparece no
código.** O que o MIXR calcula é:

```
S = Pt · Gr · rℓ² · σ · G_int / L ,   com  rℓ = 1/(4π R²)
```

com o quadrado de rℓ vindo de rℓ ter sido aplicado uma vez na ida e outra na volta. **O
comprimento de onda existe na `Emission` e é usado para Doppler, não para o balanço de
potência.** Quem comparar resultados do MIXR com uma planilha de equação de radar padrão vai
encontrar uma diferença constante — e ela está aqui.

**POR QUÊ a perda de percurso é pré-calculada na emissão.** O campo `lossRng` da `Emission`:
o transmissor, que já conhece a distância ao alvo, calcula 1/(4πR²) uma vez e o embarca na
mensagem. O receptor **nunca precisa saber a que distância está o emissor** — basta
multiplicar. É o mesmo valor aplicado duas vezes que produz o R⁴ do eco, sem que ninguém
eleve nada à quarta potência.

Dois pontos que a leitura apressada inverte:

- O limiar `getRfThreshold()` é comparado em **dB**, não em razão linear.
- `receive()` **não entrega nada ao `TrackManager`**: ele enfileira relatórios em
  `rptQueue`, e é `Radar::process()` — na fase 3, ao fim da varredura — que consolida o
  melhor sinal por alvo e só então chama `newReport()`.

**Subclasses de `RfSensor`:**

- `Jammer` — transmite ruído que eleva o nível de interferência nos receptores inimigos.
  **Não há distinção entre banda larga e *spot***: `Jammer::transmit()` marca toda emissão
  como `Emission::ECM_NOISE`, e é o `Radar` receptor que soma esse sinal ao seu termo de
  interferência.
- `Rwr` (*Radar Warning Receiver*) — recebe `Emission`s sem transmitir e alimenta o
  `RwrTrkMgr` com as ameaças detectadas.
- `SensorMgr` — peculiar: tem `EMPTY_SLOTTABLE`, `EMPTY_COPYDATA`, `EMPTY_DELETEDATA` e
  construtor vazio; o arquivo inteiro tem doze linhas úteis.

**POR QUÊ `SensorMgr`, uma classe vazia que existe para agrupar.** Não é código morto; é um
recipiente. `RfSensor` herda de `System`, e portanto é um `Component` com filhos — pendurar
vários radares sob um `SensorMgr` os faz atualizar juntos, na ordem da lista, sob um único
nó nomeado da árvore. O ganho está em EDL: um
`sensor: ( SensorMgr { ( Radar ... ) ( Rwr ... ) } )` dá ao *player* um conjunto de sensores
em vez de um; e como `SensorMgr` **é** um `RfSensor`, todo o código que espera um sensor
continua funcionando sem saber que recebeu um grupo. É o padrão *Composite* clássico.

## 23.6 *Track managers*

O `TrackManager` é o elo entre o sensor (que detecta um alvo num único quadro) e o
`OnboardComputer` (que precisa de histórico). Mantém a lista de `Track`s, filtra associações
entre relatórios e *tracks*, e propaga a cinemática entre quadros.

```cpp
// resumo de include/mixr/models/system/trackmanager/TrackManager.hpp
class TrackManager : public System {
    // Slots:
    //   maxTracks     <Number>   -- max de tracks (default: MAX_TRKS)
    //   maxTrackAge   <Time>     -- tempo maximo de vida de um track (default: 3s)
    //   firstTrackId  <Integer>  -- primeiro ID de track (default: 1000)
    //   alpha         <Number>   -- parametro alpha do filtro (default: 1.0)
    //   beta          <Number>   -- beta (default: 0.0)
    //   gamma         <Number>   -- gamma (default: 0.0)

    Track*       tracks[MAX_TRKS] {};   // lista de tracks ativos
    unsigned int nTrks {};
    mutable long trkListLock {};        // semaforo que protege a lista

    // Filas de entrada (de Emission + SNR)
    base::safe_queue<Emission*> emQueue { MAX_TRKS };
    base::safe_queue<double>    snQueue { MAX_TRKS };

    // Matriz de predicao A para o filtro alfa-beta
    double A[3][3] {};   // computada em makeMatrixA(dt)
    double alpha {1.0};
    double beta  {};
    double gamma {};

    double maxTrackAge {3.0};  // segundos
};
```

Quando `trkPlayer != nullptr`, a cinemática é atualizada **diretamente da posição do player
local** — sem correlação de sinal. Para players *networked* ou não resolvidos,
`processTrackList()` aplica o filtro:

```cpp
// src/models/system/trackmanager/RwrTrkMgr.cpp -- processTrackList()
// X(k+1) = A * X(k) + B * U(k)
// onde U(k) = (posicao_observada - posicao_predita)
if (haveU[i]) {
    double b0 { alpha };  // correcao de posicao
    double b1 {};         // correcao de velocidade (= beta / dt)
    double b2 {};         // correcao de aceleracao (= gamma / 2dt^2)
    tracks[i]->setPosition(
        (tpos*A[0][0] + tvel*A[0][1] + tacc*A[0][2]) + (u[i]*b0));
    tracks[i]->setVelocity(
        (tpos*A[1][0] + tvel*A[1][1] + tacc*A[1][2]) + (u[i]*b1));
    tracks[i]->setAcceleration(
        (tpos*A[2][0] + tvel*A[2][1] + tacc*A[2][2]) + (u[i]*b2));
} else {
    // Sem observacao neste frame: apenas prediz com o modelo cinematico
    tracks[i]->setPosition(tpos*A[0][0] + tvel*A[0][1] + tacc*A[0][2]);
    // ...
}
```

**ARMADILHA — β é usado por dois gerentes; γ por NENHUM.** Os três *slots* `alpha`, `beta`
e `gamma` sugerem um filtro α-β-γ completo. O código conta outra história:

- Em `RwrTrkMgr`, os ganhos de velocidade e aceleração são declarados como
  `double b1{}; double b2{};` — literalmente zero. **Nem β nem γ têm qualquer efeito ali.**
- Em `AirTrkMgr` e `GmtiTrkMgr`, β **é** usado, na forma b₁ = β/idade — dividido pela
  **idade da pista**, não pelo Δt do quadro.
- A linha que calcularia b₂ a partir de γ está **comentada** nos dois. `gamma` é lido para
  um membro e nunca mais consultado.

Na prática: **ajustar `gamma:` num arquivo EDL não produz efeito nenhum, e não gera aviso.**
O filtro efetivo é α-β em dois dos quatro gerentes, e α puro nos demais.

**Os gerentes concretos:**

- `AirTrkMgr` — pistas aéreas (usado pelo `Tws` e pelo `OnboardComputer`).
- `GmtiTrkMgr` — alvos terrestres em movimento.
- `RwrTrkMgr` — ameaças detectadas pelo RWR.
- `AngleOnlyTrackManager` — base intermediária; acrescenta os *slots* `azimuthBin` e
  `elevationBin` (tamanho da célula de resolução angular usada para associar relatórios a
  pistas).
- `AirAngleOnlyTrkMgr` — rastreia usando **apenas medições angulares** (azimute e
  elevação), sem alcance — modo de buscadores IR passivos e *seekers* de mísseis antes do
  *endgame*.
- `AirAngleOnlyTrkMgrPT` — **`PT` é de *perceived truth*, não *predictive tracking***.
  Existe para consumir os relatórios do `MergingIrSensor`, que funde em um único retorno os
  alvos caídos dentro da mesma célula angular. A "verdade percebida" é esse centroide de
  dois ou mais alvos — e o *slot* `usePerceivedPosVel` decide se posição e velocidade
  fundidas também são aproveitadas, ou apenas os ângulos.

## 23.7 Sensores especializados

### Cadeia IR — `IrSeeker` e `IrQueryMsg`

O equivalente IR opera com o mesmo ciclo de fases, mas o **protocolo é de *query/response***
em vez de emissão livre. Na fase 1, o `IrSeeker` constrói uma `IrQueryMsg` por alvo e a
envia ao *player* alvo via `Player::irReceivedQuery()`:

```cpp
// src/models/system/IrSeeker.cpp -- irRequestSignature() (simplificado)
unsigned int ntgts { tdb0->computeBoresightData() };

for (unsigned int i = 0; i < ntgts; i++) {
    // Filtra pelo alcance maximo do sensor
    if (maximumRange > 0.0 && ranges[i] > maximumRange) continue;

    // Obtem um pacote livre do pool (evita alocacao por frame)
    IrQueryMsg* query { freeQueryStack.pop() };
    if (query == nullptr) query = new IrQueryMsg();

    query->setRange(ranges[i]);
    query->setAngleAspect(anglesOffBoresight[i]);
    query->setSendingSensor(this);
    query->setGimbal(this);

    // Envia ao alvo: alvo consultara seu IrSignature e preenchera o sinal
    targets[i]->irReceivedQuery(query, this);
}
```

Na fase 2, o alvo preenche a potência do sinal por banda espectral de volta na
`IrQueryMsg`, e `WorldModel::getAtmosphere()` atenua o sinal pela transmitância
atmosférica. O `IrSensor` calcula o **SNR** — razão entre o sinal atenuado e a irradiância
equivalente de ruído (**NEI**) multiplicada pelo **IFOV** — e, se acima do limiar, gera um
relatório para o `AngleOnlyTrackManager`.

### Variantes de radar

- **`Tws` (*Track While Scan*)** — subclasse de `Radar` que mantém a varredura contínua
  enquanto rastreia múltiplos alvos. **Toda a lógica TWS vive no `AirTrkMgr` associado** — o
  `Tws` funciona como etiqueta de intenção que configura o `TrackManager` correto.
- **`Stt` (*Single Target Track*)** — implementa `dynamics(dt)` com lógica real: aponta a
  antena para o primeiro *track* do `TrackManager` usando varredura cônica
  (`CONICAL_SCAN`), estimando o erro de apontamento pelo desequilíbrio de potência entre os
  lados do cone. Na ausência de *track*, reverte para varredura horizontal de busca.
- **`Gmti` (*Ground Moving Target Indicator*)** — aponta a antena para um *Point of
  Interest* (vetor NED, *slot* `poi`) e alimenta o `GmtiTrkMgr`.

### `Sar` — imagem sintética de abertura

`Sar` herda de **`Radar`** — não de `Gimbal`. O fluxo é disparado por um `ActionImagingSar`:
a ação chama `setStarePoint()` com latitude, longitude e elevação do ponto de interesse,
depois `requestImage()` com largura, altura e resolução. O `Sar` aponta a antena, marca
`isImagingInProgress()` e, decorrido o tempo de integração, produz um objeto `Image`.

O único *slot* da classe é `chipSize`; **o tempo de integração é uma constante de arquivo de
dez segundos, não configurável**.

**ARMADILHA — o `Sar` não usa o terreno e não gera imagem real.**

- **Não há terreno.** `Sar.cpp` não inclui nem menciona `terrain::Terrain`; não há chamada a
  `getElevations()`, nenhum cálculo de ângulo de incidência, nenhuma geometria de visada.
- **A imagem é um padrão de teste.** Ao concluir, o código faz literalmente
  `p->testImage(width, height)` — figura sintética gerada proceduralmente, sem relação com o
  cenário.
- **Não existe `SAR_COMPLETE_EVENT`**: uma busca por esse identificador em toda a árvore não
  devolve nada. A conclusão é **consultada**, não notificada — `ActionImagingSar::process()`
  chama `sar->isImagingInProgress()` a cada quadro até que devolva falso.

O que está pronto é o **protocolo**: pedido, apontamento, espera, entrega de um `Image`. O
conteúdo da imagem é o ponto de extensão deixado em aberto.

## 23.8 Comunicações

### `Radio`, `CommRadio` e `Datalink`

`Radio` deriva de **`RfSystem`**, não de `System` diretamente. Essa escolha de herança é a
chave: **no MIXR, uma mensagem de rádio é uma emissão de RF como qualquer outra**.

```cpp
// src/models/system/CommRadio.cpp
bool CommRadio::transmitDataMessage(base::Object* const msg)
{
   bool sent{};
   if (getOwnship() == nullptr) return sent;

   if (msg != nullptr && isTransmitterEnabled() && getAntenna() != nullptr) {
      // A mensagem viaja DENTRO de uma Emission
      const auto em = new Emission();
      em->setDataMessage(msg);                    // <- a carga util
      em->setFrequency(getFrequency());
      em->setBandwidth(getBandwidth());
      em->setPower(getPeakPower());
      em->setTransmitLoss(getRfTransmitLoss());
      em->setMaxRangeNM(getMaxDetectRange());
      em->setTransmitter(this);
      em->setReturnRequest(false);                // nao queremos eco
      getAntenna()->rfTransmit(em);               // mesma cadeia RF de sempre
      em->unref();
      sent = true;
   }
   return sent;
}
```

**POR QUÊ rádio reusa a cadeia de RF inteira.** Não há varredura da lista de *players*, não
há comparação de canal, não há entrega direta. A mensagem entra na mesma `Antenna` e
percorre o mesmo caminho de `Emission`. Isso significa que a comunicação herda, de graça,
tudo o que a cadeia RF já modela: alcance limitado pela potência, perda de percurso,
direcionalidade da antena, e a possibilidade de ser interferida por um `Jammer`. **Um rádio
no MIXR não é uma abstração de mensagem — é um transmissor.**

```cpp
// src/models/system/CommRadio.cpp
void CommRadio::receivedEmissionReport(Emission* const em)
{
   if (em != nullptr && datalink != nullptr) {
      base::Object* msg{em->getDataMessage()};
      if (msg != nullptr) datalink->event(DATALINK_MESSAGE, msg);
   }
}
```

Repare no nome do *token*: **`DATALINK_MESSAGE`, sem sufixo `_EVENT`**. E note que a carga é
um `base::Object*` qualquer — **o framework não impõe formato de mensagem**.

`Datalink` estende `System` (e **não** `Radio`) e é o destinatário dessas mensagens; é ele
que as enfileira e as apresenta ao `OnboardComputer`. A separação faz sentido: o rádio é o
meio físico, o *datalink* é o protocolo.

### `Iff` — identificação amigo ou inimigo

```
iff1: ( Iff
    mode1:   45     // 5 bits octal
    mode2:   1234   // 12 bits
    mode3a:  7777   // squawk (codigo ATC) -- o slot e "mode3a"
    // mode4 e mode5: criptografados (omitidos em EDL)
)
```

Os *slots* reais são **onze**: os códigos `mode1`, `mode2`, `mode3a`, `mode4a` e `mode4b`;
os interruptores `enableMode1`, `enableMode2`, `enableMode3a`, `enableMode4` e
`enableModeC`; e o seletor `whichMode4`, que escolhe entre os códigos 4A e 4B.

**ARMADILHA — não há modo 5, e o `TrackManager` não consulta o `Iff`.** Primeira: **não
existe `mode5`** — o modo 5 (Link 16) não é modelado. O que existe e passa despercebido é o
**modo C**, o de altitude, com seu próprio interruptor. Segunda: **nenhum dos quatro
gerentes de pista consulta o `Iff` de nada** — uma busca por `Iff` no diretório dos *track
managers* não devolve nada. O campo `iffCode` de um `Track` existe, mas quem o preenche tem
de ser código da aplicação.

## 23.9 `StoresMgr` — gerenciamento de armas

```cpp
// src/models/system/Stores.cpp
AbstractWeapon* Stores::releaseWeapon(AbstractWeapon* const wpn)
{
    AbstractWeapon* flyout {};
    Player* own { getOwnship() };

    if (wpn != nullptr && own != nullptr) {
        wpn->setLaunchVehicle(own);   // herda pos/vel/hdg do lançador
        flyout = wpn->release();      // muda modo para ACTIVE, cria flyout
    }
    return flyout;  // pre-ref()'d -- caller deve unref()
}
```

Após o retorno, `wpn->release()` já adicionou a arma à lista ativa da `Simulation` via
`addNewPlayer()`: a partir desse momento a arma tem física própria e é processada como
qualquer outra entidade no ciclo de quatro fases.

```cpp
// src/models/system/SimpleStoresMgr.cpp
AbstractWeapon* SimpleStoresMgr::getCurrentWeapon()
{
   AbstractWeapon* wpn{};
   if ( isWeaponDeliveryMode(A2A) ) {
      wpn = getNextMissile();    // We need a missile
   } else {
      wpn = getNextBomb();       // We need a bomb
   }
   return wpn;
}
```

```
components: {
  sms: ( StoresMgr           // <- nome de fabrica "StoresMgr" = SimpleStoresMgr!
    stores: {
        1: ( Missile         // estacao 1
            type:   "AIM-120C"    // slot 'type', herdado de Player
            maxTOF: ( Seconds 60.0 )
            tsg:    ( Seconds  1.5 )   // so guia apos 1.5 s de voo
        )
        3: ( Missile
            type:   "AIM-120C"
            maxTOF: ( Seconds 60.0 )
            tsg:    ( Seconds  1.5 )
        )
        5: ( Bomb            // estacao 5
            type:     "MK-82"
            fuzeTime: ( Seconds 4.0 )   // e 'fuzeTime', nao 'maxFuseTime'
        )
        7: ( Chaff  maxTOF: ( Seconds 8.0 ) )
    }
  )
}
```

Três correções escondidas nesse bloco:

**ARMADILHA — `( StoresMgr )` NÃO instancia `StoresMgr`.** O nome de fábrica `"StoresMgr"`
pertence a `SimpleStoresMgr`. A classe `StoresMgr` registra-se como `"BaseStoresMgr"` — e
**nem sequer está registrada** em `models/factory.cpp`. Escrever `( StoresMgr ... )`
constrói, portanto, um `SimpleStoresMgr`, com a sua seleção de arma por modo A2A/A2G. É o
caso mais consequente da divergência entre nome de classe e nome de fábrica.

Além disso: **armas não têm um *slot* `model`**. A identidade de uma arma vem do *slot*
`type`, herdado de `Player`, e é essa *string* que o `Ntm` usa para casar tipos DIS. E o
*slot* de espoleta da `Bomb` chama-se **`fuzeTime`**, não `maxFuseTime`.

### `Gun` — o canhão como um *store*

A classe chama-se **`Gun`**, no singular, e deriva de **`ExternalStore`**, exatamente como
`Stores` — e não de `System` nem de `StoresMgr`.

**POR QUÊ `ExternalStore` é o elo que falta.** `ExternalStore` representa *qualquer coisa
pendurada numa estação*: tem os *slots* `type` e `jettisonable` e mais nada. Dela derivam
`Stores` (um cabide que contém outros *stores*), `Gun` (um canhão), `FuelTank` (com
`fuelWt` e `capacity`) e `AvionicsPod`. É essa herança comum que permite tratar
uniformemente "o que está pendurado" — inclusive para *jettison*.

O disparo é governado por `Gun::process()`, que mantém um `burstFrameTimer` e usa
`computeBulletRatePerSecond()` para decidir quantos projéteis cabem no intervalo;
`burstFrame()` então os emite. Os *slots* são `bulletType`, `rounds`, `unlimited`, `rate`,
`burstRate` e a orientação da boca (`position`, `roll`, `pitch`, `yaw`). Cada `Bullet` é um
*player* com trajetória balística própria. **Não existe `StoresMgr::releaseOneBullet()`.**

## 23.10 Navegação

### `Navigation` — o sistema base

Herda de `System` e mantém o estado de navegação completo. A cada `updateData(dt)`,
consulta a `Route` ativa e produz os dados de pilotagem que o `Autopilot` consome:

```cpp
// resumo de include/mixr/models/navigation/Navigation.hpp
// Produzidos por Route.computeSteerpointData() e lidos pelo Autopilot
double tbrg {};   // True bearing to next steerpoint   (deg)
double mbrg {};   // Magnetic bearing                  (deg)
double dst  {};   // Distance to next steerpoint       (NM)
double ttg  {};   // Time-To-Go                        (sec)
double eta  {};   // Estimated Time of Arrival         (sec UTC)
double xte  {};   // Cross-Track Error                 (NM)  negativo = esq.
double tcrs {};   // True Course of current leg        (deg)
double mcrs {};   // Magnetic Course of current leg    (deg)
```

**ARMADILHA — `Ins` e `Gps` são recipientes de dados, não modelos.** É tentador supor que
`Ins` simule deriva inercial e que `Gps` simule latência de posicionamento. **Nenhum dos
dois simula coisa alguma.** Ambos são `EMPTY_SLOTTABLE` — não têm um único *slot* — e nenhum
dos dois sobrescreve `updateData()`, `process()`, `dynamics()` ou `reset()`. `Ins` tem 80
linhas de *getters*/*setters* para `gyroBias`, `accelBias`, `wander`, `alignMode`,
`alignTTG` e `quality`; `Gps` tem 83 linhas de enumerações de estado e armazenamento de
chaves criptográficas. **Não há integração de acelerações, não há deriva acumulada, não há
atualização a 1 Hz — não há nenhum código que *execute*.** Um cenário que declare `( Ins )`
obtém um componente inerte que se comporta exatamente como a `Navigation` base — lendo
posição verdadeira do *player*.

### `Steerpoint` e `Route`

Um `Steerpoint` carrega posição geodésica, parâmetros de voo e uma `Action` opcional a
executar ao chegar. O campo `next` (identificador) nomeia o próximo *steerpoint*,
permitindo rotas não lineares.

O método de cálculo é `compute(nav, from)` — o segundo argumento é o *steerpoint* anterior,
e é ele que permite distinguir o cálculo "direto para" (sem `from`) do cálculo "ao longo da
perna" (com), que é o que produz o *cross-track error*.

```cpp
// resumo de include/mixr/models/navigation/Steerpoint.hpp
// Campos calculados pelo metodo compute(nav) a cada updateData()
double tbrg {};   // True bearing direct-to  (deg)
double dst  {};   // Distance direct-to      (NM)
double ttg  {};   // Time-To-Go              (sec)
double xte  {};   // Cross-Track Error       (NM)
double eta  {};   // Est. Time of Arrival    (sec UTC)
double elt  {};   // PTA Early/Late Time     (sec)

// Configurados via slots no EDL
double cmdAlt      {};   // Commanded altitude   (m)
double cmdAirspeed {};   // Commanded airspeed   (kts)
double pta         {};   // Planned Time of Arrival (sec UTC)
```

```cpp
// src/models/navigation/Route.cpp
void Route::updateData(const double dt)
{
    BaseClass::updateData(dt);

    const Navigation* nav {
        static_cast<const Navigation*>(
            findContainerByType(typeid(Navigation))) };

    if (nav != nullptr) {
        // 1. Computa tbrg, dst, ttg, xte para cada steerpoint da rota
        computeSteerpointData(dt, nav);

        // 2. Verifica se chegou ao steerpoint corrente e avanca
        autoSequencer(dt, nav);
    }
}
```

**O sequenciamento automático exige TRÊS condições simultâneas**, e a terceira costuma
passar despercebida:

1. o *slot* `autoSequence` tem de estar ligado (padrão: sim);
2. a distância ao *steerpoint* tem de ser menor que `autoSeqDistNM` (padrão 2 NM);
3. **o rumo ao *steerpoint* tem de diferir do rumo atual em pelo menos 90°.**

Essa última condição significa "o ponto já ficou para trás" — sem ela, uma aeronave que
apenas passasse perto de um *waypoint* o consumiria indevidamente. Ao capturar, a rota
dispara a `Action` do *steerpoint* (se houver) e avança com `incStpt()`.

```cpp
// Desvio para waypoint por nome (chamado pelo OnboardComputer)
route->directTo("wp3");         // vai direto para wp3
route->directTo(3);             // vai direto para o 3o steerpoint
```

`Bullseye` é subclasse trivial de `Steerpoint` — semanticamente um ponto fixo de referência
tático, não um *waypoint* de rota. O *slot* `bullseye:` da `Navigation` aceita um
`Bullseye` para coordenação entre pilotos no formato "090/30 do *bullseye*".

## 23.11 Piloto, autopiloto e computador de bordo

### `Pilot` e `Autopilot`

`Pilot` é um *stub* — herda de `System` sem lógica adicional, servindo de base para
subclasses de pilotagem autônoma. `Autopilot` é a subclasse concreta: **seis modos
independentes**, ativáveis em qualquer combinação, todos processados no laço TC.

```cpp
// src/models/system/Autopilot.cpp
void Autopilot::process(const double dt)
{
    modeManager();         // re-trava os modos ativos
    headingController();   // produz cmdHdg para o DynamicsModel
    altitudeController();  // produz cmdAlt
    velocityController();  // produz cmdSpd
    BaseClass::process(dt);
}
```

```cpp
// src/models/system/Autopilot.cpp
bool Autopilot::headingController()
{
   // Re-trava o modo (o autopiloto e a fonte da verdade, nao o modelo)
   setHeadingHoldMode( isHeadingHoldOn() );

   Player* pv{getOwnship()};
   if (pv != nullptr) {
      DynamicsModel* md{pv->getDynamicsModel()};
      if (md != nullptr) {
         if ( isHeadingHoldOn() || isNavModeOn() ) {
            // Quantiza o comando a 0.1 grau antes de enviar
            const int ihdg10{static_cast<int>( getCommandedHeadingD() * 10.0f )};
            const double hdg{static_cast<double>(ihdg10) / 10.0};
            md->setCommandedHeadingD(hdg, maxTurnRateDps, maxBankAngleDegs);
            md->setHeadingHoldOn( true );      // <- retorno IGNORADO
         } else {
            md->setHeadingHoldOn( false );     // <- retorno IGNORADO
            md->setControlStickRollInput( getControlStickRollInput() );
         }
      }
   }
   return true;                                 // sempre true
}
```

Dois detalhes que só se veem lendo o código: a **quantização a 0,1°** (evita que ruído
numérico no cálculo de rumo produza microcorreções contínuas) e o fato de a decisão entre
modo *hold* e manche depender exclusivamente do estado do autopiloto.

```cpp
// src/models/system/Autopilot.cpp
bool Autopilot::setNavMode(const bool flag)
{
    bool navModeOn1 { navModeOn };

    // navModeOn requer que os dois SAS estejam ativos
    navModeOn = flag && isRollSasOn() && isPitchSasOn();

    if (navModeOn) {
        setHeadingHoldMode(true);
        setAltitudeHoldMode(true);
        setVelocityHoldMode(true);
    }

    // Ao desligar: captura valores correntes do player para evitar
    // transiente brusco na retomada do controle manual
    if (!navModeOn && navModeOn1) {
        Player* pv { getOwnship() };
        if (pv != nullptr) {
            setCommandedHeadingD(   pv->getHeadingD() );
            setCommandedAltitudeFt( pv->getAltitudeFt() );
            setCommandedVelocityKts(pv->getTotalVelocityKts() );
        }
    }
    return (flag == navModeOn);
}
```

### O algoritmo `flyCRS()` — interceptação de curso

```cpp
// src/models/system/Autopilot.cpp (simplificado)
bool Autopilot::flyCRS(const double latDeg, const double lonDeg, const double crsDeg)
{
    Player* pPlr { getOwnship() };
    const double MAX_BANK_RAD { maxBankAngleDegs * base::angle::D2RCC };
    const double velMps       { pPlr->getTotalVelocity() };

    double brgDeg {}, distNM {};
    base::nav::fll2bd(pPlr->getLatitude(), pPlr->getLongitude(),
                      latDeg, lonDeg, &brgDeg, &distNM);

    // Erro de posicao relativo ao curso comandado
    const double posErrDeg { base::angle::aepcdDeg(brgDeg - crsDeg) };
    const double posErrRad { posErrDeg * base::angle::D2RCC };

    // Raio de curvatura para o bank maximo configurado
    const double rocMtr { velMps * velMps / base::ETHGM / std::tan(MAX_BANK_RAD) };

    // XTE em unidades de raio de curvatura
    const double xtRngMtr { std::fabs(distNM * std::sin(posErrRad)) * base::distance::NM2M };
    const double xtRngRoc { xtRngMtr / rocMtr };

    double hdgCmdDeg {};
    if (xtRngRoc >= 1.2) {
        // XTE grande: intercepta perpendicularmente (90 graus da pista)
        hdgCmdDeg = base::sign(posErrDeg) * 90.0 + crsDeg;
    } else {
        // XTE pequeno: angulo proporcional -- convergencia suave
        double x { 1.0 - xtRngRoc };
        if (x >  1.0) x =  1.0;
        if (x < -1.0) x = -1.0;
        const double alfaDeg { std::acos(x) * base::angle::R2DCC };
        const double y       { (rocMtr - 20.0) / rocMtr };
        const double betaDeg { std::acos(y) * base::angle::R2DCC };
        const double gamaDeg { base::sign(posErrDeg) * (alfaDeg - betaDeg) };
        hdgCmdDeg = base::angle::aepcdDeg(gamaDeg + crsDeg);
    }

    return setCommandedHeadingD(hdgCmdDeg);
}
```

O princípio é idêntico ao LNAV de sistemas FMS reais: para erros pequenos, o ângulo de
interceptação é proporcional ao XTE, garantindo convergência suave; para erros grandes, a
aeronave vira perpendicular à pista para capturar rapidamente. **A constante 1,2 define a
fronteira entre os dois regimes**, em unidades do raio de curvatura máximo.

```
components: {
  pilot1: ( Autopilot
    -- Modos iniciais
    navMode:            true    -- segue a rota de Navigation
    altitudeHoldMode:   true
    velocityHoldMode:   true
    headingHoldMode:    true

    -- Limites de performance ("pilot limits", nao "aircraft limits").
    -- Todos tem padrao 0.0: e OBRIGATORIO declara-los.
    maxBankAngle:       30.0   -- deg
    maxRateOfTurnDps:    3.0   -- deg/s (3 = "standard rate turn")
    maxClimbRateMps:    10.0   -- m/s
    maxPitchAngle:      15.0   -- deg
    maxAcceleration:     5.0   -- m/s2

    -- Valores comandados iniciais
    holdAltitude:    ( Feet 25000 )
    holdVelocityKts:   350.0
    holdHeading:     ( Degrees 90 )

    -- Follow-the-lead (formacao): tres slots escalares, nao um vetor
    leadPlayerName:              "alpha"
    leadFollowingDistanceTrail: ( Meters 500 )
    leadFollowingDistanceRight: ( Meters 150 )
    leadFollowingDeltaAltitude: ( Meters   0 )
  )
}
```

**ARMADILHA — os limites do autopiloto nascem em ZERO.** `maxBankAngleDegs` e
`maxTurnRateDps` são membros **inicializados com zero**, não com 30° e 3°/s. A consequência
é concreta: em `flyCRS()` o raio de curvatura é r = v²/(g·tan(bank)), e com *bank* igual a
zero esse raio é **infinito**. O erro de trajetória normalizado vai a zero, o algoritmo entra
permanentemente no ramo de "erro pequeno" e a aeronave nunca faz uma curva de interceptação
decente. **Um `Autopilot` sem esses dois *slots* declarados não é um autopiloto com valores
conservadores — é um autopiloto quebrado, e em silêncio.**

### `OnboardComputer` — *shoot list* e ações de missão

```cpp
// src/models/system/OnboardComputer.cpp
void OnboardComputer::updateData(const double dt)
{
    BaseClass::updateData(dt);
    actionManager(dt);    // gerencia Action ativa do steerpoint corrente
    updateShootList();    // recalcula a shoot list a partir do TrackManager
}
```

```cpp
// src/models/system/OnboardComputer.cpp (simplificado)
void OnboardComputer::updateShootList(const bool step)
{
    const unsigned int MAX_TRKS { 20 };
    base::safe_ptr<Track> trackList[MAX_TRKS];

    // Busca o AirTrkMgr; cai para qualquer TrackManager disponivel
    TrackManager* tm { getTrackManagerByType(typeid(AirTrkMgr)) };
    if (tm == nullptr) tm = getTrackManagerByType(typeid(TrackManager));
    int n { (tm != nullptr) ? tm->getTrackList(trackList, MAX_TRKS) : 0 };

    if (n > 0) {
        // Encontra o indice do nextToShoot atual na lista
        int cNTS { -1 };
        for (int i = 0; i < n && cNTS < 0; i++) {
            if (nextToShoot == trackList[i]) cNTS = i;
        }

        // Se nao temos NTS ou o track sumiu: pega o mais proximo
        if (cNTS < 0) {
            int closest { 0 };
            double minRange { trackList[0]->getRange() };
            for (int i = 1; i < n; i++) {
                if (trackList[i]->getRange() < minRange) {
                    minRange = trackList[i]->getRange();
                    closest  = i;
                }
            }
            setNextToShoot(trackList[closest]);
        }
    }
}
```

```cpp
// src/models/system/OnboardComputer.cpp
void OnboardComputer::actionManager(const double dt)
{
    if (action != nullptr) {
        action->process(dt);          // avanca o estado da action
        if (action->isCompleted()) {
            action = nullptr;         // descarta ao completar
        }
    }
}
// triggerAction() e chamado pela Route.autoSequencer() ao capturar um steerpoint
```

### `CollisionDetect`

Opera em **duas fases separadas**: `updateData()` (background) filtra a lista de *players*;
`process()` (TC) verifica distâncias.

```
cd1: ( CollisionDetect
    collisionRange:    ( Meters 4.0 )        -- distancia de colisao (padrao: 4 m)
    maxRange2Players:  ( NauticalMiles 1.0 ) -- varre players ate 1 NM
    maxAngle2Players:  ( Degrees 0.0 )       -- 0 = sem restricao angular
    maxPlayers:        20                    -- max de players of interest

    playerTypes: {                           -- tipos a verificar
        t1: "air"
        t2: "weapon"
    }

    localOnly:         false    -- inclui players networked
    sendCrashEvents:   true     -- envia CRASH_EVENT ao colidir
    useWorldCoordinates: true   -- usa ECEF (vs. NED local)
)
```

O evento `CRASH_EVENT` é enviado tanto ao *ownship* quanto ao *player* com o qual colidiu.
O *player* intercepta em `Player::event(CRASH_EVENT)` e muda seu modo para `CRASHED`,
iniciando o fluxo de remoção via `DELETE_REQUEST`.
## 23.12 Assinaturas RF e IR

Sensores detectam alvos porque alvos **refletem ou emitem energia**. As classes de
assinatura encapsulam essa propriedade — separando "quão visível é este alvo" de "como o
sensor calcula o que recebeu".

### `RfSignature` e subclasses

```cpp
// include/mixr/models/Signatures.hpp
class RfSignature : public base::Component {
public:
    // Retorna o RCS (m2) para a Emission recebida.
    // em->getAzimuthAoi() e em->getElevationAoi() dao o angulo de chegada
    // do radar em relacao ao alvo -- usado por SigAzEl.
    virtual double getRCS(const Emission* const em) = 0;
};
```

**O nome de fábrica da base é `"Signature"`**, não `"RfSignature"`.

```cpp
// src/models/Signatures.cpp
// SigConstant: RCS fixo -- aceita Number(m2), Decibel(dBsm) ou Area
double SigConstant::getRCS(const Emission* const)
{
    return rcs;   // valor pre-convertido para m2 no setSlotRCS()
}
// slot "rcs": base::Number (m2) | base::Decibel (dBsm) | base::Area (qualquer unidade)

// SigSphere: RCS analitico de esfera -- invariante com o angulo de chegada
// computeRcs(r) = PI * r * r
double SigSphere::getRCS(const Emission* const)
{
    return rcs;   // = PI * radius * radius, calculado em setRadius()
}

// SigAzEl: interpola Table2(az, el) -> RCS
double SigAzEl::getRCS(const Emission* const em)
{
    double iv1 { em->getAzimuthAoi()   };  // rad (ou graus se inDegrees)
    double iv2 { em->getElevationAoi() };
    if (isOrderSwapped()) std::swap(iv1, iv2);  // suporta tabelas El x Az
    if (isInDegrees()) {
        iv1 *= base::angle::R2DCC;
        iv2 *= base::angle::R2DCC;
    }
    double rcs { tbl->lfi(iv1, iv2) };    // interpolacao bilinear (LFI)
    if (isDecibel())
        rcs = std::pow(10.0, rcs / 10.0); // dBsm -> m2
    return rcs;
}

// SigSwitch: seleciona entre sub-assinaturas pelo camouflageType do player
// camouflageType=0 -> componente 1, camouflageType=1 -> componente 2, etc.
double SigSwitch::getRCS(const Emission* const em)
{
    const Player* own { static_cast<const Player*>(
        findContainerByType(typeid(Player))) };
    if (own == nullptr) return 0.0;

    unsigned int cam { own->getCamouflageType() + 1 };  // 1-based
    base::Pair* pair { findByIndex(cam) };
    if (pair != nullptr) {
        const auto sig = dynamic_cast<RfSignature*>(pair->object());
        if (sig != nullptr) return sig->getRCS(em);
    }
    return 0.0;
}
```

Três formas analíticas fechadas, todas **independentes do ângulo de chegada**:

| Classe | Fórmula | *Slots* |
|---|---|---|
| `SigPlate` | σ = 4π(ab)²/λ² | `a` e `b` |
| `SigDihedralCR` | σ = 8πa⁴/λ² | `EMPTY_SLOTTABLE` (herda `a`,`b` de `SigPlate`; usa só `a`) |
| `SigTrihedralCR` | σ = 12πa⁴/λ² | `EMPTY_SLOTTABLE` (idem) |

Servem a calibração e *test ranges*, onde o refletor é de fato um objeto geométrico
conhecido.

**Vale sublinhar:** apesar do nome, **`SigPlate` não modela uma placa vista de vários
ângulos** — devolve sempre o mesmo RCS. **A única subclasse com dependência angular é
`SigAzEl`.**

```
// Alvo simples: RCS constante de 5 m2
signature: ( SigConstant  rcs: 5.0 )

// Ou em dBsm:
signature: ( SigConstant  rcs: ( dB 7.0 ) )   // 7 dBsm ~ 5 m2

// Esfera de raio 1 m: RCS = pi * 1^2 ~ 3.14 m2
signature: ( SigSphere  radius: ( Meters 1.0 ) )

// Tabela az x el (para aeronaves reais -- captura dependencia angular)
signature: ( SigAzEl
    table: ( Table2
        x: [ -3.14159  0.0  3.14159 ]   // azimute (rad)
        y: [  -1.5708  0.0  1.5708  ]   // elevacao (rad)
        data: [ 2.0  3.5  2.0         // linha az=-pi
                4.0  8.0  4.0         // linha az=0 (frente)
                2.0  3.5  2.0 ]       // linha az=+pi
    )
    inDegrees: false
    inDecibel: false
)
```

### Assinaturas IR: `IrSignature` e `AircraftIrSignature`

```cpp
// src/models/IrSignature.cpp
bool IrSignature::getIrSignature(IrQueryMsg* const msg)
{
   bool ok{};
   if (msg != nullptr) {
      double projectedAreaInFOV{getSignatureArea(msg)};
      msg->setProjectedArea(projectedAreaInFOV);
      // if no projectedAreaInFOV, then target was not in FOV
      if (projectedAreaInFOV > 0.0){
         ok = true;
         // FAB - set simple signature value
         msg->setSignatureAtRange(getCalculatedHeatSignature());
         msg->setEmissivity(getEmissivity());
      }
   }
   return ok;
}
```

`IrSignature` usa um `IrShape` (`IrSphere`, `IrBox`) para calcular a área projetada.

`AircraftIrSignature` decompõe o sinal em **três componentes físicos independentes** — cada
um com uma tabela de *lookup* própria:

```cpp
// src/models/AircraftIrSignature.cpp
// Tabelas de lookup:
//   airframeSignatureTable: Table4(mach, alt_m, az_rad, el_rad) -> W/sr
//   plumeSignatureTable:    Table5(PLA, mach, alt_m, az_rad, el_rad) -> W/sr
//   hotPartsSignatureTable: Table5(PLA, vel_kt, alt_m, az_rad, el_rad) -> W/sr
//   *WavebandFactorTable:   Table2(centro_banda, largura) -> fator [0..1]
//   Nota: os fatores de banda de cada componente devem somar 1.0

// Calculo do PLA (Power Lever Angle) a partir das forcas de empuxo
double AircraftIrSignature::getPLA(const AirVehicle* const airModel)
{
    double currentThrust {}, idleValue {}, milValue {}, maxValue {};
    airModel->getEngThrust(     &currentThrust, 1);
    airModel->getEngThrustIdle( &idleValue,     1);
    airModel->getEngThrustMil(  &milValue,      1);
    airModel->getEngThrustAb(   &maxValue,      1);

    // PLA: 0.0 = idle, 1.0 = MIL power, 2.0 = afterburner
    if (currentThrust < milValue) {
        return (currentThrust - idleValue) / (milValue - idleValue);
    } else if (currentThrust == milValue) {
        return 1.0;
    } else if (currentThrust < maxValue) {
        return 1.0 + (currentThrust - milValue) / (maxValue - milValue);
    }
    return 2.0;  // afterburner pleno
}
```

O afterburner (PLA = 2,0) produz sinal IR ordens de magnitude maior que o voo de cruzeiro
(PLA ≈ 0,4–0,6). O sinal final por banda espectral é:

```
S_banda = (S_airframe + S_plume + S_hotParts)_banda × f_waveband
```

O array `signatureByWaveband` resultante é gravado na `IrQueryMsg` e consumido por
`IrAtmosphere`, que atenua cada banda pela transmitância atmosférica antes de retornar o
sinal ao `IrSensor`.

```
irSignature: ( AircraftIrSignature

    // Airframe: calor por atrito -- funcao de mach, altitude, az, el
    airframeSignatureTable: ( Table4 ... )
    airframeWavebandFactorTable: ( Table2
        // x=centro_banda(um) y=largura(um) -> fator
        x: [ 3.0  5.0 ]   // bandas MWIR (3-5 um)
        y: [ 2.0  2.0 ]
        data: [ 0.6  0.4 ]  // 60% na banda 3-5, 40% na banda 5-7
    )

    // Plume: escapamento -- fortemente dependente do PLA
    plumeSignatureTable: ( Table5 ... )
    plumeWavebandFactorTable: ( Table2 ... )

    // Hot parts: bocal + turbina -- sempre quentes
    hotPartsSignatureTable:    ( Table5 ... )
    hotPartsWavebandFactorTable: ( Table2 ... )
)
```

## 23.13 Ambiente: atmosfera e propagação IR

`AbstractAtmosphere` define o contrato; duas implementações concretas cobrem fidelidades
diferentes.

### `IrAtmosphere` — modelo simples

```cpp
// src/models/environment/IrAtmosphere.cpp
// transmissivityTable1: Table1 onde x = centro da banda (um)
// e data = coeficiente de absorcao alpha (por km)
// tau = exp( -alpha * range * 0.001 )   (range em metros -> km)

double IrAtmosphere::getTransmissivity(const double wavebandCenter,
                                        const double range) const
{
    double trans { 1.0 };
    if (transmissivityTable1 != nullptr) {
        trans = transmissivityTable1->lfi(wavebandCenter);  // alpha para esta banda
        trans = std::exp(trans * -0.001 * range);           // tau = e^(-alpha * r/1000)
    }
    return trans;   // [0.0 = bloqueado, 1.0 = sem perda]
}
```

A radiância de fundo é decidida pela **geometria sensor–alvo**:

```cpp
// src/models/environment/IrAtmosphere.cpp
// --- 1. Para onde o sensor esta olhando (angulo de arfagem, NED) ---
const base::Matrixd mm{msg->getGimbal()->getRotMat() * msg->getOwnship()->getRotMat()};
base::Vec3d angles;
base::nav::computeEulerAngles(mm, &angles);
currentViewAngle = angles[Player::IPITCH];

// --- 2. Onde esta o horizonte, dada a altitude do ownship ---
const double er{base::nav::ERAD60 * base::distance::NM2M};   // raio da Terra
const double distEC{msg->getOwnship()->getAltitudeM() + er}; // dist ao centro
const double dh2{(distEC * distEC) - (er * er)};
hDist   = std::sqrt(dh2);          // distancia ao horizonte
hTanAng = ( hDist / er );
viewAngleToHorizon = std::atan(hTanAng);

// --- 3. Que fracao do campo de visada e ceu e que fracao e terra ---
const double angleToHorizon{currentViewAngle + viewAngleToHorizon};
const double fovtheta{msg->getSendingSensor()->getIFOVTheta()};

if (angleToHorizon - fovtheta >= 0) {
    backgroundRadiance = getSkyRadiance();          // so ceu
} else if (fovtheta - angleToHorizon >= 2*fovtheta) {
    backgroundRadiance = getEarthRadiance();        // so terra
} else  {
    // olhando para os dois: mistura proporcional
    const double ratio{0.5 + 0.5 * angleToHorizon / fovtheta};
    backgroundRadiance = ratio * getSkyRadiance() + (1.0-ratio) * getEarthRadiance();
}
```

**POR QUÊ o fundo depende do SENSOR, não só da geometria.** O termo decisivo é
`getIFOVTheta()`: um sensor de campo estreito apontado logo acima do horizonte vê *só* céu,
enquanto um de campo largo na mesma direção vê os dois. Modelar o fundo sem o campo de
visada daria a mesma resposta para os dois — e é justamente contra o fundo que o sinal do
alvo precisa se destacar.

A soma por banda é ponderada **duas vezes**: pela fração que aquela banda ocupa da faixa
atmosférica total, e pelo *overlap* entre a banda e os limites espectrais do sensor:

```cpp
// src/models/environment/IrAtmosphere.cpp
const double fractionOfBandToTotal{(upperBandBound - lowerBandBound) / totalWavelengthRange};
const double overlapRatio{(upperOverlap - lowerOverlap) / (upperBandBound - lowerBandBound)};

const double backgroundRadianceInBand{backgroundRadiance * fractionOfBandToTotal * overlapRatio};

*totalSignal += radiantIntensityInBin * getTransmissivity(i, range2D);
```

```
atmosphere: ( IrAtmosphere
    waveBands: ( Table1
        x:    [ 3.200  3.775  4.325  4.750 ]   // centros das bandas (um)
        data: [ 0.4    0.75   0.35   0.5   ]   // larguras das bandas (um)
    )
    transmissivityTable1: ( Table1
        x:    [ 3.200  3.775  4.325  4.750 ]   // centros (igual ao waveBands)
        data: [ 0.011  0.004  0.280  0.017 ]   // alpha (coef. absorcao+espalhamento / km)
    )
    skyRadiance:   11.2   // W/sr.m2 -- fundo celeste
    earthRadiance:  1.0   // W/sr.m2 -- fundo terrestre
)
```

### `IrAtmosphere1` — modelo de alta fidelidade

Substitui a transmitância 1D e o fundo constante por **três tabelas multidimensionais**:

```cpp
// include/mixr/models/environment/IrAtmosphere1.hpp
class IrAtmosphere1 : public IrAtmosphere {
private:
    // Table2(banda, alt_alvo) -> W/sr
    // Radiacao solar refletida pelo alvo: varia com a altitude
    const base::Table2* solarRadiationTable {};

    // Table3(banda, alt_sensor, angulo_visada) -> W/sr
    // Radiancia de fundo: sensor a 45 graus abaixo mistura solo quente e ceu frio
    // (0 = para baixo, PI = para cima)
    const base::Table3* backgroundRadiationTable {};

    // Table4(banda, alt_sensor, alt_alvo, range) -> fracao [0..1]
    // Transmissividade: funcao completa da geometria do caminho
    const base::Table4* transmissivityTable {};
};
```

```cpp
// src/models/environment/IrAtmosphere1.cpp
double IrAtmosphere1::getTransmissivity(
                        const double wavebandCenter,
                        const double seekerAltitude,
                        const double targetAltitude,
                        const double range) const
{
   double trans{1.0};
   if (transmissivityTable != nullptr){
      trans = transmissivityTable->lfi(wavebandCenter,seekerAltitude,targetAltitude,range);
   } else {
      // use base class' simple transmissivity if no table
      trans = BaseClass::getTransmissivity(wavebandCenter,range);
   }
   return trans;
}
```

**Note o `else`:** sem a tabela de quatro dimensões, `IrAtmosphere1` **não** devolve
transmitância unitária — ele **recua para o modelo exponencial simples da classe base**.
Isso o torna seguro de declarar em EDL antes de ter os dados em mãos: a fidelidade cai para
a do modelo simples, mas nada deixa de funcionar. Há ainda uma segunda sobrecarga, de cinco
argumentos, que recebe os limites inferior e superior da banda em vez do centro.

```cpp
// src/models/environment/IrAtmosphere1.cpp
// tang do angulo ao alvo corrigido pelo raio da Terra (12 756 776 m = diametro)
const double tanPhi      { (target->getAltitudeM() - ownship->getAltitudeM())
                           / range2D };
const double tanPhiPrime { tanPhi - (range2D / 12756776.0) };

double viewingAngle { std::atan(tanPhiPrime) };
// Converte para [0, PI]: 0 = apontado para baixo (terra), PI = para cima (ceu)
viewingAngle += base::PI / 2.0;

double bkg { backgroundRadiationTable->lfi(center, seekerAlt, viewingAngle) };
```

**Quando usar cada modelo.** `IrAtmosphere` é suficiente para simulações táticas de médio
alcance onde a fidelidade absoluta de detecção IR não é o objetivo. `IrAtmosphere1` é
necessário quando a probabilidade de detecção e de *lock* de *seekers* IR precisa ser
comparada com dados de ensaio — qualificação de sistemas, análise de eficácia de
contramedidas IR e definição de envelopes de emprego.

## 23.14 Armas e efeitos

Armas e efeitos **são `Player`** — participam do mesmo ciclo de quatro fases, têm física
própria e são processados pela `Simulation` como qualquer aeronave. O que os distingue é o
ciclo de vida.

### `AbstractWeapon::release()` — arma inicial vs. *flyout*

A distinção central é entre a **arma inicial** (o objeto declarado no EDL, preso ao
lançador) e o **flyout** (o clone que voa de fato).

```cpp
// src/models/player/weapon/AbstractWeapon.cpp (simplificado)
AbstractWeapon* AbstractWeapon::release()
{
    AbstractWeapon* flyout {};

    if (!isReleased() && !isBlocked() && !isJettisoned()) {
        Player*     lplayer { getLaunchVehicle() };
        WorldModel* sim     { static_cast<WorldModel*>(
            findContainerByType(typeid(WorldModel))) };

        if (!getWillHang() && lplayer != nullptr && sim != nullptr) {

            // Clona a arma inicial -- este clone e o flyout que ira voar
            flyout = this->clone();
            flyout->container(sim);
            flyout->reset();

            // Liga os ponteiros de identidade entre inicial e flyout
            flyout->setFlyoutWeapon(flyout);     // flyout aponta para si mesmo
            flyout->setInitialWeapon(this);
            flyout->setID(sim->getNewReleasedWeaponID());

            // Herda o lançador e o lado (BLUE/RED)
            flyout->setLaunchVehicle(lplayer);
            flyout->setSide(lplayer->getSide());

            // Flyout entra na lista em PRE_RELEASE (vai a ACTIVE na fase 0)
            flyout->setMode(PRE_RELEASE);
            flyout->setReleased(true);
            flyout->setReleaseHold(false);

            // Marca a arma inicial como lançada
            setMode(Player::LAUNCHED);
            setFlyoutWeapon(flyout);
            setInitialWeapon(this);
            setReleased(true);

            // Adiciona o flyout ao player list da Simulation
            char pname[32];
            std::sprintf(pname, "W%05d", flyout->getID());
            sim->addNewPlayer(pname, flyout);
        }
    }
    return flyout;   // pre-ref()'d -- caller deve unref()
}
```

```cpp
// src/models/player/weapon/AbstractWeapon.cpp
void AbstractWeapon::updateTC(const double dt)
{
    BaseClass::updateTC(dt);

    unsigned int ph { getWorldModel()->phase() };

    // Fase 0: transicao PRE_RELEASE -> ACTIVE apos dynamics() da base,
    // garantindo que a posicao relativa ao lançador ja foi calculada
    if (ph == 0 && isMode(PRE_RELEASE) && !isReleaseHold()) {
        atReleaseInit();    // inicializa DynamicsModel com estado do lançador
        setMode(ACTIVE);
    }

    // Fase 3: rastreamento de alvo e atualizacao do TOF
    if (ph == 3 && isActive() && isLocalPlayer() && !isJettisoned() && !isDummy()) {
        if (posTrkEnb) positionTracking();      // atualiza tgtPos a partir do track
        if (isMode(ACTIVE)) updateTOF(dt * 4.0);
    }
}
```

```cpp
// src/models/player/weapon/AbstractWeapon.cpp (estrutura)
void AbstractWeapon::dynamics(const double dt)
{
    if (isMode(PRE_RELEASE)) {
        // Presa ao lancador: posicao, orientacao e velocidade sao DERIVADAS dele
        setSide(getLaunchVehicle()->getSide());
        //   orientacao = computeRotationalMatrix(initAngles) * matriz do lancador
        //   posicao    = lvM * initPos + posicao do lancador
        //   velocidade = velocidade do lancador;  aceleracao = 0
        // NOTA: BaseClass::dynamics() NAO e chamada neste ramo.
    }
    else {
        // Em voo: se nao ha DynamicsModel configurado, a arma usa o SEU PROPRIO
        if (isLocalPlayer() && !isDummy() && getDynamicsModel() == nullptr) {
            weaponGuidance(dt);    // lei de guiagem embutida
            weaponDynamics(dt);    // integracao embutida
        }
        BaseClass::dynamics(dt);
    }
}
```

**POR QUÊ armas têm física própria, ao contrário dos demais *players*.** Este é o ponto em
que a regra "a física vem do `DynamicsModel`" tem a sua exceção, e ela é deliberada. **Um
míssil declarado sem `DynamicsModel` NÃO fica parado**: `weaponGuidance()` e
`weaponDynamics()` assumem, com lei de guiagem proporcional e integração simples. Cada
subtipo sobrescreve os dois conforme a sua natureza — `Missile` implementa perseguição
proporcional, `Effect` implementa balística com arrasto, `Bullet` implementa balística pura.
A razão é prática: uma simulação pode ter centenas de armas em voo simultaneamente.

### Detonação

Dois caminhos, cada um com seu método: **`collisionNotification()`** para impacto em outro
*player*, e **`crashNotification()`** para impacto no solo (resultado
`DETONATE_GROUND_IMPACT`). **`AbstractWeapon` NÃO sobrescreve `killedNotification()`.**

```cpp
// src/models/player/weapon/AbstractWeapon.cpp
bool AbstractWeapon::collisionNotification(Player* const other)
{
    // Guarda: so detona se for um player local e o override nao estiver ativo
    if (isCrashOverride() || !isLocalPlayer()) return false;

    bool ok { killedNotification(other) };   // avisa a cadeia normal PRIMEIRO

    setMode(DETONATED);
    setDetonationResults(DETONATE_ENTITY_IMPACT);

    // Calcula ponto de detonacao no sistema de coordenadas do alvo
    setTargetPlayer(other, false);
    setLocationOfDetonation();

    // Verifica quem foi afetado pela detonacao (raio lethal e burst)
    checkDetonationEffect();

    // Grava o evento para o recorder e interop/DIS
    BEGIN_RECORD_DATA_SAMPLE(getWorldModel()->getDataRecorder(),
                             REID_WEAPON_DETONATION)
        SAMPLE_3_OBJECTS(this, getLaunchVehicle(), getTargetPlayer())
        SAMPLE_2_VALUES(DETONATE_ENTITY_IMPACT, getDetonationRange())
    END_RECORD_DATA_SAMPLE()

    return ok;
}
```

A ordem importa: `killedNotification()` é chamado **antes** da detonação, para que a cadeia
normal de notificação de abate corra com a arma ainda em voo.

### *Slots* de `AbstractWeapon` e seus padrões reais

| Slot | Tipo | Padrão | Significado |
|---|---|---:|---|
| `maxTOF` | `Time` | 60,0 s | tempo máximo de voo |
| `tsg` | `Time` | **9999,0 s** | tempo para iniciar guiagem |
| `maxBurstRng` | `Distance` | 500,0 m | raio de dano |
| `lethalRange` | `Distance` | 50,0 m | raio letal |
| `sobt` | `Time` | **9999,0 s** | início de queima |
| `eobt` | `Time` | 0,0 s | fim de queima |
| `maxGimbal` | `Angle` | **0,0°** | ângulo máximo do buscador |

**Três armadilhas escondidas nesses padrões:**

1. **`maxGimbal`**: o comentário de documentação no cabeçalho anuncia `30.0f * D2RCC`, mas o
   membro é inicializado com `double maxGimbal {};` — ou seja, **zero**. O comentário está
   desatualizado em relação ao código.
2. **Os dois valores sentinela de 9999,0 s.** `tsg` e `sobt` não são "muito grandes" por
   acaso: como `isGuidanceEnabled()` exige `getTOF() >= tsg` e `isEngineBurnEnabled()` exige
   `tof >= sobt`, **o padrão DESLIGA efetivamente a guiagem e a queima de motor**. Uma arma
   só guia e só acende o motor se o EDL disser explicitamente quando.
3. **O modo inicial.** O padrão de `AbstractPlayer` é `ACTIVE`; o construtor de
   `AbstractWeapon` o sobrescreve com `setMode(INACTIVE)` e `setInitMode(INACTIVE)`. **Armas
   nascem dormentes.**

```cpp
// src/models/player/weapon/AbstractWeapon.cpp
bool AbstractWeapon::isGuidanceEnabled() const
{
    return (getTOF() >= tsg)                       // passou do tempo de inicio
        && ((getCategory() & GUIDED) != 0)         // arma e do tipo guiado
        && isTargetPositionValid();                // tem posicao de alvo valida
}

bool AbstractWeapon::isEngineBurnEnabled() const
{
    return (tof >= sobt && tof <= eobt);  // dentro da janela de queima
}
```

### Subtipos de arma

**`Missile`** não contém um buscador como membro nem faz rastreio na fase 2. A sua guiagem é
`Missile::weaponGuidance()`, alimentada por `calculateVectors()`, que obtém a geometria ou
do *player* alvo diretamente, ou de um `Track` do `TrackManager` do lançador.

`Aam` (ar–ar, nome de fábrica `AamMissile`), `Sam` (superfície–ar) e `Agm` (ar–superfície,
nome de fábrica `AgmMissile`) especializam a lógica de *endgame*. `Bomb` consulta o
`Designator` do `OnboardComputer` para o ponto de impacto terminal (JDAM). `Bullet` tem
trajetória balística pura e vida útil muito curta, criado em rajadas pelo `Gun`.

### Contramedidas: `Chaff`, `Flare` e `Decoy`

São `Effect`, que é subclasse de **`AbstractWeapon`** — e não de `Player` diretamente.
Herdam todo o ciclo de vida de arma. `Effect` acrescenta um único *slot*, `dragIndex`, e uma
única lógica: `weaponDynamics()`, trajetória balística com arrasto.

**ARMADILHA — `Chaff`, `Flare` e `Decoy` são o MESMO código.** Os três arquivos são **byte a
byte equivalentes**: vinte linhas cada, com `EMPTY_SLOTTABLE`, `EMPTY_COPYDATA`,
`EMPTY_DELETEDATA`, `getCategory()` devolvendo `GRAVITY`, e um construtor que apenas fixa a
*string* de tipo. Nenhum dos três toca num `RfSensor`, num buscador IR, numa assinatura ou
num `DynamicsModel`.

Em particular: **`Chaff` NÃO satura radar nenhum, `Flare` NÃO tem assinatura IR elevada, e
`Decoy` NÃO é um mini-veículo com dinâmica própria.** As três classes existem para dar nome
e tipo DIS a um objeto balístico de vida curta — a diferença entre elas é **exclusivamente
semântica**.

O efeito de contramedida, se desejado, tem de ser construído **por composição em EDL**:

```
storesMgr: ( StoresMgr
    stores: {
        7: ( Chaff
            maxTOF:       ( Seconds 8.0 )
            maxBurstRng:  ( Meters 200.0 )
            signature: ( SigConstant  rcs: ( dB 15.0 ) )  // 15 dBsm >> aeronave
        )
        8: ( Flare
            maxTOF:       ( Seconds 4.0 )
            irSignature: ( AircraftIrSignature ... )
        )
    }
)
```

**O framework fornece as peças; a eficácia da contramedida é propriedade do cenário, não da
classe.**

## 23.15 Tipos de *player*: ar, solo, mar e espaço

**REGRA — a taxonomia de *players* é quase toda NOMINAL.** Dos cerca de vinte tipos
concretos, **apenas seis contêm comportamento**: `AirVehicle`, `GroundVehicle`,
`SamVehicle`, `LifeForm`, `SpaceVehicle` e as classes de arma e efeito. Todos os demais —
`Aircraft`, `Helicopter`, `UnmannedAirVehicle`, `Tank`, `Artillery`, `ArmoredVehicle`,
`WheeledVehicle`, `Ship`, `Building`, `MannedSpaceVehicle`, `UnmannedSpaceVehicle`,
`InfantryMan`, `Parachutist` — são arquivos de vinte linhas cujo construtor apenas fixa uma
*string* de tipo padrão.

Isso **não é deficiência**: é consequência coerente do princípio da composição dirigida por
dados. O que distingue um caça de um helicóptero no MIXR **não é a classe**, é o
`DynamicsModel`, a assinatura e os sensores que o arquivo EDL pendura neles. A classe serve
para dar um nome de fábrica, um tipo DIS e um ponto de ancoragem.

### Veículos aéreos

```cpp
// src/models/player/air/AirVehicle.cpp
double AirVehicle::getGload() const
{
    const AerodynamicsModel* aero { getAerodynamicsModel() };
    return (aero != nullptr) ? aero->getGload() : 0.0;
}

double AirVehicle::getMach() const
{
    const AerodynamicsModel* aero { getAerodynamicsModel() };
    // Se nao ha modelo aerodinamico, usa a velocidade total do Player (aproximado)
    return (aero != nullptr) ? aero->getMach() : BaseClass::getMach();
}

double AirVehicle::getCalibratedAirspeed() const
{
    const AerodynamicsModel* aero { getAerodynamicsModel() };
    return (aero != nullptr) ? aero->getCalibratedAirspeed()
                              : getTotalVelocityKts();
}
```

`Aircraft` (asa fixa), `Helicopter` (asa rotativa) e `UnmannedAirVehicle` (VANT) **não
acrescentam nada além da *string* de tipo padrão** — nem sequer sobrescrevem
`getMajorType()`, definido uma única vez em `AirVehicle`. Também é `AirVehicle` quem expõe
`getLandingGearPosition()` e `getWeaponBayDoorPosition()`, delegando ao
`AerodynamicsModel`, para alimentar o campo *appearance* do PDU DIS. **O único *slot* de
toda a família aérea é o `initGearPos` de `AirVehicle`.**

### Veículos terrestres e o lançador dinâmico

```cpp
// src/models/player/ground/GroundVehicle.cpp
void GroundVehicle::launcherDynamics(const double dt)
{
    if (lnchrMoveTime > 0 && cmdLnchrPos != NONE) {

        // Taxa de movimento: (max - min) / tempo de deslocamento total
        double rate  { (lnchrUpAngle - lnchrDownAngle) / lnchrMoveTime };
        double angle { lnchrAngle };

        if (cmdLnchrPos == UP && lnchrAngle != lnchrUpAngle) {
            angle = lnchrAngle + (rate * dt);
            if (angle >= lnchrUpAngle) { angle = lnchrUpAngle; rate = 0.0; }
        } else if (cmdLnchrPos == DOWN && lnchrAngle != lnchrDownAngle) {
            angle = lnchrAngle - (rate * dt);
            if (angle <= lnchrDownAngle) { angle = lnchrDownAngle; rate = 0.0; }
        }

        lnchrAngle = angle;
        lnchrRate  = rate;
    }
}

void GroundVehicle::dynamics(const double dt)
{
    BaseClass::dynamics(dt);
    launcherDynamics(dt);   // move o lançador para posicao comandada
}
```

```
sam1: ( SamVehicle
    type: "SA-6"

    // Posicao comandada e limites do lancador
    commandedPosition: up                   // 'up' ou 'down' (Identifier)
    launcherUpAngle:  ( Degrees  75.0 )     // angulo maximo
    launcherDownAngle:( Degrees   0.0 )     // angulo minimo
    launcherMoveTime: ( Seconds   8.0 )     // tempo para ir de DOWN a UP

    // Envelope de lancamento
    maxLaunchRange: ( KiloMeters 25.0 )
    minLaunchRange: ( KiloMeters  2.0 )

    // Subsistemas sao COMPONENTES, nao slots
    components: {
        sms: ( StoresMgr
            stores: {
                1: ( Sam  type: "SA-6" )
                2: ( Sam  type: "SA-6" )
            }
        )
    }
)
```

Toda a maquinaria de lançador vive em `GroundVehicle` e é herdada por igual. **`Tank` e
`Artillery` são *stubs* de vinte linhas** que só fixam a *string* de tipo — não modelam
canhão nem elevação de cano. **A única subclasse terrestre com comportamento próprio é
`SamVehicle`** (~160 linhas), que acrescenta o envelope de lançamento. `GroundStation`,
`GroundStationUav` e `GroundStationRadar` são postos fixos, tipicamente sem `DynamicsModel`.

### `LifeForm` — o mais rico dos tipos simples

Mantém um `actionState` com **15 posturas**, atualizado automaticamente pela velocidade:

```cpp
// src/models/player/LifeForm.cpp
bool LifeForm::setVelocity(const double ue, const double ve, const double we)
{
    bool ok { BaseClass::setVelocity(ue, ve, we) };

    const double tempX { std::fabs(ue) };
    const double tempY { std::fabs(ve) };

    // Nao altera a postura se estiver em paraquedas
    if (actionState != PARACHUTING) {
        if (tempX == 0 && tempY == 0)    actionState = UPRIGHT_STANDING;
        if (tempX > 0 || tempY > 0)      actionState = UPRIGHT_WALKING;
        if (tempX > 8 || tempY > 8)      actionState = UPRIGHT_RUNNING;
    }
    return ok;
}
```

O `actionState` é mapeado diretamente para os **bits 16–19** do campo *appearance* do PDU
`EntityState`:

```cpp
// src/interop/dis/Nib_entity_state.cpp
unsigned int bits { 1 };  // padrao: UPRIGHT_STANDING
if      (lf->getActionState() == LifeForm::UPRIGHT_STANDING) bits =  1;
else if (lf->getActionState() == LifeForm::UPRIGHT_WALKING)  bits =  2;
else if (lf->getActionState() == LifeForm::UPRIGHT_RUNNING)  bits =  3;
else if (lf->getActionState() == LifeForm::KNEELING)         bits =  4;
else if (lf->getActionState() == LifeForm::PRONE)            bits =  5;
else if (lf->getActionState() == LifeForm::CRAWLING)         bits =  6;
else if (lf->getActionState() == LifeForm::SWIMMING)         bits =  7;
else if (lf->getActionState() == LifeForm::PARACHUTING)      bits =  8;
else if (lf->getActionState() == LifeForm::JUMPING)          bits =  9;
else if (lf->getActionState() == LifeForm::SITTING)          bits = 10;
else if (lf->getActionState() == LifeForm::SQUATTING)        bits = 11;
else if (lf->getActionState() == LifeForm::CROUCHING)        bits = 12;
else if (lf->getActionState() == LifeForm::WADING)           bits = 13;
else if (lf->getActionState() == LifeForm::SURRENDER)        bits = 14;
else if (lf->getActionState() == LifeForm::DETAINED)         bits = 15;

pdu->appearance |= (bits << 16);  // bits 16-19
```

```cpp
// src/models/player/LifeForm.cpp (simplificado)
void LifeForm::look(const double up, const double sdws)
{
    if (getDamage() < 1 && lockMode != LOCKED) {
        lockMode = SEARCHING;
        // sdws roda o corpo (modifica heading)
        double hdg { getEulerAngles().z() };
        hdg = base::angle::aepcdRad(hdg + sdws);
        // 'up' muda apenas lookAngle, nao o pitch da aeronave
        lookAngle += up;
    }
}

void LifeForm::fire()
{
    StoresMgr* mgr { getStoresManagement() };
    if (mgr != nullptr) {
        if (weaponSel == LF_GUN) {
            Gun* myGun { mgr->getGun() };
            if (myGun != nullptr) {
                myGun->setPitch(lookAngle * base::angle::D2RCC);
                myGun->fireControl(true);
            }
        } else {  // LF_MISSILE
            Missile* msl { mgr->releaseOneMissile() };
            if (msl != nullptr && tgtAquired && tgtPlayer != nullptr)
                msl->setTargetPlayer(tgtPlayer, true);
        }
    }
}
```

### `SpaceVehicle` e `BoosterSpaceVehicle`

`SpaceVehicle` retorna `getMajorType() = SPACE_VEHICLE` e encaminha comandos de empuxo e
orientação ao `SpaceDynamicsModel`, localizado por `dynamic_cast` sobre o `DynamicsModel`
configurado.

**ARMADILHA — `BoosterSpaceVehicle` é um *stub* vazio.** Não há `liftoffTime`, não há fase
de *boost*, não há separação de estágio. O arquivo tem vinte linhas: `EMPTY_SLOTTABLE`,
`EMPTY_COPYDATA`, `EMPTY_DELETEDATA` e um construtor que fixa
`"GenericBoosterSpaceVehicle"`. E **`isEngineBurnEnabled()` é método de `AbstractWeapon`** —
um `SpaceVehicle` não o possui. Modelar um perfil de lançamento exige escrever o
`SpaceDynamicsModel` correspondente.

## 23.16 Dados e mensagens entre sistemas

Classes que herdam de `Object` — sem *slots*, sem ciclo de simulação; existem apenas para
carregar dados de um ponto a outro com segurança de ciclo de vida.

### `Emission` — pacote de energia RF em trânsito

```cpp
// resumo de include/mixr/models/Emission.hpp
double freq {};          // Frequencia                    (Hz)
double lambda {};        // Comprimento de onda           (m)  = c/freq
double pw {};            // Largura de pulso              (s)
double bw {};            // Largura de banda              (Hz)
double prf {};           // Pulse Repetition Frequency    (Hz)
unsigned int pulses {1}; // Pulsos por pacote (prf/frameRate)
double power {};         // Potencia Efetiva Irradiada    (W)
double gain {};          // Ganho efetivo da antena
double lossRng {1.0};    // Perda de espaco livre: 1/(4*pi*r^2)
double lossAtmos {1.0};  // Perda atmosferica
double lossXmit {1.0};   // Perda do transmissor
double rcs {};           // Radar Cross Section           (m2)
Antenna::Polarization polar {Antenna::NONE};

// Ponteiro RAW deliberado -- nao faz ref():
// Emission nunca sobrevive ao RfSystem que a criou;
// um ref() criaria um ciclo de referencia.
RfSystem* transmitter {};

// setRange(): atualiza lossRng para evitar recalculo no receptor
void Emission::setRange(const double r)
{
    BaseClass::setRange(r);  // salva o range
    if (r > 1.0)
        lossRng = 1.0 / (4.0 * base::PI * r * r);
    else
        lossRng = 1.0;  // protecao contra distancia zero/negativa
}
```

```cpp
// Doppler shift (Hz) -- equacao 1-3 de Hovanessian
double getDopplerShift() const
{
    if (lambda > 0.0)
        return (-2.0 * getRangeRate() / lambda);
    return 0.0;
}
```

### `SensorMsg` e `IrQueryMsg`

```cpp
// resumo de include/mixr/models/SensorMsg.hpp
// Par origem/destino (safe_ptr: seguranca de ciclo de vida)
base::safe_ptr<Player>      ownship;  // player que enviou
base::safe_ptr<Player>      target;   // player alvo
Gimbal*                     gimbal{}; // gimbal que estava apontado

// Geometria da medicao
double maxRng  {};   // Alcance maximo do sensor            (NM)
double rng     {};   // Alcance ao alvo                     (m)
double rngRate {};   // Taxa de variacao do alcance         (m/s)
double gaz     {};   // Azimute no referencial do gimbal    (rad)
double gel     {};   // Elevacao no referencial do gimbal   (rad)
double iaz     {};   // AOI azimute no referencial inercial (rad)
double iel     {};   // AOI elevacao no referencial inercial(rad)
base::Vec3d losO2T; // LOS ownship->alvo (NED do ownship)
base::Vec3d losT2O; // LOS alvo->ownship (NED do alvo)
base::Vec3d aoi;    // Angulo de incidencia normalizado

// returnReq: distingue radar ATIVO (quer que o alvo calcule e retorne o RCS)
// de sensor PASSIVO tipo RWR (apenas detecta a presenca da emissao)
bool returnReq {};

// localOnly: se true, ignora players networked
bool localOnly {};
```

```cpp
// IrQueryMsg -- extensao IR
// Parametros do sensor IR (campos de ENTRADA)
double lowerWavelength {};   // Limite inferior da banda espectral (um)
double upperWavelength {};   // Limite superior                    (um)
double ifov {};              // Instantaneous Field of View        (sr)
double nei  {};              // Noise Equivalent Irradiance        (W/sr)

// Campos de SAIDA (preenchidos pelo IrSignature do alvo):
double signatureAtRange {};              // Potencia total ao alcance (W/sr)
const double* signatureByWaveband {};    // Potencia por banda espectral (W/sr)
double projectedArea {};                 // Area projetada do alvo (m2)
```

### `Tdb` — o *buffer* de geometria por quadro

```cpp
// include/mixr/models/Tdb.hpp
   Player**    targets {};       // Target pointer
   unsigned int maxTargets {};   // Max number of targets (i.e., size of the arrays)
   unsigned int numTgts {};      // Number of targets

   base::Vec3d* losG {};         // Normalized LOS vector (gimbal to target) in Gimbal coord
   base::Vec3d* losO2T {};       // Ownship to target normalized LOS vector (ownship's NED)
   base::Vec3d* losT2O {};       // Target to ownship normalized LOS vector (target's NED)

   double* ranges {};        // Range to target (meters)
   double* rngRates {};      // Range Rate (m/s)
   double* aar {};           // Compute angle off antenna boresight (radians)
   double* aazr {};          // Compute azimuth off boresight (radians)
   double* aelr {};          // Compute elevation off boresight (radians)

   double* xa {};            // termos intermediarios da conversao para
   double* ya {};            // azimute/elevacao, mantidos como arrays para
   double* za {};            // permitir as operacoes vetorizadas
   double* ra2 {};
   double* ra {};
```

Duas observações: **não há um `gainTgt`** — o ganho da antena por alvo é um array local de
`Antenna::rfTransmit()`, não campo do `Tdb` (o ganho é propriedade da antena, não da
geometria). E o **`aar`**, ângulo total fora do *boresight*, é o que o `IrSeeker` consulta
para decidir se um alvo está dentro do campo de visada — não o par azimute/elevação.

O nome `Tdb` significa *Track Data Block*, mas **não tem relação com `Track`**: guarda alvos
candidatos, não pistas rastreadas. Nome de fábrica: `"Gimbal_Tdb"`.

### `Track` — o estado de um alvo rastreado

```cpp
// resumo de include/mixr/models/Track.hpp
// Cinematica do alvo (NED, relativa ao ownship)
base::Vec3d pos;    // posicao  (m)
base::Vec3d vel;    // velocidade (m/s)
base::Vec3d accel;  // aceleracao (m/s2)
double      age {}; // tempo desde o ultimo update (s)

// Identificacao: um enum, nao um inteiro solto
enum IffCode { UNKNOWN, FRIENDLY, FOE, COMMERCIAL, OTHER };
IffCode iffCode {UNKNOWN};

// Alvo resolvido: ponteiro CRU, nao safe_ptr
Player* tgt {};                 // acessor: getTarget()

// Historico de sinal -- nao ha um campo 'snDbl'
double lastSN[4] {};  double avgSig {};  double maxSig {};  int nSig {};

// Ordenacao na shoot list
int shootList {};
```

Três correções para quem for ler o código: `Track` vive em
`include/mixr/models/Track.hpp` (não no cabeçalho do `TrackManager`); **não existe campo
`trackSide`**; e o alvo resolvido é um **ponteiro cru `tgt`**, não um `safe_ptr`. As
subclasses concretas — `RfTrack` e `IrTrack` — acrescentam o vínculo com a medição que
originou a pista (`getLastEmission()`, `getLastQuery()`) e o *setter* de sinal.

### `Message`, `TargetData`, `Image` e `Designator`

As quatro têm em comum: **são estruturas prontas à espera de consumidores**. Estão
registradas na fábrica, são configuráveis, e **nenhuma participa de um caminho de execução
do framework**.

`Message` carrega remetente, lista de destinatários, *timestamp* e tempo de vida
(`lifeSpan`, padrão 5 s), mais um código de confirmação.

**ARMADILHA — `Message` e `TargetData` não são usadas por ninguém.** Uma busca por
`models::Message` fora do seu próprio `.cpp` e da fábrica não devolve nada. `CommRadio` e
`Datalink` transportam um `base::Object*` **opaco** — nunca um `Message`. O mesmo vale para
`TargetData`: não há *slot* em `Steerpoint` que a receba, e nenhum código em
`OnboardComputer` ou `StoresMgr` a lê. As duas são **vocabulário oferecido**, não mecanismos
em funcionamento.

Para registro, os *slots* de `TargetData` são: `enabled`, `completed`, `weaponType`,
`quantity`, `manualAssign`, `stickType`, `stickDistance`, `interval`, `maxMissDistance`,
`armDelay`, `angle`, `azimuth`, `velocity`.

**`Image`** (nome de fábrica `"SarImage"`) **não é uma grade de reflectividade**: é um
*buffer* de pixels cru (`unsigned char* data`, mais largura, altura e profundidade),
acompanhado do ponto de mira geodésico, da orientação e da resolução. É criada pelo `Sar` e
depositada numa lista interna dele, de onde a aplicação a recolhe — **não há evento de
entrega**.

**`Designator`** é um ponto de *targeting* laser, e os seus dados são **geodésicos**
(latitude, longitude, elevação), não ECEF, mais frequência, potência e código. Não guarda
referência ao *player* que designa.

**ARMADILHA — nenhuma arma consulta o `Designator`.** O único consumidor de um
`DESIGNATOR_EVENT` em toda a árvore é `AbstractWeapon::onDesignatorEvent()`, e o corpo desse
método é explicitamente um não-fazer-nada. O próprio comentário do fonte diz: *"no futuro,
vamos querer passar isto ao nosso detector LASER. Mas ainda não temos nenhum."* **`Bomb` nem
sequer inclui `Designator.hpp`. Guiagem laser não está modelada.**

## 23.17 Agentes (UBF em `models`) e ações

### `SimAgent` e `MultiActorAgent`

`SimAgent` estende `base::ubf::Agent` resolvendo dois problemas que o UBF genérico deixa em
aberto: **onde** está a simulação, e **quem** é o ator.

```cpp
// src/models/SimAgent.cpp
simulation::Station* SimAgent::getStation()
{
   if ( myStation==nullptr ) {
      const auto s = dynamic_cast<simulation::Station*>(
                        findContainerByType(typeid(simulation::Station)));
      if (s != nullptr) {
         myStation = s;
      }
   }
   return myStation;
}

WorldModel* SimAgent::getWorldModel()
{
   WorldModel* sim{};
   simulation::Station* s{getStation()};
   if (s != nullptr) {
      sim = dynamic_cast<WorldModel*>(s->getSimulation());
   }
   return sim;
}
```

`myStation` é ponteiro cru **deliberadamente** — o agente está *contido* na `Station`, e uma
referência contada nessa direção criaria um ciclo.

```cpp
// src/models/SimAgent.cpp
void SimAgent::initActor()
{
   if (getActor() == nullptr ) {
      if (actorPlayerName == nullptr) {
         // not correctly specified as a SimAgent, try baseClass ?
         BaseClass::initActor();
      } else {
         WorldModel* sim{getWorldModel()};
         if ( sim != nullptr ) {
            base::Component* player{
                  sim->findPlayerByName(actorPlayerName->getString())};
            if (actorComponentName == nullptr) {
               // no player component specified, so the player is the actor
               setActor(player);
            } else if (player != nullptr) {
               base::Pair* pair{
                     player->findByName(actorComponentName->getString())};
               if (pair != nullptr) {
                  setActor(dynamic_cast<base::Component*>( pair->object() ));
               }
            }
         }
      }
   }
}
```

**REGRA — `actorComponentName` permite agir sobre um subsistema, não sobre a aeronave
inteira.** Os dois *slots* compõem-se: `actorPlayerName` sozinho faz do *player* o ator;
acrescentar `actorComponentName` desce a busca para *dentro* dele, via `findByName()`. Com
`actorPlayerName: falcao1` o comportamento comanda a aeronave; com
`actorComponentName: "radar1"` acrescentado, o mesmo comportamento comanda apenas o radar —
modos, alcance, varredura — sem poder tocar na trajetória.

**Cuidado defensivo com consequência:** se o nome do componente for declarado mas não for
encontrado, `setActor()` nunca é chamado e o agente fica **sem ator, inerte**. Um erro de
digitação no nome não gera diagnóstico — gera silêncio.

```cpp
// src/models/MultiActorAgent.cpp
void MultiActorAgent::controller(const double dt)
{
    if (getState() == nullptr || nAgents == 0) return;

    // 1. Atualiza o estado global uma vez para todos os agentes
    getState()->updateGlobalState();

    // 2. Para cada par (actor, behavior):
    for (unsigned int i = 0; i < nAgents; i++) {
        if (agentList[i].actor == nullptr) continue;

        setActor(agentList[i].actor);
        base::ubf::AbstractBehavior* behavior { agentList[i].behavior };

        getState()->updateState(agentList[i].actor);   // PERCEPCAO

        base::ubf::AbstractAction* action {              // DECISAO
            behavior->genAction(getState(), dt) };

        if (action != nullptr) {                          // ATUACAO
            action->execute(getActor());
            action->unref();
        }
    }
    setActor(nullptr);
}
```

```
// Declarado em Station.components:
formacao: ( MultiActorAgent
    state: ( MinhaEstadoTatico )

    // Slot 'agentList': chave = nome do player, valor = behavior
    agentList: {
        alpha: ( BehaviorLider    vote: 100 )
        bravo: ( BehaviorAla      vote: 100 )
    }
)
```

O estado global (`updateGlobalState()`) é atualizado **uma única vez** antes do laço; o
estado local (`updateState(actor)`) é atualizado individualmente para cada ator.

### `Actions` — integração UBF com lógica de missão

```cpp
// src/models/Actions.cpp
// Estado da action e determinado por 'manager' (safe_ptr<OnboardComputer>):
//   manager != nullptr => inProgress
//   manager == nullptr => nao iniciada ou concluida

bool Action::isReadyToStart() { return !isInProgress() && !isCompleted(); }
bool Action::isInProgress()   { return (manager != nullptr); }
bool Action::isCompleted()    { return completed; }

// trigger(): inicia a acao e registra o manager
bool Action::trigger(OnboardComputer* const mgr)
{
    setManager(mgr);   // manager != nullptr => isInProgress() = true
    completed = false;
    return true;
}

// setCompleted(): libera o manager (sai de inProgress) e marca como feito
void Action::setCompleted(const bool flg)
{
    if (flg) setManager(nullptr);
    completed = flg;
}

bool Action::cancel()
{
    completed = true;
    setManager(nullptr);
    return true;
}

// execute() -- interface UBF: localiza o OBC e chama trigger()
bool Action::execute(base::Component* actor)
{
    auto* obc = dynamic_cast<OnboardComputer*>(actor);
    if (obc == nullptr) {
        // actor pode ser o Player -- busca o OBC na arvore
        obc = dynamic_cast<OnboardComputer*>(
            actor->findByType(typeid(OnboardComputer)));
    }
    if (obc != nullptr) return trigger(obc);
    return false;
}
```

As quatro subclasses concretas: `ActionImagingSar`, `ActionWeaponRelease`,
`ActionDecoyRelease`, `ActionCamouflageType`.

```cpp
// src/models/Actions.cpp -- a acao mais simples (one-shot)
bool ActionCamouflageType::trigger(OnboardComputer* const mgr)
{
    if (mgr != nullptr) {
        Player* own { static_cast<Player*>(
            mgr->findContainerByType(typeid(Player))) };
        if (own != nullptr) {
            // Muda o camouflageType do player --
            // SigSwitch.getRCS() usara o novo tipo para selecionar
            // a sub-assinatura correta (ex: stealth ON/OFF)
            own->setCamouflageType(getCamouflageType());
            BaseClass::setCompleted(true);  // one-shot: completa imediatamente
            return true;
        }
    }
    return false;
}
```

```cpp
// src/models/Actions.cpp -- acao multi-frame
bool ActionImagingSar::trigger(OnboardComputer* const mgr)
{
    Player* own { static_cast<Player*>(
        mgr->findContainerByType(typeid(Player))) };
    if (own != nullptr) {
        base::Pair* pair { own->getSensorByType(typeid(Sar)) };
        if (pair != nullptr) {
            setSarSystem(static_cast<Sar*>(pair->object()));
        }
    }
    if (sar != nullptr) {
        sar->setStarePoint(getSarLatitude(), getSarLongitude(), getSarElevation());
        sar->requestImage(getImageSize(), getImageSize(), getResolution());
        setManager(mgr);   // fica inProgress() == true
        return true;
    }
    return false;
}

void ActionImagingSar::process(const double dt)
{
    BaseClass::process(dt);
    if (isInProgress() && sar != nullptr) {
        // Verifica se o SAR terminou (imageamento nao esta mais em progresso)
        if (!sar->isImagingInProgress()) {
            setCompleted(true);   // sai de inProgress(), libera o manager
        }
    }
}
```

```
wp3: ( Steerpoint
    latitude:  ( LatLon direction: "s" degrees: 23 minutes: 10 )
    longitude: ( LatLon direction: "w" degrees: 46 minutes: 20 )
    altitude:  ( Feet 15000 )

    // Ao capturar este waypoint, lanca uma arma da estacao 1
    action: ( ActionWeaponRelease
        targetLatitude:   ( LatLon direction: "s" degrees: 23 minutes: 15 )
        targetLongitude:  ( LatLon direction: "w" degrees: 46 minutes: 25 )
        targetElevation:  0.0
        station:          1
    )
)

wp4: ( Steerpoint
    // Ao capturar wp4, inicia captura SAR -- multi-frame
    action: ( ActionImagingSar
        sarLatitude:  ( LatLon direction: "s" degrees: 23 minutes: 20 )
        sarLongitude: ( LatLon direction: "w" degrees: 46 minutes: 30 )
        sarElevation: ( Feet 100.0 )
        resolution:   ( Meters 3.0 )
        imageSize:    512
    )
)
```
---

# 24. CAMADA `linearsystem`

Fornece blocos de processamento de sinais em tempo discreto — filtros, funções de
transferência, limitadores e *sample-and-hold*. Depende apenas de `mixr_base` e compila em
`libmixr_linearsystem.so`.

**ARMADILHA — esta camada não é usada por nenhuma outra, e NÃO TEM FÁBRICA.**

- **Nenhum módulo do MIXR depende dele.** O `meson.build` de `models` lista `base`,
  `simulation`, `terrain` e `jsbsim` — **não `linearsystem`**. Uma busca por
  `#include "mixr/linearsystem/..."` fora do próprio diretório não devolve nada. Em
  particular, **o `Autopilot` NÃO usa `LagFilter`** nem nenhum outro bloco daqui.
- **Não existe `linearsystem/factory.cpp`.** Diferentemente de todos os outros módulos,
  este **não registra nenhuma classe**. Nenhum `LagFilter`, `SaH` ou `LowpassFilter` pode
  ser criado a partir de um arquivo EDL sem que a aplicação escreva a sua própria fábrica.

A biblioteca é compilada, instalada e exportada pelo pacote Conan — é uma **oferta ao
consumidor**, não uma peça em uso interno.

## 24.1 A transformação de Tustin

A decisão arquitetural central é a **transformação de Tustin** (bilinear), método padrão de
discretização de sistemas de controle contínuos. Substitui *s* por:

```
s ≈ (2/T) · (z - 1)/(z + 1)
```

convertendo a função de transferência contínua numa **equação de diferenças** — recorrência
calculável a cada *frame*. Os coeficientes são calculados **uma única vez** em
`initialize()` e reutilizados em todas as chamadas subsequentes a `g(x)`.

Os 13 arquivos organizam-se em três grupos: o motor numérico
(`ScalerFunc`/`DiffEquation`), as funções de transferência de primeira ordem e suas
especializações, e os blocos auxiliares (atrasos, *sample-and-hold* e limitadores).

## 24.2 `ScalerFunc` e `DiffEquation`

`ScalerFunc` é a raiz e herda de **`Object`** — não de `Component`: um filtro não tem ciclo
de simulação próprio, é chamado diretamente pelo código que o usa.

```cpp
// include/mixr/linearsystem/ScalerFunc.hpp
class ScalerFunc : public base::Object {
public:
    // Interface publica: uma unica chamada por frame
    virtual double g(const double x) = 0;   // calcula Y(n) = f(X(n))
    bool isValid() const override;           // arrays alocados, rate > 0
    unsigned int getRate() const { return rate; }

    // Inicializacao dos valores anteriores
    virtual bool setX0(const double);  // seta px[0..n-1] = x0
    virtual bool setY0(const double);  // seta py[0..n-1] = y0
    virtual bool setRate(const unsigned int);

    // Slots EDL: rate (Frequency ou Number), x0 (Number), y0 (Number)

protected:
    virtual void initialize();        // calcula coeficientes (subclasses)
    virtual void allocateMemory(unsigned int n);
    virtual void clearMemory();

    unsigned int n    {};   // ordem do sistema (tamanho dos arrays)
    double*      px   {};   // historico de entradas X(n), X(n-1), ...
    double*      py   {};   // historico de saidas  Y(n), Y(n-1), ...

    unsigned int rate {};   // taxa de chamada (Hz)
    double       x0   {};   // valor inicial de X
    double       y0   {};   // valor inicial de Y
};
```

```cpp
// src/linearsystem/DiffEquation.cpp
double DiffEquation::g(const double xn)
{
    if (!isValid()) return xn;  // passthrough se invalido

    // Passo 1: envelhecer o historico (shift right)
    for (unsigned int k = (n-1); k > 0; k--) {
        px[k] = px[k-1];
        py[k] = py[k-1];
    }

    // Passo 2: registrar a entrada atual
    px[0] = xn;

    // Passo 3: equacao de diferencas linear
    py[0] = 0;
    for (unsigned int k = 1; k < n; k++)
        py[0] += pa[k] * py[k];   // saidas passadas (feedback)
    for (unsigned int k = 0; k < n; k++)
        py[0] += pb[k] * px[k];   // entradas (feedforward)

    return py[0];
}
```

## 24.3 Funções de transferência de primeira ordem

`FirstOrderTf` implementa a discretização de Tustin de `H(s) = (N1·s + N2)/(D1·s + D2)`:

```cpp
// src/linearsystem/FirstOrderTf.cpp
void FirstOrderTf::initialize()
{
    BaseClass::initialize();
    if (!isValid()) return;

    const double T { 1.0 / static_cast<double>(rate) };

    pa[0] = 0;
    pa[1] = -(T*d2 - 2.0*d1) / (T*d2 + 2.0*d1);

    pb[0] =  (T*n2 + 2.0*n1) / (T*d2 + 2.0*d1);
    pb[1] =  (T*n2 - 2.0*n1) / (T*d2 + 2.0*d1);

    for (unsigned int k = 0; k < n; k++) {
        px[k] = x0;
        py[k] = y0;
    }
}

// isValid(): causalidade (ordem den >= ordem num) + den != 0 + rate > 0
bool FirstOrderTf::isValid() const
{
    unsigned int orderN { (n1 != 0) ? 1u : 0u };
    unsigned int orderD { (d1 != 0) ? 1u : 0u };
    bool valid { n == ORDER && rate > 0
                 && (d1 != 0 || d2 != 0)
                 && (orderD >= orderN) };
    return valid && BaseClass::isValid();
}
```

Duas especializações simplificam os casos mais usados:

```cpp
// LagFilter: H(s) = 1 / (tau*s + 1)   =>  N1=0, N2=1, D1=tau, D2=1
bool LagFilter::setTau(const double v)
{
    tau = v;
    setN1(0.0);  setN2(1.0);
    setD1(tau);  setD2(1.0);
    initialize();
    return true;
}

// LowpassFilter: H(s) = wc / (s + wc)  =>  N1=0, N2=wc, D1=1, D2=wc
bool LowpassFilter::setWc(const double v)
{
    wc = v;
    setN1(0.0);  setN2(wc);
    setD1(1.0);  setD2(wc);
    initialize();
    return true;
}

// slot 'wc' aceita Frequency (Hz -> rad/s via *2pi) ou Number (rad/s direto)
bool LowpassFilter::setSlotWc(const base::Frequency* const msg)
{
    double hz  { base::Hertz::convertStatic(*msg) };
    double rps { hz * 2.0 * base::PI };
    if (rps > 0) { setWc(rps); return true; }
    return false;
}
```

`LagFilter` suavizaria erros de rumo e altitude antes de enviá-los como comandos ao
`DynamicsModel`: quanto maior τ, mais suave — e mais atrasado. `LowpassFilter` atenua ruído
de alta frequência; como o `LagFilter`, tem ganho unitário em CC (H(0) = 1).

### `SecondOrderTf` — pior do que "não implementado"

`SecondOrderTf` está presente na hierarquia (e `Sz2` herda dele), mas o corpo de
`initialize()` contém apenas o comentário `##### IN-WORK #####` — **os coeficientes Tustin
de segunda ordem nunca são calculados**.

**ARMADILHA — `SecondOrderTf` LÊ MEMÓRIA NÃO INICIALIZADA.** A consequência é mais séria do
que "o filtro não filtra". `DiffEquation::clearMemory()` zera o vetor `pa` — **duas vezes,
por um erro de digitação** — e **nunca zera `pb`**. Em condições normais isso não importa,
porque `initialize()` preenche `pb` logo em seguida. Como `SecondOrderTf::initialize()` não
preenche nada, `pb` permanece com o conteúdo arbitrário do *heap*, e `DiffEquation::g()` o
multiplica pelas entradas a cada quadro.

**O resultado não é zero nem passagem direta: é ruído determinado pelo lixo de memória, que
muda a cada execução. `SecondOrderTf` e `Sz2` NÃO devem ser instanciados.**

## 24.4 Atrasos, *sample-and-hold* e limitadores

### `Sz1` — os coeficientes crus de primeira ordem

O nome sugere um atraso de um *sample* (z⁻¹). **Não é isso.** `Sz1` deriva de `FirstOrderTf`
e o seu `.cpp` **não contém nenhum `g()`**: usa o `DiffEquation::g()` herdado. O que ele
acrescenta são **oito *slots*** — `n1`/`N1`, `n2`/`N2`, `d1`/`D1`, `d2`/`D2` — e mais nada.

**POR QUÊ `Sz1` existe.** `FirstOrderTf` é `EMPTY_SLOTTABLE`: os seus quatro coeficientes
não são configuráveis de fora. `LagFilter` e `LowpassFilter` resolvem isso expondo um
parâmetro de alto nível (τ, ω_c) e derivando os coeficientes. `Sz1` é a terceira
alternativa: **expõe N1, N2, D1, D2 diretamente**, para quem já tem a função de
transferência e não quer que o framework a interprete. `Sz2` faz o mesmo para segunda ordem
— com a ressalva grave acima.

### `SaH` — *sample-and-hold*

```cpp
// src/linearsystem/SaH.cpp
double SaH::g(const double xn)
{
    if (!isValid()) return xn;

    px[0] = xn;
    time += 1.0 / rate;           // avanca o contador pelo periodo da taxa mestre

    if (time >= stime) {          // chegou o momento de amostrar?
        py[0] = px[0];            //   sim: captura a entrada
        time  = 0;                //   zera o contador
    }
    return py[0];                 // segura o ultimo valor amostrado
}

void SaH::initialize()
{
    BaseClass::initialize();
    if (!isValid()) return;

    px[0] = x0;
    py[0] = y0;
    stime = 1.0 / sampleRate;     // periodo de amostragem (s)

    // Truque: age alem do maximo para forcar um sample na 1a chamada a g()
    time  = stime + 1.0;
}
```

Uso típico: simular latência de sensores que atualizam a taxa menor que o laço TC — um radar
que atualiza a 10 Hz num laço de 50 Hz usa `SaH(rate=50, sampleRate=10)`.

### Uso a partir de C++ (a forma efetivamente disponível neste fork)

```cpp
// Em initData() ou reset() do seu componente:
hdgFilter = new linearsystem::LagFilter();
hdgFilter->setRate(50);                 // a taxa em que g() sera chamado, Hz
hdgFilter->setTau(0.5);                 // constante de tempo, segundos
hdgFilter->setX0(0.0);
hdgFilter->setY0(0.0);

rangeHold = new linearsystem::SaH();
rangeHold->setRate(50);                 // taxa mestre
rangeHold->setSampleRate(10);           // simula um radar a 10 Hz

// A cada quadro, no updateTC() ou process():
const double hdgSuave { hdgFilter->g(hdgBruto) };
const double rngRetido{ rangeHold->g(rngBruto) };
```

Para usá-los a partir de EDL, a aplicação precisa registrá-los na sua própria fábrica:

```cpp
base::Object* minhaFactory(const std::string& name)
{
    base::Object* obj {};

    if      (name == linearsystem::LagFilter::getFactoryName())
        obj = new linearsystem::LagFilter();
    else if (name == linearsystem::LowpassFilter::getFactoryName())
        obj = new linearsystem::LowpassFilter();
    else if (name == linearsystem::SaH::getFactoryName())
        obj = new linearsystem::SaH();
    // ... demais blocos ...

    if (obj == nullptr) obj = models::factory(name);
    if (obj == nullptr) obj = base::factory(name);
    return obj;
}
```

Feito isso, a forma EDL correspondente seria (os nomes de fábrica coincidem com os nomes das
classes neste módulo):

```
( LagFilter
    rate: ( Hertz 50 )      // chamado a 50 Hz
    tau:  ( Seconds 0.5 )
    x0:   0.0
    y0:   0.0
)
```

### Limitadores

```cpp
// src/linearsystem/LimitFunc.cpp
double LimitFunc::g(const double xn)
{
    if (!isValid()) return xn;
    px[0] = xn;
    double tmp { xn };
    if      (tmp > upper) tmp = upper;
    else if (tmp < lower) tmp = lower;
    py[0] = tmp;
    return py[0];
}

// Limit01 e Limit11 sao atalhos de construcao -- sem SLOTS proprios:
Limit01::Limit01() : LimitFunc(0,1) {}   // [0, 1]  -- throttle
Limit11::Limit11() : LimitFunc(1,1) {}   // <<< DEFEITO (veja abaixo)
// Limit: versao configuravel via EDL (slots 'lower' e 'upper')
```

**ARMADILHA — `Limit11` está QUEBRADO no fonte.** O cabeçalho de `Limit11` diz *"Limits
between -1 and 1"*. O construtor, em `src/linearsystem/Limit11.cpp`, escreve
`LimitFunc(1,1)` — limite inferior **um**, não menos um.

O efeito não é um limite estranho: **é a destruição do sinal**. Com `lower == upper == 1`,
`LimitFunc::g()` satura toda entrada para a constante 1,0. Um comando de superfície de
controle que passe por um `Limit11` deixa de ser um comando e vira deflexão máxima
permanente. **Use `( Limit lower: -1.0 upper: 1.0 )` enquanto o defeito não for corrigido.**

Nota: o que `Limit01` e `Limit11` não têm é uma **tabela de *slots***.
`LimitFunc::initialize()` existe e é chamada pelo construtor e por ambos os *setters*. O que
os dispensa de configuração é outra coisa: **`LimitFunc::isValid()` verifica apenas
`lower <= upper` e NÃO exige `rate`** — ao contrário de todos os demais blocos do módulo.

---

# 25. CAMADA `linkage` — E/S COM HARDWARE

Implementa concretamente as interfaces abstratas de `base/concepts/linkage`: transforma
dispositivos físicos — joysticks, pedais, painéis de *cockpit* — em canais de dados que a
simulação consome, e converte saídas da simulação em sinais para dispositivos físicos.
Depende apenas de `mixr_base`.

**As quatro camadas de E/S**, com responsabilidades estritamente separadas:

```
   Station
      |
   IoHandler        <- orquestrador; mantem inData e outData
      |
   IoDevice         <- le/escreve o hardware (UsbJoystick, MockDevice)
      |
   Adapter          <- mapeia UM canal do dispositivo para UM canal do IoData
      |
   IoData           <- buffer central de canais AI/AO/DI/DO
```

Fluxo de **entrada** (de baixo para cima): `IoDevice` lê do hardware, cada `Adapter`
transforma e copia o seu canal para o `IoData`, e o `IoHandler` disponibiliza o buffer para
a simulação. O fluxo de **saída** percorre o sentido inverso.

A `Station` chama `inputDevices(dt)` **antes** da física e `outputDevices(dt)` **depois** —
garantindo que o hardware seja lido antes do modelo de dinâmica executar e escrito depois.

## 25.1 `IoData` — o *buffer* central

```cpp
// resumo de include/mixr/linkage/IoData.hpp
class IoData : public base::AbstractIoData {
private:
    std::vector<double> ai_table;  // Analog  Inputs  (double)
    std::vector<double> ao_table;  // Analog  Outputs (double)
    std::vector<double> di_table;  // Discrete Inputs (double -- bool semantics)
    std::vector<double> do_table;  // Discrete Outputs
    // Slots: numAI, numAO, numDI, numDO  (Number)
};

// Todos os acessores sao 1-based e retornam false sem abortar se invalido:
bool IoData::getAnalogInput(const int channel, double* const value) const
{
    bool ok {};
    if (value != nullptr &&
        channel > 0 && channel <= static_cast<int>(ai_table.size())) {
        *value = ai_table[channel-1];   // canal 1 -> indice 0
        ok = true;
    }
    return ok;
}

void IoData::clear()
{
    std::fill(ai_table.begin(), ai_table.end(), 0.0);
    std::fill(ao_table.begin(), ao_table.end(), 0.0);
    std::fill(di_table.begin(), di_table.end(), false);
    std::fill(do_table.begin(), do_table.end(), false);
}

bool IoData::setNumAI(const int num) { ai_table.resize(num); return true; }
```

Canais **analógicos** (`double`) representam eixos contínuos — joystick, pedais,
potenciômetros. Canais **discretos** (`bool`) representam estados binários — botões,
*switches*, LEDs.

A separação entre `inputData` e `outputData` no `IoHandler` é puramente de **sentido de
fluxo**. O *slot* `ioData` aponta os dois para o **mesmo** buffer; os *slots* `inputData` e
`outputData` os separam.

**ARMADILHA — canais de DISPOSITIVO são 0-based; canais de `IoData` são 1-based.** Esta é a
fonte de erro mais comum do módulo, e as duas convenções coexistem a poucos centímetros uma
da outra num arquivo EDL. O cabeçalho de `AbstractIoDevice` diz: *"All Channel (and port)
numbers start with zero"*. O de `IoData` diz: *"Channel numbers are all one(1) based"*.

Num `AnalogInput`, portanto, `channel: 0` refere-se ao primeiro eixo do joystick, e `ai: 1`
refere-se ao primeiro canal do `IoData`. **Os dois números apontam para "o primeiro", e são
diferentes.**

## 25.2 `IoDevice` e dispositivos concretos

`IoDevice` (nome de fábrica `"BaseIoDevice"`) implementa `AbstractIoDevice` e é a base de
todo dispositivo. Tem um único *slot*, `adapters`, e não contém sub-dispositivos.

```cpp
// include/mixr/linkage/IoDevice.hpp
void processInputsImpl(const double, base::AbstractIoData* const inData) final {
   readInputs();                 // 1. le do hardware para os buffers da classe
   processInputAdapters(inData); // 2. adaptadores levam dos buffers ao IoData
}

void processOutputsImpl(const double, const base::AbstractIoData* const outData) final {
   processOutputAdapters(outData); // 1. adaptadores levam do IoData aos buffers
   writeOutputs();                 // 2. escreve os buffers no hardware
}
```

A **inversão da ordem** é a coisa mais importante do módulo: em ambos os casos o hardware
fica numa extremidade e o `IoData` na outra; o que muda é o sentido da travessia.

### `UsbJoystick` (Linux)

```cpp
// src/linkage/platform/UsbJoystick_linux.cpp
void UsbJoystick::readInputs()
{
    js_event js;

    // Laco nao-bloqueante: le todos os eventos pendentes
    while (true) {
        const int status { static_cast<int>(read(stream, &js, sizeof(js))) };
        if (status != sizeof(js)) break;  // sem mais eventos

        switch (js.type & ~JS_EVENT_INIT) {

            case JS_EVENT_AXIS:
                // Normaliza [-32767, +32767] para [-1.0, +1.0]
                if (js.number < static_cast<int>(ai.size()))
                    ai[js.number] = js.value / 32767.0;
                break;

            case JS_EVENT_BUTTON:
                if (js.number < static_cast<int>(di.size()))
                    di[js.number] = static_cast<bool>(js.value);
                break;
        }
    }
}

// Nao ha um open(): o dispositivo e aberto em reset(), que
//   1. tenta  /dev/js%d       (caminho antigo) -- PRIMEIRO
//   2. tenta  /dev/input/js%d (caminho atual)  -- se o primeiro falhar
//   3. consulta versao e nome:  JSIOCGVERSION, JSIOCGNAME
//   4. consulta capacidades:    JSIOCGAXES, JSIOCGBUTTONS
//      (ambas devolvem unsigned char, nao int)
//   5. ai.resize(numAxes);  di.resize(numButtons);
```

### `MockDevice`

Dispositivo simulado sem acesso a hardware real, para desenvolvimento sem bancada.

**ARMADILHA — `MockDevice` NÃO é um `IoDevice`.** Apesar do nome e do papel, deriva
diretamente de `base::AbstractIoDevice`, **saltando `IoDevice`**. Consequência prática: ele
**não tem o *slot* `adapters`** — o seu único *slot* é `generators`, e o conteúdo é
verificado contra `typeid(AbstractGenerator)`. Colocar um `AnalogInput` ali é rejeitado com
uma mensagem no `cerr`.

## 25.3 Adaptadores — mapeamento canal a canal

Cada adaptador mapeia **um canal** do dispositivo físico para **um canal** do `IoData`, com
transformações opcionais. Todos herdam de `AbstractAdapter` e implementam
`processInputsImpl()` e `processOutputsImpl()`.

### `AnalogInput` — pipeline de três estágios

```cpp
// src/linkage/adapters/AnalogInput.cpp
double AnalogInput::convert(const double vin)
{
    // Estagio 1: zona morta
    double v1 { vin };
    if (deadband != 0 && vin < deadband && vin > -deadband)
        v1 = 0;

    // Estagio 2: offset e ganho
    const double v2 { (v1 - offset) * gain };

    // Estagio 3: shaping (opcional) -- table e uma Table1 (LFI)
    double v3 { v2 };
    if (table != nullptr) v3 = table->lfi(v2);

    return v3;
}

void AnalogInput::processInputsImpl(
    const base::AbstractIoDevice* const device,
    base::AbstractIoData* const inData)
{
    double vin {};
    if (device != nullptr)
        device->getAnalogInput(&vin, channel);  // le do hardware

    double vout { convert(vin) };

    if (inData != nullptr)
        inData->setAnalogInput(location, vout); // escreve no IoData
}

// Slots: ai (location no IoData), channel (canal no hardware),
//        deadband [0,1], offset, gain, table (Table1 opcional)
```

### Demais adaptadores

- **`DiscreteInput`** — lê um botão do dispositivo (*slots* `channel` e `port`) e escreve
  num canal discreto do `IoData` (*slot* `di`). O *slot* `inverted` inverte o valor antes de
  escrever — útil para *switches* normalmente fechados.
- **`Ai2DiSwitch`** — converte canal analógico em discreto. O *slot* de comparação chama-se
  **`level`** (não `threshold`) e o teste é `vin >= level` — **inclusive**. Demais *slots*:
  `di`, `channel`, `inverted`. Útil para gatilhos e pedais de freio que fisicamente são
  analógicos mas logicamente devem se comportar como botões.
- **`AnalogOutput` e `DiscreteOutput`** — o inverso: lêem do `IoData` e escrevem no
  hardware. Usados para *displays* físicos, LEDs e solenóides.

**ARMADILHA — `AnalogOutput` NÃO é o espelho de `AnalogInput`.** A transformação é a
inversa, e não a mesma: `vout = (value / gain) + offset` — **o ganho DIVIDE e o *offset* é
somado DEPOIS**. Não há zona morta. E há um defeito: `AnalogOutput` declara um *slot*
`table`, mas **`processOutputsImpl()` nunca o aplica**. Uma tabela de conformação declarada
na saída é aceita pelo *parser* e ignorada em silêncio.

## 25.4 Geradores de sinal sintético

**REGRA — geradores NÃO são adaptadores.** `AbstractGenerator` é uma hierarquia **paralela**
a `AbstractAdapter`, não subclasse dela. As assinaturas diferem: um adaptador recebe o
dispositivo *e* o `IoData`; um gerador recebe `dt` e o `IoData`, e **nunca toca num
dispositivo**. Por isso vão no *slot* `generators` de um `MockDevice`, e não em `adapters`.

- **`AnalogInputFixed`** — injeta valor constante num canal AI (*slots* `ai` e `value`).
- **`DiscreteInputFixed`** — injeta estado fixo num canal DI; *slots* `di` e **`signal`**,
  este último recebendo os identificadores **`ON` ou `OFF`**, não um booleano.
- **`AnalogSignalGen`** — gera sinais periódicos. As formas de onda são **quatro**: `sine`,
  `cosine`, `square` e `saw` — **não existem `"sawtooth"` nem `"pulse"`**. Demais *slots*:
  `ai`, `frequency` (uma `base::Frequency`, não número solto) e `phase` (um `base::Angle`).
  **Não há *slot* de amplitude**: a saída é sempre limitada a [−1, +1]. O avanço é
  integração do `dt` recebido: `time += dt`, depois α = 2πft + φ, normalizado para (−π, π]
  por `aepcdRad()`. O *slot* `phase` é deslocamento fixo, não acumulador, e `reset()` zera o
  tempo.

## 25.5 `IoHandler` — o orquestrador

```cpp
// resumo de include/mixr/linkage/IoHandler.hpp
class IoHandler : public base::AbstractIoHandler {
private:
    base::safe_ptr<base::AbstractIoData> inData;   // recebido do hardware
    base::safe_ptr<base::AbstractIoData> outData;  // enviado ao hardware
    base::safe_ptr<base::PairStream>     devices;  // lista de IoDevice

    // Thread assincrono (opcional)
    double rate  {50};   // Hz (slot: rate <Frequency>)
    double pri   {0.5};  // [0..1] (slot: priority <Number>)
    base::safe_ptr<IoPeriodicThread> periodicThread;

    // Slot 'ioData' seta inData = outData = mesmo buffer (combinado)
    // Slots 'inputData' e 'outputData' setam buffers separados
};

// readDeviceInputs(): percorre devices e chama processInputs(dt, inData)
void IoHandler::readDeviceInputs(const double dt)
{
    if (devices == nullptr) return;
    base::List::Item* item { devices->getFirstItem() };
    while (item != nullptr) {
        const auto pair = static_cast<base::Pair*>(item->getValue());
        const auto p    = static_cast<base::AbstractIoDevice*>(pair->object());
        p->processInputs(dt, inData);
        item = item->getNext();
    }
}

// startAsyncProcessingImpl(): cria IoPeriodicThread (subclasse de PeriodicThread)
void IoHandler::startAsyncProcessingImpl()
{
    if (periodicThread == nullptr) {
        periodicThread = new IoPeriodicThread(this, getRate());
        periodicThread->unref();  // safe_ptr mantem a referencia
        bool ok { periodicThread->start(getPriority()) };
        if (!ok) periodicThread = nullptr;
    }
}
```

O portão do modo assíncrono:

```cpp
// include/mixr/base/concepts/linkage/AbstractIoHandler.hpp
void inputDevices(const double dt)  { if (!async()) inputDevicesImpl(dt);  }
void outputDevices(const double dt) { if (!async()) outputDevicesImpl(dt); }
```

E `IoHandler::async()` devolve simplesmente `periodicThread != nullptr`. Ou seja: assim que
`startAsyncProcessing()` tiver sucesso, as chamadas que a `Station` faz a cada quadro
**tornam-se inócuas** — o que é exatamente a intenção, já que quem passa a fazer o trabalho
é a *thread*.

**ARMADILHA — o modo assíncrono, tal como está, NÃO EXECUTA NADA.** Aqui o mecanismo se
fecha sobre si mesmo. A `IoPeriodicThread` chama, no seu laço,
`ioHandler->inputDevices(dt)` e `ioHandler->outputDevices(dt)` — **os mesmos métodos
guardados pelo portão**. Como o portão está fechado justamente por a *thread* existir, as
chamadas dela também não fazem nada.

**O resultado é que ligar o processamento assíncrono DESLIGA a E/S.** Uma implementação
correta faria a *thread* chamar `inputDevicesImpl()` e `outputDevicesImpl()` diretamente.
Como o módulo exige de todo modo um `IoHandler` concreto escrito pela aplicação, essa é uma
das coisas que convém corrigir ao escrevê-lo.

Note ainda que **não há nenhum travamento sobre o `IoData`**: se a E/S de fato rodasse numa
*thread* separada, a leitura pela *thread* de tempo crítico seria uma corrida.

**ARMADILHA — não existe um `IoHandler` concreto no framework.** Três fatos que se somam:

1. O nome de fábrica de `linkage::IoHandler` é **`"BaseIoHandler"`**, não `"IoHandler"` (o
   comentário no cabeçalho que diz o contrário está desatualizado).
2. Ele **não está registrado** em `linkage/factory.cpp`, que conhece apenas `IoData`, os
   cinco adaptadores, os três geradores, `MockDevice` e `UsbJoystick`.
3. Ele é **genuinamente abstrato**: `AbstractIoHandler` declara `inputDevicesImpl()` e
   `outputDevicesImpl()` como virtuais puros, e **nenhuma classe em toda a árvore os
   implementa**. `linkage::IoHandler` oferece apenas os auxiliares protegidos
   `readDeviceInputs()` e `writeDeviceOutputs()`.

Concluir daí que o módulo é inútil seria errado: **a divisão é intencional**. O framework
entrega os dispositivos, os adaptadores e o *buffer*; **a aplicação entrega a política** —
em que ordem ler, quando escrever, o que fazer quando um dispositivo falha. O que falta é
uma classe de dez linhas:

```cpp
class MeuIoHandler final : public linkage::IoHandler {
    DECLARE_SUBCLASS(MeuIoHandler, linkage::IoHandler)
public:
    MeuIoHandler()  { STANDARD_CONSTRUCTOR() }
private:
    void inputDevicesImpl(const double dt) final  { readDeviceInputs(dt);  }
    void outputDevicesImpl(const double dt) final { writeDeviceOutputs(dt); }
};
IMPLEMENT_SUBCLASS(MeuIoHandler, "MeuIoHandler")
EMPTY_SLOTTABLE(MeuIoHandler)
EMPTY_COPYDATA(MeuIoHandler)
EMPTY_DELETEDATA(MeuIoHandler)
```

### Configuração EDL completa

```
ioHandler: ( MeuIoHandler
    ioData: ( IoData  numAI: 6  numDI: 16 )

    devices: {
        stick: ( UsbJoystick
            deviceIndex: 0      // /dev/input/js0

            adapters: {
                // Canal 0 do joystick -> IoData.ai[1]  (roll)
                roll: ( AnalogInput
                    ai:       1
                    channel:  0
                    deadband: 0.05
                    gain:     1.0
                )

                // Canal 1 do joystick -> IoData.ai[2]  (pitch, invertido)
                pitch: ( AnalogInput
                    ai:       2
                    channel:  1
                    gain:    -1.0
                    // Shaping: mais sensivel no centro do eixo
                    table: ( Table1
                        x:    [ -1.0  -0.5   0.0   0.5   1.0 ]
                        data: [ -1.0  -0.15  0.0   0.15  1.0 ]
                    )
                )

                // Botao 0 -> IoData.di[1]  (trigger)
                fire: ( DiscreteInput
                    di:      1
                    channel: 0
                )
            }
        )
    }
)
```

A `Station` lê `IoData.ai[1]` para *roll* e `ai[2]` para *pitch*, **sem saber que o hardware
subjacente é um joystick USB** — a zona morta, a inversão de sinal e a curva não-linear são
responsabilidade dos adaptadores.

---

# 26. CAMADA `recorder` — GRAVAÇÃO COM PROTOCOL BUFFERS

Sistema completo de gravação e reprodução de dados de simulação. Captura eventos em tempo
real, serializa com **Protocol Buffers** e permite analisar, imprimir ou retransmitir via
rede depois. Depende de `mixr_base`, `mixr_simulation`, `mixr_models` e da biblioteca
`protobuf`.

O protobuf motiva uma particularidade no *build*: antes de compilar qualquer `.cpp`, o Meson
chama `protoc` sobre `DataRecord.proto` para gerar `DataRecord.pb.h` e `DataRecord.pb.cc`.
Esse passo está encapsulado num `custom_target` no `proto/meson.build` do módulo.

## 26.1 `DataRecord.proto` e `DataRecordHandle`

```protobuf
// src/recorder/proto/DataRecord.proto
syntax = "proto2";                  // 'required'/'optional' sao proto2

package mixr.recorder.pb;

message DataRecord {
    required Time   time = 1;       // instante da amostra
    required uint32 id   = 2;       // REID_* -- discriminante do evento

    // Mensagens do gravador e da simulacao
    optional FileIdMsg               file_id_msg               = 11;
    optional UnknownIdMsg            unknown_id_msg            = 13;
    optional MarkerMsg               marker_msg                = 14;
    optional InputDeviceMsg          input_device_msg          = 15;

    // Mensagens de player
    optional NewPlayerEventMsg       new_player_event_msg      = 31;
    optional PlayerRemovedEventMsg   player_removed_event_msg  = 32;
    optional PlayerDataMsg           player_data_msg           = 33;
    optional PlayerDamagedEventMsg   player_damaged_event_msg  = 34;
    optional PlayerCollisionEventMsg player_collision_event_msg= 35;
    optional PlayerCrashEventMsg     player_crash_event_msg    = 36;
    optional PlayerKilledEventMsg    player_killed_event_msg   = 37;

    // Mensagens de arma, track, etc.
    // ...
}
```

O campo `id` é o **discriminante** — o código `REID_*`. Apenas o campo opcional
correspondente é preenchido, implementando o padrão protobuf de *union* tipada; do lado da
leitura, o cabeçalho recomenda usar `id` combinado com as funções `hasXxxx()` geradas.

Duas observações práticas: o esquema é **proto2** (daí `required`/`optional`, que proto3
eliminou); e a numeração dos campos deixa **lacunas deliberadas** entre os blocos temáticos
(11–15 recorder, 31–37 *players*, …) — espaço reservado para extensões. O cabeçalho documenta
explicitamente que aplicações devem usar o mecanismo de *extensions* do protobuf para
acrescentar mensagens e códigos `REID_*` próprios, sem alterar o esquema do framework.

`DataRecordHandle` é um *wrapper* de ciclo de vida sobre `pb::DataRecord*`: herda de
`base::Object` e participa da contagem de referências. **A razão de existir é a segurança
entre *threads***: o `DataRecorder` cria registros no *thread* TC (a 50 Hz) e os enfileira
com `addToQueue()`; os `OutputHandler`s os processam no *background* com `processQueue()`. O
`DataRecordHandle` carrega o `ref()` entre os dois contextos.

## 26.2 O vocabulário: `dataRecorderTokens.hpp`

Todo evento gravável tem um número — um *token* `REID_*` — em
`include/mixr/simulation/dataRecorderTokens.hpp`. Os valores são agrupados por faixa.

| Valor | Token | Significado e cargas úteis |
|---:|---|---|
| | **Controle de arquivo e sessão** | |
| 0 | `REID_END_OF_DATA` | fim do arquivo — **não pode ser desabilitado** |
| 1 | `REID_FILE_ID` | cabeçalho com os metadados da sessão |
| 2 | `REID_UNHANDLED_ID_TOKEN` | *token* desconhecido ou não tratado |
| 3 | `REID_RESET_EVENT` | a simulação foi reiniciada |
| | **Entrada do operador (faixa 20)** | |
| 21 | `REID_MARKER` | marcador manual: V1 = id, V2 = fonte |
| 22 | `REID_DI_EVENT` | entrada discreta (chave): V3 = valor |
| 23 | `REID_AI_EVENT` | entrada analógica (manche): V3 = valor |
| | **Ciclo de vida do *player* (faixa 40)** | |
| 41 | `REID_NEW_PLAYER` | P1 = o *player* criado |
| 42 | `REID_PLAYER_REMOVED` | P1 = o *player* |
| 43 | `REID_PLAYER_DATA` | amostra periódica; P1 = o *player* |
| 44 | `REID_PLAYER_DAMAGED` | P1 = alvo, P2 = arma |
| 45 | `REID_PLAYER_COLLISION` | P1, P2 = os dois *players* |
| 46 | `REID_PLAYER_CRASH` | P1 = o *player* |
| 47 | `REID_PLAYER_KILLED` | P1 = alvo, P2 = atirador |
| | **Armamento (faixa 60)** | |
| 61 | `REID_WEAPON_RELEASED` | P1 = arma, P2 = atirador, P3 = alvo |
| 62 | `REID_WEAPON_HUNG` | mesma tripla — a arma não saiu do cabide |
| 63 | `REID_WEAPON_DETONATION` | mesma tripla |
| 64 | `REID_GUN_FIRED` | P1 = atirador, V1 = n.º de tiros |
| | **Rastreio (faixa 80)** | |
| 81 | `REID_NEW_TRACK` | P1 = *player*, P2 = pista |
| 82 | `REID_TRACK_REMOVED` | idem |
| 83 | `REID_TRACK_DATA` | idem, periódico |
| | **Faixas reservadas** | |
| 500–999 | reservado | uso futuro do framework |
| 1000–9999 | eventos do usuário | livres para a aplicação |

**REGRA — `P1..P4` são os `objs[]`; `V1..V4` são os `values[]`.** A notação dos comentários
mapeia diretamente na assinatura de `recordData(token, objs[4], values[4])`: P*n* é
`objs[n-1]` — um `Object*` contado por referência — e V*n* é `values[n-1]`, um `double` cru.
**Ambos os vetores têm exatamente quatro posições, sempre, para qualquer evento.**

**Ao definir um evento próprio, escolha um número entre 1000 e 9999.** Abaixo disso o número
pode colidir com um *token* que uma versão futura do MIXR venha a definir.

## 26.3 `DataRecorder`

Implementação concreta de `AbstractDataRecorder` e o objeto que a `Station` recebe pelo
*slot* `dataRecorder`.

**Os *slots*: metadados da sessão.** **Nove dos dez** *slots* não afetam o que se grava —
apenas identificam *qual* gravação é esta: `eventName`, `application`, `caseNum`,
`missionNum`, `subjectNum`, `runNum`, `day`, `month` e `year` são copiados uma única vez
para o registro `REID_FILE_ID` no início do arquivo. **O décimo, `outputHandler`, é o único
com efeito operacional**: é o destino dos dados.

Não parece muito, e é decisivo na prática: sem esses campos, uma pasta com duzentos arquivos
`.rcd` de uma campanha de testes é indistinguível por inspeção.

### Macros de gravação

```cpp
// src/models/player/weapon/AbstractWeapon.cpp -- uso tipico
BEGIN_RECORD_DATA_SAMPLE(getWorldModel()->getDataRecorder(),
                         REID_WEAPON_DETONATION)
    // objs[]: players e armas relevantes para este evento
    SAMPLE_3_OBJECTS(this, getLaunchVehicle(), getTargetPlayer())
    // values[]: campos numericos (tipo de detonacao e alcance)
    SAMPLE_2_VALUES(DETONATE_ENTITY_IMPACT, getDetonationRange())
END_RECORD_DATA_SAMPLE()
```

```cpp
// resumo de include/mixr/simulation/recorder_macros.hpp
#define BEGIN_RECORD_DATA_SAMPLE(pRecorder, token)                    \
{  ::mixr::simulation::AbstractDataRecorder* _p = pRecorder;          \
   if (pRecorder != nullptr)  {          /* <- UNICA guarda */        \
      unsigned int _token = token;                                    \
      const ::mixr::base::Object* _obj[4] = { nullptr, ... };         \
      double _val[4] = { 0.0, 0.0, 0.0, 0.0 };

#define END_RECORD_DATA_SAMPLE()                                      \
      _p->recordData(_token, _obj, _val);  } }
```

**ARMADILHA — o filtro NÃO é "custo zero".** A única guarda dentro da macro é
`pRecorder != nullptr`. O teste `isDataEnabled()` acontece **um nível abaixo**, já dentro de
`AbstractDataRecorder::recordData()`.

Consequência: quando um evento está filtrado **mas existe um gravador**, **os argumentos são
todos avaliados assim mesmo**. No exemplo acima, `getLaunchVehicle()`, `getTargetPlayer()` e
`getDetonationRange()` são chamados, e só então o registro é descartado. **Custo zero de
verdade existe apenas quando NÃO há gravador nenhum.** Para quem instrumenta um laço quente:
se a coleta dos argumentos for cara, ela precisa ser guardada explicitamente por um
`isDataEnabled()` escrito à mão, antes da macro.

### `recordDataImp()` — despacho e serialização

```cpp
// src/recorder/DataRecorder.cpp
void DataRecorder::timeStamp(pb::DataRecord* const msg)
{
    if (msg == nullptr) return;
    simulation::Simulation* sim { getSimulation() };
    pb::Time* time { msg->mutable_time() };
    if (sim != nullptr) {
        time->set_exec_time( sim->getExecTimeSec()    );
        time->set_sim_time ( sim->getSimTimeOfDay()   );
        time->set_utc_time ( sim->getSysTimeOfDay()   );
    } else {
        time->set_exec_time(-1.0);
        time->set_sim_time (-1.0);
        time->set_utc_time (-1.0);
    }
}

bool DataRecorder::recordNewPlayer(const base::Object* objs[4],
                                    const double values[4])
{
    const auto player = dynamic_cast<const models::Player*>(objs[0]);
    if (player == nullptr) return false;

    const auto msg = new pb::DataRecord();
    timeStamp(msg);
    msg->set_id(REID_NEW_PLAYER);

    pb::NewPlayerEventMsg* newPlayerMsg { msg->mutable_new_player_event_msg() };
    genPlayerId(    newPlayerMsg->mutable_id(),    player );  // id + nome + tipo
    genPlayerState( newPlayerMsg->mutable_state(), player );  // pos + vel + euler

    sendDataRecord(msg);  // empacota em DataRecordHandle e chama outputHandler
    return true;
}
```

`sendDataRecord()` chama `outputHandler->addToQueue(handle)` **na *thread* TC** — enfileira
sem bloquear. Na primeira chamada da sessão, envia antes um registro `REID_FILE_ID` com os
metadados. `processRecords()` — chamado por `Station::updateData()` — **drena a fila no
*background*** via `outputHandler->processQueue()`, compatível com a latência variável de
disco e rede.

## 26.4 *Handlers* de saída

Os `OutputHandler`s formam uma **cadeia de responsabilidade** via `components`:
`processRecord(handle)` aplica `isDataTypeEnabled()`, chama `processRecordImp()` e propaga
para cada componente filho. Um único `DataRecordHandle` pode ser gravado em arquivo **e**
enviado via rede **e** impresso, numa única chamada ao topo da cadeia.

```cpp
// src/recorder/OutputHandler.cpp
void OutputHandler::processRecord(const DataRecordHandle* const dataRecord)
{
    if (dataRecord != nullptr && isDataTypeEnabled(dataRecord)) {
        processRecordImp(dataRecord);  // implementacao concreta (virtual)

        // Propaga para subcomponentes (cadeia de responsabilidade)
        base::PairStream* subs { getComponents() };
        if (subs != nullptr) {
            for (auto* item = subs->getFirstItem();
                 item != nullptr; item = item->getNext()) {
                auto* pair = static_cast<base::Pair*>(item->getValue());
                auto* sc   = static_cast<OutputHandler*>(pair->object());
                sc->processRecord(dataRecord);
            }
            subs->unref();
        }
    }
}

void OutputHandler::processQueue()
{
    base::lock(semaphore);
    const DataRecordHandle* dr {
        static_cast<const DataRecordHandle*>(queue.get()) };
    base::unlock(semaphore);

    while (dr != nullptr) {
        processRecord(dr);
        dr->unref();

        base::lock(semaphore);
        dr = static_cast<const DataRecordHandle*>(queue.get());
        base::unlock(semaphore);
    }
}
```

### `FileWriter` (nome de fábrica `RecorderFileWriter`)

Serializa cada `DataRecord` num formato auto-descritivo. O enquadramento é simples o
bastante para ser lido a olho num *hex dump*: **quatro bytes ASCII com o tamanho do
registro**, seguidos pelos bytes serializados.

```
//  registro de 123 bytes:   ' ' '1' '2' '3'  <123 bytes de protobuf>
//  registro de  12 bytes:   ' ' ' ' '1' '2'  <12 bytes de protobuf>
//
//  sprintf(nbuff, "%04d", n)  e depois os '0' iniciais viram ' '
```

O arquivo é encerrado por um registro `REID_END_OF_DATA` — que é um `DataRecord` normal,
enquadrado com o seu tamanho real como qualquer outro. **Não existe um quadro de tamanho
`"   0"` marcando o fim**: quem lê detecta o fim pelo *end-of-file* do próprio *stream*.

O versionamento automático evita sobrescrever arquivos, mas **o sufixo é anexado ao nome
inteiro, extensão inclusive**: `output.rcd` vira `output.rcd_v01`, depois `output.rcd_v02`,
até `_v99`.

### `NetOutput` (nome de fábrica `RecorderNetOutput`)

Serializa com `SerializeToString(&wireFormat)` e envia via `netHandler->sendData()`. O
*slot* `netHandler` aceita qualquer subclasse de `NetHandler` — tipicamente
`UdpUnicastHandler` para servidor centralizado ou `UdpMulticastHandler` para múltiplos
clientes.

### `TabPrinter`

Decodifica cada `DataRecord` e imprime campos em formato tabular. O *slot* `msgHdrOptn`
controla os cabeçalhos:

| Valor | Quando o cabeçalho é impresso |
|---|---|
| `NO_HDR` | nunca |
| `ALL_MSGS` | antes de todo registro |
| `ON_CHANGE` | quando o **tipo** do evento muda em relação ao anterior |
| `NEW_MSG` | uma **única vez por tipo** de evento, na primeira ocorrência |

Sem o *slot* `filename`, a saída vai para `stdout`.

```
dataRecorder: ( DataRecorder
    eventName:  "Missao_ALFA"
    caseNum:    1
    missionNum: 1

    // Cadeia de OutputHandlers como components:
    outputHandler: ( RecorderFileWriter
        filename: "missao_alfa"
        pathname: "/data/gravacoes"

        // Filho na cadeia: alem do arquivo, imprime no terminal
        components: {
            printer: ( TabPrinter
                msgHdrOptn: "NEW_MSG"
            )
        }
    )
)
```

## 26.5 *Handlers* de entrada: leitura e análise

**ARMADILHA — `InputHandler` NÃO é o espelho de `OutputHandler`.** A simetria entre os dois
lados é bem menor do que os nomes sugerem. `InputHandler` tem, na íntegra, **dois métodos**:
`readRecord()` e o virtual puro `readRecordImp()`. **Não há cadeia de responsabilidade**:
sem propagação a componentes filhos, sem fila, sem semáforo, sem filtro de tipo em
`processComponents()`. A única lógica de `readRecord()` é repetir `readRecordImp()`
descartando os registros que `isDataEnabled()` rejeitar. **A cadeia — essa sim, com
propagação e com a restrição de tipo que a torna segura — existe apenas do lado da saída.**

- **`FileReader`** (nome de fábrica `RecorderFileReader`) — a cada `readRecordImp()`: lê os
  4 bytes de tamanho e os converte com `atoi()`; se o resultado for zero, pula o registro;
  caso contrário lê os bytes e desserializa com `ParseFromString()`. O fim é detectado por
  `eof()`/`fail()` do *stream*. O `DataRecordHandle` montado é **devolvido ao chamador** —
  não há um *handler* configurado a jusante para o qual empurrá-lo.
- **`NetInput`** (nome de fábrica `RecorderNetInput`) — equivalente em rede: recebe bytes
  serializados via `NetHandler`, desserializa e distribui para a cadeia de `InputHandler`s.
  Permite análise em tempo real — um processo separado, em outra máquina, recebe eventos via
  `UdpMulticastHandler` enquanto a simulação roda.
- **`PrintHandler`** — a base comum dos impressores. `TabPrinter`, `PrintPlayer` e
  `PrintSelected` não herdam diretamente de `OutputHandler`, mas de `PrintHandler`, que
  concentra: o *slot* `filename` (com `pathname` opcional), a abertura do arquivo com o
  mesmo versionamento automático de `FileWriter`, e o *fallback* para `stdout`.
- **`PrintPlayer`** — filtra registros de um *player* específico (*slot* `playerName`).
- **`PrintSelected`** — apesar do nome, **não seleciona *campos*: seleciona *registros***.
  Compara um campo nomeado (`fieldName`) de um tipo de mensagem (`messageToken`) contra um
  valor de referência (`compareToValS`, `compareToValI` ou `compareToValD`) usando uma
  `condition` (`EQ`, `LT` ou `GT`), e imprime apenas os registros que satisfizerem o teste.
  É um **filtro predicado** — útil, por exemplo, para listar só as detonações a menos de
  10 m do alvo.

---

# 27. CAMADA `interop` — DIS E HLA

Conecta a simulação MIXR a outras simulações via protocolos padrão da indústria de simulação
militar. A árvore contempla **DIS** (*Distributed Interactive Simulation*, IEEE 1278) e
**HLA** (*High Level Architecture*, IEEE 1516, com o *object model* RPR-FOM) — mas **apenas
o primeiro é efetivamente construído neste fork**.

Duas bibliotecas:

- `libmixr_interop_common.so` — código compartilhado: gerência de NIBs e lógica de *dead
  reckoning*
- `libmixr_interop_dis.so` — implementação DIS completa

**ARMADILHA — HLA e RPR-FOM não são compilados.**

```python
# src/interop/meson.build -- na integra
subdir('./common')
subdir('./dis')

# TODO: figure out how to compile this
# subdir('./hla')    <= error: ISO C++17 does not allow dynamic exception specifications
# subdir('./rprfom') <= error: unimplemented virtual method
```

Os diretórios `src/interop/hla` (`Ambassador`, `hla::NetIO`, `hla::Nib`) e
`src/interop/rprfom` estão presentes e **completos** no repositório; o que falta é adequá-los
ao padrão C++ usado no *build* — as especificações dinâmicas de exceção (`throw(...)` em
assinaturas), removidas em C++17, vêm da API do *RTI*. A dependência `openrti` continua
declarada no `conanfile.py` e resolvida pelo Conan, ainda que nenhuma fonte que a utilize
entre no *build* atual.

A divisão entre `interop_common` e `interop_dis` reflete uma decisão arquitetural: **os
mecanismos de gerência de NIBs e o *executive* de rede são idênticos entre DIS e HLA** —
apenas o formato dos pacotes e o protocolo de transporte diferem. O ponto de integração com
a `Station` é o *slot* `networks`, que aceita uma `PairStream` de `AbstractNetIO`.

## 27.1 `interop/common` — `Nib` e *dead reckoning*

```cpp
// src/interop/common/Nib.cpp
// resetDeadReckoning(): chamado ao receber um novo update da rede
bool Nib::resetDeadReckoning(
    const unsigned char  dr,   // algoritmo: RPW_DRM, RVW_DRM, etc.
    const base::Vec3d&   p,    // posicao ECEF ao instante T0  (m)
    const base::Vec3d&   v,    // velocidade ECEF / corpo       (m/s)
    const base::Vec3d&   a,    // aceleracao ECEF / corpo       (m/s2)
    const base::Vec3d&   rpy,  // angulos de Euler [phi,theta,psi] (rad)
    const base::Vec3d&   av,   // velocidade angular            (rad/s)
    const double         time  // tempo inicial (exec time, s)
)
{
    drP0   = p;    drV0 = v;  drA0 = a;
    drRPY0 = rpy;  drAV0 = av;

    // Pre-computa matrizes para os algoritmos body-referenced (RPB, RVB)
    drComputeMatrixR0(drRPY0,  &drR0);     // R0: rotacao mundo->corpo em T0
    drComputeMatrixWwT(drAV0,  &drWwT);    // wwT: outer product de av
    drComputeMatrixOmega(drAV0, &drOmega); // Omega: matrix anti-simetrica

    // Suavizacao: se a nova posicao esta a < 1km, interpola sobre 2s
    // para evitar descontinuidade visual na recepcao do update
    ...
    drTime = time;  // zera o contador de DR
    return true;
}

// updateDeadReckoning(): chamado a cada frame por Player::deadReckonPosition()
bool Nib::updateDeadReckoning(const double dt,
                               base::Vec3d* const pNewPos,
                               base::Vec3d* const pNewAngles)
{
    double time { updateDrTime(dt) };   // incrementa drTime por dt

    mainDeadReckoning(time, &drPos, &drAngles);  // extrapola posicao/orientacao

    // Suavizacao: corrige lentamente o salto de posicao
    if (smoothTime > 0.0) {
        drPos += (smoothVel * smoothTime);
        smoothTime -= dt;
    }

    *pNewPos    = drPos;
    *pNewAngles = drAngles;
    return true;
}
```

### Os dez valores do `enum DeadReckoning`

**Atenção ao nome: o sufixo é `_DRM`, não o prefixo** — escreve-se `RVW_DRM`, não
`DRM_RVW`. As duas dimensões que os distinguem são o **referencial** (mundo/ECEF ou corpo) e
a **ordem** de cada extrapolação:

| Constante | Referencial | Rotação | Translação |
|---|---|---|---|
| `OTHER_DRM` | — | definido pelo usuário (`dynamicsOther()`) | — |
| `STATIC_DRM` | — | sem *dead reckoning* — estado congelado | (idem) |
| `FPW_DRM` | mundo | nenhuma | 1ª ordem (velocidade constante) |
| `RPW_DRM` | mundo | 1ª ordem | 1ª ordem (velocidade constante) |
| `RVW_DRM` | mundo | 1ª ordem | 2ª ordem (aceleração constante) |
| `FVW_DRM` | mundo | nenhuma | 2ª ordem (aceleração constante) |
| `FPB_DRM` | corpo | nenhuma | 1ª ordem |
| `RPB_DRM` | corpo | 1ª ordem | 1ª ordem |
| `RVB_DRM` | corpo | 1ª ordem | 2ª ordem |
| `FVB_DRM` | corpo | nenhuma | 2ª ordem |

As letras da sigla são, em ordem: **F**ixed/**R**ate de rotação, **P**osition/**V**elocity de
translação, e **W**orld/**B**ody de referencial.

**O ponto que a nomenclatura esconde:** `FPW_DRM` **não** significa "posição fixa". O *F*
refere-se à **rotação** (que não é extrapolada), enquanto *P* indica que a extrapolação de
translação é de primeira ordem — ou seja, **a entidade continua se movendo em linha reta a
velocidade constante**. Quem quer de fato congelar a entidade usa `STATIC_DRM`, que é o
**valor padrão de `drNum`** e o único que não extrapola nada.

Fórmulas de translação:

```
1a ordem (FPW_DRM, RPW_DRM):  p_novo = p_DR + v_DR * t_DR
2a ordem (RVW_DRM, FVW_DRM):  p_novo = p_DR + v_DR * t_DR + (1/2) a_DR * t_DR^2
```

Os quatro algoritmos *body-referenced* (sufixo `B_DRM`) exigem transformação adicional via as
matrizes `drR0`, `drWwT` e `drOmega` pré-computadas — neles, os vetores v e a chegam
expressos no referencial do **corpo** da entidade, não em ECEF.

A implementação de todos eles é `Nib::mainDeadReckoning()`, em `src/interop/common/Nib.cpp`
— um `switch` sobre `drNum` com um *case* por constante.

**ARMADILHA — `navDR_utils` existe, mas é CÓDIGO MORTO.** Há em
`src/base/util/navDR_utils.cpp` uma função livre `base::navDR::deadReckoning()` que
implementa exatamente os mesmos oito algoritmos, com o mesmo `switch`. É natural supor que
seja ela a usada. **Não é**: uma busca por `navDR::` em toda a árvore não encontra nenhum
chamador fora do próprio arquivo. O *dead reckoning* que roda é o de `Nib`; `navDR_utils` é
duplicação herdada que ninguém removeu.

### Quando um *update* é enviado

O método é `Nib::isPlayerStateUpdateRequired()`, e recebe o tempo de execução corrente. Ele
responde **sim** em qualquer destes casos:

1. o *player* mudou de modo, ou pediu remoção (`DELETE_REQUEST`);
2. o *flag* de congelamento mudou;
3. a aparência mudou — dano, fumaça, chamas ou camuflagem;
4. alguma parte articulada mudou (trem de pouso, portas, elevação de lançador);
5. o erro de posição passou de `maxPositionError`;
6. o erro de orientação passou de `maxOrientationError`;
7. passou-se `maxTimeDR` desde o último envio (o ***heartbeat***).

| Slot | Padrão | Papel |
|---|---:|---|
| `maxPositionError` | 3,0 m | erro de posição tolerado |
| `maxOrientationError` | 3,0° | erro de orientação tolerado |
| `maxTimeDR` | 5,0 s | *heartbeat*: envio mesmo sem erro |
| `maxAge` | 12,5 s | sem receber, o NIB é descartado |

O ganho é fácil de estimar: uma aeronave em voo reto e nivelado a 250 m/s acumularia 3 m de
erro em ~12 ms **se o receptor não extrapolasse** — mas com `RVW_DRM` o receptor *também*
propaga velocidade e aceleração, de modo que o erro só cresce com o que a extrapolação não
prevê. Em voo estável, **o gatilho que dispara não é o erro: é o *heartbeat* de 5 s**.
Frente a um envio a 50 Hz, isso é uma redução de 250×.

**ARMADILHA — esses *slots* aceitam TAMBÉM uma lista.** Os cinco *slots* de limiar são
registrados **duas vezes** em `dis::NetIO`: uma vez para um número escalar e outra para uma
`PairStream`. A segunda forma permite valores distintos por *kind*/*domain* de entidade — um
míssil pode exigir precisão bem maior que um navio. É um recurso útil e que passa
despercebido porque as duas declarações têm o mesmo nome.

## 27.2 `interop/common` — `NetIO`

### O repositório de NIBs

`NetIO` mantém duas tabelas, `inputList` e `outputList`, ambas mantidas **ordenadas por
chave** e consultadas com `std::bsearch` — portanto **O(log n), não O(1)**. A chave não é o
trio (site, aplicação, player) que se poderia esperar:

```cpp
// resumo de include/mixr/interop/common/NetIO.hpp
struct NibKey {
    unsigned short             id;      // ID do player
    base::safe_ptr<const String> fName; // nome do federado
};
```

**Site e aplicação estão dentro do *nome do federado*:** `dis::NetIO` codifica os dois numa
*string* no formato `"SnnAmm"` através de `makeFederateName()`, e a decodifica com
`parseFederateName()`. É justamente isso que permite ao código comum de `interop` indexar
NIBs **sem conhecer o esquema de endereçamento do DIS**. O mesmo vale para
`makeFederationName()`, que transforma o *exercise ID* em `"Ennn"`.

As tabelas têm tamanho fixo, definido por `MIXR_CONFIG_MAX_NETIO_ENTITIES` (padrão 5000) —
daí não haver alocação durante o laço.

### `inputFrame(dt)` e `outputFrame(dt)`

```cpp
// src/interop/common/NetIO.cpp
void NetIO::inputFrame(const double)
{
   if (isNetworkInitialized()) {
      netInputHander();     // virtual puro: a subclasse le e despacha os PDUs
      processInputList();   // virtual puro: aplica o estado aos IPlayers
      cleanupInputList();   // descarta NIBs vencidos
   }
}
```

`cleanupInputList()` remove um NIB quando o tempo desde o seu último *update* passa de
`maxAge`, **ou** quando o *player* associado já está em `DELETE_REQUEST`. A causalidade é
essa: **o `DELETE_REQUEST` é uma ENTRADA da decisão de remover, não uma consequência dela**.

`outputFrame(dt)` tem dois passos — `updateOutputList()` e `processOutputList()` — e é o
segundo que, para cada NIB de saída, aciona em ordem os três fabricantes de mensagem:
`munitionDetonationMsgFactory()`, `entityStateManager()` e `weaponFireMsgFactory()`. **O
estado que eles serializam vem do `SynchronizedState` do *player***, e não de uma leitura
direta — é isso que permite à *thread* de rede trabalhar sem correr atrás da *thread* de
tempo crítico.

## 27.3 `interop/dis` — `NetIO` e tipos de PDU

`dis::NetIO` estende `interop::NetIO`. **O seu nome de fábrica é `"DisNetIO"`** — não
`NetIO`.

```
( DisNetIO
    siteID:        1         // padrao: 1
    applicationID: 1         // padrao: 1
    exerciseID:    1         // padrao: 1;  ZERO funciona como curinga

    // Dois handlers separados: DIS envia e recebe por sockets distintos
    netInput:  ( UdpMulticastHandler multicastGroup: "225.0.0.1" port: 3000
                 localPort: 3000 )
    netOutput: ( UdpMulticastHandler multicastGroup: "225.0.0.1" port: 3000 )
)
```

O *slot* `version` aceita a **faixa 0–7**, e não apenas "5, 6 ou 7": os valores nomeiam
revisões do padrão (`VERSION_1278`, `VERSION_1278_1`, `VERSION_1278_1A`, `VERSION_7`…), com
**padrão 6 (`VERSION_1278_1A`)**.

### Tipos de PDU efetivamente despachados

| Tipo | PDU | Conteúdo |
|---:|---|---|
| 1 | Entity State | posição, velocidade, orientação, aparência |
| 2 | Fire | arma disparada (origem, destino, munição) |
| 3 | Detonation | detonação (posição, resultado) |
| 13 | Start/Resume | gerência de exercício |
| 14 | Stop/Freeze | idem |
| 15 | Acknowledge | idem |
| 16 | Action Request | idem |
| 18 | Data Query | idem |
| 20 | Data | troca de dados numerados |
| 22 | Comment | texto livre |
| 23 | Electromagnetic Emission | emissão de radar/*jammer* |
| 25 | Transmitter | rádio transmitindo |
| 26 | Signal | conteúdo de áudio/dados do rádio |
| 56 | Action Request-R | variante confiável (*reliable*) |
| 57 | Action Response-R | idem |

Há ainda o tratamento de IFF/ATC-NAVAIDS, e qualquer outro tipo cai em `processUserPDU()`,
o ponto de extensão da aplicação.

**ARMADILHA — Designator e Receiver não são tratados.** O PDU de *Designator* (24) tem uma
`struct` declarada em `pdu.hpp`, mas **nenhum caso de despacho** — ao chegar, cai em
`processUserPDU()`. E o PDU de *Receiver* (27) **não tem sequer `struct`**: não existe no
fonte.

### Entity State PDU — serialização

```cpp
// src/interop/dis/Nib_entity_state.cpp (simplificado)
// Posicao: ECEF, double 64 bits -- precisao centimetrica em qualquer ponto.
// geocPos vem do SynchronizedState do player, nao de uma leitura direta.
pdu->entityLocation.X_coord = geocPos[base::nav::IX];
pdu->entityLocation.Y_coord = geocPos[base::nav::IY];
pdu->entityLocation.Z_coord = geocPos[base::nav::IZ];

// Orientacao: angulos de Euler, float 32 bits (~0.003 graus de precisao).
// ATENCAO a ordem: o indice 0 e PHI (rolagem), nao PSI.
pdu->entityOrientation.phi   = static_cast<float>(geocAngles[base::nav::IPHI]);
pdu->entityOrientation.theta = static_cast<float>(geocAngles[base::nav::ITHETA]);
pdu->entityOrientation.psi   = static_cast<float>(geocAngles[base::nav::IPSI]);

// appearance: bitfield de 32 bits, montado direto no PDU
pdu->appearance = 0x0;
// bits 3-4: dano (0=intacto, 1=leve, 2=moderado, 3=destruido)
pdu->appearance |= (damageState & 0x3) << 3;
// bits 5-6: fumaca
pdu->appearance |= (smokeState  & 0x3) << 5;
// bit 15: em chamas  (FLAMES_BIT = 0x00008000)
if (isOnFire) pdu->appearance |= FLAMES_BIT;
// bits 16-19: postura (LifeForm::ActionState)
if (lf != nullptr) {
    unsigned int bits {1};  // padrao: UPRIGHT_STANDING
    // ... (15 posturas mapeadas, bits 1 a 15)
    pdu->appearance |= (bits << 16);
}

// Byte swap CONDICIONAL: so quando o host nao e big-endian.
// Num host big-endian a conversao seria uma inversao indevida.
if (base::NetHandler::isNotNetworkByteOrder()) pdu->swapBytes();
```

`netInputHander()` — **este é o nome real, com a coquilha do próprio fonte** — lê os PDUs do
*socket* em lotes e aplica **dois filtros** antes de despachar por `PDUType`:

1. **O exercício**: `exerciseID` zero funciona como **curinga** e aceita tudo; qualquer
   outro valor descarta os PDUs de exercícios diferentes.
2. **REGRA — o filtro de auto-eco.** Cada caso de despacho descarta os PDUs cujo par
   *site*+*aplicação* seja o **seu próprio**. Sem isso, um *host* em *multicast* receberia
   de volta tudo o que enviou e criaria **cópias fantasmas dos seus próprios *players***. É
   uma única linha por caso, e é o que faz o *multicast* ser utilizável.

Para um *Entity State PDU* de entidade desconhecida, `processEntityStatePDU()` cria o NIB de
entrada com `createNewInputNib()` e o insere com `addNib2InputList()`. **O *player*
correspondente não nasce aí**: quem o cria é `NetIO::createIPlayer()`, mais adiante, clonando
o protótipo do `Ntm` que casou.

### Fire e Detonation PDUs

Quando um *player* local lança uma arma, `dis::Nib` gera um *Fire PDU* com o ID da arma, do
lançador e do alvo. Quando a arma detona, um *Detonation PDU* é gerado com posição de
impacto e código de resultado.

O "código de resultado" é o enumerado `AbstractWeapon::Detonation`, com **sete desfechos** —
e **só dois significam "acertou"**:

| Valor | Constante | Significado |
|---:|---|---|
| 0 | `DETONATE_OTHER` | desfecho não classificado |
| 1 | `DETONATE_ENTITY_IMPACT` | **impacto direto na entidade** |
| 2 | `DETONATE_ENTITY_PROXIMATE_DETONATION` | **espoleta de proximidade junto ao alvo** |
| 3 | `DETONATE_GROUND_IMPACT` | impacto no solo |
| 4 | `DETONATE_GROUND_PROXIMATE_DETONATION` | detonação próxima ao solo |
| 5 | `DETONATE_DETONATION` | autodestruição / fim de tempo de voo |
| 6 | `DETONATE_NONE` | ainda não detonou — **é o valor inicial** |

O valor de partida de `results` é `DETONATE_NONE`, e **não** `DETONATE_OTHER`: uma arma em
voo tem um resultado que significa explicitamente "ainda não há resultado". Os códigos 1 e 2
são os que a lógica de dano trata como acerto; 3, 4 e 5 são desfechos sem vítima.

### Electromagnetic Emission PDU e o `EmissionPduHandler`

O PDU 23 é despachado, mas **quem o constrói e o interpreta não é o `Nib` nem o `NetIO`**: é
uma classe configurável em EDL, listada no *slot* `emissionPduHandlers` do `dis::NetIO`.

A razão é que **não existe uma tradução única entre um radar MIXR e um sistema emissor
DIS**. O padrão identifica emissores por um *emitter name* numérico — um código de catálogo
— e cada exercício adota a sua própria convenção. Um `EmissionPduHandler` é essa convenção,
escrita como objeto:

```
emissionPduHandlers: {
    ( EmissionPduHandler
        emitterName:     1105        -- codigo DIS deste emissor
        emitterFunction: 1           -- funcao (busca/aquisicao/rastreio...)
        sensor:  ( Radar   typeId: "APG-68" )  -- modelo LOCAL correspondente
        antenna: ( Antenna )         -- antena a clonar na entrada
        defaultOut: true             -- usado quando nada mais casa (saida)
    )
    ( EmissionPduHandler
        emitterName: 0
        defaultIn:   true            -- captura toda emissao nao reconhecida
    )
}
```

A busca do *handler* certo é **assimétrica** — duas sobrecargas de
`findEmissionPduHandler()`, cada uma com critério diferente:

| Sentido | Recebe | Casa por |
|---|---|---|
| Saída (local → rede) | `models::RfSensor*` | `typeId` igual ao do sensor-modelo |
| Entrada (rede → local) | `EmissionSystem*` | `emitterName` igual ao do PDU |

Faz sentido: ao publicar, sabe-se qual é o radar local e procura-se o código; ao receber,
tem-se o código e procura-se o radar. Se nenhum *handler* casar, faz-se segunda varredura à
procura de um marcado com `defaultOut` ou `defaultIn`; se nem isso houver, a função devolve
`nullptr` e **a emissão simplesmente não atravessa a rede**.

**ARMADILHA — o casamento de saída é por `typeId`, não por classe.**
`isMatchingRfSystemType()` compara `std::strcmp` entre o `getTypeId()` do sensor-modelo
declarado no *slot* `sensor` e o do radar do *player*. **Não há `dynamic_cast`, não há
comparação de nome de classe — é comparação de *strings*.** Duas consequências: o `typeId`
do `sensor` no *handler* precisa ser idêntico caractere a caractere; e sem nenhum *handler*
com `defaultOut`, um radar cujo `typeId` ninguém previu **emite localmente sem nunca
aparecer na rede — silenciosamente**.

Casado o *handler*, os *slots* `sensor` e `antenna` ganham o **segundo papel**: na entrada,
eles são **protótipos a clonar**. Ao receber uma emissão de uma entidade remota, o *handler*
instancia cópias desses modelos e as pendura no *player* de entrada — que passa a ter um
radar plausível, com diagrama de antena e tudo, embora quem o simula esteja noutra máquina.

## 27.4 `EntityType` e o mapeamento por `Ntm`

```cpp
// resumo de include/mixr/interop/dis/structs.hpp
struct EntityType {
    uint8_t  kind;        // plataforma, municao, lifeform, ...
    uint8_t  domain;      // terra, ar, mar, subsuperficie, espaco
    uint16_t country;     // codigo de pais SISO
    uint8_t  category;    // depende de kind+domain (caca, ataque, ...)
    uint8_t  subcategory;
    uint8_t  specific;
    uint8_t  extra;       // configuracao especifica

    // operator==: ZERO funciona como CURINGA em qualquer dos quatro
    // campos finais -- os tres primeiros tem de bater EXATAMENTE.
    bool operator==(const EntityType& a) const {
        return kind == a.kind && domain == a.domain && country == a.country
            && (category    == a.category    || category    == 0 || a.category    == 0)
            && (subcategory == a.subcategory || subcategory == 0 || a.subcategory == 0)
            && (specific    == a.specific    || specific    == 0 || a.specific    == 0)
            && (extra       == a.extra       || extra       == 0 || a.extra       == 0);
    }
};
```

### `Ntm` — um PROTÓTIPO, não uma linha de tabela

```cpp
// include/mixr/interop/common/Ntm.hpp
class Ntm : public base::Object {
    DECLARE_SUBCLASS(Ntm, base::Object)
public:
    // slot "template" <Player>
    const models::Player* getTemplatePlayer() const  { return tPlayer; }
};
```

Ao chegar uma entidade nova da rede, o framework localiza o `Ntm` correspondente e **clona o
seu *player* protótipo** para criar o *IPlayer* (*Interoperability Player*) que entra na
lista da simulação. **O que se configura não é "que classe instanciar", e sim "qual
aeronave, já pronta, copiar"** — com assinatura, sensores, dinâmica e tudo mais que o
protótipo tiver.

**POR QUÊ protótipo em vez de nome de classe.** Uma entidade DIS remota precisa de mais do
que uma classe: precisa de um RCS plausível, de um modelo de dinâmica para o *dead
reckoning* e de um tipo declarado. Clonar um protótipo declarado em EDL resolve tudo isso de
uma vez, e mantém a decisão nas mãos de quem escreve o cenário.

### Duas listas, não um mapa bidirecional

O `NetIO` mantém **duas** listas independentes, alimentadas por dois *slots* distintos —
`inputEntityTypes` e `outputEntityTypes`. Não há mapa único percorrido nos dois sentidos,
porque **as duas direções fazem perguntas diferentes**: a de entrada casa *sete números*
contra protótipos; a de saída casa *uma classe C++ mais uma string de tipo* contra códigos.

```
( DisNetIO
    // ... siteID, applicationID, exerciseID, netInput, netOutput ...

    // Rede -> simulacao: que player criar para cada tipo DIS recebido
    inputEntityTypes: {
        ( DisNtm
            disEntityType: [ 1 2 225 1 1 1 0 ]   // kind domain country cat sub spec extra
            template: ( Aircraft
                type: "F-16C"
                signature: ( SigConstant rcs: ( dB 5.0 ) )
            )
        )
        ( DisNtm
            disEntityType: [ 1 1 225 1 1 0 0 ]
            template: ( Tank type: "M1A2" )
        )
    }

    // Simulacao -> rede: que codigo DIS emitir para cada player local
    outputEntityTypes: {
        ( DisNtm
            disEntityType: [ 1 2 225 1 1 1 0 ]
            template: ( Aircraft type: "F-16C" )
        )
    }
)
```

Na prática essas listas são longas e vivem em arquivos à parte, incluídos no cenário — o
próprio código-fonte cita `DisIncomingEntityTypes.epp` e `DisOutgoingEntityTypes.epp`.

**A busca de ENTRADA: uma árvore de sete níveis.** Casar sete números contra centenas de
protótipos com varredura linear seria caro a cada PDU recebido. O DIS resolve com uma árvore
de decisão — os `NtmInputNode` — em que cada nível discrimina por um campo do `EntityType`,
na ordem *kind* → *domain* → *country* → *category* → *subcategory* → *specific* → *extra*.

**A busca de SAÍDA: melhor casamento por classe e prefixo de tipo.** A direção contrária não
pode ser uma árvore, porque a chave é um objeto C++. A regra tem duas partes: a **classe** do
protótipo tem de bater com a do *player* (ou ser uma classe base dele), **e** a *string* de
tipo do protótipo tem de ser **prefixo** da do *player*. Entre os que casam, vence o mais
específico.

Para um `Aircraft` de tipo `"F-16C"`:

| Protótipo declarado | Casa? | Por quê |
|---|---|---|
| `( Aircraft type: "F-16C" )` | **sim (melhor)** | exato |
| `( Aircraft type: "F-16" )` | sim | prefixo |
| `( AirVehicle type: "F-16C" )` | sim | classe base, tipo exato |
| `( AirVehicle type: "F-16" )` | sim (pior) | classe base, prefixo |
| `( Aircraft type: "F-16A" )` | não | *string* diverge |
| `( Aircraft type: "F-16C1" )` | não | mais longa que a do alvo |
| `( Ship type: "F-16C" )` | não | classe incompatível |

**ARMADILHA — sem `Ntm` de entrada, a rede é ignorada em SILÊNCIO.** Uma entidade cujo tipo
não case com nenhum `Ntm` da lista de entrada **não é criada**. Não vira um *player*
genérico, não gera aviso: é simplesmente descartada. E o caso extremo é o mais fácil de
cair: **sem o *slot* `inputEntityTypes`, TODAS as entidades da rede são ignoradas** — a
simulação conecta, recebe PDUs e não mostra ninguém. Há ainda um segundo filtro depois desse,
o *slot* `maxEntityRange`, que descarta entidades novas além de uma distância.

### Aparência e estado de dano

`dis::Nib` mantém o campo `appearance` atualizado a cada quadro a partir do estado do
*player*: nível de dano (0–3), fumaça (0–3), chamas, mobilidade comprometida, potência
comprometida e tipo de camuflagem. Para `LifeForm`, os bits 16–19 mapeiam diretamente para
`ActionState`. Receptores DIS conformes usam esses bits para renderizar o estado visual
correto.
---

# 28. SISTEMA DE COMPILAÇÃO: MESON E CONAN 2

A integração do MIXR num projeto externo envolve três decisões interdependentes: gerência de
dependências, descrição do *build* e ligação das bibliotecas. Este fork adota **Conan 2**
para dependências e **Meson** para a descrição do *build*, com a comunicação entre eles feita
por `PkgConfigDeps` e `MesonToolchain`.

## 28.1 Compilando o próprio MIXR

O repositório do MIXR é ele próprio um pacote Conan construído por Meson. O `Makefile` da
raiz é um invólucro fino com quatro alvos:

```bash
make configure   # conan install + meson setup (buildtype debug, prefix ../dist)
make build       # meson compile -C ./build
make install     # meson install -C ./build
make package     # conan create, em Debug e Release
```

```python
# meson.build da raiz
project('mixr', 'cpp',
    version: '1.0.5',
    license: 'LGPL-3.0',
    default_options : [
        'default_library=shared',
        'c_std=c11',
        'cpp_std=c++11',        # <-- o MIXR compila como C++11
        'c_args=-fPIC',
        'cpp_args=-fPIC',
    ],
)

add_project_arguments('-fpermissive', language : 'cpp')   # <-- afrouxa a conformidade
```

- **`cpp_std=c++11`**: apesar do uso extensivo de inicialização uniforme com chaves e de
  `nullptr` no código, o MIXR é construído contra o padrão C++11.
- **`-fpermissive`**: rebaixa a avisos erros de conformidade — é o que permite ao
  código-fonte, herdado de base bem mais antiga, compilar sem revisão completa.

**Nenhum dos dois impede que uma aplicação consumidora use padrão mais recente** (o exemplo
usa C++17), já que a fronteira entre elas são cabeçalhos e símbolos, não unidades de
tradução compartilhadas.

### Dependências externas

| Dependência | Versão | Usada por |
|---|---|---|
| `jsbsim` | 1.1.11 | `models` — `JSBSimModel` |
| `protobuf` | 3.21.12 | `recorder` — serialização de `DataRecord` |
| `openrti` | (*commit* fixo) | declarada para HLA, que **não** é compilado |

(`zlib` entra por transitividade do protobuf.)

O `package_info()` do `conanfile.py` expõe o resultado como **nove componentes Conan**
(`base`, `interop_common`, `interop_dis`, `linearsystem`, `linkage`, `models`, `recorder`,
`simulation`, `terrain`), e o `pkg.generate()` da raiz produz um **`mixr.pc` agregador** que
lista as nove bibliotecas na ordem correta de ligação.

## 28.2 Estrutura do `conanfile.py` de um projeto consumidor

```python
from conan import ConanFile
from conan.tools.meson import MesonToolchain
from conan.tools.gnu   import PkgConfigDeps

class MixrProjectConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = []  # gerado manualmente em generate()

    def requirements(self):
        self.requires("mixr/VERSION")
        # demais dependencias diretas do projeto

    def layout(self):
        # build/ e generators/ no MESMO diretorio
        self.folders.build      = "build"
        self.folders.generators = "build"

    def generate(self):
        # 1. Gera os arquivos .pc para cada biblioteca MIXR
        pc = PkgConfigDeps(self)
        pc.generate()

        # 2. Gera cross_file/native_file com flags de compilador
        tc = MesonToolchain(self)
        tc.generate()
```

Pontos críticos:

- **`requirements()`** — todas as dependências diretas do projeto são declaradas aqui. O
  MIXR arrasta consigo as suas; não é necessário declará-las explicitamente a menos que o
  projeto as use diretamente. O `jsbsim` é declarado com `transitive_headers=True`, de modo
  que os seus cabeçalhos ficam visíveis a quem inclua `JSBSimModel.hpp`.
- **`layout()`** — `build` e `generators` devem apontar para o **mesmo diretório**. O Meson
  espera que o `native_file` e os arquivos `.pc` gerados pelo Conan estejam no mesmo local
  que a pasta de build configurada.
- **`generate()`** — `PkgConfigDeps` gera um `.pc` por biblioteca MIXR instalada;
  `MesonToolchain` gera o `conan_meson_native.ini` (ou `cross`).

## 28.3 Descrição do *build* com Meson

```python
project('poc-mixr', 'cpp',
    version         : '0.1.0',
    default_options : ['cpp_std=c++17'])

# Uma UNICA declaracao cobre todas as bibliotecas MIXR
# expostas pelo arquivo .pc gerado pelo Conan
mixr_dep = dependency('mixr', method : 'pkg-config')

executable('poc-mixr',
    sources  : ['main.cpp'],
    dependencies : [mixr_dep])
```

**POR QUÊ uma única chamada a `dependency()`** em vez de uma por módulo: o arquivo `.pc`
gerado pelo Conan para o pacote `mixr` já lista todas as `Requires:` das sub-bibliotecas na
**ordem correta de ligação**. Declarar cada módulo separadamente seria redundante e
quebraria a precedência de ligação em plataformas que exigem ordem explícita de `-l`.

## 28.4 Fluxo de configuração e compilação

Conan e Meson **não se conhecem**. O que os liga são **dois artefatos** gerados pelo
primeiro e lidos pelo segundo:

```
conanfile.py  (requirements())
      |
   conan install    -- baixa/compila as dependencias
      |
      +--> *.pc                       -- ONDE estao cabecalhos e bibliotecas
      |         \--> chega ao Meson por  PKG_CONFIG_PATH  (variavel de ambiente)
      |
      +--> conan_meson_native.ini     -- COM QUE compilador e flags
                \--> chega ao Meson por  --native-file  (argumento de linha)
      |
   meson setup
      |
   meson compile
```

```bash
# 1. Instala as dependencias e gera os arquivos de integracao
conan install . --output-folder=build --build=missing

# 2. Configura o Meson apontando para o native file do Conan
#    e para o diretorio onde os .pc foram gerados
PKG_CONFIG_PATH="$(pwd)/build" \
meson setup build \
    --native-file build/conan_meson_native.ini \
    --buildtype=release

# 3. Compila
meson compile -C build

# 4. (opcional) Roda os testes
meson test -C build
```

**ARMADILHA — esquecer o `PKG_CONFIG_PATH` dá um erro ENGANOSO.** Sem a variável, o
`pkg-config` procura os `.pc` apenas nos caminhos do sistema — e o Conan os gerou dentro de
`build/`. O Meson então falha com `Dependency "mixr" not found`, que se lê naturalmente como
"o MIXR não está instalado". **Não é isso**: o MIXR está compilado e o `.pc` existe; o que
falta é dizer onde procurar. O sintoma é idêntico ao de uma dependência genuinamente
ausente, e é a **primeira coisa a conferir antes de reinstalar qualquer coisa**.

**REGRA — o `layout()` do `conanfile.py` não é decorativo.** `folders.build` e
`folders.generators` devem apontar para o **mesmo** diretório: os dois artefatos gerados
precisam estar onde `meson setup` vai procurá-los — um pela variável de ambiente, o outro
por caminho relativo. Separá-los faz uma das duas metades do elo se perder, e o erro
resultante aponta para o lado errado do problema.

## 28.5 Dependências circulares internas: o caso `models/`

O módulo `models/` apresenta **dependências circulares internas**: `Player` referencia
`System`; `System` usa ponteiros de volta para o `Player`; `Navigation` e o `StoresMgr`
dependem de ambos.

A solução adotada é compilar **todos esses arquivos num único alvo de biblioteca** em vez de
incrementalmente, eliminando a circularidade do ponto de vista do enlace. Toda a pasta
`src/models/` é compilada como um único `shared_library('mixr_models', ...)`.

**Para projetos que estendem o MIXR com novas subclasses** (por exemplo, um `DynamicsModel`
personalizado), a mesma regra se aplica: **nunca compilar a extensão num alvo separado que
ligue contra `mixr_models` se a extensão e `mixr_models` tiverem dependências mútuas**. O
arquivo da extensão deve entrar na mesma lista `sources` que os demais arquivos de
`models/`.

Além disso, **arquivos específicos de plataforma devem sempre ser incluídos ao lado do
arquivo genérico correspondente**; omiti-los gera erros de símbolo não resolvido que só
aparecem no enlace final. No MIXR esses arquivos ficam todos em `base` e `linkage`. **O
módulo `models` não tem código de plataforma próprio.**

```python
# src/models/meson.build (estrutura real)
source_files = [
    './WorldModel.cpp',
    './factory.cpp',
    './player/Player.cpp',
    './system/System.cpp',
    './navigation/Navigation.cpp',
    # ... 107 arquivos no total, em uma unica lista ...
]

mixr_models = library(
    'mixr_models',
    source_files,
    include_directories: public_headers,
    dependencies: [
        mixr_base_dep,
        mixr_simulation_dep,
        mixr_terrain_dep,
        jsbsim_dep,
    ],
    link_args : [ static_flags ],
    install : true,
)

mixr_models_dep = declare_dependency(link_with : mixr_models)
pkg.generate(mixr_models, name : 'mixr-models')
```

A ordem dos arquivos dentro de `source_files` **não tem significado** — `factory.cpp`
aparece perto do início da lista, e isso é irrelevante: cada unidade de tradução é compilada
isoladamente e as dependências entre elas se resolvem no enlace.

**Qual biblioteca ligar depende do arquivo EDL.** Um cenário com `QuadMap` exige
`mixr_terrain`; com `DataRecorder`, exige `mixr_recorder` e, por transitividade, *protobuf*;
com `dis::NetIO`, exige `mixr_interop_dis`. **Trocar uma linha de EDL pode mudar o que se
liga.**

---

# 29. APLICAÇÃO COMPLETA — DO EDL AO EXECUTÁVEL

Cenário: **uma interceptação**. Uma aeronave de caça com radar e mísseis, seguindo uma rota
sob autopiloto, detecta um alvo que voa em rota de colisão. Há terreno, gravação de dados e
rede DIS.

## 29.1 A árvore do cenário

```
                        Station
                           |
          +----------------+----------------+
          |                |                |
      DisNetIO         WorldModel      DataRecorder
       (rede)     (ponto de ref.,        (-> arquivo)
                     terreno)
                          |
                +---------+---------+
                |                   |
            ownship               alvo
           (Aircraft)          (Aircraft)
                |
     +----------+----------+----------+
     |          |          |          |
 LaeroModel   Radar    Autopilot  AirTrkMgr
             (+Antenna)
```

**Duas relações diferentes aparecem nesse desenho, e confundi-las é o erro mais comum de
quem escreve o primeiro cenário:**

- A `Station` guarda os seus subsistemas em ***slots* nomeados** (`simulation`,
  `dataRecorder`, `networks`, `ioHandler`, `igHosts`).
- Um *player* guarda os seus subsistemas em **`components`** — e é **por tipo, não por
  nome**, que o `Player` os encontra depois.

## 29.2 `intercepcao.edl` — o cenário completo

```
( Station

  //-------------------------------------------------------------------------
  // Taxas das threads
  //-------------------------------------------------------------------------
  tcRate:      50        // fisica e sensores a 50 Hz
  tcPriority:  0.8       // SCHED_FIFO no Linux
  bgRate:      10        // tarefas de fundo a 10 Hz
  netRate:     20        // rede a 20 Hz

  ownship: "falcao1"     // de quem e a perspectiva da aplicacao

  //-------------------------------------------------------------------------
  // O executive, situado no globo
  //-------------------------------------------------------------------------
  simulation: ( WorldModel

      // Ponto de referencia: centro da gaming area (Vale do Paraiba)
      latitude:  ( LatLon direction: s  degrees: 23  minutes: 10 )
      longitude: ( LatLon direction: w  degrees: 45  minutes: 50 )
      gamingAreaRange: ( KiloMeters 150 )

      earthModel: wgs84
      gamingAreaUseEarthModel: true

      // Terreno: quatro tiles em grade 2x2
      terrain: ( QuadMap
          components: {
              sw: ( DtedFile  path: "/data/terrain"  file: "s24w046.dt1" )
              se: ( DtedFile  path: "/data/terrain"  file: "s24w045.dt1" )
              nw: ( DtedFile  path: "/data/terrain"  file: "s23w046.dt1" )
              ne: ( DtedFile  path: "/data/terrain"  file: "s23w045.dt1" )
          }
      )

      players: {

          //-----------------------------------------------------------------
          // OWNSHIP -- o cacador
          //-----------------------------------------------------------------
          falcao1: ( Aircraft
              type: "F-16C"
              side: blue

              initLatitude:   ( LatLon direction: s  degrees: 23  minutes: 30 )
              initLongitude:  ( LatLon direction: w  degrees: 46  minutes: 10 )
              initAlt:        ( Feet 25000 )
              initHeading:    ( Degrees 45 )
              initVelocityKts: 420

              // Assinatura radar: constante, 5 m2
              signature: ( SigConstant  rcs: ( SquareMeters 5.0 ) )

              components: {

                  // --- Dinamica de voo ---
                  dinamica: ( LaeroModel )

                  // --- Cadeia RF: Radar contem a Antenna ---
                  radar1: ( Radar
                      // O radar acha o gerente de pistas PELO NOME:
                      trackManagerName: "gerentePistas"
                      antennaName:      "antena1"

                      frequency:  ( GigaHertz 10 )
                      powerPeak:  ( KiloWatts 100 )
                      bandwidth:  ( MegaHertz 1 )
                      ranges:     [ 20 40 80 160 ]
                      initRangeIdx: 3

                      components: {
                          antena1: ( Antenna
                              polarization: vertical
                              gain: 8000.0
                              scanMode:  horizontal
                              scanWidth: ( Degrees 120 )
                              numBars:   4
                          )
                      }
                  )

                  // --- Gerente de pistas ---
                  gerentePistas: ( AirTrkMgr
                      maxTracks:    20
                      maxTrackAge:  3.0
                      firstTrackId: 1000
                      alpha: 1.0
                      beta:  0.5      // gamma nao teria efeito
                  )

                  // --- Navegacao e rota ---
                  nav1: ( Navigation
                      route: ( Route
                          autoSequence:    true
                          autoSeqDistance: ( NauticalMiles 2.0 )
                          to: 1
                          components: {
                              wp1: ( Steerpoint
                                  latitude:  ( LatLon direction: s degrees: 23 minutes: 20 )
                                  longitude: ( LatLon direction: w degrees: 46 minutes:  0 )
                                  altitude:  ( Feet 25000 )
                                  airspeed:  420.0
                              )
                              wp2: ( Steerpoint
                                  latitude:  ( LatLon direction: s degrees: 23 minutes:  0 )
                                  longitude: ( LatLon direction: w degrees: 45 minutes: 40 )
                                  altitude:  ( Feet 28000 )
                                  airspeed:  420.0
                              )
                          }
                      )
                  )

                  // --- Autopiloto ---
                  // Os limites NAO tem default util: precisam ser declarados.
                  piloto: ( Autopilot
                      navMode:            true
                      altitudeHoldMode:   true
                      velocityHoldMode:   true
                      maxBankAngle:       30.0
                      maxRateOfTurnDps:    3.0
                      maxPitchAngle:      15.0
                      maxClimbRateMps:    10.0
                      maxAcceleration:     5.0
                  )

                  // --- Armamento ---
                  // "StoresMgr" constroi um SimpleStoresMgr.
                  sms: ( StoresMgr
                      stores: {
                          1: ( Missile  type: "AIM-120C"  maxSpeed: 1400.0 )
                          3: ( Missile  type: "AIM-120C"  maxSpeed: 1400.0 )
                      }
                  )
              }
          )

          //-----------------------------------------------------------------
          // ALVO -- rota de colisao, sem sensores
          //-----------------------------------------------------------------
          alvo1: ( Aircraft
              type: "Su-27"
              side: red

              initLatitude:   ( LatLon direction: s  degrees: 22  minutes: 50 )
              initLongitude:  ( LatLon direction: w  degrees: 45  minutes: 30 )
              initAlt:        ( Feet 26000 )
              initHeading:    ( Degrees 225 )
              initVelocityKts: 450

              signature: ( SigConstant  rcs: ( dB 8.0 ) )   // 8 dBsm

              components: {
                  // RacModel: convergencia de primeira ordem
                  dinamica: ( RacModel
                      cmdAltitude: ( Feet 26000 )
                      cmdHeading:  ( Degrees 225 )
                      cmdSpeed:    450.0
                  )
              }
          )
      }
  )

  //-------------------------------------------------------------------------
  // Gravador -- note o nome de fabrica do FileWriter
  //-------------------------------------------------------------------------
  dataRecorder: ( DataRecorder
      eventName:  "intercepcao"
      caseNum:    1
      missionNum: 1
      outputHandler: ( RecorderFileWriter
          pathname: "/data/gravacoes"
          filename: "intercepcao.rcd"
      )
  )

  //-------------------------------------------------------------------------
  // Rede DIS
  //-------------------------------------------------------------------------
  networks: {
      dis1: ( DisNetIO
          siteID:        10
          applicationID:  1
          exerciseID:    17

          netInput:  ( UdpMulticastHandler
              multicastGroup: "225.0.0.17"  port: 3000  localPort: 3000 )
          netOutput: ( UdpMulticastHandler
              multicastGroup: "225.0.0.17"  port: 3000 )

          // Sem inputEntityTypes, TODA entidade da rede e ignorada.
          inputEntityTypes: {
              ( DisNtm
                  disEntityType: [ 1 2 225 1 1 1 0 ]
                  template: ( Aircraft type: "F-16C" )
              )
          }
          outputEntityTypes: {
              ( DisNtm
                  disEntityType: [ 1 2 225 1 1 1 0 ]
                  template: ( Aircraft type: "F-16C" )
              )
          }
      )
  }
)
```

## 29.3 `main.cpp` — a aplicação mínima completa

```cpp
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/util/system_utils.hpp"
#include "mixr/simulation/Station.hpp"

#include "mixr/base/factory.hpp"
#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/interop/dis/factory.hpp"
#include "mixr/recorder/factory.hpp"
#include "mixr/linkage/factory.hpp"

#include <cstdlib>
#include <iostream>

namespace mixr {

//---------------------------------------------------------------------------
// 1. A FABRICA -- a ordem AQUI define a precedencia dos nomes.
//    Nao existe registro global: o encadeamento e este codigo.
//---------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
    base::Object* obj {};

    // Classes proprias da aplicacao viriam primeiro, para poder
    // sobrepor um nome de fabrica do framework:
    //   obj = minhaApp::factory(name);

    if (obj == nullptr) obj = models::factory(name);
    if (obj == nullptr) obj = dis::factory(name);
    if (obj == nullptr) obj = recorder::factory(name);
    if (obj == nullptr) obj = linkage::factory(name);
    if (obj == nullptr) obj = simulation::factory(name);
    if (obj == nullptr) obj = terrain::factory(name);
    if (obj == nullptr) obj = base::factory(name);

    return obj;
}

//---------------------------------------------------------------------------
// 2. CARGA -- construir a arvore a partir do arquivo, e CONFERIR os erros
//---------------------------------------------------------------------------
simulation::Station* carrega(const char* const arquivo)
{
    int nErros {};
    base::Object* raiz { base::edl_parser(arquivo, factory, &nErros) };

    // Um ponteiro nao-nulo NAO significa que o arquivo estava correto:
    // slots mal escritos apenas incrementam o contador.
    if (nErros > 0) {
        std::cerr << "erros na configuracao: " << nErros << std::endl;
        return nullptr;
    }

    // O parser devolve um Pair quando o arquivo comeca com "nome: ( ... )",
    // e o objeto nu quando comeca com "( ... )". Aceitar os dois:
    if (const auto par = dynamic_cast<base::Pair*>(raiz)) {
        raiz = par->object();
    }

    const auto station = dynamic_cast<simulation::Station*>(raiz);
    if (station == nullptr) {
        std::cerr << "a raiz do arquivo nao e uma Station" << std::endl;
        return nullptr;
    }
    return station;
}

}  // namespace mixr

//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    const char* arquivo { (argc > 1) ? argv[1] : "intercepcao.edl" };

    mixr::simulation::Station* station { mixr::carrega(arquivo) };
    if (station == nullptr) return EXIT_FAILURE;

    //-----------------------------------------------------------------------
    // 3. RESET -- condicoes iniciais. So DEPOIS disso a arvore esta viva.
    //    E aqui que o terreno e lido do disco e o pool de threads e criado.
    //-----------------------------------------------------------------------
    station->event(mixr::base::Component::RESET_EVENT);

    // A thread de tempo critico e criada EXPLICITAMENTE pela aplicacao --
    // ao contrario das de fundo e de rede.
    station->createTimeCriticalProcess();

    //-----------------------------------------------------------------------
    // 4. O LACO -- aqui so restam as tarefas de fundo: a thread TC ja roda.
    //-----------------------------------------------------------------------
    const double dt { 1.0 / 10.0 };          // 10 Hz de fundo
    for (;;) {
        station->updateData(dt);
        mixr::base::msleep(100);             // 100 ms
    }

    return EXIT_SUCCESS;
}
```

**As quatro responsabilidades de todo `main()` MIXR:**

1. **A fábrica é código, não configuração.** Não há registro global nem descoberta
   automática; **a ordem das linhas *é* a precedência**.
2. **`nErros` tem de ser conferido.** O *parser* devolve a árvore mesmo com dezenas de
   *slots* malsucedidos; ignorar o contador é subir uma simulação silenciosamente
   malconfigurada.
3. **O `RESET_EVENT` não é opcional.** É ele que carrega o terreno, monta a lista de
   *players* e cria o *pool* de *threads*. Antes dele, a árvore existe mas não está
   inicializada.
4. **A *thread* de tempo crítico é criada à mão.** Omitir `createTimeCriticalProcess()` é
   escolha válida — significa que a aplicação vai chamar `station->updateTC(dt)` ela mesma,
   de um laço próprio ou de uma interrupção externa.

## 29.4 `meson.build` da aplicação e comandos

```python
project('intercepcao', 'cpp',
    version         : '0.1.0',
    default_options : ['cpp_std=c++17'])

# Uma unica declaracao cobre as nove bibliotecas do MIXR:
# o .pc gerado pelo Conan ja as lista na ordem correta de ligacao.
mixr_dep = dependency('mixr', method : 'pkg-config')

executable('intercepcao',
    sources      : ['main.cpp'],
    dependencies : [mixr_dep],
    install      : true)
```

```bash
# 1. Dependencias e arquivos de integracao (gera os .pc e o native file)
conan install . --output-folder=build --build=missing

# 2. Configuracao do Meson, apontando para o que o Conan gerou
PKG_CONFIG_PATH="$(pwd)/build" \
meson setup build \
    --native-file build/conan_meson_native.ini \
    --buildtype=release

# 3. Compilacao
meson compile -C build

# 4. Execucao
./build/intercepcao intercepcao.edl
```

## 29.5 O que acontece a cada quadro (roteiro de leitura)

1. **Fase 0.** `LaeroModel` integra a posição do *ownship*; `RacModel` converge o alvo para
   o seu rumo e altitude comandados. Ambos escrevem via `setPosition()`, que sincroniza NED,
   ECEF e geodésicas.
2. **Fundo.** `RfSystem::updateData()` monta o `Tdb` com os alvos de interesse — e note que
   isso é a *thread* de fundo, **não a fase 1**.
3. **Fase 1.** `Antenna::rfTransmit()` calcula o ganho na direção do alvo, monta uma
   `Emission` com a perda de percurso já embutida e a entrega ao alvo.
4. **Fase 2.** O eco volta; `Radar::receive()` aplica a RCS e a segunda perda de percurso,
   compara com o limiar **em dB** e enfileira o relatório.
5. **Fase 3.** `Radar::process()` consolida os relatórios da varredura e chama
   `AirTrkMgr::newReport()`; o filtro α-β passa a manter a pista. `Autopilot::process()` lê a
   rota e comanda rumo, altitude e velocidade.
6. **Em paralelo.** A *thread* de rede publica o *Entity State PDU* do *ownship* quando o
   erro de *dead reckoning* passar de 3 m ou 3°, ou a cada 5 s. O gravador drena a sua fila
   e escreve o arquivo.

## 29.6 REGRA — o roteiro de depuração

Quando um cenário não se comporta como esperado, a ordem de investigação que poupa mais
tempo é esta:

1. **`nErros` do *parser* é zero?**
2. **O `RESET_EVENT` foi enviado?**
3. **Os subsistemas estão em `components`, e não em *slots* inventados?** (Um filho de tipo
   errado some sem aviso.)
4. **Os nomes cruzados batem?** `trackManagerName` e `antennaName` são resolvidos por
   *string*, e um erro de digitação neles **não produz erro de carga**.
5. **Ligue `enableMessageType: DEBUG` no nó suspeito** — a política desce por toda a
   subárvore.

## 29.7 Verificação automática de EDL contra o fonte

O manual acompanha um verificador (`scripts/check-edl.py`) que extrai do código-fonte três
coisas: os nomes de fábrica de todo `IMPLEMENT_*SUBCLASS`, a cadeia de herança de todo
`DECLARE_SUBCLASS`, e os *slots* de todo `BEGIN_SLOTTABLE`. Depois percorre todos os blocos
EDL e verifica, com a estrutura de aninhamento correta:

1. que cada `( Nome ... )` corresponde a um nome de fábrica que **existe** *e* está
   **registrado** em alguma `factory.cpp`;
2. que cada `nome:` é um *slot* da classe corrente ou de um ancestral — respeitando que
   nomes dentro de `{ }` são nomes de `Pair`, e **não** *slots*.

```bash
$ python3 scripts/check-edl.py
nomes de fabrica no fonte: 334   registrados: 224
blocos EDL no manual: 36

EDL: nenhuma violacao.
```

---

# 30. CATÁLOGO CONSOLIDADO DE ARMADILHAS

Índice de todos os comportamentos que contrariam a expectativa razoável, agrupados por
tema. Cada linha remete à seção onde o mecanismo é explicado.

## 30.1 Nomes e fábricas

| # | Armadilha |
|---|---|
| 1 | **Nome de fábrica ≠ nome de classe** em 56 classes. A fonte de verdade é o `IMPLEMENT_*SUBCLASS` no `.cpp`, não o comentário do cabeçalho. |
| 2 | **`( StoresMgr )` constrói um `SimpleStoresMgr`** — silenciosamente, com outro comportamento. `StoresMgr` registra-se como `"BaseStoresMgr"` e nem sequer é registrada. |
| 3 | **Operadores usam símbolos**: `( / 600.0 3.6 )`, não `( Divide ... )`. |
| 4 | **Cores em minúscula**: `( rgb ... )`, não `( Rgb ... )`. `( Color ... )` existe mas tem `EMPTY_SLOTTABLE`. |
| 5 | **Dois `FileReader`**: `base::FileReader` = `"FileReader"`; `recorder::FileReader` = `"RecorderFileReader"`. |
| 6 | **`UbfAgentTC` não é registrado** por nenhuma fábrica do MIXR. |
| 7 | **`BaseIoHandler` não é registrado E é abstrato** — nenhuma classe implementa `inputDevicesImpl()`/`outputDevicesImpl()`. |
| 8 | **Todo o módulo `linearsystem` não tem `factory.cpp`.** |
| 9 | **`Component` não é instanciável em EDL** (não está em `base/factory.cpp`). |

## 30.2 *Slots* e nomes de *slot*

| # | Armadilha |
|---|---|
| 10 | **`Terrain` usa `path`/`file`**, não `pathname`/`filename` (que são os nomes dos *acessores*). |
| 11 | **`Bomb` usa `fuzeTime`**, não `maxFuseTime`. |
| 12 | **`Ai2DiSwitch` usa `level`**, não `threshold`; o teste é `>=` (inclusive). |
| 13 | **`DiscreteInputFixed` usa `signal`** com identificadores `ON`/`OFF`, não booleano. |
| 14 | **Armas não têm *slot* `model`** — a identidade vem de `type`, herdado de `Player`. |
| 15 | **`Iff` não tem `mode5`**; tem `enableModeC` (altitude), que passa despercebido. |
| 16 | **`AnalogSignalGen` não tem *slot* de amplitude** — saída sempre em [−1, +1]. As ondas são `sine`, `cosine`, `square`, `saw`. |
| 17 | **`AnalogOutput` declara `table` mas NUNCA a aplica.** |
| 18 | **Um *slot* com tipo errado retorna `false` sem tentar a base** — vira `"error while setting slot name: ..."`. |

## 30.3 Valores padrão surpreendentes

| # | Armadilha |
|---|---|
| 19 | **`AbstractPlayer::mode` nasce `ACTIVE`**, não `INACTIVE`. (Exceção: `AbstractWeapon` sobrescreve para `INACTIVE`.) |
| 20 | **`Autopilot::maxBankAngleDegs` e `maxTurnRateDps` nascem ZERO** — o raio de curvatura vira infinito e `flyCRS()` nunca faz curva decente. **É obrigatório declará-los.** |
| 21 | **`AbstractWeapon::maxGimbal` nasce ZERO**, apesar de o comentário do cabeçalho anunciar 30°. |
| 22 | **`tsg` e `sobt` nascem em 9999,0 s** — o padrão **desliga** guiagem e queima de motor. |
| 23 | **`Nib::drNum` nasce `STATIC_DRM`** — sem *dead reckoning*. |
| 24 | **`TcpServerMultiple::backlog` nasce 1** — recusa a segunda conexão simultânea. |
| 25 | **`Gimbal::maxPlayersOfInterest` nasce 200**; `MAX_PLAYERS` (4000) é outra coisa e **não limita o *slot***. |

## 30.4 Falhas silenciosas

| # | Armadilha |
|---|---|
| 26 | **Filho de tipo errado em `components:` desaparece sem aviso** (`dynamic_cast` falha, objeto omitido, `isValid()` continua `true`). |
| 27 | **Seleção que não resolve congela a subárvore inteira** — imprime erro mas mantém a seleção ativa. |
| 28 | **`num_errors` do *parser* ignorado** = simulação sobe com *slots* no padrão. |
| 29 | **`QuadMap` ignora o 5º *tile*** sem aviso (`MAX_DATA_FILES` = 4). |
| 30 | **Sem `inputEntityTypes`, TODAS as entidades DIS são ignoradas.** |
| 31 | **`EmissionPduHandler` sem `defaultOut`**: radar emite localmente e nunca aparece na rede. |
| 32 | **`SimAgent` com `actorComponentName` inválido fica sem ator, inerte** — sem diagnóstico. |
| 33 | **Transições de `StateMachine` falham em silêncio** (`goTo()` para estado inexistente, `call()` com pilha cheia, `rtn()` com pilha vazia). |
| 34 | **`numTcThreads` rejeitado** numa máquina com menos CPUs → simulação monothread, sem explicação. |
| 35 | **Nomes cruzados por *string*** (`trackManagerName`, `antennaName`, `leadPlayerName`) não produzem erro de carga se estiverem errados. |

## 30.5 Semântica que engana

| # | Armadilha |
|---|---|
| 36 | **`freeze()` NÃO propaga** — escreve um `bool` e nada mais; cada subclasse precisa consultar `isFrozen()`. |
| 37 | **Eventos de simulação NÃO sobem** na árvore; só teclas (*token* ≤ `MAX_KEY_EVENT` = 999). |
| 38 | **`findByName("a.b.c")` não tem retrocesso**; e o *buffer* do primeiro segmento tem 128 bytes **sem verificação de limite**. |
| 39 | **`PairStream::findName()` devolve um CLONE** — o chamador deve `unref()`. |
| 40 | **`List::removeHead()` TRANSFERE posse** (não chama `unref()`); `remove(obj)` chama. |
| 41 | **`port`/`localPort` mudam de papel** conforme enviando ou recebendo; sem `localPort`, liga-se a `port`. |
| 42 | **Canais de dispositivo são 0-based; canais de `IoData` são 1-based.** |
| 43 | **`columns[lon][lat]`** — longitude primeiro. Trocar não gera erro de compilação. |
| 44 | **`FPW_DRM` não é "posição fixa"** — o *F* é da rotação; a entidade continua em linha reta. Congelar = `STATIC_DRM`. |
| 45 | **O sufixo é `_DRM`**, não prefixo: `RVW_DRM`, não `DRM_RVW`. |
| 46 | **`AirAngleOnlyTrkMgrPT`: PT = *perceived truth***, não *predictive tracking*. |
| 47 | **`Tdb` significa *Track Data Block* mas não tem relação com `Track`** — guarda candidatos, não pistas. |
| 48 | **`Sz1` não é um atraso z⁻¹** — expõe os coeficientes N1/N2/D1/D2 ao EDL. |
| 49 | **`PrintSelected` seleciona REGISTROS, não campos.** |
| 50 | **`Emission::setRange()`** (não o `Tdb`) é quem calcula `lossRng`. |

## 30.6 Funcionalidade ausente ou pela metade

| # | Armadilha |
|---|---|
| 51 | **`SecondOrderTf` lê memória NÃO INICIALIZADA** — `clearMemory()` zera `pa` duas vezes e nunca `pb`. Não instanciar (nem `Sz2`). |
| 52 | **`Limit11` está quebrado**: `LimitFunc(1,1)` satura tudo para 1,0. Usar `( Limit lower: -1.0 upper: 1.0 )`. |
| 53 | **Modo assíncrono do `IoHandler` DESLIGA a E/S** — a *thread* chama os métodos guardados pelo próprio portão. |
| 54 | **`Ins` e `Gps` não simulam nada** — `EMPTY_SLOTTABLE`, sem `updateData()`/`process()`. |
| 55 | **`Chaff`, `Flare` e `Decoy` são o mesmo código** — diferença exclusivamente semântica. |
| 56 | **`Sar` não usa terreno e gera padrão de teste** (`testImage()`); não existe `SAR_COMPLETE_EVENT`. |
| 57 | **`γ` (gamma) do `TrackManager` não tem efeito nenhum**; `β` só em `AirTrkMgr` e `GmtiTrkMgr`. |
| 58 | **Nenhuma arma consulta o `Designator`** — guiagem laser não está modelada. |
| 59 | **`Message` e `TargetData` não são usadas por ninguém.** |
| 60 | **`BoosterSpaceVehicle` é *stub* vazio.** |
| 61 | **HLA e RPR-FOM não são compilados** (`meson.build` comentado). |
| 62 | **PDUs Designator (24) e Receiver (27) não são tratados.** |
| 63 | **`navDR_utils` é código morto** — o *dead reckoning* que roda é o de `Nib`. |
| 64 | **Windows não é compilado** — fontes Linux listadas incondicionalmente. |
| 65 | **`Locus` não tem nenhum consumidor no fonte.** |

## 30.7 Desempenho e custo

| # | Armadilha |
|---|---|
| 66 | **As macros de gravação NÃO são custo zero** quando há gravador: os argumentos são avaliados mesmo com o evento filtrado. |
| 67 | **Só o caminho TC é instrumentado** — não existe `dataFrame()`. |
| 68 | **`Tdb` montado na *thread* de fundo** pode estar um ou dois quadros defasado. |
| 69 | **`SendData` guarda ponteiro cru** — chamar `empty()` ao alterar a árvore. |
| 70 | **Tabelas de `NetIO` usam `bsearch`** — O(log n), não O(1). |

## 30.8 Matemática e unidades

| # | Armadilha |
|---|---|
| 71 | **A equação do radar do MIXR não é a do livro-texto** — sem λ² e sem (4π)³. |
| 72 | **O ganho da antena é dividido por 10 antes da exponenciação** (10^(dB/10)). |
| 73 | **O limiar do radar é comparado em dB**, não em razão linear. |
| 74 | **`Angle` tem `Semicircles` como referência interna**, não radianos. |
| 75 | **Não existem `Knots`, `Mach` nem `RPM`** como classes — são métodos sobre m/s. |
| 76 | **DTED/SRTM usam sinal-magnitude**, não complemento de dois. |
| 77 | **SRTM: o `switch` é sobre o tamanho EXATO em bytes** — um byte a mais e o arquivo é rejeitado. |
| 78 | **Exceções de tabela lançadas por PONTEIRO**: `catch (ExpInvalidTable* e)`. |
| 79 | **`Δt` das fases: dividido por 4 na descida, multiplicado por 4 na chegada** — cada método de fase recebe o Δt INTEGRAL. |
| 80 | **A integração de `Player::positionUpdate()` é trapezoidal**, não Euler. |

## 30.9 Ciclo de vida e memória

| # | Armadilha |
|---|---|
| 81 | **Nunca `delete`, apenas `unref()`.** |
| 82 | **O ponteiro para o pai é CRU** — evita ciclo de referências. |
| 83 | **`shutdownNotification()` deve ser idempotente** e chamar a base. |
| 84 | **`copyData()` anula o ponteiro de continente** — clone nasce órfão. |
| 85 | **`AbstractPlayer::copyData()` não copia os NIBs.** |
| 86 | **`acceptConnection()` devolve objeto já possuído** — não chamar `ref()`. |
| 87 | **`Agent`/`AgentTC` não propagam `update*()` aos filhos** — o `state` nunca é atualizado pelo ciclo normal. |
| 88 | **`DECLARE_SUBCLASS` termina em `private:`** — reabrir com `public:`. |
| 89 | **`Emission::transmitter` é ponteiro cru deliberado** (evita ciclo). |
| 90 | **`Track::tgt` é ponteiro cru**, não `safe_ptr`; não existe campo `trackSide`. |

---

# 31. GLOSSÁRIO

**ciclo** — Conjunto de 16 *frames* consecutivos. Serve de relógio para tarefas de baixa
frequência, agendadas por `frame() % N`.

**DRM** (*Dead Reckoning Model*) — Um dos oito algoritmos padronizados pelo DIS para
extrapolar a posição de uma entidade remota entre atualizações de rede. No MIXR o sufixo é
`_DRM` (`RVW_DRM`).

**ECEF** (*Earth-Centered, Earth-Fixed*) — Sistema cartesiano com origem no centro da Terra e
eixos fixos a ela. É o referencial em que o estado trafega na rede.

**EDL** (*English Description Language*) — A linguagem de configuração do MIXR, na qual se
descreve a estrutura de uma simulação: quais objetos existem, como se aninham e com que
parâmetros.

**ERP** (*Effective Radiated Power*) — A potência efetivamente irradiada numa direção:
potência do transmissor multiplicada pelo ganho da antena naquela direção.

**fase** — Uma das quatro subdivisões de um *frame* de tempo crítico (dinâmica, transmissão,
recepção, lógica). A lista de *players* é percorrida uma vez por fase, **mas cada método de
fase recebe o Δt integral do *frame***.

**flyout** — O clone de uma arma que efetivamente voa. A arma declarada em EDL permanece
presa ao cabide e passa a `LAUNCHED`; o clone entra na lista de *players* em `PRE_RELEASE` e
vira `ACTIVE` no quadro seguinte.

**gaming area** — A região de interesse da simulação, definida por um ponto de referência
geodésico e um raio. Serve de origem do plano tangente local (NED).

**LFI** (*Linear Function Interpolation*) — A interpolação linear multidimensional do MIXR,
usada em toda função tabelada — coeficientes aerodinâmicos, padrões de antena, transmitância
atmosférica.

**LLA** (*Latitude, Longitude, Altitude*) — Coordenadas geodésicas: latitude e longitude em
graus, altitude em metros acima do elipsoide.

**NED** (*North-East-Down*) — Plano tangente local, ancorado no ponto de referência da
*gaming area*. Referencial de trabalho da dinâmica de voo e dos sensores.

**NIB** (*Network Interface Block*) — O objeto que representa a relação entre um *player* e a
rede: guarda o estado do último *update* e extrapola a posição entre eles.

**nome de fábrica** — A *string* pela qual uma classe é instanciável a partir de um arquivo
EDL. Definida no segundo argumento de `IMPLEMENT_SUBCLASS`; **não precisa coincidir com o
nome da classe C++** — em 56 classes do MIXR ela difere.

**Ntm** (*Network Type Mapper*) — O objeto que associa um tipo de entidade da rede a uma
classe de *player* do MIXR, em cada direção (entrada e saída) separadamente. Carrega um
*player* protótipo a clonar.

**ownship** — O *player* sob controle da estação — aquele cuja perspectiva a aplicação
apresenta. Indicado pelo *slot* `ownship` da `Station`; pode ser trocado em tempo de
execução.

**PDU** (*Protocol Data Unit*) — A unidade de mensagem do protocolo DIS. Cada tipo (Entity
State, Fire, Detonation, …) tem leiaute binário fixo, transmitido em *big-endian*.

**PLA** (*Power Lever Angle*) — Posição normalizada da manete: 0,0 = *idle*, 1,0 = potência
militar, 2,0 = pós-combustor. Variável de entrada das tabelas de assinatura IR de
escapamento.

**RCS** (*Radar Cross Section*) — A área equivalente, em m², que descreve quanta energia de
radar um alvo reflete de volta ao emissor. Frequentemente expressa em dBsm.

**slot** — Atributo nomeado que um objeto expõe ao arquivo de configuração. Cada classe
declara os seus numa *slot table*; o *parser* resolve o nome para um índice global e despacha
para a função C++ correspondente.

**Tdb** — O *buffer* de geometria que um *gimbal* preenche com os alvos de interesse de um
quadro. Apesar do nome (*Track Data Block*), **não tem relação com `Track`**: guarda alvos
candidatos, não pistas rastreadas.

**TOF** (*Time of Flight*) — Tempo decorrido desde o lançamento de uma arma. Governa o início
da guiagem, a janela de queima do motor e a autodestruição.

---

# 32. MAPA DO REPOSITÓRIO

## 32.1 Árvore de cabeçalhos (`include/mixr/`)

```
include/mixr/
├── config.hpp                    (os 8 parametros globais + MIXR_VERSION)
├── base/
│   ├── colors/                   Color, Rgb, Rgba, Hsv, Hsva, Hls, Cmy, Cie, Yiq
│   ├── concepts/linkage/         AbstractIoDevice, AbstractIoData, AbstractIoHandler
│   ├── functors/                 Function, Func1..Func5, Table1..Table5,
│   │                             TableStorage, FStorage, Polynomial
│   ├── network/                  NetHandler, PosixHandler, Udp*Handler, Tcp*
│   ├── numeric/                  Number, Integer, Float, Boolean, Complex,
│   │                             Decibel, Operators (Add/Subtract/Multiply/Divide)
│   ├── osg/                      Vec2/3/4 d/f, Matrixd/f, Quat  (tipos de VALOR)
│   ├── threads/                  AbstractThread, PeriodicThread, SyncThread,
│   │                             OneShotThread
│   ├── ubf/                      AbstractState, AbstractBehavior, AbstractAction,
│   │                             Agent, AgentTC, Arbiter
│   ├── units/                    Distances, Angles, Times, Masses, Forces,
│   │                             Powers, Frequencies, Energies, Areas, Volumes,
│   │                             LinearVelocity, AngularVelocity, Density, FlowRate
│   └── util/                     constants, math_utils, nav_utils, navDR_utils,
│       └── platform/             str_utils, lfi, atomics, system_utils
├── interop/
│   ├── common/                   Nib, NetIO, Ntm, NtmInputNode
│   ├── dis/                      NetIO, Nib, Ntm, EmissionPduHandler, pdu, structs
│   ├── hla/                      (presente, NAO compilado)
│   └── rprfom/                   (presente, NAO compilado)
├── linearsystem/                 ScalerFunc, DiffEquation, FirstOrderTf,
│                                 SecondOrderTf, LagFilter, LowpassFilter,
│                                 Sz1, Sz2, SaH, LimitFunc, Limit, Limit01, Limit11
├── linkage/
│   ├── adapters/                 AnalogInput, AnalogOutput, DiscreteInput,
│   │                             DiscreteOutput, Ai2DiSwitch
│   └── generators/               AnalogInputFixed, DiscreteInputFixed, AnalogSignalGen
│                                 (+ IoData, IoDevice, IoHandler, MockDevice, UsbJoystick)
├── models/
│   ├── dynamics/                 DynamicsModel, AerodynamicsModel, RacModel,
│   │                             LaeroModel, JSBSimModel, SpaceDynamicsModel
│   ├── environment/              AbstractAtmosphere, IrAtmosphere, IrAtmosphere1
│   ├── navigation/               Navigation, Ins, Gps, Route, Steerpoint, Bullseye
│   ├── player/
│   │   ├── air/                  AirVehicle, Aircraft, Helicopter, UnmannedAirVehicle
│   │   ├── effect/               Effect, Chaff, Flare, Decoy
│   │   ├── ground/               GroundVehicle, Tank, Artillery, ArmoredVehicle,
│   │   │                         WheeledVehicle, SamVehicle, GroundStation*
│   │   ├── space/                SpaceVehicle, MannedSpaceVehicle,
│   │   │                         UnmannedSpaceVehicle, BoosterSpaceVehicle
│   │   └── weapon/               AbstractWeapon, Missile, Aam, Agm, Sam, Bomb, Bullet
│   │                             (+ Player, LifeForm, Ship, Building)
│   ├── sensor/                   Radar, Tws, Stt, Gmti, Sar, Rwr, Jammer,
│   │                             IrSensor, MergingIrSensor
│   ├── system/                   System, Antenna, Gimbal, ScanGimbal,
│   │   └── trackmanager/         StabilizingGimbal, RfSystem, RfSensor, SensorMgr,
│   │                             IrSystem, IrSeeker, Radio, CommRadio, Datalink,
│   │                             Iff, Pilot, Autopilot, OnboardComputer,
│   │                             CollisionDetect, Stores, StoresMgr,
│   │                             SimpleStoresMgr, ExternalStore, Gun, FuelTank,
│   │                             AvionicsPod
│   │                             TrackManager, AirTrkMgr, GmtiTrkMgr, RwrTrkMgr,
│   │                             AngleOnlyTrackManager, AirAngleOnlyTrkMgr,
│   │                             AirAngleOnlyTrkMgrPT
│   └── (raiz)                    WorldModel, Emission, SensorMsg, IrQueryMsg, Tdb,
│                                 TdbIr, Track, RfTrack, IrTrack, Signatures,
│                                 IrSignature, AircraftIrSignature, IrShapes,
│                                 Image, Designator, Message, TargetData,
│                                 SynchronizedState, Actions, SimAgent,
│                                 MultiActorAgent, factory
├── recorder/                     DataRecorder, DataRecordHandle, OutputHandler,
│                                 InputHandler, FileWriter, FileReader, NetInput,
│                                 NetOutput, PrintHandler, TabPrinter, PrintPlayer,
│                                 PrintSelected
├── simulation/                   Station, Simulation, AbstractPlayer,
│                                 AbstractNetIO, AbstractNib, AbstractIgHost,
│                                 AbstractDataRecorder, AbstractRecorderComponent,
│                                 Station*PeriodicThread, Simulation*SyncThread,
│                                 dataRecorderTokens.hpp, recorder_macros.hpp
└── terrain/
    ├── ded/                      DedFile
    ├── dted/                     DtedFile
    └── srtm/                     SrtmHgtFile
                                  (+ Terrain, DataFile, QuadMap)
```

## 32.2 Cabeçalhos-chave para consulta rápida

| Preciso de… | Abra |
|---|---|
| macros do modelo de classes | `include/mixr/base/macros.hpp` |
| *tokens* de evento | `include/mixr/base/eventTokens.hpp` (incluído *dentro* de `Component`) |
| *tokens* de gravação | `include/mixr/simulation/dataRecorderTokens.hpp` |
| macros de gravação | `include/mixr/simulation/recorder_macros.hpp` |
| parâmetros globais | `include/mixr/config.hpp` |
| constantes matemáticas | `include/mixr/base/util/constants.hpp` |
| geodésia | `include/mixr/base/util/nav_utils.hpp` |
| interpolação | `include/mixr/base/util/lfi.hpp` |
| contrato de tempo (ciclo/frame/fase) | `include/mixr/simulation/Simulation.hpp` (comentário de cabeçalho) |
| algoritmos de *dead reckoning* | `include/mixr/interop/common/Nib.hpp` (`enum DeadReckoning`) |
| regra de casamento do `Ntm` de saída | `include/mixr/interop/common/Ntm.hpp` (comentário) |

## 32.3 Onde os módulos são registrados

| Arquivo | Registra |
|---|---|
| `src/base/factory.cpp` | ~102 nomes (números, unidades, cores, rede, tabelas, UBF) |
| `src/simulation/factory.cpp` | `Simulation`, `Station` |
| `src/terrain/factory.cpp` | `QuadMap`, `DedFile`, `DtedFile`, `SrtmHgtFile` |
| `src/models/factory.cpp` | ~96 nomes |
| `src/linkage/factory.cpp` | 11 nomes (**sem** `IoHandler`) |
| `src/recorder/factory.cpp` | 9 nomes |
| `src/interop/dis/factory.cpp` | `DisNetIO`, `DisNtm`, `EmissionPduHandler` |
| `src/linearsystem/` | **não existe `factory.cpp`** |

---

# 33. RESUMO EXECUTIVO — OS DEZ PONTOS QUE MAIS IMPORTAM

1. **Estrutura em EDL, comportamento em C++.** Reconfigurar não exige recompilar. Três
   mecanismos sustentam isso: fábrica por nome, sistema de *slots* e contagem de
   referências.

2. **Tudo é um `Component`.** A cadeia `Referenced → Object → Component` acrescenta,
   respectivamente: ciclo de vida por contagem, identidade + *slots*, e árvore + tempo +
   eventos + reset/freeze/shutdown.

3. **Nunca `delete`, apenas `unref()`.** O ponteiro para o pai é cru justamente para não
   fechar ciclo de referências.

4. **O tempo é ciclo → *frame* → fase.** 16 *frames* por ciclo, 4 fases por *frame*
   (dinâmica, transmite, recebe, processa). A lista de *players* é percorrida 4× por
   *frame*, **mas cada método de fase recebe o Δt integral** (`dt4 = dt * 4.0`).

5. **A raiz é sempre uma `Station`.** Ela guarda subsistemas em *slots* nomeados; *players*
   guardam subsistemas em `components`, descobertos **por tipo**.

6. **O nome de fábrica é uma *string* independente do nome da classe.** Em 56 classes eles
   divergem — e `( StoresMgr )` constrói `SimpleStoresMgr` silenciosamente.

7. **Não há registro global de fábricas.** O encadeamento é código escrito à mão na
   aplicação, e a ordem das linhas *é* a precedência.

8. **`num_errors` do *parser* precisa ser conferido.** Um ponteiro não-nulo não significa que
   o arquivo estava correto.

9. **O `RESET_EVENT` não é opcional.** É ele que carrega o terreno, monta a lista de
   *players* e cria o *pool* de *threads*.

10. **Este fork: Linux apenas, C++11 com `-fpermissive`, sem HLA/RPR-FOM, `linearsystem` sem
    fábrica e sem consumidores, e sem um `IoHandler` concreto.** Planejar integração sem
    saber disso custa tempo.
