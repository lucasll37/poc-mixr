# Como usar: política em Python, política em ONNX, inferência em nó de BT

Guia prático das três peças acrescentadas ao modelo `flight`. Cada seção é uma receita que você
roda; os números mostrados são de execuções reais.

**Pré-requisito único**, uma vez:

```bash
make configure && make sdk && make install
```

Todos os comandos abaixo rodam **a partir da raiz do repositório** — mesma convenção de todo
binário deste projeto.

---

## 0. O mapa mental

Uma aeronave decide assim:

```
Player (falcon1)
 └─ agent: ( FlightAgentTC )              fase 3 do frame, 50 Hz
     └─ behavior: ( UbfArbiter )          escolhe por VOTO
         ├─ ( AltitudeSafetyBehavior vote: 90 )   piso anti-CFIT — sempre vence
         └─ ( BtBehavior vote: 50 treeFile: "..." )
                └─ a ÁRVORE, que é onde as peças novas entram
```

O `treeFile:` do cenário decide qual árvore roda. **Trocar de política é apontar para outro
arquivo** — não recompilar. Três árvores vêm instaladas:

| arquivo | quem decide |
|---|---|
| `flight_tree.xml` | as regras em C++ (produção — **intocada**) |
| `flight_tree_py.xml` | um script Python |
| `flight_tree_onnx.xml` | uma política `.onnx` |

Todas em `dist/share/mixr-plugins/flight/`.

O `AltitudeSafetyBehavior` com voto 90 é a razão de você poder experimentar à vontade: uma
política ruim que comande altitude contra o terreno é **sobreposta** pelo árbitro nativo. Você não
precisa acertar a segurança no script.

---

## 1. Escrever um comportamento em Python

### 1.1 O contrato

Seu arquivo define **uma** função:

```python
def decide(obs):
    return (heading_deg, altitude_m, speed_kts)
```

`obs` é uma lista de 28 floats na ordem canônica. Para descobrir os índices sem contar na mão:

```bash
python3 src/poc/rl-training/tools/export_onnx.py --campos
```

```
 0 northM        7 fuelFraction   14 contactRelBearingDeg   21 alertAltitudeM
 1 eastM         8 mach           15 contactDeltaAltM       22 alertRangeM
 2 altitudeM     9 gLoad          16 contactNorthM          23 valid
 3 headingDeg   10 alphaDeg       17 contactEastM           24 terrainValid
 4 speedKts     11 terrainElevM   18 contactAltitudeM       25 hasContact
 5 rollDeg      12 altitudeAglM   19 alertNorthM            26 hasAlert
 6 pitchDeg     13 contactRangeM  20 alertEastM             27 weaponReady
```

A lista sai do C++, não de uma cópia em Python — é a mesma que o `.onnx` recebe na entrada.

### 1.2 O laço rápido

O nó lê o script de `dist/share/mixr-plugins/flight/`. Para iterar, **edite a cópia instalada
direto** — nenhum passo de build:

```bash
$EDITOR dist/share/mixr-plugins/flight/policy_example.py
make run-multi-thread          # ou o comando de -deterministic abaixo
```

Quando a regra se provar, copie de volta para `models/A4/configs/policy_example.py` — é essa a
versão versionada, e `make install` a republica.

### 1.3 Demonstração medida

Com `ALTITUDE_PATRULHA_M = 1750.0`, 400 frames:

```
frame=100  alt=1751.5  bt=PY
frame=400  alt=1757.6  bt=PY      ← praticamente nivelado
```

Trocando **só esse número** para `3000.0`, sem recompilar nada:

```
frame=100  alt=1753.3  bt=PY
frame=400  alt=1785.2  bt=PY      ← subindo
```

A subida é gradual porque o `Autopilot` do c310 está calibrado com `maxClimbRateMps: 8.0` — o
script mandou 3000 no primeiro tick; quem limita é a aeronave.

### 1.4 O que o script pode e não pode

**Pode:** guardar estado entre ticks numa variável global — cada aeronave tem o **seu** dicionário
de globais, então é o jeito de escrever histerese. `import` de módulos padrão e de numpy funciona.

**Não pode:** ler relógio, sortear sem semente, escrever arquivo, fazer I/O. Isso quebraria o
determinismo dos `make check-*`. Também não guarde estado em um **módulo importado** — o
`sys.modules` é compartilhado entre as aeronaves.

**Custo:** ~8 µs por chamada com uma thread, ~18 µs com quatro (o GIL serializa). A 4 aeronaves ×
50 Hz isso é 0,35% do frame. Há folga; não há folga para I/O.

**Se algo der errado** — sem Python no sistema, script ausente, sem `decide()`, exceção — o nó
devolve `FAILURE`, a `Patrol` assume e a aeronave continua voando. O erro aparece como `LOG(ERROR)`
(visível na aba Log do `./app`).

---

## 2. Treinar e implantar uma política de RL

### 2.1 Treinar

