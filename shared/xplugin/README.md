# `shared/xplugin` — carga dinâmica de modelos MIXR

Compilar um modelo à parte, contra um contrato explícito, declará-lo no `.epp` e carregá-lo em
tempo de execução — **sem recompilar a aplicação**.

> **Leia isto antes de qualquer outra coisa:** *"sem recompilar tudo"* — sim. *"sem reiniciar o
> processo"* — **não**. Nunca chamamos `dlclose`, e a razão está na seção Limites. Recarregar um
> plugin é reiniciar o processo.

## Como se usa

No cenário, como **primeira entrada de `components:`** da Station:

```
   components: {
      plugins: ( PluginLoader
         searchPaths: { "./build/src/single-thread/src/"  "./dist/lib/mixr-plugins/" }
         modules: {
            ( PluginModule  file: "libsingle_thread_model.so"
               provides: { AlertDatalink TacticalAlert FlightState
                           BtBehavior AltitudeSafetyBehavior FlightAction } )
         }
      )
      ...
```

Daí em diante, essas classes valem no resto do arquivo como se fossem do framework.
O primeiro uso vem logo abaixo: o `state: ( FlightState )` do agente.

No plugin, um arquivo de cola de quinze linhas (`src/<poc>/src/plugin.cpp`) e a macro
`MIXR_PLUGIN_DEFINE`. **Use sempre a macro** — ela emite o `extern "C"` com visibilidade default,
que é o que impede a armadilha mais clássica do assunto (ver Limites, item 8).

## Por que o bloco tem de vir antes de `simulation:`

É a única regra que o autor do cenário precisa lembrar, e ela é mecânica, não estética.

A produção `arglist` do `edl_parser` é **recursiva à esquerda** (`edl_parser.y:143-165`), e
`form : '(' IDENT arglist ')' { $$ = parse($2, $3); }` executa `parse()` — que é
`factory(name)` → todos os `setSlotByName` → **`isValid()`** — no fecha-parêntese de cada forma.
Logo:

- **filhos antes dos pais** (aninhamento), e
- **irmãos na ordem do texto** (recursão à esquerda).

A carga acontece no `isValid()` do `( PluginLoader )`. Um bloco escrito antes de `simulation:`
portanto carrega as `.so` antes de qualquer player ser construído — **num único parse, no mesmo
arquivo, com a factory de produção inteira**. Sem passe descartável e sem pré-scan de texto.

Fora de ordem, o parser chega na classe do plugin sem ninguém que responda por ela e
`mixrFactory` aborta explicando exatamente isso. Não há silêncio nem SIGSEGV.

## Regras para escrever um plugin

1. **`shared_module()`, não `library()`.** Produz artefato não-linkável, o que impede
   estruturalmente que alguém o ponha num `link_with:` e acabe com duas cópias de
   `Player::metaObject` no processo (o símbolo é `GLOBAL OBJECT` forte — exatamente um por
   processo).
2. **`dependencies:` só pode conter `mixr_dep`, `xplugin_abi_dep`, `xlog_dep` e `xboard_dep`.**
   Nunca `xmsg_dep`, `xtacview_dep`, `xclock_dep` ou `xjoystick_dep`: as quatro são
   `static_library()`, e o `.so` ganharia a **própria cópia** dos estáticos delas. É a armadilha de
   `contexts/BTCPP-CONTEXT.md:7262-7270`.

   `xlog` e `xboard` são exceção porque foram **promovidas a `shared_library()`** exatamente por
   isto — é a saída documentada no item 6 dos Limites, aplicada. As libs do MIXR já eram `.so` de
   verdade, com tudo exportado.

   **Biblioteca estática de terceiro é caso à parte:** a BehaviorTree.CPP deste pacote Conan é um
   `.a` com 447 símbolos `T` globais, e `gnu_symbol_visibility: 'hidden'` **não se aplica a
   objetos vindos de um arquivo**. Um plugin que a linke precisa de `-Wl,--exclude-libs,ALL`,
   senão esses símbolos vazam para o `.dynsym`. E o executável não deve linká-la também, ou o
   processo fica com duas cópias dos estáticos dela.
