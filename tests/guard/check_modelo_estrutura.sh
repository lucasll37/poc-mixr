#!/usr/bin/env bash
#
# TODO PROJETO DE MODELO TEM tests/, docs/, README.md E CHANGELOG.md.
#
# Nao e estilo: e o que faz cada modelo AUTOCONTIDO e verificavel sozinho, sem
# depender do resto do repositorio estar em mente -- alguem que abre o VS Code
# so em models/<nome>/ tem, ali dentro, como compilar (Makefile), como provar
# que continua certo (tests/), o "porque" das decisoes (docs/), a porta de
# entrada (README.md) e o que mudou desde a ultima vez (CHANGELOG.md).
#
# Vale em dobro para models/player/fixtures/stub: ele e o ponto de partida copiavel
# (models/README.md secao 2), entao o que falta la falta em todo modelo que
# nascer dele.
#
# Os projetos sao descobertos por 'find' (todo diretorio sob models/ com um
# meson.build de projeto), NAO por lista fixa -- um modelo novo passa a ser
# cobrado sem editar este arquivo. E a mesma licao ja registrada no cabecalho
# de check_host_opaco.sh: glob/lista fixa envelhece em silencio.
#
# plugins/ fica de fora de proposito: e um DEPOSITO para .so de
# terceiro (ja compilado fora deste repositorio), nao um projeto -- nao tem
# fonte, nao tem build, e nada ali e nosso para documentar.
set -u
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1
fail=0

# O './build' de cada projeto tem meson.build gerados dentro; -not -path
# corta os diretorios de build/dist locais, que nao sao projetos.
projetos="$(find models -mindepth 2 -name meson.build \
               -not -path '*/build/*' -not -path '*/dist/*' \
               -not -path '*/subprojects/*' 2>/dev/null \
            | xargs -r -n1 dirname | sort -u)"

if [ -z "$projetos" ]; then
   echo "  FALHA nenhum projeto de modelo encontrado sob models/ -- o find quebrou?"
   exit 1
fi

# Um meson.build de SUBDIRETORIO (tests/meson.build, por exemplo) nao e
# projeto: so conta quem chama project().
while IFS= read -r dir; do
   [ -z "$dir" ] && continue
   grep -q '^project(' "$dir/meson.build" || continue

   faltando=""
   for peca in tests docs; do
      [ -d "$dir/$peca" ] || faltando="$faltando $peca/"
   done
   for peca in README.md CHANGELOG.md Makefile; do
      [ -f "$dir/$peca" ] || faltando="$faltando $peca"
   done

   # Diretorio que existe mas esta vazio nao conta -- .gitkeep tambem nao,
   # porque a peca so serve para alguma coisa se tiver conteudo.
   for peca in tests docs; do
      if [ -d "$dir/$peca" ]; then
         n="$(find "$dir/$peca" -type f -not -name '.gitkeep' | head -1)"
         [ -n "$n" ] || faltando="$faltando $peca(vazio)"
      fi
   done

   if [ -n "$faltando" ]; then
      echo "  FALHA $dir esta sem:$faltando"
      fail=1
   else
      echo "  OK   $dir tem tests/ docs/ README.md CHANGELOG.md Makefile"
   fi
done <<< "$projetos"

if [ "$fail" -ne 0 ]; then
   echo
   echo "Ver models/README.md (a regra e o porque) e models/player/fixtures/stub/"
   echo "(o ponto de partida copiavel, que ja traz as cinco pecas prontas)."
   exit 1
fi
exit 0
