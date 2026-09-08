#!/usr/bin/env python3
"""PDU DIS malformado/truncado, enviado cru por UDP contra o cenario de PRODUCAO.

mixr::dis::NetIO e dependencia BINARIA (nao ha o que corrigir por dentro deste
repositorio -- mesma regra ja aplicada a libs/xlog/libs/xmsg no CLAUDE.md).
Este teste so pode DOCUMENTAR/TRAVAR o comportamento observado, nunca
"consertar" o parser.

POR QUE O VENUE E ESTE, especificamente (nao uma fixture hermetica): TODAS as
fixtures de tests/scenario/make_fixture.py removem 'networks:' de proposito
(CLAUDE.md, secao "Testes automatizados", armadilha 1 -- '-deterministic' com
o cenario de producao NAO e hermetico, abre a porta DIS de verdade e processa
PDUs que chegarem durante station->updateData(dt)). E exatamente esse caminho
que precisa ser exercitado aqui -- nenhuma fixture alcanca netInputHander().

O QUE FOI LIDO NO FONTE VENDORIZADO antes de escrever isto (contexts/src/mixr/
src/interop/dis/), nao suposto pela assinatura do header:

  * NetIO::netInputHander() (NetIO.cpp:237-431) le bytes crus pra um slot de
    tamanho FIXO ('inputBuffer[500][384]', MAX_PDU_SIZE=1536) e despacha por
    'header->PDUType' -- o retorno de recvData() (quantos bytes CHEGARAM de
    verdade) e descartado logo depois do teste '> 0'. Nao ha checagem de
    tamanho nenhuma entre "isto e um PDUType valido" e "isto tem bytes
    suficientes pra ser a struct que aquele PDUType promete".
  * Nib::processArticulationParameters() (Nib_entity_state.cpp:218-231) itera
    'for (i = 0; i < pdu->numberOfArticulationParameters; i++)' SEM TETO --
    os dois loops IRMAOS no mesmo arquivo/vizinhanca sao limitados
    (MAX_EM_SYSTEMS, MAX_EM_BEAMS), este nao. 'numberOfArticulationParameters'
    e um byte cru vindo da rede (0..255); getArticulationParameter(idx)
    (pdu.hpp) e ponteiro-aritmetica pura, sem checar quantos bytes o
    datagrama de fato trouxe.

O caso (c) abaixo e o alvo de maior interesse: um EntityStatePDU do TAMANHO
CERTO (144 bytes, sizeof(EntityStatePDU) confirmado via offsetof()) mas
'numberOfArticulationParameters=255' -- o campo promete 255*16=4080 bytes de
VpArticulatedPart que nunca foram enviados. O loop vulneravel NAO fica atras
de nenhum gate de Ntm/criacao de fantasma -- EntityStatePDU::swapBytes()
(pdu.hpp:104-123) roda incondicionalmente logo apos o dispatch por PDUType
('if (isNotNetworkByteOrder()) pPdu->swapBytes();', NetIO.cpp:~266, ANTES do
teste de siteID/applicationID que decide se um fantasma e' criado), e a
ULTIMA parte dela e exatamente 'for (i=0; i<numberOfArticulationParameters;
i++) getArticulationParameter(i)->swapBytes();' -- ESCRITA fora dos 144 bytes
reais, nao so leitura. Neste host (x86_64, little-endian),
NetHandler::checkByteOrder() garante isNotNetworkByteOrder()==true, entao a
chamada acontece sempre que o PDUType/exerciseID batem -- nao depende de
'inputEntityTypes:' casar nem de um fantasma aparecer no dump. E' por isso
que a assercao deste teste (so' "nao crasha") continua valida mesmo sem
nenhum player fantasma novo no dump: NAO ter fantasma NAO significa que o
loop vulneravel nao rodou.

Medido, nao suposto: com numberOfArticulationParameters=255 (o MAXIMO
possivel pro campo, um uint8_t), o excedente e' 4080-(1536-144)=2688 bytes
ALEM do proprio slot de 1536 bytes de 'inputBuffer[j0]' -- mas ainda
BEM dentro do array contiguo inteiro ('inputBuffer[500][384]' =~ 768000
bytes), entao a escrita fora dos limites corrompe slots VIZINHOS de
'inputBuffer' (um defeito de memoria real, UB de verdade) sem tocar memoria
fora do objeto NetIO -- coerente com rodar sem crashar, confirmado em
multiplas repeticoes reais (nao e' lacuna do teste, e' o tamanho do array
tornando ESTE overflow especifico nao-fatal NESTA configuracao).

Os offsets/tamanhos abaixo (EntityStatePDU=144, entityType=20,
numberOfArticulationParameters=19, PDUType=2 dentro do header) NAO sao
adivinhados -- vieram de um probe C++ com offsetof() contra os headers REAIS
instalados pelo Conan (nao ha 'pragma pack' nestas structs; a sorte de todo
campo cair em fronteira alinhada e' o que torna hand-rolled struct.pack()
seguro aqui -- confirmado, nao suposto). O byte order (big-endian/'network')
tambem foi confirmado no fonte, nao suposto pela convencao DIS: NetHandler.cpp
inicializa 'netByteOrder = checkByteOrder()' (o proprio host), e
'isNotNetworkByteOrder()' == true neste host faz TODO EntityStatePDU recebido
passar por swapBytes() -- ou seja o codigo assume fio em big-endian de
verdade, e struct.pack('>...') aqui reflete exatamente isso.

ASSERCAO: so' que o processo NAO TERMINA POR SINAL (SIGSEGV/SIGABRT/etc) --
teste NEGATIVO puro, no molde de run_app_quit_test.py/run_plugin_negatives.py.
Sem diagnostico esperado no stderr (nao e' um plugin de teste deste
repositorio, e' o binario de producao recebendo lixo de rede).

Se algum dos casos travar: e' 'limitacao conhecida do MIXR vendorizado', nao
um bug deste repositorio -- decidir separadamente (fora deste script) se vira
'expected fail' documentado ou reforca a decisao ja tomada de manter cenarios
de producao sempre atras de rede confiavel. Resultado medido hoje: os 5
casos passam (nenhum derruba o processo) -- este teste fica de guarda contra
regressao futura (uma versao nova do MIXR vendorizado, ou uma mudanca no
tamanho de 'inputBuffer', que torne este overflow especifico fatal).
"""

