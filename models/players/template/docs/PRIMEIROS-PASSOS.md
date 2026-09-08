# Primeiros passos — de `template` a um modelo com nome próprio

Este roteiro assume que você já rodou, **uma vez, na raiz do repositório**, o pré-requisito que
todo projeto de modelo precisa (publica o SDK de plugin e os pacotes do Conan):

```bash
cd ../../.. && make configure && make sdk
```

Sem isso, `check-root` (dependência de `configure`/`build`/`test`/`install` — não de `clean`,
`help` nem `uninstall-host`) falha com uma mensagem dizendo exatamente o que rodar.

## Passo 0 — confirme que o template, do jeito que está, funciona

Antes de mudar qualquer coisa, compile e teste o template como ele é. Se isto falhar, o problema
é do seu ambiente (SDK não publicado, Conan não instalado), não do que você vai escrever depois:

```bash
cd models/players/template
make build      # -> ./dist/lib/mixr-plugins/libtemplate.so (bare `make` so mostra `make help`)
make test       # 4 casos de domain/ + a forma do .so (1 símbolo T, deps resolvidas)
```

## Passo 1 — copie e escolha um nome

```bash
cp -r models/players/template models/players/meu-modelo
cd models/players/meu-modelo
rm -rf build dist   # se a cópia trouxe artefatos de build do template
rm -f src/mirror.cpp   # NAO faz parte do scaffold -- ver o aviso no topo do proprio arquivo
```

`meu-modelo` aqui é só um exemplo — use um nome descritivo do que o SEU modelo pilota ou decide
(o mesmo espírito de `A-4`, não `modelo1`/`modelo2`).

