# `tests/docker/` — o README basta?

Todo o resto de `tests/` pergunta se o **código** está certo. Esta pasta pergunta outra coisa:
se a **documentação** está certa — se alguém que nunca viu este repositório, numa máquina
recém-instalada, chega de `git clone` a binário rodando seguindo apenas o
[README](../../README.md) §2 (Pré-requisitos) e §3 (Build).

```bash
make check-docs-ubuntu24                    # minutos -- os achados que travam qualquer recem-chegado
make check-docs-ubuntu24 PERFIL=completo    # o mesmo, com as correcoes de pacote aplicadas

# ate 'make build'/'make install' -- HORAS. O remote privado e as credenciais
# sao dados de quem roda, nunca do repositorio:
CONAN_REMOTE_NOME=<nome> CONAN_REMOTE_URL=<url> \
CONAN_REMOTE_USUARIO=<usuario> CONAN_REMOTE_SENHA=<senha> \
  make check-docs-ubuntu24 MODO=completo
```

## Por que um container, e por que 24.04 puro

A máquina de quem desenvolve é a pior testemunha possível: ela já tem o perfil do Conan criado, o
*remote* privado declarado, as credenciais em cache e ~300 pacotes compilados em `~/.conan2`. Todo
passo que falta no README já está feito nela — por construção, o README **sempre** parece
suficiente ali. Só um ambiente sem nenhuma dessas coisas responde a pergunta.

Por isso a base é `ubuntu:24.04` cru, e não uma imagem com *toolchain* pronta: uma imagem que já
trouxesse `conan`/`meson`/`pkg-config` esconderia justamente os dois primeiros achados e passaria
verde provando nada.

E por isso o contexto de build é **`git archive HEAD`**, não a pasta: mandar a árvore de trabalho
enviaria `build/`, `dist/` e `contexts/src/` — gigabytes de artefato que o recém-chegado não tem, e
que fariam o teste mentir a favor do repositório. O harness (esta pasta) é a exceção, e vem da
árvore de trabalho: sem isso não daria para editar o `Dockerfile` e ver o efeito antes de commitar.

## O desenho: portões, não "buildou / não buildou"

`gates.sh` roda os comandos do README **por portões**: tenta, falha, aplica **uma** correção não
documentada, tenta de novo. Cada correção necessária vira um achado.

| portão | comando | correção que o precede |
|---|---|---|
| `configure-virgem` | `make configure` | — (Conan virgem) |
| `configure-com-perfil` | `make configure` | `conan profile detect` |
| `configure-com-remote` | `make configure` | `conan remote add …` (só com `CONAN_REMOTE_*`) |
| `configure-autenticado` | `make configure` | autenticação no remote (só `MODO=completo`) |
| `build` / `install` | `make build`, `make install` | — (só `MODO=completo`) |

É essa escada que transforma *"não buildou"* (inútil) em *"faltou exatamente isto, nesta ordem"*
(acionável). Um teste que só rodasse tudo de uma vez e olhasse o código de saída diria a mesma
coisa — nada — no primeiro erro.

## Agnóstico ao registry — e por quê

Nenhum nome de *remote*, endereço ou credencial está escrito neste repositório. O achado que o
portão 3 mede **não** é *"falta o remote X"* — é *"o `conanfile.py` pede pacotes que o conancenter
não tem, e o README não diz de onde eles vêm"*. Escrito assim, o teste continua valendo se a
organização trocar de registry, e serve a qualquer instalação deste projeto.

Duas consequências práticas:

- **Os pacotes são descobertos, não listados.** `padrao_pacotes_privados()` lê os `self.requires(...)`
  do [`conanfile.py`](../../conanfile.py) e monta a assinatura com o que estiver lá. Uma lista
  escrita no teste envelheceria em silêncio: trocar de dependência deixaria o teste procurando um
  nome que ninguém mais pede, e ele ficaria verde por não achar nada.
- **Sem `CONAN_REMOTE_NOME`/`CONAN_REMOTE_URL` o portão é pulado, não inventado.** O teste não tem
  como adivinhar um endereço — e essa impossibilidade *é* o achado, então ele é reportado do mesmo
  jeito. `CONAN_REMOTE_USUARIO`/`CONAN_REMOTE_SENHA` cobrem a autenticação; o nome das variáveis que
  o Conan de fato lê (`CONAN_LOGIN_USERNAME_<REMOTE>`) é derivado do nome do *remote* dentro do
  container, para quem chama não precisar conhecer esse formato.

