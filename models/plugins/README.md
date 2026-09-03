# `models/plugins/` — o depósito COMPARTILHADO de todo plugin (local ou de TERCEIRO)

Diferente de [`models/flight/`](../flight/), [`models/missile/`](../missile/) e
[`models/fixtures/stub/`](../fixtures/stub/) — que são projetos Meson **compilados por este
repositório** — esta pasta não compila nada. É um **depósito**.

Por isso a regra das cinco peças de um projeto de modelo (`tests/`, `docs/`, `README.md`,
`CHANGELOG.md` e `Makefile`, ver [../README.md](../README.md)) **não** se aplica aqui, e a guarda
`tests/guard/check_modelo_estrutura.sh` pula esta pasta de propósito: não há fonte, não há build,
e o `.so` de terceiro é documentado por quem o compilou.

**Dois tipos de conteúdo pousam aqui, e a partir do momento em que pousam são
INDISTINGUÍVEIS:**

1. **Local**: `flight`/`missile`/`stub` — cada um constrói no PRÓPRIO projeto (`./build`,
   `./dist`) e o alvo `install-host` de cada `Makefile` deposita SÓ o resultado final aqui
   (`.so` flat; `data/flight/` para a árvore + a aeronave, a única exceção ao depósito flat).
   `make models`, na raiz, chama os três `install-host` em sequência.
2. **Terceiro**: você coloca aqui um `.so` já pronto, vindo de fora (outra equipe, outro
   repositório, um fornecedor).

Nos dois casos, **nada aqui é visível a um cenário ainda** — `make install` (alvo `sync-plugins`
do Makefile raiz) é quem copia `models/plugins/*.so` para `dist/lib/mixr-plugins/` e
`models/plugins/data/` para `dist/share/mixr-plugins/`, o mesmo lugar de sempre. De lá em diante,
um cenário carrega esse `.so` exatamente como carrega `libflight.so`/`libmissile.so`/
`libstub.so`, pelo mesmo mecanismo `( PluginModule file: "..." provides: { ... } )`.

**Por que unificar os dois**: compilar um modelo (local) nunca precisou saber onde o HOST guarda
os artefatos dele — só RODAR algo precisa disso, e isso só acontece em `dlopen()`, em tempo de
execução. Antes, `flight`/`missile`/`stub` escreviam direto em `dist/`, acoplando "compilar o
modelo" a "onde o host instala" sem necessidade — e só o terceiro passava por este depósito
intermediário. Agora os quatro passam pelo mesmo lugar, com a mesma regra.

## O que colocar aqui

Qualquer `arquivo.so` que já cumpra o contrato de plugin deste repositório — ver
[`models/fixtures/stub/docs/CONTRATO.md`](../fixtures/stub/docs/CONTRATO.md) para a lista
completa (empacotamento via `MIXR_PLUGIN_DEFINE`, `provides:` batendo exatamente com o que o
cenário espera, publicação em `shared/xboard` para o host mostrar a decisão). Em especial, o
`.so` precisa ter sido compilado contra o **mesmo SDK** que este repositório publica (`make sdk`
→ `dist/include`, `dist/lib/pkgconfig/poc-mixr-sdk.pc`) — o contrato de ABI
(`shared/xplugin/PluginAbi.hpp`) confere isso em tempo de carga (versão do contrato, ABI de
`std::string`, `sizeof(models::Player)`) e recusa um `.so` incompatível com uma mensagem clara,
em vez de um crash silencioso.

Não há subpastas nem convenção de nome exigida — o **nome do arquivo** é o que vai no `file:` do
cenário (`( PluginModule file: "meuplugin.so" ... )`), então chame como quiser, só evite colidir
com os nomes que este repositório já usa (`libflight.so`, `libflight_tc.so`, `libmissile.so`,
`libstub.so`).

## Integração com o build

Não é `make models` quem copia daqui para `dist/` — `make models` só CRIA o conteúdo desta pasta
(o seu, de terceiro, mais o que `flight`/`missile`/`stub` acabaram de depositar via o próprio
`install-host`). Quem copia `models/plugins/*.so` (+ `models/plugins/data/`) para
`dist/lib/mixr-plugins/` (+ `dist/share/mixr-plugins/`) é o alvo `sync-plugins` do
[Makefile](../../Makefile) raiz, chamado por `make install` — nenhum passo manual além de soltar
o arquivo nesta pasta antes de rodar `make install`/`make test`. Um `.so` com dependência não
resolvida (`ldd` acusando `not found`) faz `sync-plugins` falhar **nesta etapa**, antes de
qualquer cenário tentar carregá-lo — mesmo tratamento pra terceiro e pra plugin compilado
localmente, já que os dois passam pelo mesmo laço de cópia.

## Como isso é verificado

`tests/plugin/run_thirdparty_deposit.py` (suíte `plugin`, alvo `plugin-deposito-terceiro` em
`make test`) prova o mecanismo de ponta a ponta: deposita um `.so` de exemplo (o stub, ver
[`models/fixtures/stub/`](../fixtures/stub/)) aqui, copia pra `dist/lib/mixr-plugins/` (a MESMA
cópia que `sync-plugins` faz), e roda o cenário de produção contra o resultado — não só confere
que o arquivo chegou em disco, confere que ele decide, escreve no `xboard` e aparece no Tacview.
Limpa os dois arquivos de teste no final; não fica nada vendorizado.

## Por que os `.so` daqui não são versionados

Ao contrário do tile SRTM em `shared/data/terrain/srtm/` (um dado grande mas ESTÁVEL, do próprio
cenário), um plugin de terceiro é, por natureza, externo a este repositório — pode ser
proprietário, mudar de versão independente deste código, ou ser grande demais pra versionar.
`.gitignore` exclui `models/plugins/*.so` (este `README.md` continua rastreado, mantendo a pasta
e o contrato documentados mesmo sem nenhum `.so` presente).