**Depois de apagar `src/mirror.cpp`, remova também o bloco `template_mirror_lib` de
`meson.build`** (o segundo `shared_module(...)`, com o comentário "SEGUNDO artefato deste
projeto") **e o teste `contrato-simbolo-unico-e-deps-resolvidas-mirror` de `tests/meson.build`** —
sem isso, `make build` falha procurando um arquivo que você acabou de apagar.

## Passo 2 — renomeie o projeto, o módulo e o namespace

São **quatro** lugares, e os quatro têm que concordar (o namespace é o mais fácil de esquecer,
porque não dá erro de build óbvio se ficar pela metade — dá um nome de classe estranho em runtime
ou, pior, uma colisão silenciosa com outro plugin, ver `docs/ARCHITECTURE.md`):

```bash
# 1) o nome do project() e do shared_module() em meson.build
sed -i "s/'template'/'meu_modelo'/g" meson.build

# 2) o namespace C++ (troque xtemplate -> xmeumodelo, ou o nome que preferir,
#    em TODOS os arquivos de uma vez)
grep -rl 'xtemplate' include src tests | xargs sed -i 's/xtemplate/xmeumodelo/g'

# 3) o primeiro argumento de MIXR_PLUGIN_DEFINE em src/plugin.cpp -- o sed
#    do passo 1 já pegou isto, porque é a MESMA string 'template'. Confira:
grep -n 'MIXR_PLUGIN_DEFINE' src/plugin.cpp

# 4) a linha ROOT do Makefile -- SÓ SE você mudou a profundidade em relação
#    à raiz do repositório. Copiado direto para models/players/meu-modelo/,
#    a profundidade é a MESMA do template (três níveis) -- nada a fazer.
#    Se você mover para outro lugar (ex.: direto em models/meu-modelo/, dois
#    níveis), troque 'ROOT := $(abspath ../../..)' por
#    'ROOT := $(abspath ../..)'.
```

Confirme que nada ficou para trás:

```bash
grep -rn 'xtemplate\|"template"' include src tests meson.build src/plugin.cpp
# (deve devolver vazio, ou só ocorrências que você quis manter)
```

## Passo 3 — esvazie o `CHANGELOG.md` e recomece pela sua versão

O `CHANGELOG.md` copiado é o deste template. Apague as entradas antigas e comece do zero, na
versão que o `project()` do seu `meson.build` declara (`0.1.0` se você não mudou o default) — ver
o cabeçalho do próprio arquivo para o formato e o "porquê" das datas saírem do commit, nunca da
mensagem.

## Passo 4 — recompile e rode a suíte, agora com o nome novo

```bash
make clean && make build && make test
```

Se `make test` reclamar de suíte incompleta, o `grep`/`sed` do passo 2 não pegou tudo — confira
`tests/meson.build` e `tests/domain/test_ExampleThreshold.cpp` (que você provavelmente já vai ter
renomeado no próximo passo).

## Passo 5 — substitua o exemplo pela SUA decisão

Isto é o trabalho de verdade, e não tem receita mecânica — mas a ordem sugerida segue a mesma
lógica de camadas de `docs/ARCHITECTURE.md`:

1. **`domain/ExampleThreshold.*`** → apague e escreva a(s) regra(s) pura(s) que o seu modelo
   precisa. Teste-as em `tests/domain/` **antes** de tocar em qualquer coisa que dependa do MIXR
   — é a camada mais barata de iterar.
2. **`ubf/ExampleState.*`** → troque `getValue()`/o que `updateState()` lê do ator pelo que a SUA
   decisão precisa enxergar do mundo.
3. **`ubf/ExampleBehavior.*`** → troque os slots e a chamada a `domain::` pela sua regra. Se
   precisar de mais de uma decisão coordenada, é aqui que entra uma árvore do BehaviorTree.CPP —
   ver "Quando isto não bastar mais" em `docs/ARCHITECTURE.md`.
4. **`ubf/ExampleAction::execute()`** → troque o corpo por comandos de verdade sobre o `Player`
   (`Autopilot`, `StoresMgr`, o que for). **Não apague as duas chamadas ao `xboard`** — são a
   única obrigação de um modelo que falha em silêncio (ver `docs/ARCHITECTURE.md` e
   `docs/CONTRATO.md` seção 3).
5. **`xnative/factory.cpp`** → atualize as três listas (o `if/else`, `NOMES[]`, `METAS[]`) para
   bater com as classes que sobraram/entraram. Se o seu modelo ficar com só 1-2 classes, considere
   eliminar `xnative/` e fazer como `src/mirror.cpp` (o mirror de contrato deste mesmo diretório,
   apagado no Passo 1): a factory inline, sem indireção.
6. **`src/plugin.cpp`** → confira que o primeiro argumento de `MIXR_PLUGIN_DEFINE` é o nome final
   do seu modelo (o passo 2 já deve ter cuidado disso).

## Passo 6 — publique para um cenário conseguir carregar

```bash
make install-host   # copia ./dist -> ../../../plugins/ (o depósito compartilhado com terceiros)
cd ../../..
make install         # sincroniza plugins/ -> dist/ -- SÓ ISSO deixa um cenário enxergar o .so
```

Um `.edl` referencia o seu modelo com um bloco assim (o nome do arquivo e o `provides:` têm que
bater **exatamente** com o que a sua `.so` exporta):

```
( PluginModule  file: "libmeu_modelo.so"
   provides: { ExampleState ExampleBehavior ExampleAction } )
```

(troque os três nomes pelos que sobraram depois do Passo 5). Veja
`src/poc/dis/flight/configs/scenario.edl.in` para um cenário de produção completo usando
este mesmo mecanismo, e `docs/CONTRATO.md` seção 2 para a tabela
completa de "nome de fábrica → classe-base exigida → onde entra".

## Passo 7 — nada a fazer aqui: o build orquestrado já descobre o seu modelo sozinho

`template` nunca aparece no alvo `models:` do Makefile raiz nem em `tests/meson.build` — mas não
por estar de fora de uma lista manual: é porque `template/` é excluído **de propósito** da busca
(`MODELOS_PRODUCAO` no [`../../../../Makefile`](../../../../Makefile) descobre projetos por `find`,
ignorando só `template/`/`tests/`/diretórios de build). O SEU modelo, uma vez copiado para fora de
`template/` (Passo 1), já **entra sozinho** em `make models`/`make test` da raiz — não há linha
nenhuma para adicionar. Confirme com `make models` na raiz: o log deve citar o nome do seu modelo
sem você ter tocado no Makefile.

Isto cobre só o `.so` em si (compilar/testar/instalar). Se você também quer que o modelo apareça
num cenário rodável pelo `./app` (`-folder <pasta> -scenario <nome>`) e, opcionalmente, ganhe
cobertura de teste automática (`tests/meson.build`), isso é um passo separado, documentado em
[`../../../README.md`](../../../README.md), seções 4.1 e 4.2 — não tem relação com este Passo 7.

## Checklist rápido, para revisar antes do primeiro commit

- [ ] `grep -rn 'xtemplate\|Example' include src tests` não acha mais nada do template original
      (a menos que você tenha decidido manter algum nome por coincidência)
- [ ] `make test` passa, com a contagem de casos que você espera (não a herdada do template)
- [ ] `CHANGELOG.md` começou do zero, na versão do seu `meson.build`
- [ ] `nm -D --defined-only dist/lib/mixr-plugins/lib<nome>.so | grep ' T '` mostra **uma** linha
- [ ] `ldd dist/lib/mixr-plugins/lib<nome>.so | grep 'not found'` não mostra nada
- [ ] o `.edl` do seu cenário carrega o `.so` e o dump/tela de status mostra `bt=`/`dec=`
      diferentes de `--`/`0` (a prova de que o `xboard` está sendo escrito de verdade)
