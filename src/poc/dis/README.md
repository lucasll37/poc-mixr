# poc/dis — as duas que só fazem sentido juntas

Não é uma poc; é o **grupo** delas. O que as junta é DIS nativo do MIXR trocado entre dois
processos separados: o intruso mora em `bandit` e chega em `flight` apenas pela rede
(`networks:`), enquanto `falcon1..4` fazem o caminho de volta e aparecem no Tacview de `bandit`.
Rodar qualquer uma sozinha é meia demonstração.

| pasta | o que isola | Tacview | callsign do handshake | DIS (escuta / emite de) |
|---|---|---|---|---|
| [`bandit/`](bandit/) | uma aeronave só, sem decisão: joystick físico ou `Autopilot` de fallback | 1235 | `poc-mixr/bandit` | 3000 / 3001 |
| [`flight/`](flight/) | 4 caças decidindo via árvore de comportamento, `( FlightAgentTC )` na fase 3 do frame de tempo crítico | 1234 | `poc-mixr/flight` | 3000 / 3002 |

> A coluna "callsign do handshake" é o slot `callsign:` do `TacviewOutput` raiz de cada cenário —
> o nome que o servidor anuncia no **handshake**, o texto de abertura da conexão que o Tacview
> exige antes de aceitar qualquer dado (`XtraLib.Stream.0\nTacview.RealTimeTelemetry.0\n
> <username>\n\0`; detalhe completo em `CLAUDE.md`, seção `libs/xtacview`) — não o rótulo que
> aparece colado em cada aeronave dentro do visualizador. Esse rótulo (`CallSign=falcon1`,
> `CallSign=bandit1`) vem automaticamente do nome do player — ver
> [`flight/README.md`](flight/README.md), seção 11.

Todos escutam na porta **3000**; cada processo emite de uma porta local diferente e ignora essa
mesma porta como origem, para não ouvir o próprio eco (`ignoreSourcePort:` == o próprio
`localPort:`). `disEntityType` é um código de 7 números inventado (não é enumeração SISO-REF-010
real — o catálogo oficial de tipos de entidade DIS, mantido pela SISO/*Simulation
Interoperability Standards Organization*) — só precisa ser **idêntico** nos dois lados de cada
par emissor/receptor; aqui é
`[ 1 2 225 1 99 0 0 ]` nos dois cenários.

```bash
# em dois terminais, a partir da raiz do repositório
./build/app/src/app -folder src/poc/dis -scenario bandit
./build/app/src/app -f src/poc/dis/flight/configs/scenario.edl.in
# equivalente: ./build/app/src/app -folder src/poc/dis -scenario flight
```

**Não há executável por poc.** Cada pasta aqui é só `configs/` + `data/` (+ `README.md`, onde
existir); quem executa é o `./app`, o runner único de todas as pocs — `-folder src/poc/dis
-scenario <bandit|flight>` navega o grupo diretamente (a descoberta ignora qualquer
`*.generated.edl` e exige exatamente um `.edl`/`.edl.in` de FONTE em `configs/`; `bandit/` só tem
um arquivo desse tipo, e `flight/` também — apesar do `scenario.generated.edl` residual descrito
abaixo, que não conta para essa checagem —, então a descoberta não é ambígua em nenhuma das duas),
ou `-f <arquivo>` aponta direto pro caminho, que
é como as fixtures de teste entram. `-f` aceita tanto `.edl` quanto `.edl.in` — é por isso que o
exemplo acima aponta direto para `scenario.edl.in`: um marcador de template ali (`@NUM_TC_THREADS@`)
é resolvido internamente antes do parse, e o `.edl` final gerado é escrito hoje em
`build/generated-scenarios/`, fora de `configs/` — um `scenario.generated.edl` ainda versionado
dentro de `flight/configs/` é resíduo de um mecanismo anterior a essa mudança, não o comportamento
atual; não confundir os dois. Mecanismo completo em `flight/README.md`, seções 5.3 ("Estrutura em
EDL, comportamento em C++") e 7.7. Ver
[`flight/README.md`](flight/README.md) para a dissecação completa do modelo e do cenário, e o
`CLAUDE.md` da raiz, seção "src/poc/dis/bandit", para o
detalhe de como um player nascido só de PDUs de rede (PDU, *Protocol Data Unit*, a mensagem que o
protocolo DIS troca entre processos — sem `dynamicsModel`/`pilot` locais) engana o radar/UBF do
outro lado exatamente como um player local enganaria.

`diagram.png` (nesta mesma pasta) é o diagrama de arquitetura desta troca DIS/Tacview — um painel
por poc (entrada de joystick/teclado → componente de decisão → `Aircraft` → `DisNetIO`/
`TacviewOutput`), os três painéis compartilhando o broadcast UDP na porta 3000 embaixo, e um único
cliente Tacview recebendo por TCP de cada porta ao final. **Está desatualizado, e não deve ser
usado para nomes/portas correntes**: ele mostra três pocs (`bandit-dis`, `single-thread`,
`multi-thread`) de uma passada anterior à renomeação para as duas atuais (`bandit`/`flight` — ver
`CLAUDE.md` §"O que é este projeto") — `single-thread` decidia via `( SimAgent )` e não existe
mais (ver `CLAUDE.md`, "Não existe mais par de subprojetos gêmeos..."), e as portas de DIS que ele
atribui a `multi-thread` (siteID 3 — o campo de identificação do site/instância dentro do Entity ID
do DIS, junto com o applicationID e o entityID — emitindo de `3003`) já divergem do `flight` de
hoje (emite de `3002`, ver a tabela acima). Mantido como registro histórico do desenho original do
grupo — para portas/nomes correntes, use a tabela desta página.
