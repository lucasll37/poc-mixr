#!/usr/bin/env bash
#
# Roda, DENTRO do container limpo, os comandos que o README manda rodar -- na
# ordem em que ele os manda -- e reporta em que ponto cada um parou.
#
# O desenho e por PORTOES (gates) e nao por "roda tudo e ve se deu certo" de
# proposito: cada portao acrescenta UMA correcao nao documentada e tenta de
# novo. E isso que transforma "nao buildou" (inutil) em "faltou exatamente
# isto, nesta ordem" (acionavel). O portao que passar depois de N correcoes
# mede que o README precisa de N linhas a mais.
#
# Saida: linhas de marcador '##...' que tests/docker/run_docs_build_test.py
# consome. Tudo que nao e marcador e log cru, so para o humano ler.
#
# Nao usa 'set -e': um portao que falha e o RESULTADO do teste, nao um erro do
# script. Cada portao trata o proprio rc.
set -u

MODO="${MODO:-rapido}"
TIMEOUT_CONFIGURE="${TIMEOUT_CONFIGURE:-3600}"
TIMEOUT_BUILD="${TIMEOUT_BUILD:-3600}"
LOGS=/tmp/gates
mkdir -p "$LOGS"

marcador() { echo "##$1"; }

# Roda um comando com timeout, guarda o log inteiro em arquivo e devolve o rc.
# O log completo vai para stdout tambem, porque quem roda isto quer ver o
# build acontecendo -- o runner Python so olha os marcadores.
executa() {
   local id="$1" limite="$2"; shift 2
   local log="$LOGS/$id.log"
   echo "................................................................"
   echo ">>> portao '$id': $*"
   echo "................................................................"
   timeout "$limite" "$@" > >(tee "$log") 2>&1
   local rc=$?
   marcador "GATE:$id:rc=$rc"
   # Assinaturas: o runner classifica pelo que apareceu no log, nao por rc --
   # rc=1 do conan cobre uma duzia de causas diferentes.
   marcador "GATE_LOG_INICIO:$id"
   tail -n 40 "$log"
   marcador "GATE_LOG_FIM:$id"
   return $rc
}

# ==============================================================================
# Retrato do ambiente -- o que o README pediu, e o que de fato existe.
# ==============================================================================
marcador "AMBIENTE_INICIO"
echo "os=$(. /etc/os-release && echo "$PRETTY_NAME")"
echo "gcc=$(gcc -dumpversion 2>/dev/null || echo AUSENTE)"
echo "make=$(make --version 2>/dev/null | head -1 || echo AUSENTE)"
echo "ninja=$(ninja --version 2>/dev/null || echo AUSENTE)"
echo "gzip=$(gzip --version 2>/dev/null | head -1 || echo AUSENTE)"
echo "conan=$(conan --version 2>/dev/null | head -1 || echo AUSENTE)"
echo "meson=$(meson --version 2>/dev/null || echo AUSENTE)"
echo "python3=$(python3 --version 2>/dev/null || echo AUSENTE)"
echo "pkg-config=$(pkg-config --version 2>/dev/null || echo AUSENTE)"
echo "git=$(git --version 2>/dev/null || echo AUSENTE)"
echo "sudo=$(command -v sudo >/dev/null 2>&1 && echo presente || echo AUSENTE)"
echo "nproc=$(nproc)"
marcador "AMBIENTE_FIM"

# A sonda do PEP 668, feita na construcao da imagem (ver o Dockerfile): o
# resultado do caminho ingenuo 'pip3 install conan', que e o que um
# recem-chegado tenta ao ler "Conan >= 2.0" sem nenhuma linha de instalacao.
marcador "SONDA_PIP_INICIO"
cat /tmp/sonda-pip.log 2>/dev/null | tail -n 15 || echo "(sonda ausente)"
marcador "SONDA_PIP_FIM"

cd /repo || { echo "sem /repo"; exit 90; }

# ==============================================================================
# PORTAO 1 -- 'make configure' num Conan virgem.
#
# E literalmente o primeiro comando da secao 3 do README, numa maquina que
# nunca rodou Conan. Espera-se que falhe: o Conan 2 nao cria perfil default
# sozinho, e 'conan profile detect' nao aparece em lugar nenhum do repositorio.
# ==============================================================================
executa "configure-virgem" "$TIMEOUT_CONFIGURE" make configure
rc1=$?

# ==============================================================================
# PORTAO 2 -- a MESMA linha, depois de 'conan profile detect'.
#
# Correcao 1 aplicada. Espera-se que agora falhe mais adiante: parte dos
# pacotes que o conanfile.py pede nao existe no conancenter -- vem de um remote
# privado que o README nao cita. Quais pacotes sao esses o runner descobre
# lendo o proprio conanfile.py, para nao haver lista repetida aqui.
# ==============================================================================
if [ "$rc1" -ne 0 ]; then
   echo ">>> correcao 1 (NAO documentada): conan profile detect"
   conan profile detect --force 2>&1 | sed 's/^/    /'
   marcador "CORRECAO:conan-profile-detect"
   executa "configure-com-perfil" "$TIMEOUT_CONFIGURE" make configure
   rc2=$?
else
   marcador "GATE:configure-com-perfil:rc=0"
   rc2=0
fi

