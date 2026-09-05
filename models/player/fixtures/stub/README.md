# `stub` — um modelo mínimo e completo

## O que é isto

A aplicação principal deste repositório (o "host") não decide nada sozinha: ela carrega a lógica
de simulação — percepção, decisão, ação — de uma biblioteca compartilhada (`.so`) compilada à
parte e aberta em tempo de execução, sem que o host precise conhecer o código-fonte dela. Essa
biblioteca é o que este repositório chama de **modelo**.

Este diretório é um modelo completo, do tamanho mínimo possível (~270 linhas em
[`src/stub.cpp`](src/stub.cpp)): voa reto e nivelado, sem nenhuma lógica de decisão interessante.
O que importa não é o comportamento — é que esse arquivo, usando só a API pública publicada para
quem escreve modelos (o "SDK de plugin") e a biblioteca de simulação MIXR, cumpre **tudo** o que
um modelo precisa cumprir para o host carregá-lo e rodar de ponta a ponta com ele.

A lista completa dessas obrigações — o que de fato é exigido, incluindo a única que falha sem
nenhum erro visível — está em [`docs/CONTRATO.md`](docs/CONTRATO.md). Leia aquele documento antes
de mexer no código; este README só cobre como compilar, testar e reaproveitar este diretório.

## Compilar e testar, sozinho

Este diretório é um projeto de build **independente** (tem seu próprio arquivo de projeto Meson e
seu próprio `Makefile`) — não é preciso abrir o resto do repositório para compilar só isto.

O único pré-requisito é que o projeto principal do repositório (a pasta acima de `models/`, três
níveis para cima daqui) já tenha rodado, **uma vez**, os dois passos que publicam o que um modelo
precisa para linkar: o SDK de plugin (os headers e as bibliotecas pequenas que fazem a ponte com o
host) e os pacotes de terceiros que o MIXR usa.

```bash
# uma vez, na raiz do repositorio:
cd ../../.. && make configure && make sdk

# daqui em diante, só aqui dentro:
cd models/player/fixtures/stub
make                  # compila -> ./dist/lib/mixr-plugins/libstub.so
make test             # confere a forma do .so (1 simbolo exportado, sem dependencia faltando)
make install-host     # copia o .so para a raiz do repositorio, onde os cenarios o procuram
```

`make help` lista todos os alvos. `./build` e `./dist` nascem e ficam dentro **deste** diretório —
nada aqui escreve fora dele, exceto `make install-host`, que existe justamente para fazer essa
única cópia (veja a próxima seção).

## Por que existe um passo separado para "publicar no host"

Um cenário só encontra um `.so` de modelo dentro de uma pasta específica, relativa à raiz do
repositório inteiro (não à raiz deste diretório) — é para lá que o campo `searchPaths:` do
carregador de plugin aponta. Compilar aqui dentro (`make`/`make test`) já é suficiente para editar
o código e conferir que ele continua válido; para que um cenário de verdade consiga carregar o
resultado, o `.so` também precisa existir naquela pasta da raiz — e `make install-host` é o único
alvo deste `Makefile` que copia algo para lá.

## Usando este diretório como ponto de partida para um modelo novo

Copiar esta pasta inteira já traz, prontos, os cinco itens que qualquer novo modelo neste
repositório precisa ter (um `Makefile` de build autocontido como este, uma pasta `tests/`, uma
pasta `docs/`, um `README.md` e um `CHANGELOG.md`), além de um arquivo de projeto Meson mínimo que
já satisfaz o empacotamento exigido. Os cinco são cobrados pela guarda
[`tests/guard/check_modelo_estrutura.sh`](../../../../tests/guard/check_modelo_estrutura.sh), que
descobre os projetos sozinha — um modelo novo já nasce cobrado:

```bash
cp -r models/player/fixtures/stub models/meu-modelo
mv models/meu-modelo/src/stub.cpp models/meu-modelo/src/meu_modelo.cpp
sed -i "s/'stub'/'meu_modelo'/g; s|files('src/stub.cpp')|files('src/meu_modelo.cpp')|" \
    models/meu-modelo/meson.build
```

Uma correção manual é necessária no `Makefile` copiado: ele calcula o caminho até a raiz do
repositório a partir de **onde este diretório está** (três níveis acima, por estar dentro de
`models/player/fixtures/`). Um modelo novo criado direto em `models/meu-modelo/` está só dois níveis
abaixo da raiz — abra o `Makefile` copiado e troque a linha `ROOT := $(abspath ../../..)` por
`ROOT := $(abspath ../..)`. Sem esse ajuste, `make` vai procurar o repositório no diretório
**pai** dele e falhar dizendo que não encontrou o SDK, mesmo que ele exista.

## O que tem em cada arquivo

| caminho | o que é |
|---|---|
| [`src/stub.cpp`](src/stub.cpp) | a implementação de referência — leia junto com `docs/CONTRATO.md` |
| [`docs/CONTRATO.md`](docs/CONTRATO.md) | a lista completa do que um modelo precisa fazer |
| [`tests/`](tests/) | confere que o `.so` compilado tem a forma certa (um símbolo exportado, dependências resolvidas) |
| [`CHANGELOG.md`](CHANGELOG.md) | o que mudou neste fixture, e por quê — ao copiar a pasta, esvazie e recomece pela versão do seu `meson.build` |
| `meson.build` | o projeto de build deste modelo |
| `Makefile` | compila, testa e publica este projeto sozinho, sem precisar do resto do repositório |

## Se você quiser mais contexto sobre o repositório inteiro

Nada acima depende disto, mas se quiser entender como os modelos se encaixam na aplicação maior —
outros modelos que existem, como um cenário aponta para um `.so`, o fluxo de build orquestrado que
constrói tudo de uma vez — o ponto de entrada é [`../../README.md`](../../../README.md).
