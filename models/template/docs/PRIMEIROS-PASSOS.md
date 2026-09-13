# Primeiros passos — de `template` a um modelo com nome próprio

Este roteiro assume que você já rodou, **uma vez**, o pré-requisito que todo projeto de modelo
precisa (publica o SDK de plugin e os pacotes do Conan) **na raiz do repositório**. O comando
abaixo parte de `models/template` (o diretório de trabalho que o Passo 0, logo adiante, também
usa) — se você já estiver na raiz, rode só `make configure && make sdk`, sem o `cd ../..`:

```bash
cd ../.. && make configure && make sdk
```

Sem isso, `check-root` (dependência de `configure`/`build`/`test`/`install` — não de `clean`,
`help` nem `uninstall-host`) falha com uma mensagem dizendo exatamente o que rodar.

## Passo 0 — confirme que o template, do jeito que está, funciona

Antes de mudar qualquer coisa, compile e teste o template como ele é. Se isto falhar, o problema
é do seu ambiente (SDK não publicado, Conan não instalado), não do que você vai escrever depois:

```bash
cd models/template
make build      # -> ./dist/lib/mixr-plugins/libtemplate.so (bare `make` so mostra `make help`)
make test       # 4 casos de domain/ + a árvore (bt/) + a forma do .so (1 símbolo T, deps resolvidas)
```

## Passo 1 — copie e escolha um nome

```bash
cp -r models/template models/players/meu-modelo
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
# 1) o nome do project() e do shared_module() em meson.build -- o MESMO nome
#    (com o MESMO hifen) da pasta do Passo 1. O Meson aceita hifen sem problema
#    (confirmado: A-4/ e C-130/ usam project('A-4', ...)/project('C-130', ...) e
#    publicam libA-4.so/libC-130.so) -- nao ha por que trocar para underscore aqui.
sed -i "s/'template'/'meu-modelo'/g" meson.build

# 2) o namespace C++ (troque xtemplate -> xmeumodelo, ou o nome que preferir,
#    em TODOS os arquivos de uma vez)
grep -rl 'xtemplate' include src tests | xargs sed -i 's/xtemplate/xmeumodelo/g'

# 3) o primeiro argumento de MIXR_PLUGIN_DEFINE em src/plugin.cpp -- o sed
#    do passo 1 já pegou isto, porque é a MESMA string 'template'. Confira:
grep -n 'MIXR_PLUGIN_DEFINE' src/plugin.cpp

# 4) a linha ROOT do Makefile -- SÓ SE você mudou a profundidade em relação
#    à raiz do repositório. O template mora em models/template/ (dois níveis).
#    Copiado para models/players/meu-modelo/ (três níveis), troque
#    'ROOT := $(abspath ../..)' por 'ROOT := $(abspath ../../..)'.
#    ('make new-model' calcula essa linha sozinho -- isto vale só para a
#    cópia manual.)
```

Confirme que nada ficou para trás:

```bash
grep -rn 'xtemplate\|"template"' include src tests meson.build src/plugin.cpp
# (deve devolver vazio, ou só ocorrências que você quis manter)
```

`make check-organization` confere os quatro pontos deste passo sozinho — sem grep manual, e sem precisar
compilar nada: `namespace-aninhado` (se o `sed` do namespace pegou TODOS os arquivos) e
`nome-plugin-consistente` (se `project()`/`shared_module()`/`MIXR_PLUGIN_DEFINE` continuam usando
o MESMO nome novo). É um linter opcional — não substitui `make test` — mas é o jeito mais rápido
de descobrir um `xtemplate` esquecido antes do primeiro `make build`.

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
   — é a camada mais barata de iterar. (O exemplo usa `>=` nos dois limiares —
   `engaged ? value >= offValue : value >= onValue` — e os defaults `onValue=1.0`/`offValue=0.0`
   fazem ele parecer sempre "ENGAGED" se o `.edl` não declarar os dois slots; se a regra nova
   também for um limiar com histerese, decida a convenção de borda de propósito, não por acidente.)
2. **`ubf/ExampleState.*`** → troque `getValue()`/o que `updateState()` lê do ator pelo que a SUA
   decisão precisa enxergar do mundo. (O exemplo lê `Player::getAltitudeM()` só porque é o único
   getter presente em QUALQUER `Player`, inclusive um que não seja veículo aéreo — não é uma dica
   de que sua decisão deveria usar altitude.)
3. **`bt/nodes/Example*.*` + `configs/example_tree.xml`** → é aqui que a decisão de fato mora.
   Um nó por condição ou ação, registrado em `src/bt/bt_factory.cpp`, lendo o que precisa pela
   interface `bt/DecisionContext.hpp` (acrescente um getter lá se faltar algo). A forma da
   decisão — a ordem das prioridades — é o XML, não o C++: dá para editá-la no Groot
   (`make open-groot`) sem recompilar. **Depois de registrar um nó novo, rode `make update-bt`**;
   o teste `tree-model-sync` cobra isso sozinho, mas o erro que ele evita (o Groot recusando a
   árvore com *"This model has not been registered"*) é chato de diagnosticar sem saber a causa.
   `make create-bt` gera um `configs/bt.xml` vazio já com a paleta deste modelo, se você preferir
   começar uma árvore do zero no editor.