import argparse
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]

PORTA_DIS = 3000
PORTA_IGNORADA = 3002          # ignoreSourcePort: do proprio flight -- nunca usar como origem
SITE_ID, APPLICATION_ID, EXERCISE_ID = 2, 1, 1     # do bloco 'networks:' do cenario
DIS_ENTITY_TYPE = (1, 2, 225, 1, 99, 0, 0)         # kind domain country category subcat specific extra

PDU_ENTITY_STATE = 1
MAX_PDU_SIZE = 1536

# offsetof() reais (nao adivinhados -- ver o cabecalho deste arquivo).
OFF_PDU_TYPE = 2
SIZEOF_ENTITY_STATE_PDU = 144


def pdu_entity_state_valido(num_articulation_params=0):
    """Um EntityStatePDU de 144 bytes, sintaticamente correto e casando o
    disEntityType/siteID/applicationID/exerciseID do cenario -- o suficiente
    pra passar pelos filtros de NetIO e (se o Ntm casar) materializar um
    fantasma. 'num_articulation_params' e o campo que MENTE sobre quanto
    payload viria a seguir -- sem de fato anexar esse payload.
    """
    header = struct.pack(
        ">BBBBIHBB",
        6,               # protocolVersion (IEEE 1278.1, arbitrario)
        EXERCISE_ID,     # exerciseIdentifier -- tem que bater com o do cenario
        PDU_ENTITY_STATE,
        1,               # protocolFamily (nao verificado no despacho)
        0,               # timeStamp
        SIZEOF_ENTITY_STATE_PDU,
        0, 0,            # status, padding
    )
    entity_id = struct.pack(">HHH", 65000, 65000, 42)   # site/app != self, ID arbitrario
    force_and_articulation = struct.pack(">BB", 1, num_articulation_params)
    entity_type = struct.pack(">BBHBBBB", *DIS_ENTITY_TYPE)
    alternative_type = struct.pack(">BBHBBBB", 0, 0, 0, 0, 0, 0, 0)
    velocity = struct.pack(">fff", 0.0, 0.0, 0.0)
    location = struct.pack(">ddd", 6378137.0, 0.0, 0.0)   # ECEF plausivel (raio equatorial)
    orientation = struct.pack(">fff", 0.0, 0.0, 0.0)
    appearance = struct.pack(">I", 0)
    dead_reckoning = struct.pack(">B", 0)
    other_params = b"\x00" * 15
    dr_accel = struct.pack(">fff", 0.0, 0.0, 0.0)
    dr_angular_vel = struct.pack(">fff", 0.0, 0.0, 0.0)
    marking = struct.pack(">B", 0) + b" " * 11
    capabilities = struct.pack(">I", 0)

    pdu = (header + entity_id + force_and_articulation + entity_type + alternative_type
           + velocity + location + orientation + appearance + dead_reckoning
           + other_params + dr_accel + dr_angular_vel + marking + capabilities)
    assert len(pdu) == SIZEOF_ENTITY_STATE_PDU, f"layout errado: {len(pdu)} != {SIZEOF_ENTITY_STATE_PDU}"
    return pdu


