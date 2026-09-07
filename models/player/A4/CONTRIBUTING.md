# Contribuindo com `flight` (A4)

Este roteiro é sobre mexer NESTE modelo, já em produção, a partir de só esta pasta aberta — para
criar um modelo **novo** do zero, o ponto de entrada é
[`../../../CONTRIBUTING.md`](../../../CONTRIBUTING.md), que costura o gerador (`make new-model`) e
os dois pontos de partida copiáveis (`fixtures/stub`, `template`). Este arquivo assume que os
pré-requisitos já estão feitos — [`INSTALL.md`](INSTALL.md) — e cobre editar, testar, editar a
árvore de comportamento com o Groot e publicar, tudo sem precisar ter o resto do repositório em
mente.

## 0. Onde cada coisa mora

Quatro camadas, dependência de mão única — detalhe completo em [`README.md`](README.md):

```
domain/   regras puras                libstdc++ so            <- 47 testes, sem MIXR
bt/       nos da arvore                BT.CPP + domain/        <- 20 testes, sem MIXR
ubf/      percepcao/decisao/atuacao    MIXR + bt/ + domain/ + xnative/
xnative/  classes MIXR proprias        MIXR + ubf/             <- 9 testes, sem Station
```

A aeronave (`data/jsbsim/`) e a árvore de produção (`configs/flight_tree*.xml`) são dado DESTE
modelo, não do cenário — calibradas para o A-4 especificamente, e publicadas junto com o `.so`. Ver
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para a calibração e as armadilhas confirmadas.

## 1. Editar e compilar

```bash
make            # compila as duas variantes: libflight.so (single-thread), libflight_tc.so (multi)
make test       # domain + tree + native, ~1 s -- nenhuma das tres levanta Station
```

`.clangd`/`compile_commands.json` já apontam para `./build` — o `meson setup` que `make` dispara
implicitamente gera esse arquivo, então IntelliSense/erros de compilação já funcionam depois da
primeira compilação, sem passo extra.

Se a mudança acrescenta uma classe ou um slot que o `.edl` de produção passa a usar, o
[`fixtures/stub`](../fixtures/stub/) (o "modelo estranho" que os testes de plugin carregam para
provar que o contrato basta) também precisa aceitar/ignorar o mesmo slot — ver a seção 4.

## 2. Editando a árvore de comportamento (Groot)

Instalação do Groot (única vez, a partir da raiz) → [`INSTALL.md`](INSTALL.md) §4.

### Abrir a árvore de produção

Os 5 `configs/flight_tree*.xml` já têm o que o Groot precisa (comentário de cabeçalho sem `--`,
bloco `<TreeNodesModel>` colado) — abrem direto, sem cópia nem edição prévia:

```bash
cd ../../.. && make open-groot
# File > Load... > models/player/A4/configs/flight_tree.xml
```

**Nunca edite o `.xml` de produção como primeiro rascunho** — copie para `/tmp`, itere na cópia, e
só substitua o arquivo de produção quando a árvore estiver pronta (`git diff` mostra exatamente o
que mudou, em vez de um histórico de tentativas).

### Registrar um nó novo (e manter o Groot sabendo dele)

1. Implemente o nó em `src/bt/nodes/` e registre em `src/bt/bt_factory.cpp` (nó nativo, sem
   dependência do SDK) ou `src/bt/bt_factory_sdk.cpp` (nó que usa `xlog`/`xrandom`/`xinfer`/
   `xpyembed` — qualquer coisa que arraste `sdk_dep`). Acrescente o `.cpp` novo a
   `bt_sources`/`bt_sdk_sources` em [`tests/meson.build`](tests/meson.build) — é de lá que
   `dump-tree-model`, `test-tree` e `test-native` compilam; esquecer esse passo faz o nó compilar
   para o `.so` de produção mas não aparecer no manifesto do Groot nem nos testes.
2. Recompile o gerador e resincronize os 5 XMLs de produção:
   ```bash
   meson compile -C build dump-tree-model
   python3 tools/sync_tree_models.py
   ```
3. `make test` cobra isso sozinho a partir daqui — o teste `tree-model-sync` (suíte `tree`) falha
   se o passo 2 for esquecido, listando exatamente quais arquivos ficaram desatualizados.

Detalhe técnico do gerador (por que ele existe em vez de manter o bloco à mão, os dois modos,
armadilhas confirmadas) → [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), seção "Editando a árvore
com o Groot".

### Criar uma árvore nova (experimental, fora de produção)

Nada aqui monta a árvore por você — o Groot não tem "começar em branco com os meus nós" (a paleta
só é populada a partir do `<TreeNodesModel>` de um arquivo já carregado). O gerador produz o
arquivo inteiro, pronto para abrir:

```bash
./build/tests/dump-tree-model --skeleton MinhaArvore > /tmp/nova_arvore.xml
cd ../../.. && make open-groot
# File > Load... > /tmp/nova_arvore.xml
```

