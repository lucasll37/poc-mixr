import os
import sys

# ARMADILHA CONFIRMADA (nao redescobrir): sem isto, a PRIMEIRA chamada a
# NativeSimulation.reset() -- que carrega libflight_tc.so via dlopen() por
# dentro de shared/xplugin/PluginRegistry.cpp -- SEGFAULTA dentro de
# std::cout (libstdc++, num codecvt de PluginRegistry::loadModule()), so
# quando roda embutido em Python E depois de 'numpy' ja ter sido importado.
# Fora do Python (executando dist/bin/single-thread etc.) ou importando
# ._native ANTES de numpy, o MESMO cenario/plugin carrega perfeitamente --
# confirmado nos dois sentidos.
#
# Causa (as duas pontas confirmadas isoladamente, uma por vez):
#   1. O CPython importa extensoes C (._native, aqui) com RTLD_LOCAL por
#      padrao (sys.getdlopenflags()) -- diferente de um executavel comum,
#      onde o dynamic linker sempre poe as dependencias DIRETAS do binario
#      (libmixr_base.so incluida) em escopo GLOBAL. Sem RTLD_GLOBAL,
#      libmixr_base.so fica fora do escopo global do processo, e o dlopen()
#      INTERNO que o PluginRegistry faz depois (pra libflight_tc.so, tambem
#      RTLD_LOCAL -- ver o cabecalho de PluginRegistry.cpp) nao resolve
#      direito o estado global de iostream/locale de libmixr_base.so.
#   2. SEPARADAMENTE, a ORDEM importa: se 'numpy' (ou qualquer outra
#      extensao C que carregue sua PROPRIA copia/versao de simbolos de
#      libstdc++) for importado ANTES de ._native, os simbolos que ela
#      trouxe ficam na frente na varredura de resolucao do linker dinamico
#      -- mesmo com ._native depois marcado RTLD_GLOBAL -- e uma chamada
#      de iostream dentro de libmixr_base.so pode acabar caindo numa versao
#      incompativel de um simbolo (ex.: template de codecvt) vinda de numpy
#      em vez da propria. Por isso ._native e importado AQUI, no topo deste
#      __init__.py, ANTES de env.py ter a chance de importar numpy/gymnasium.
#
# Os dois efeitos foram confirmados isoladamente (RTLD_GLOBAL sozinho nao
# bastou com numpy importado antes; import antes de numpy sozinho tambem
# resolveu) -- os dois juntos, nesta ordem, sao o fix.
_prev_dlopenflags = sys.getdlopenflags()
sys.setdlopenflags(_prev_dlopenflags | os.RTLD_GLOBAL)
try:
    from . import _native  # noqa: F401  (import so pelo efeito colateral do dlopen)
finally:
    sys.setdlopenflags(_prev_dlopenflags)

from .env import MixrFlightEnv, default_reward  # numpy/gymnasium importados so AQUI, depois

__all__ = ["MixrFlightEnv", "default_reward"]