4. **`ubf/ExampleBehavior.*`** → troque os slots pelos parâmetros que o SEU `.edl` vai configurar.
   O corpo de `genAction()` normalmente não muda: ele carrega a árvore, tica uma vez e devolve a
   ação com o rótulo que a árvore produziu.
5. **`ubf/ExampleAction::execute()`** → troque o corpo por comandos de verdade sobre o `Player`
   (`Autopilot`, `StoresMgr`, o que for). **Não apague as duas chamadas ao `xboard`** — são a
   única obrigação de um modelo que falha em silêncio (ver `docs/ARCHITECTURE.md` e
   `docs/CONTRATO.md` seção 3).
6. **`xnative/factory.cpp`** → atualize as três listas (o `if/else`, `NOMES[]`, `METAS[]`) para
   bater com as classes que sobraram/entraram. Se o seu modelo ficar com só 1-2 classes, considere
   eliminar `xnative/` e fazer como `src/mirror.cpp` (o mirror de contrato deste mesmo diretório,
   apagado no Passo 1): a factory inline, sem indireção.
7. **`src/plugin.cpp`** → confira que o primeiro argumento de `MIXR_PLUGIN_DEFINE` é o nome final
   do seu modelo (o passo 2 já deve ter cuidado disso).

## Passo 6 — publique para um cenário conseguir carregar

```bash
make install-host   # copia ./dist -> ../../plugins/ (o depósito compartilhado com terceiros)
cd ../..
make install         # sincroniza plugins/ -> dist/ -- SÓ ISSO deixa um cenário enxergar o .so
```

Um `.edl` referencia o seu modelo com um bloco assim (o nome do arquivo e o `provides:` têm que
bater **exatamente** com o que a sua `.so` exporta):

```
( PluginModule  file: "libmeu-modelo.so"
   provides: { ExampleState ExampleBehavior ExampleAction } )
```

(troque os três nomes pelos que sobraram depois do Passo 5). Veja
`src/poc/dis/flight/configs/scenario.edl.in` para um cenário de produção completo usando
este mesmo mecanismo, e `docs/CONTRATO.md` seção 2 para a tabela
completa de "nome de fábrica → classe-base exigida → onde entra".

## Passo 7 — nada a fazer aqui: o build orquestrado já descobre o seu modelo sozinho

`template` nunca entra na lista `MODELOS_PRODUCAO` (a descoberta automática por `find`, em
[`../../../Makefile`](../../../Makefile)) — mas isso não significa que `template/` fique de fora
do alvo `models:` como um todo: a receita do alvo chama `$(MAKE) -C models/template install-host
...` **à parte**, depois do laço sobre `MODELOS_PRODUCAO`, porque o segundo artefato do template
(`libtemplate_mirror.so`, o mirror de contrato) é usado pelos próprios testes de plugin do host
(`tests/meson.build` referencia `libtemplate_mirror.so` diretamente, em mais de um teste). O que
importa para o SEU modelo é só a parte da lista automática: uma vez copiado para fora de
`template/` (Passo 1), ele **entra sozinho** em `MODELOS_PRODUCAO` e portanto em `make
models`/`make test` da raiz — não há linha nenhuma para adicionar. Confirme com `make models` na
raiz: o log deve citar o nome do seu modelo sem você ter tocado no Makefile.

Isto cobre só o `.so` em si (compilar/testar/instalar). Se você também quer que o modelo apareça
num cenário rodável pelo `./app` (`-folder <pasta> -scenario <nome>`) e, opcionalmente, ganhe
cobertura de teste automática (`tests/meson.build`), isso é um passo separado, documentado em
[`../../../CONTRIBUTING.md`](../../../CONTRIBUTING.md), seções 5.2 e 5.4 — não tem relação
com este Passo 7.

Antes do primeiro commit, vale ler também [`../CLAUDE.md`](../CLAUDE.md) (este projeto,
não o da raiz): registra armadilhas do próprio scaffold que nenhum outro `.md` cobre — em
particular, `.vscode/launch.json`/`meson_options.txt` **não** são reescritos por
`scripts/models.sh` (ficam com caminho/descrição do template original) e a suíte de testes
copiada cobre `domain/`, a árvore de comportamento (`bt/`) e a forma do `.so` publicado, com
**zero** cobertura automática da integração ponta a ponta das classes `ubf/` concretas
(percepção→regra→rótulo) até você escrever a sua.

## Checklist rápido, para revisar antes do primeiro commit

- [ ] `grep -rn 'xtemplate\|Example' include src tests` não acha mais nada do template original
      (a menos que você tenha decidido manter algum nome por coincidência)
- [ ] `make check-organization` passa sem FALHA (organização interna — namespace, as três listas da factory,
      as cinco peças de todo projeto de modelo (`tests/`, `docs/`, `README.md`, `CHANGELOG.md`,
      `Makefile`), etc.; ver `tools/check_organization.py`)
- [ ] `make test` passa, com a contagem de casos que você espera (não a herdada do template)
- [ ] `CHANGELOG.md` começou do zero, na versão do seu `meson.build`
- [ ] `nm -D --defined-only dist/lib/mixr-plugins/lib<nome>.so | grep ' T '` mostra **uma** linha
- [ ] `ldd dist/lib/mixr-plugins/lib<nome>.so | grep 'not found'` não mostra nada
- [ ] o `.edl` do seu cenário carrega o `.so` e o dump/tela de status mostra `bt=`/`dec=`
      diferentes de `--`/`0` (a prova de que o `xboard` está sendo escrito de verdade)