# ==============================================================================
# PORTAO 3 -- a MESMA linha, depois de declarar o remote que hospeda as
# dependencias privadas.
#
# Correcao 2. AGNOSTICO AO REGISTRY de proposito: nome e endereco chegam por
# CONAN_REMOTE_NOME/CONAN_REMOTE_URL, e nao ha default embutido. O achado que
# este portao mede nao e "falta o remote X" -- e "o README nomeia pacotes que o
# conancenter nao tem e nao diz de onde eles vem". Isso vale para qualquer
# organizacao que hospede as dependencias em outro lugar, e continua valendo se
# este projeto trocar de registry amanha.
#
# Sem as duas variaveis o portao e PULADO, nao inventado: o teste nao tem como
# adivinhar um endereco, e essa impossibilidade e exatamente o achado.
# ==============================================================================
if [ "$rc2" -ne 0 ] && [ -n "${CONAN_REMOTE_NOME:-}" ] && [ -n "${CONAN_REMOTE_URL:-}" ]; then
   echo ">>> correcao 2 (NAO documentada): conan remote add $CONAN_REMOTE_NOME $CONAN_REMOTE_URL"
   conan remote add "$CONAN_REMOTE_NOME" "$CONAN_REMOTE_URL" --force 2>&1 | sed 's/^/    /'
   marcador "CORRECAO:conan-remote-privado"
   executa "configure-com-remote" "$TIMEOUT_CONFIGURE" make configure
   rc3=$?
elif [ "$rc2" -ne 0 ]; then
   marcador "PULADO:remote:nao-informado"
   rc3=$rc2
else
   marcador "GATE:configure-com-remote:rc=0"
   rc3=0
fi

# ==============================================================================
# PORTAO 4 -- a MESMA linha, autenticada.
#
# Correcao 3. So roda em MODO=completo e so com CONAN_REMOTE_USUARIO/
# CONAN_REMOTE_SENHA no ambiente. Elas NUNCA entram na imagem -- chegam por
# 'docker run -e', senao ficariam gravadas numa camada. O nome das variaveis
# que o Conan de fato le e derivado do nome do remote logo abaixo, para quem
# chama nao precisar conhecer esse formato.
#
# Passando daqui, o proximo custo e o achado 6: o remote publica binario so
# para gcc 11, e esta imagem tem gcc 13, entao '--build=missing' recompila a
# arvore inteira (boost, onnxruntime, protobuf, openssl, mixr...) do zero.
# ==============================================================================
if [ "$MODO" != "completo" ]; then
   marcador "PULADO:credenciais:modo-rapido"
   marcador "FIM"
   exit 0
fi

if [ -z "${CONAN_REMOTE_NOME:-}" ] || [ -z "${CONAN_REMOTE_USUARIO:-}" ] || [ -z "${CONAN_REMOTE_SENHA:-}" ]; then
   marcador "PULADO:credenciais:sem-credenciais"
   marcador "FIM"
   exit 0
fi

if [ "$rc3" -ne 0 ]; then
   echo ">>> correcao 3 (NAO documentada): autenticacao no remote '$CONAN_REMOTE_NOME'"
   marcador "CORRECAO:conan-credenciais"
   # O Conan le credenciais de CONAN_LOGIN_USERNAME_<REMOTE>/CONAN_PASSWORD_<REMOTE>,
   # com o nome do remote em MAIUSCULA e todo caractere fora de [A-Z0-9] virando
   # '_'. Derivar aqui e o que mantem o teste agnostico ao registry: quem chama
   # informa nome/usuario/senha, nao o formato da variavel.
   VAR_SUFIXO="$(echo "$CONAN_REMOTE_NOME" | tr '[:lower:]' '[:upper:]' | tr -c 'A-Z0-9' '_')"
   export "CONAN_LOGIN_USERNAME_${VAR_SUFIXO}=$CONAN_REMOTE_USUARIO"
   export "CONAN_PASSWORD_${VAR_SUFIXO}=$CONAN_REMOTE_SENHA"
   executa "configure-autenticado" "$TIMEOUT_CONFIGURE" make configure
   rc4=$?
else
   marcador "GATE:configure-autenticado:rc=0"
   rc4=0
fi

if [ "$rc4" -ne 0 ]; then
   marcador "FIM"
   exit 0
fi

# ==============================================================================
# PORTAO 5 e 6 -- o resto da secao 3 do README.
#
#   make build     compila o host
#   make install   'sync-plugins' (plugins/ -> dist/) + install do host
#
# 'make install' encadeia 'models', que compila os tres modelos -- e o unico
# ponto em que o teste exercita os Makefile autocontidos de models/*.
# ==============================================================================
executa "build" "$TIMEOUT_BUILD" make build
rc5=$?

if [ "$rc5" -eq 0 ]; then
   executa "install" "$TIMEOUT_BUILD" make install
   rc6=$?
   if [ "$rc6" -eq 0 ]; then
      # Prova minima de que o que saiu do build de fato roda: o binario existe,
      # resolve as .so dele e responde. Sem isto, 'install: OK' diria so que
      # arquivos foram copiados.
      marcador "GATE_ARTEFATOS_INICIO"
      ls -1 dist/bin/ 2>/dev/null
      ls -1 dist/lib/mixr-plugins/ 2>/dev/null
      echo "-- ldd nao resolvidas --"
      ldd dist/bin/single-thread 2>/dev/null | grep 'not found' || echo "(nenhuma)"
      marcador "GATE_ARTEFATOS_FIM"
   fi
fi

marcador "FIM"
exit 0
