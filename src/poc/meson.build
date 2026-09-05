# ==============================================================================
# As POCs NAO SAO MAIS ALVOS DE BUILD -- nao ha subdir() nenhum aqui.
#
# Cada pasta sob src/poc/ e hoje so DADO: 'configs/' (o cenario EDL), 'data/'
# (o que a execucao escreve) e o README. Quem as executa e o ./app, o runner
# unico -- 'app -scenario <chave>' (ver app/ScenarioCatalog.hpp) ou
# 'app -f <arquivo>' para um cenario fora do catalogo.
#
# POR QUE: cada poc carregava uma copia byte a byte da camada de aplicacao
# (~1.500 linhas: include/app/ + src/app/ + mixr_factory), sustentada pela
# guarda tests/guard/check_duplication.sh. Era duplicacao real, e ja tinha
# cobrado o preco: a correcao de ordem do encerramento (calar a thread T/C
# antes do SHUTDOWN_EVENT) teve de ser copiada, uma a uma, para as cinco.
# Hoje a duplicacao esta dissolvida POR CONSTRUCAO -- do mesmo jeito que
# aconteceu quando o MODELO saiu para models/A4 -- e a guarda que a
# sustentava foi aposentada junto.
#
# PROVA DE NEUTRALIDADE (medida na troca): o dump 'frame=' de single-thread,
# multi-thread, python-flight, onnx-policy e built-in_mixr_1 rodados pelo
# ./app saiu BYTE-IDENTICO ao dos binarios proprios que existiam antes.
#
# Este arquivo continua existindo porque src/meson.build o chama por
# subdir('./poc') -- e o lugar onde esta escrito o porque de ele estar vazio.
# ==============================================================================