def payloads():
    casos = []

    # (a) header truncado -- so 4 dos 12 bytes de PDUHeader.
    casos.append(("header truncado (4 de 12 bytes)", pdu_entity_state_valido()[:4]))

    # (b) header completo, PDUType=ENTITY_STATE, corpo bem menor que
    #     sizeof(EntityStatePDU) -- o parser ainda tenta o cast completo.
    casos.append(("EntityStatePDU truncado (20 de 144 bytes)", pdu_entity_state_valido()[:20]))

    # (c) o alvo de maior interesse: tamanho CERTO, mas o campo
    #     numberOfArticulationParameters promete 255*16=4080 bytes que nunca
    #     foram enviados -- ver o cabecalho do arquivo.
    casos.append(("EntityStatePDU 144 bytes com numberOfArticulationParameters=255",
                  pdu_entity_state_valido(num_articulation_params=255)))

    # (d) datagrama maior que MAX_PDU_SIZE -- exercita o truncamento que
    #     recvData() aplica contra o 'maxSize' fixo.
    grande = pdu_entity_state_valido() + (b"\x41" * (MAX_PDU_SIZE + 1 - SIZEOF_ENTITY_STATE_PDU))
    assert len(grande) == MAX_PDU_SIZE + 1
    casos.append((f"datagrama de {len(grande)} bytes (MAX_PDU_SIZE+1)", grande))

    # (e) PDUType desconhecido -- cai em processUserPDU() (so devolve true).
    #     Controle NEGATIVO/baseline: nao deveria fazer diferenca nenhuma.
    lixo = bytearray(pdu_entity_state_valido())
    lixo[OFF_PDU_TYPE] = 255
    casos.append(("PDUType desconhecido (255)", bytes(lixo)))

    return casos


def sobe_processo(binario, frames):
    return subprocess.Popen(
        [binario, "-f", "src/poc/dis/flight/configs/scenario.edl.in",
         "-threads", "1", "-deterministic", str(frames)],
        cwd=RAIZ, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1,
    )