3. **`gnu_symbol_visibility: 'hidden'` + `-Wl,--no-undefined`.** O primeiro deixa sair só o ponto
   de entrada; o segundo garante que o plugin não dependa de símbolo da aplicação (que não
   exporta nenhum), com o erro chegando em tempo de link.
4. **rpath com `mixr_libdir`.** Mesma regra de qualquer alvo novo.

## O que o contrato garante

`PluginDescV1` (ver `PluginAbi.hpp`) é um POD com `struct_size` na frente, devolvido por um
símbolo `extern "C"` de nome fixo. **Dois mecanismos de versão, para dois tipos de mudança:**
`struct_size` + `abi` cobrem mudança *aditiva* (campo novo no fim); o `_v1` **no nome do símbolo**
cobre mudança *destrutiva* — um host novo procura `mixr_plugin_v2` e um plugin velho falha com
"símbolo ausente" em vez de ter os bytes reinterpretados.

| campo | ação na divergência |
|---|---|
| `abi`, `cxx11_abi`, `struct_size`, `player_size` | **recusa** |
| `mixr_version`, `mixr_pkg_version`, `build_id` | **aviso** |
| `cxx_standard` | só diagnóstico, nunca compara |

**Por que `cxx_standard` nunca é comparado por igualdade:** o host compila `gnu++17`
(`__cplusplus == 201703`) e `libmixr_*.so` foi compilada `c++11` (`201103`) — e os dois
interoperam hoje, em produção. Uma checagem de igualdade rejeitaria todo plugin correto.

**Por que a contagem de slots não é usada como guarda:** `BEGIN_SLOT_MAP` lê
`BaseClass::getSlotTable().n()` em **runtime**, via PLT (`macros.hpp:305`), e `SlotTable::n()`
percorre a cadeia viva. Medido: o plugin vê os 45 slots acumulados do host e põe os seus em 46 e
47. Drift de contagem na base **não desalinha** o plugin — recusar por isso mataria plugins que
funcionam.

## Limites — o que este contrato NÃO garante

Esta seção existe porque um contrato que promete demais é pior do que nenhum.

1. **Não há versionamento de SONAME no fork do MIXR.** `readelf -d libmixr_base.so` →
   `SONAME: libmixr_base.so`, sem `.so.1.0.5`, sem symlink. Consequência: **nada em tempo de
   carga** detecta um plugin construído contra headers diferentes. A comparação de
   `mixr_pkg_version` é convenção nossa, contornável com um `-D`. A correção real seria
   `soversion:` mais um version script no fork — mudança upstream, fora deste repositório.
2. **`sizeof(Player)` é canário, não prova.** Não pega reordenação de membro com `sizeof`
   constante, inserção de virtual **no meio** da vtable (`Player` declara mais de 150), nem
   renumeração de enum.
3. **Funções inline dos headers do MIXR são copiadas para dentro do plugin.**
   `Referenced::ref()`/`unref()` mexem em `refCount`/`semaphore` por offset fixo;
   `MetaObject::getClassName()` lê um `std::string` membro. Depois de um rebuild do MIXR, o
   processo contém duas versões da mesma lógica de ref-counting. **Não há checagem possível.** A
   única correção estrutural seria uma fachada C estável — para o `Player`, reexprimir ~150
   virtuais. É outro projeto.
4. **Macros de `config.hpp` que dimensionam arrays membros.** `TrackManager.hpp:90` tem
   `Track* tracks[MIXR_CONFIG_MAX_TRACKS]` sob `#ifndef`; um `-DMIXR_CONFIG_MAX_TRACKS=400` no
   plugin muda o layout **sem mudar `sizeof(Player)`**. Idem
   `MIXR_CONFIG_MAX_PLAYERS_OF_INTEREST` e `MIXR_CONFIG_MAX_INTERVAL_TIMERS`.
5. **Sem descarga e sem hot-reload em processo vivo.** Nunca chamamos `dlclose`, e usamos
   `RTLD_NODELETE` para que um `dlclose` acidental vire no-op. O argumento é concreto:
   `STANDARD_CONSTRUCTOR()` faz `slotTable = &slottable` (toda instância viva guarda ponteiro
   para o `.data` do plugin), `SlotTable` encadeia `baseTable`, a vtable aponta para o
   `.data.rel.ro` dele, e `STANDARD_DESTRUCTOR()` **escreve** `metaObject.count--` na imagem do
   plugin. Mesma conclusão do BehaviorTree.CPP (`BTCPP-CONTEXT.md:7257`).