O ambiente Gymnasium já existe (ver `src/rl/README.md`); quem treina contra ele é o consumidor em
`src/poc/rl-training/` (venv próprio, separado do de `src/rl`, com `stable-baselines3`+`torch` já em
`requirements.txt`), via o `Makefile` autocontido de lá — `train.py` é o caso de uso completo (PPO
de verdade, checkpoints periódicos, salvamento no `Ctrl+C`), não um trecho solto:

```bash
cd src/poc/rl-training
make venv
make train ARGS="--timesteps 200000"
```

Ver `src/poc/rl-training/README.md` para as opções (`--player`, `--scenario`, `--seed`...) e
`train.py --help` — ou o equivalente interativo, célula a célula, em
`src/poc/rl-training/notebooks/train.ipynb`.

> Um `MixrFlightEnv` por **processo** — o registro de plugins é selado depois do primeiro parse.
> Para vários episódios em paralelo, um processo por env (`multiprocessing`).

### 2.2 Exportar

```bash
PYTHONPATH=./dist/python python3 src/poc/rl-training/tools/export_onnx.py \
    --sb3 runs/ppo_falcon.zip -o models/A4/configs/policy_example.onnx
```

O exportador **lê a ordem dos campos do C++**, não de uma lista em Python. É o que impede a deriva
silenciosa: um `.onnx` treinado contra outra ordem produziria comandos errados sem nenhum erro.

Para exercitar a cadeia sem treinar (pesos aleatórios):

```bash
PYTHONPATH=./dist/python python3 src/poc/rl-training/tools/export_onnx.py --random -o /tmp/teste.onnx
```

### 2.3 Implantar

```bash
make install     # copia o .onnx para dist/share/mixr-plugins/flight/
```

Depois aponte o `treeFile:` do cenário para `flight_tree_onnx.xml`. Pronto — **sem Python no
processo de simulação**.

### 2.4 O que confere sozinho

Se a forma do `.onnx` não bater com o contrato (28 entradas → 3 saídas), o nó **recusa** e loga:

```
[OnnxPolicy] 'policy.onnx' tem forma 24->3, mas o contrato e 28->3
```

e a `Patrol` assume. O teste `xinfer-degradacao` trava isso contra o arquivo instalado de verdade.

**Custo:** ~50 µs por inferência, 0,25% do frame. A sessão é cacheada por caminho — as quatro
aeronaves compartilham uma, porque criar uma custa 9 ms e quatro não caberiam em 20 ms.

---

## 3. Inferência como condição de árvore

O nó `OnnxScore` é genérico: roda um modelo e compara **uma** saída com um limiar. Serve para
"vale a pena engajar?", "isto é uma ameaça?" — qualquer pergunta de sim/não que você prefira
treinar a escrever.

```xml
<Fallback name="root">

  <Sequence name="engajar">
    <OnnxScore model="./dist/share/mixr-plugins/flight/ameaca.onnx"
               index="0" threshold="0.7" above="true"/>
    <ReportAndEvade/>
  </Sequence>

  <Patrol/>

</Fallback>
```

A entrada é a mesma observação de 28 campos; a saída pode ter qualquer número de valores, e `index`
escolhe qual comparar. Falha de qualquer tipo → `FAILURE`, e o `Fallback` segue para o próximo ramo.

---

## 4. Rodar e observar

```bash
# tempo real, com Tacview na porta 1234
make run-multi-thread

# passo fixo, comparável, imprimindo o dump a cada 100 frames
./build/src/poc/dis/multi-thread/src/multi-thread -threads 4 -deterministic 600
```

No dump, a coluna `bt=` diz **qual nó decidiu**: `PATROL`/`EVADE`/… (C++), `PY` (Python), `ONNX`
(política treinada). Se aparecer `PATROL` quando você esperava `PY` ou `ONNX`, o nó caiu no
`Fallback` — veja o `LOG(ERROR)`.

O `./app` (TUI) mostra o mesmo ao vivo, com a árvore desenhada e a folha ativa destacada:

```bash
make run-app
```

---

## 5. Antes de confiar no resultado

```bash
make test                    # 40 testes; inclui as duas políticas ponta a ponta
make check-multi-thread      # dumps byte-idênticos com 1, 2 e 4 threads
```

O segundo é o que importa se você mexeu em política: ele prova que a decisão continua
determinística com quatro aeronaves decidindo em paralelo. Um script que lê relógio ou sorteia sem
semente **quebra aqui** — que é onde você quer descobrir isso.

---

## 6. Onde está o quê

| | |
|---|---|
| o script de exemplo | `models/A4/configs/policy_example.py` |
| a política de exemplo | `models/A4/configs/policy_example.onnx` (**pesos aleatórios**) |
| as árvores | `models/A4/configs/flight_tree_{py,onnx}.xml` |
| o exportador | `src/poc/rl-training/tools/export_onnx.py` |
| a ordem canônica dos campos | `shared/xrlbridge/ObservationFields.hpp` |
| o motor de inferência | `shared/xinfer/README.md` |
| o interpretador embarcado | `shared/xpyembed/README.md` |
