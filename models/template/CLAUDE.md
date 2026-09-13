# CLAUDE.md — diário deste modelo

Complementa o `CLAUDE.md` da raiz e os `.md` deste projeto (`README.md`, `CHANGELOG.md`,
`docs/ARCHITECTURE.md`, `docs/PRIMEIROS-PASSOS.md`, se existir). Só entra aqui o que não está em
nenhum dos dois — e só depois de confirmado **rodando**, nunca por suposição.

**O que este arquivo É:** o diário de armadilhas e decisões de arquitetura confirmadas rodando
**neste modelo específico** — o mesmo papel que o `CLAUDE.md` da raiz cumpre para o repositório
inteiro, na escala de um projeto de modelo só.

**O que este arquivo NÃO é:** não é tutorial (isso é `docs/PRIMEIROS-PASSOS.md`, quando este
projeto tiver um) nem estado atual de features (isso é `README.md`). Um `CLAUDE.md` recém-copiado
do template, sem nada ainda confirmado rodando no domínio novo, começa **vazio de narrativa
própria** — esse é o estado inicial correto, não uma lacuna para preencher artificialmente.

## Duas armadilhas de mecanismo do scaffold, válidas para qualquer modelo copiado do template

- **`.vscode/launch.json` e `meson_options.txt` não são tocados por `make new-model`/rename
  manual.** `scripts/models.sh` só reescreve `lib<nome>.so` em `Makefile`/`*.md` — o
  `launch.json` copiado (que pode apontar para um caminho de build que nunca existiu de verdade
  neste projeto) e a descrição em `meson_options.txt` (que pode continuar citando o nome do
  template) ficam com o texto original mesmo depois de copiado/renomeado. Não confiar neles como
  reflexo do build real; ajustar à mão ao notar a divergência.
- **Registrar uma classe nova em `xnative/factory.cpp` exige sincronizar 3 listas à mão** — o
  `if/else` de `factory()`, mais os arrays de nomes e de meta-objetos (`NOMES[]`/`METAS[]` no
  scaffold original, ou o que este projeto os tiver renomeado para). Nada no **compilador** força
  essa sincronia; só o registro de plugin em runtime recusa a carga se divergirem.

## A suíte de testes gerada não cobre a integração UBF de ponta a ponta

A suíte gerada cobre três camadas: as regras puras de `domain/` (sem MIXR), a árvore de
comportamento em si (contra um contexto falso, sem `Station`) e a FORMA do `.so` publicado
(contrato de plugin). O que ela **não** cobre é a integração ponta a ponta das classes UBF
concretas — `State`/`Behavior`/`Action` reais, lendo um ator de verdade e emitindo o rótulo certo a
partir de uma leitura simulada. Quem copia o template herda essa lacuna de cobertura também;
fechá-la com um teste próprio é trabalho de quem escreve o modelo, não algo que já vem pronto.
