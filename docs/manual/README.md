# `docs/manual/` — explorador de execução, EDL e classes built-in do MIXR

Página estática (React embutido, zero rede para abrir) com quatro visões sobre o framework,
**curadas, não instrumentadas** — sem processo MIXR rodando por trás, herança, nome de fábrica,
registro, slots, fases e os trechos de código (com arquivo e linha reais) vêm de
`tools/generate_manual_catalog.py` (que reaproveita `tools/mixr_source_scan.py` e
`tools/extract_execution_chain.py` como módulos) escaneando o fonte de verdade
(`contexts/src/mixr/` e `models/players/A-4/`) — nada digitado à mão.

## Como se usar

```bash
make open-docs   # zero rede, nenhum servidor -- Linux nativo (xdg-open) e WSL2
                 # (wslview/explorer.exe) pelo mesmo alvo, ver scripts/open_browser.sh
```

**Execução** — o ciclo de fases do MIXR (dynamics/transmit/receive/process + as duas threads de
decisão/fundo) desenhado sobre a árvore de um `( Aircraft )` com os dez sistemas primários que
`Player::updateSystemPointers()` resolve por tipo (~72 nós, o mesmo cenário exaustivo de
`src/poc/built-in_mixr_1`). Grafo navegável (pan/zoom/arrastar, timeline com transporte,
tema claro/escuro); clicar num nó fixa um popup com nome de fábrica/registro/contagem de slots;
clicar num quadradinho de fase pula direto pro passo em que aquele nó roda naquela fase.

**Comportamento** — a cadeia de decisão real de produção, `FlightAgentTC → Agent::controller →
UbfArbiter → {AltitudeSafetyBehavior, BtBehavior} → flight_tree.xml → FlightAction`, através dos
cenários que a usam. Ao contrário das outras duas abas, é um "ensaio" escrito **à mão** — não
extraído automaticamente do fonte — então é a única das três sujeita a envelhecer em silêncio se
a cadeia de decisão mudar sem que este texto acompanhe.

**Catálogo** — as 225 classes nativas do MIXR (`base`/`models`/`simulation`/`terrain`/
`interop::dis`/`linkage`/`recorder`, o mesmo escopo de [`models/BUILT-IN.md`](../../models/BUILT-IN.md))
mais as 9 do plugin de produção `models/players/A-4` — só classe com despacho **real** num
`factory.cpp` (`new X()` alcançável), nunca "toda classe com `DECLARE_SUBCLASS` em algum header".
Por construção, toda entrada aqui já é "registrada em fábrica" — não existe mais um filtro/bloco
separado para classes declaradas-mas-não-despachadas. Busca por classe/fábrica/slot, filtros
(nome divergente, trabalha em fase, no cenário, decisão/UBF); clicar numa classe mostra o corpo
real de qualquer método que ela sobrescreve, quando conhecido.

**Estrutura** — diagrama de classe UML sobre um recorte curado de 19 classes fundacionais do MIXR
(`Referenced`/`Object`/`Component`/`Player`/`Station`/`Simulation`/`Agent`/`NetIO`/...): caixa
completa com atributos, componentes (membros de composição) e métodos, extraídos de verdade do
header C++ por `tools/extract_class_diagram.py` — não as 342 classes do Catálogo, e não uma
segunda extração automática do mesmo escopo. Mais 28 alvos de composição em caixa mínima (Tier 2 —
só nome + base real, sem corpo). A TOPOLOGIA do diagrama (quem aparece filho de quem) e as notas de
"filosofia de emprego" no card de detalhe são organizadas **à mão** (`STRUCT_TOPOLOGY`/
`STRUCT_NOTES` em `doc.jsx`) — mesma ressalva da aba Comportamento: uma classe MIXR raramente tem
um único "pai" (ex.: `Player` é composto por `AbstractPlayer` via herança E é o molde de `Ntm` E é
apontado de volta por `System.ownship`), então a árvore escolhe UMA aresta primária por nó pra
caber num desenho legível; referências que fechariam ciclo aparecem tracejadas, sem diamante de
composição, sujeitas a envelhecer se a composição real mudar sem que a curadoria acompanhe. Nasce
totalmente expandido (recolher por caixa ou "recolher tudo"); clicar numa caixa cujo nome também
existe no Catálogo oferece "ver no Catálogo →" — ausente para `Referenced`/`Object`/`Component` e
outras que não têm entrada lá.

## Regenerar depois de editar `doc.jsx`

```bash
make docs   # ou: python3 tools/generate_manual_catalog.py > docs/manual/catalog.generated.js && node docs/manual/compile.js
```

A primeira execução baixa React/ReactDOM UMD + Babel para `docs/manual/.cache/` (gitignored); as
próximas rodam sem rede. `index.html` **e** `catalog.generated.js` são committed — abrir a página
não exige gerar nada antes.

`CLASS_DIAGRAM` (a aba Estrutura) segue o mesmo precedente, já imperfeito, de `MODEL`/
`FLIGHT_MODEL`: não há passo de `make docs` que regenere sozinho — `python3
tools/extract_class_diagram.py` imprime o JSON (com auto-verificação embutida, sai com código≠0 se
falhar) pra colar manualmente como `const CLASS_DIAGRAM = {...};` em `doc.jsx`, precedido do
comentário `/* GERADO por tools/extract_class_diagram.py. Nao editar. */`. `STRUCT_TOPOLOGY`/
`STRUCT_BACKREFS`/`STRUCT_NOTES` (a curadoria) não são gerados por nada — são editados direto no
`doc.jsx`.

## Leia mais

[CLAUDE.md](../../CLAUDE.md), seção `docs/` — toda decisão de design e armadilha confirmada
rodando. O editor gráfico de cenário `.edl` (ferramenta de autoria, não visualização) mora em
[`src/ui/`](../../src/ui/README.md), não aqui.
