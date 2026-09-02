# `models/plugins/` — depósito de plugins de TERCEIROS (`.so` pré-compilado)

Diferente de [`models/flight/`](../flight/), [`models/missile/`](../missile/) e
[`models/fixtures/stub/`](../fixtures/stub/) — que são projetos Meson **compilados por este
repositório** — esta pasta não compila nada. É um **depósito**: você coloca aqui um `.so` já
pronto, vindo de fora (outra equipe, outro repositório, um fornecedor), e `make models` copia
esse arquivo para `dist/lib/mixr-plugins/`, o mesmo lugar onde os plugins compilados localmente
são instalados — de lá em diante, um cenário carrega esse `.so` exatamente como carrega
`libflight.so`/`libmissile.so`/`libstub.so`, pelo mesmo mecanismo `( PluginModule file: "..."
provides: { ... } )`.

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

`make models` (alvo do [Makefile](../../Makefile) raiz), depois de compilar/instalar
flight+missile+stub, copia qualquer `*.so` encontrado aqui para `dist/lib/mixr-plugins/` —
nenhum passo manual além de soltar o arquivo nesta pasta antes de rodar `make models`/`make
build`. Um `.so` com dependência não resolvida (`ldd` acusando `not found`) faz o alvo falhar
**nesta etapa**, antes de qualquer cenário tentar carregá-lo — mesmo tratamento que os plugins
compilados localmente já recebem.

## Como isso é verificado

`tests/plugin/run_thirdparty_deposit.py` (suíte `plugin`, alvo `plugin-deposito-terceiro` em
`make test`) prova o mecanismo de ponta a ponta: deposita um `.so` de exemplo (o stub, ver
[`models/fixtures/stub/`](../fixtures/stub/)) aqui, copia pra `dist/lib/mixr-plugins/` (a MESMA
cópia que `make models` faz), e roda o cenário de produção contra o resultado — não só confere
que o arquivo chegou em disco, confere que ele decide, escreve no `xboard` e aparece no Tacview.
Limpa os dois arquivos de teste no final; não fica nada vendorizado.

## Por que os `.so` daqui não são versionados

Ao contrário do tile SRTM em `shared/data/terrain/srtm/` (um dado grande mas ESTÁVEL, do próprio
cenário), um plugin de terceiro é, por natureza, externo a este repositório — pode ser
proprietário, mudar de versão independente deste código, ou ser grande demais pra versionar.
`.gitignore` exclui `models/plugins/*.so` (este `README.md` continua rastreado, mantendo a pasta
e o contrato documentados mesmo sem nenhum `.so` presente).
