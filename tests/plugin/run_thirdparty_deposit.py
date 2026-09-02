#!/usr/bin/env python3
"""
O DEPOSITO DE TERCEIRO -- prova que models/plugins/ nao e so uma pasta com
um README bonito: um .so colocado ali de fato chega em dist/lib/mixr-plugins/
pelo mecanismo que 'make models' usa (Makefile: cp models/plugins/*.so
dist/lib/mixr-plugins/), e o resultado dessa copia e REALMENTE carregavel e
USAVEL numa simulacao -- nao so um arquivo presente em disco.

Reaproveita o STUB (models/fixtures/stub) fazendo o papel do "terceiro": e
um modelo ja pronto, compilado, e ja provado suficiente para rodar o
cenario de producao (ver plugin-modelo-estranho) -- sem ser nenhum dos
plugins que o host normalmente consome direto de dist_plugins/. Isso isola
exatamente a variavel que importa aqui: o CAMINHO pelo qual o .so chegou
(deposito -> copia), nao se o modelo em si e valido (isso quem ja prova e
plugin-modelo-estranho).

NAO reimplementa nenhuma assercao sobre "a simulacao funcionou de
verdade" -- chama run_stub_model.py (a MESMA bateria inteira: carga,
provides:, escrita no xboard, movimento entre frames, varredura de radar
no Tacview) apontando pro .so que acabou de ser depositado e copiado, em
vez do dist_plugins/libstub.so que 'make models' ja instala direto. Duas
baterias idénticas, duas origens de arquivo diferentes.

Por que a copia e replicada aqui em vez de chamar 'make models' de
verdade: 'make models' tambem recompila flight/missile/stub -- pesado
demais pra suite rapida (meson test, chamada a cada 'make test'). A linha
do Makefile que importa e um 'cp' puro; testar essa copia isolada nao
perde cobertura nenhuma sobre o que pode dar errado nela.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--binario", required=True)
    ap.add_argument("--poc", required=True)
    ap.add_argument("--source-so", required=True,
                     help="um .so ja compilado (o stub) que faz o papel do 'terceiro'")
    ap.add_argument("--run-stub-model", required=True,
                     help="caminho de run_stub_model.py, pra reusar as mesmas assercoes")
    args = ap.parse_args()

    source = Path(args.source_so)
    if not source.is_file():
        print(f"FALHA: {source} nao existe -- rode 'make models'")
        return 1

    # Nome DISTINTO de qualquer plugin real -- nao pode colidir com
    # libflight.so/libflight_tc.so/libmissile.so/libstub.so (os quatro que
    # 'make models' ja instala direto), senao o teste nao provaria nada:
    # estaria so verificando um arquivo que ja estava la por outro motivo.
    nome = "libthirdparty_deposit_test.so"
    deposito = RAIZ / "models" / "plugins" / nome
    instalado = RAIZ / "dist" / "lib" / "mixr-plugins" / nome

    try:
        # 1) "Alguem deposita" o .so em models/plugins/ -- exatamente o que
        #    um terceiro faria, sem tocar em nenhum projeto Meson deste
        #    repositorio.
        deposito.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, deposito)
        if not deposito.is_file():
            print(f"FALHA: nao consegui depositar em {deposito}")
            return 1

        # 2) O MECANISMO sob teste -- a MESMA copia que o alvo 'models' do
        #    Makefile raiz faz depois de compilar flight/missile/stub.
        instalado.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(deposito, instalado)
        if not instalado.is_file():
            print(f"FALHA: {instalado} nao apareceu depois da copia -- o mecanismo de "
                  f"deposito (models/plugins/ -> dist/lib/mixr-plugins/) nao funcionou")
            return 1
        print(f"  OK   depositado e copiado: {deposito.relative_to(RAIZ)} -> "
              f"{instalado.relative_to(RAIZ)}")

        # 3) A PROVA: reusa a MESMA bateria de plugin-modelo-estranho
        #    (carga, provides:, xboard, movimento, varredura de radar),
        #    agora apontando pro .so que chegou pelo deposito -- nao pro
        #    dist_plugins/libstub.so que 'make models' ja instala direto.
        r = subprocess.run(
            [sys.executable, args.run_stub_model,
             "--binario", args.binario, "--poc", args.poc,
             "--stub", str(instalado)],
            cwd=RAIZ,
        )
        if r.returncode != 0:
            print("plugin de deposito de terceiro: FALHOU (ver saida de run_stub_model.py acima)")
            return r.returncode

        print("  OK   o .so depositado em models/plugins/ rodou a simulacao de producao")
        print("plugin de deposito de terceiro: OK")
        return 0
    finally:
        # Nao e pra ficar vendorizado nem instalado -- era so a prova de
        # que o mecanismo funciona, nao um plugin de verdade.
        deposito.unlink(missing_ok=True)
        instalado.unlink(missing_ok=True)


if __name__ == "__main__":
    sys.exit(main())
