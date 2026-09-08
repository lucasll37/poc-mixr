# poc/dis — as duas que só fazem sentido juntas

Não é uma poc; é o **grupo** delas. O que as junta é DIS nativo do MIXR trocado entre dois
processos separados: o intruso mora em `bandit` e chega em `flight` apenas pela rede
(`networks:`), enquanto `falcon1..4` fazem o caminho de volta e aparecem no Tacview de `bandit`.
Rodar qualquer uma sozinha é meia demonstração.

| pasta | o que isola | Tacview | DIS (escuta / emite de) |
|---|---|---|---|
| [`bandit/`](bandit/) | uma aeronave só, sem decisão: joystick físico ou `Autopilot` de fallback | 1235 | 3000 / 3001 |
| [`flight/`](flight/) | 4 caças decidindo via árvore de comportamento, `( FlightAgentTC )` na fase 3 do frame de tempo crítico | 1234 | 3000 / 3002 |

Todos escutam na porta **3000**; cada processo emite de uma porta local diferente e ignora essa
mesma porta como origem, para não ouvir o próprio eco (`ignoreSourcePort:` == o próprio
`localPort:`). `disEntityType` é um código de 7 números inventado (não é enumeração SISO-REF-010
real) — só precisa ser **idêntico** nos dois lados de cada par emissor/receptor; aqui é
`[ 1 2 225 1 99 0 0 ]` nos dois cenários.

```bash
# em dois terminais, a partir da raiz do repositório
./build/app/src/app -folder src/poc/dis -scenario bandit
./build/app/src/app -f src/poc/dis/flight/configs/scenario.edl.in
# equivalente: ./build/app/src/app -folder src/poc/dis -scenario flight
```

**Não há executável por poc.** Cada pasta aqui é só `configs/` + `data/` (+ `README.md`, onde
existir); quem executa é o `./app`, o runner único de todas as pocs — `-folder src/poc/dis
-scenario <bandit|flight>` navega o grupo diretamente (as duas têm exatamente um `.edl`/`.edl.in`
em `configs/`, então a descoberta não é ambígua), ou `-f <arquivo>` aponta direto pro caminho, que
é como as fixtures de teste entram. Ver [`flight/README.md`](flight/README.md) para a dissecação
completa do modelo e do cenário, e o `CLAUDE.md` da raiz, seção "src/poc/dis/bandit", para o
detalhe de como um player nascido só de PDUs de rede (sem `dynamicsModel`/`pilot` locais) engana o
radar/UBF do outro lado exatamente como um player local enganaria.