## Dois eixos independentes

**`MODO`** — até onde ir. `rapido` (default) para no portão do *remote* privado: minutos, e já
basta para os cinco achados que travam qualquer recém-chegado. `completo` segue até
`make build`/`make install`, exige credenciais no ambiente e leva **horas** — ver o achado
`sem-binarios-para-gcc13`.

**`PERFIL`** — o que a imagem instala. `readme` (default) instala só o que a §2 lista, e é assim que
se mede o que falta. `completo` acrescenta os pacotes de sistema que os achados apontam, e é assim
que se **prova que a lista está completa**: os achados de pacote desaparecem, e só sobram os que são
passo de execução (perfil, *remote*, credenciais). Medir o que falta e provar que a lista basta são
perguntas diferentes, e é por isso que são dois eixos e não um.

## O veredito

Por default o teste compara os achados observados com [`gaps_conhecidos.json`](gaps_conhecidos.json)
e só fica **vermelho quando o conjunto muda** — é uma guarda de regressão, no mesmo espírito de
[`tests/guard/`](../guard/): pega o dia em que o README quebrar mais um degrau, e o dia em que
alguém consertar um e esquecer de atualizar a baseline. `--exigir-suficiente` inverte isso e cobra a
resposta absoluta (verde só com zero achados), que é o alvo de verdade.

Um achado resolvido é um **sucesso**, e o teste pede a atualização explicitamente
(`--atualizar-baseline`) em vez de aceitar em silêncio — senão a baseline viraria um registro do
passado em vez de um contrato.

## Fora de `make test`, de propósito

Mesmo critério de `test-asan`/`test-rl`: depende de Docker e de rede, e o `MODO=completo` leva horas.
`make test` tem de continuar hermético e rápido.

## Armadilhas confirmadas — não redescobrir

1. **Sem TTY, a falha de credencial não se parece com uma falha de credencial.** O Conan detecta que
   o *remote* exige autenticação, tenta **perguntar** o usuário, encontra `stdin` fechado e morre com
   `Package 'mixr/1.0.5' not resolved: EOF when reading a line` — sem a palavra "credencial" em lugar
   nenhum, e fácil de confundir com erro de rede. Casar só em `401|Unauthorized` dava falso negativo;
   por isso a assinatura em `run_docs_build_test.py` cobre também `EOF when reading a line`.
2. **A classificação é por assinatura de texto, nunca por código de retorno.** `conan install`
   devolve `rc=1` para perfil ausente, pacote não encontrado, 401 e falha de compilação — e
   distinguir essas quatro é o teste inteiro.
3. **O achado do PEP 668 tem de ser *medido*, não afirmado.** O `Dockerfile` contorna o bloqueio com
   um venv para conseguir chegar aos achados seguintes; se apenas contornasse, o teste jamais
   provaria que o bloqueio existe. Por isso há uma **sonda** antes do contorno, que tenta o caminho
   ingênuo (`pip3 install conan`) e guarda o log em vez de deixar a imagem quebrar. Medido: `rc=1`,
   `error: externally-managed-environment`.
4. **`docker build -f … -` com o tar por `stdin` não funciona** — o BuildKit recusa com
   *"ambiguous Dockerfile source: both stdin and flag correspond to Dockerfiles"*. O contexto é
   materializado num diretório temporário, que funciona igual no builder clássico e no BuildKit.
5. **Credenciais nunca entram na imagem.** Chegam por `docker run -e` (`CONAN_REMOTE_USUARIO`/
   `CONAN_REMOTE_SENHA`) e são traduzidas dentro do container para as variáveis que o Conan lê. Num
   `ARG`/`ENV` de `Dockerfile` ficariam gravadas numa camada.
6. **`MODO=completo` recompila a árvore inteira.** O *remote* deste projeto publica binário só para
   **gcc 11**; o Ubuntu 24.04 traz gcc 13, o `package_id` não casa e o `--build=missing` do
   `make configure` compila `boost`, `onnxruntime`, `openssl`, `protobuf`, `jsbsim` e `mixr` do
   fonte. Use `--cache-conan <volume>` para pagar esse preço uma vez só.
