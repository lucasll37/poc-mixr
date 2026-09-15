import os
import sys

# 'mixr_gym' precisa ser importado antes de 'numpy'/'gymnasium' neste
# processo. Sem isso, a primeira chamada a NativeSimulation.reset() -- que
# carrega libA-4.so via dlopen() dentro de libs/xplugin/PluginRegistry.cpp
# -- segfauta dentro de std::cout (libstdc++), quando roda embutido em
# Python e numpy ja foi importado antes.
#
# Duas causas: (1) o CPython importa extensoes C com RTLD_LOCAL por padrao,
# deixando libmixr_base.so fora do escopo global do processo, o que impede
# o dlopen() interno para libA-4.so de resolver corretamente o estado
# global de iostream/locale; (2) se numpy (ou outra extensao C que carregue
# copia propria de simbolos de libstdc++) for importado antes de ._native,
# seus simbolos ficam a frente na resolucao do linker dinamico, podendo
# causar incompatibilidade de simbolo dentro de libmixr_base.so.
#
# Corrigido carregando ._native sob RTLD_GLOBAL logo no topo do modulo,
# antes de env.py poder importar numpy/gymnasium; se esses modulos ja
# estiverem carregados quando mixr_gym e importado, um erro claro e
# levantado em vez de deixar o processo segfaultar mais adiante.
_ja_carregado = [m for m in ("numpy", "gymnasium") if m in sys.modules]
if _ja_carregado:
    raise RuntimeError(
        "mixr_gym precisa ser importado ANTES de " + ", ".join(_ja_carregado) + " "
        "(ja carregado(s) neste processo) -- ver o comentario no topo de "
        "mixr_gym/__init__.py: importar fora desta ordem corrompe simbolos "
        "de libstdc++ e costuma terminar em segfault dentro do dlopen() do "
        "plugin do modelo, nao aqui."
    )

_prev_dlopenflags = sys.getdlopenflags()
sys.setdlopenflags(_prev_dlopenflags | os.RTLD_GLOBAL)
try:
    from . import _native  # noqa: F401  (import so pelo efeito colateral do dlopen)
finally:
    sys.setdlopenflags(_prev_dlopenflags)

from .env import MixrFlightEnv, default_reward  # numpy/gymnasium importados so AQUI, depois

__all__ = ["MixrFlightEnv", "default_reward"]