def espera_rede_pronta(proc, prazo=15.0):
    """Le stdout linha a linha ate ver a confirmacao de que a rede DIS subiu
    -- sem isso, mandar os pacotes cedo demais e desperdicar a janela curta
    (o cenario roda MILHARES de frames por segundo em modo deterministico).
    """
    fim = time.monotonic() + prazo
    linhas = []
    while time.monotonic() < fim:
        linha = proc.stdout.readline()
        if not linha:
            return False, linhas
        linhas.append(linha)
        if "netInput Initialize OK" in linha:
            return True, linhas
    return False, linhas


def roda_um_caso(binario, frames, nome, payload):
    proc = sobe_processo(binario, frames)
    try:
        pronto, linhas_iniciais = espera_rede_pronta(proc)
        if not pronto:
            # FALHA, nao aviso-e-segue: um "pulei porque a rede nao subiu"
            # que devolve sucesso e' exatamente o passar vacuo que este
            # projeto evita em toda guarda (CLAUDE.md, secao "Testes
            # automatizados") -- nunca visto acontecer nas repeticoes desta
            # sessao, mas silenciar teria o efeito pratico de nunca testar
            # nada e ainda assim reportar verde.
            print(f"FALHA [{nome}]: nao vi 'netInput Initialize OK' a tempo -- rede nao subiu")
            resto = proc.communicate(timeout=30)[0]
            return False, "".join(linhas_iniciais) + resto

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.bind(("127.0.0.1", 0))
            if sock.getsockname()[1] == PORTA_IGNORADA:
                sock.close()
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.bind(("127.0.0.1", 0))
            sock.sendto(payload, ("127.0.0.1", PORTA_DIS))
        finally:
            sock.close()

        try:
            saida_resto, _ = proc.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            proc.kill()
            saida_resto, _ = proc.communicate(timeout=10)
            print(f"FALHA [{nome}]: processo nao terminou em 30s apos o envio -- pendurado")
            return False, "".join(linhas_iniciais) + saida_resto

        rc = proc.returncode
        saida = "".join(linhas_iniciais) + saida_resto

        # Um processo morto por SINAL sai com rc NEGATIVO (Popen) igual a
        # -signal.SIGSEGV etc. Nao ha diagnostico esperado no stderr -- e'
        # teste negativo puro.
        if rc is not None and rc < 0:
            print(f"FALHA [{nome}]: terminou por SINAL {-rc} ({payload_desc(payload)})")
            print("\n".join("        " + l for l in saida.splitlines()[-15:]))
            return False, saida

        print(f"  ok  {nome} (rc={rc})")
        return True, saida
    finally:
        if proc.poll() is None:
            proc.kill()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                pass


def payload_desc(payload):
    return f"{len(payload)} bytes"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--frames", type=int, default=200,
                     help="frames deterministicos -- so precisa dar tempo do laco rodar depois "
                          "do envio (netRate default e' 0: sem thread propria, processado INLINE "
                          "a cada updateData(), entao nao depende de correr contra um relogio de "
                          "parede -- medido estavel com 200 em varias repeticoes)")
    args = ap.parse_args()

    binario = str(Path(args.binario).resolve())

    print("--- PDU DIS malformado/truncado (cenario de producao, rede de verdade) ---")

    falhas = []
    for nome, payload in payloads():
        ok, _ = roda_um_caso(binario, args.frames, nome, payload)
        if not ok:
            falhas.append(nome)

    if falhas:
        print(f"\nPDU DIS malformado: FALHOU ({len(falhas)})")
        for f in falhas:
            print(f"  - {f}")
        print("\nSe isto e' um crash real do parser DIS vendorizado (nao deste repositorio),")
        print("ver o cabecalho deste arquivo: 'limitacao conhecida do MIXR' e o veredito")
        print("esperado, nao um bug para corrigir aqui.")
        return 1

    print("\nPDU DIS malformado: OK -- nenhum caso derrubou o processo")
    return 0


if __name__ == "__main__":
    sys.exit(main())