Sai com uma raiz `<Fallback>` vazia e a paleta já mostrando todos os nós que este modelo registra
(em azul, distintos dos nativos do BT.CPP) — arraste da paleta pro canvas, conecte arrastando de
uma saída pra uma entrada, `File > Save`. Aponte `treeFile:` de um cenário de **teste** para o
arquivo salvo primeiro; só promova a árvore para `configs/` (e para um `.edl` de produção) depois
de validada.

### Monitorar uma árvore ao vivo

O modelo já tem esse hook pronto, opt-in por variável de ambiente — implementação em
`src/ubf/BtBehavior.cpp` (`startGrootMonitorIfRequested()`):

```bash
cd ../../..    # a partir da raiz do poc-mixr
MIXR_GROOT_MONITOR=falcon1 ./dist/bin/app -folder src/poc/dis -scenario multi-thread
```

Em outro terminal com display: `make open-groot` (raiz) → aba **Monitor** → conectar em
`localhost`, portas 1666 (status) / 1667 (topologia). A árvore daquele player aparece se colorindo
em tempo real conforme tica. Sem a variável de ambiente, nenhuma porta abre — zero custo por
padrão. Armadilhas de ciclo de vida do `BT::PublisherZMQ` (por que ele precisa ser derrubado
**antes** de qualquer `reset()`/`shutdownNotification()`/cópia da árvore) →
[`../../../CLAUDE.md`](../../../CLAUDE.md), seção "Groot — editor e monitor ao vivo".

## 3. Publicar a mudança para um cenário de verdade

Compilar e rodar `make test` aqui prova a lógica isolada, mas não deixa nada executável num
cenário — é preciso publicar (ver [`../README.md`](../../README.md) §1 para o porquê de
`install-host` e `make install` da raiz serem passos separados):

```bash
make install-host              # -> ../../../plugins/ (o mesmo deposito que um .so de terceiro usaria)
cd ../../.. && make install     # sincroniza plugins/ -> dist/ -- unico alvo que toca dist/ pelo modelo
```

Rodar um cenário que carrega este modelo, a partir da raiz:

```bash
./dist/bin/app -folder src/poc/dis -scenario multi-thread
```

## 4. Antes de propor a mudança

- `make test` (aqui) e, se a mudança mexeu em algo que o host também exercita (slot novo, nome de
  fábrica novo, mudança de comportamento visível no dump `frame=`), `cd ../../.. && make test` —
  as duas suítes (modelo + host), incluindo os testes de plugin e determinismo.
- **`provides:` é igualdade EXATA de conjunto** entre o `.edl` e o que o `.so` exporta. Um nome de
  fábrica novo obriga atualizar `provides:` em TODO cenário que carrega `libflight.so`/
  `libflight_tc.so` — não só o que motivou a mudança. `python3 tests/guard/
  check_colisao_fabrica.py` (na raiz; o hook `check-colisao-fabrica.sh` já roda isso sozinho após
  editar `.cpp`/`.hpp` sob `models/`) cobra colisão de nome entre plugins carregados juntos no
  mesmo processo (já aconteceu de verdade — ver `CLAUDE.md`, "vigésima terceira passada").
- **Se a mudança acrescenta uma classe ou um slot que o `.edl` de produção passa a usar, atualize o
  `fixtures/stub` junto** ([`../fixtures/stub/docs/CONTRATO.md`](../fixtures/stub/docs/CONTRATO.md))
  — ele roda o MESMO cenário de produção trocando só o `file:` do `( PluginModule )`, e existe
  justamente para quebrar quando o contrato muda; um slot/nome que só o `A4` conhece derruba
  `plugin-modelo-estranho`/`plugin-deposito-terceiro`.
- **`CHANGELOG.md`** — uma entrada por mudança que alguém precisaria saber antes de mexer neste
  modelo, não uma por commit (a versão é a do `project()` em `meson.build`; as datas saem da data
  de commit, nunca da mensagem — todo commit deste repositório se chama `up`).

## Ler também

- [`README.md`](README.md) — as quatro camadas, o build autocontido, os testes
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — calibração do A-4, o gerador de
  `<TreeNodesModel>` em detalhe, armadilhas confirmadas
- [`INSTALL.md`](INSTALL.md) — pré-requisitos de sistema, o SDK do host, Groot
- [`../fixtures/stub/docs/CONTRATO.md`](../fixtures/stub/docs/CONTRATO.md) — o que TODO modelo
  (este incluído) tem que fazer
- [`../../../CLAUDE.md`](../../../CLAUDE.md) — arquitetura do repositório inteiro; seção "Groot —
  editor e monitor ao vivo" tem a lista completa de armadilhas do editor/monitor
- [`../../../CONTRIBUTING.md`](../../../CONTRIBUTING.md) — criar um modelo **novo** (não é este
  roteiro, que é sobre mexer no `A4` já existente)