6. **O plugin não consegue chamar a aplicação.** O executável não exporta símbolo nenhum e
   `--no-undefined` está ligado — sem `LOG(...)`, sem `app::*`. Os canais são as virtuais das
   classes base do MIXR e o `stdout`. Linkar um `shared/x*` **não é a saída** (regra 2 acima); a
   saída honesta é promover a peça necessária a `shared_library` instalada com SONAME.
7. **`MetaObject::count/mc/tc` não são atômicos** (`int` cru, `macros.hpp:249`) — corrida real
   quando objetos nascem e morrem em threads T/C diferentes. Plugin não piora, mas acrescenta
   mais uma classe contada. Por isso o teste de vazamento roda com `-threads 1`.
8. **`dlsym` só enxerga símbolo dinâmico exportado.** Escrever a assinatura à mão em vez de usar
   `MIXR_PLUGIN_DEFINE` num alvo com `-fvisibility=hidden` produz um ponto de entrada invisível,
   com o sintoma aparecendo longe — é a armadilha de `BTCPP-CONTEXT.md:7248`. Há um teste de
   guarda para isso (`tests/plugin/check_plugin_symbol.sh`).
9. **Se um dia precisar de isolamento de verdade** (plugin de terceiro não confiável, ou plugin
   que precise de outra versão do MIXR), nenhum ajuste de `dlopen` resolve — a resposta é modelo
   **fora do processo**, com o `Player` local virando proxy sobre IPC. Troca um problema de ABI
   (insolúvel) por um de protocolo (resolvível), ao custo de latência por frame.

## Por que `RTLD_LOCAL`, contra o prior art do BehaviorTree.CPP

O BT.CPP usa `RTLD_GLOBAL` (`BTCPP-CONTEXT.md:8641`) com um comentário herdado do POCO dizendo
que *"RTTI não funciona para tipos definidos na shared library"*. Isso está **obsoleto para este
caso**: no Linux/GCC `__GXX_MERGED_TYPEINFO_NAMES == 0`, então `type_info::operator==` cai em
`strcmp`, e o payload real na `.so` entregue (`_ZTSN4mixr6models6PlayerE` = `"N4mixr6models6PlayerE\0"`)
não tem o `*` inicial que forçaria comparação por endereço. Além disso, o escopo de busca de um
objeto `RTLD_LOCAL` **já inclui** o escopo global.

E `RTLD_GLOBAL` traria risco real: `Player::metaObject` e `Player::slottable`, embora `private:`
em C++, são símbolos `GLOBAL OBJECT` no `.dynsym`. Dois plugins que declarassem classes de mesmo
nome mangled se interporiam em silêncio — contadores fundidos, tabelas de slot cruzadas.

Confirmado rodando, com o plugin em `-fvisibility=hidden` e `RTLD_LOCAL`:
`dynamic_cast<AirVehicle*>`, `isClassType(typeid(Player))` e `findByType` funcionam. É o teste
`plugin-contrato`, e ele é o **gate** dessa decisão: se ficar vermelho, tire
`gnu_symbol_visibility: 'hidden'` do `meson.build` do plugin.

## Testes

`meson test -C build --suite plugin` — quatro camadas:

| teste | prova |
|---|---|
| `plugin-contrato` | descritor, RTTI através do `.so`, slots próprio e herdado, `MetaObject` |
| `plugin-simbolo` | só `mixr_plugin_v1` sai, e ele sai como `T` |
| `plugin-negativos` | 7 modos de falha, **cada um afirmando `rc != 139`** |
| `plugin-hotswap` | mesmo binário + `.so` diferente = voo diferente (o sentido da curva de patrulha) |

`make check-plugin-hotswap` é a demonstração ao vivo: edita `domain/PatrolPlan.cpp`, rebuilda **só** o `.so`,
confere que o executável não foi tocado (mesmo `sha256`, mesmo `mtime`) e mostra o comportamento
mudando.
