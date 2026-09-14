#!/usr/bin/env python3
"""Cobertura do parametro 'fields' de MixrFlightEnv -- peca nova desta
passada: a observacao passou a ter 38 campos no lado C++ (RWR + navegacao,
ver libs/xrlbridge/ObservationFields.hpp), mas o DEFAULT de MixrFlightEnv
continua exatamente os 28 historicos ("classic28") -- os campos novos so
ficam visiveis com fields="all" ou uma lista explicita que os inclua.

Processo PROPRIO (nao entra em test_contract.py/test_smoke.py): constroi
VARIAS MixrFlightEnv no mesmo processo, o que e' seguro porque
NativeSimulation() so guarda o caminho do cenario -- a Station so e montada,
preguicosamente, no primeiro reset() (ver StationBuilder). So UMA das
instancias abaixo chama reset(), respeitando "uma Station por processo
inteiro" (a mesma armadilha documentada em test_bad_player.py/test_smoke.py).
"""

import os
import sys

if not os.path.exists("./src/rl/configs/scenario_rl.edl"):
    print("ERRO: rode este script com cwd na raiz do repositorio", file=sys.stderr)
    sys.exit(1)

from mixr_gym import MixrFlightEnv  # noqa: E402  (import antes de numpy -- ver os outros testes)


def main() -> int:
    falhou = False

    # Omitido == "classic28": os 28 campos historicos, o observation_space
    # de sempre -- nao muda so' porque o C++ passou a publicar 38.
    env_default = MixrFlightEnv(max_episode_steps=2)
    n_default = len(env_default.observation_space.spaces)
    if n_default != 28:
        print(f"FALHOU: fields omitido deveria dar 28 campos, deu {n_default}", file=sys.stderr)
        falhou = True
    else:
        print("OK  fields omitido (default) = 28 campos ('classic28')")

    # "all": os 38 completos, incluindo RWR/navegacao -- antes desta passada
    # esses 10 campos nao existiam em lugar nenhum do lado Python.
    env_all = MixrFlightEnv(fields="all", max_episode_steps=2)
    campos_all = set(env_all.observation_space.spaces.keys())
    if len(campos_all) != 38 or "rwrThreatRangeM" not in campos_all or "hasNavSteering" not in campos_all:
        print(f"FALHOU: fields='all' deveria ter 38 campos com RWR/navegacao, "
              f"tem {len(campos_all)}", file=sys.stderr)
        falhou = True
    else:
        print("OK  fields='all' = 38 campos, incluindo RWR/navegacao")

    # Lista explicita: so os pedidos, nada mais.
    env_custom = MixrFlightEnv(fields=["hasContact", "northM"], max_episode_steps=2)
    campos_custom = set(env_custom.observation_space.spaces.keys())
    if campos_custom != {"hasContact", "northM"}:
        print(f"FALHOU: fields explicito deveria ter exatamente 2 campos, "
              f"tem {campos_custom}", file=sys.stderr)
        falhou = True
    else:
        print("OK  fields=['hasContact','northM'] = exatamente esses 2 campos")

    # Nome desconhecido -> ValueError, levantada ANTES de tocar em
    # NativeSimulation nenhuma (ver env.py: _resolve_fields() roda primeiro)
    # -- seguro chamar aqui, no meio do processo, sem afetar a Station unica.
    try:
        MixrFlightEnv(fields=["campoInventado"], max_episode_steps=2)
        print("FALHOU: fields com nome desconhecido deveria levantar ValueError", file=sys.stderr)
        falhou = True
    except ValueError as exc:
        if "campoInventado" not in str(exc):
            print(f"FALHOU: ValueError sem o nome do campo na mensagem: {exc}", file=sys.stderr)
            falhou = True
        else:
            print("OK  fields com nome desconhecido levanta ValueError nomeando-o")

    # A UNICA Station deste processo: confirma que o campo novo chega de
    # verdade no dict devolvido por reset(), nao so' no observation_space
    # declarado.
    obs, _info = env_all.reset()
    if "rwrThreatRangeM" not in obs:
        print("FALHOU: reset() com fields='all' nao devolveu rwrThreatRangeM", file=sys.stderr)
        falhou = True
    else:
        print("OK  reset() com fields='all' devolve rwrThreatRangeM no dict de observacao")

    env_all.close()
    env_default.close()
    env_custom.close()

    if falhou:
        return 1
    print("\ntest_fields_param: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
