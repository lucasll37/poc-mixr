# poc-mixr  v0.1

Demonstração funcional do MIXR com gravação de trajetória para o **Tacview**
(formato ACMI 2.2).

> **Nota:** esta cópia já inclui as correções de build descritas em
> `CORRECOES.md`. Veja esse arquivo para o detalhe de cada bug e o porquê
> da correção.

## O que está implementado

| Componente | Classe | Descrição |
|---|---|---|
| Station | `MyStation` | Container raiz com threads TC/BG |
| WorldModel | `MySimulation` | Ponto de referência geodésico (GRU, SP) |
| Aeronave | `MyAircraft` | AirVehicle que imprime estado no console |
| Dinâmica | `mixr::models::JSBSimModel` | JSBSim 6DOF, instanciado direto pela fábrica sob o nome "MyDynamics" |
| Navegação | `MyNavigation` | Navigation com Route de 4 Steerpoints |
| Autopilot | `Autopilot` | Modo NAV (segue rota automaticamente) |
| Radar | `MyRadar` | Radar banda-X com AirTrkMgr |
| Alvo | `AirVehicle` + `RacModel` | Alvo vermelho cinemático |
| **ACMI** | `AcmiWriter` | Grava ACMI 2.2 para Tacview |

---

## Pré-requisitos

- MIXR instalado (`libmixr_base`, `libmixr_simulation`, `libmixr_models`)
- JSBSim instalado com dados de aeronave (`f16.xml`)
- Meson ≥ 1.0 e Ninja
- GCC ≥ 7 ou Clang ≥ 5 (C++17)
- [Tacview](https://www.tacview.net/) para visualizar o ACMI

---

## Build

O projeto usa **Conan + Meson**, orquestrados pelo `Makefile` na raiz
(forma padrão de uso). O MIXR (e suas dependências transitivas — JSBSim,
OpenRTI, Protobuf) é resolvido via `conanfile.py`.

```bash
make configure   # conan install + meson setup (gera ./build e ./dist)
make build       # ninja compile
make install     # meson install em ./dist
make run ARGS="60 output/flight.acmi"   # executa o binário gerado
```

Outros alvos úteis: `make clean` (remove `build/` e `dist/`) e
`make package` (empacota o próprio poc-mixr via `conan create`, em Debug e
Release).

<details>
<summary>Equivalente manual, passo a passo (sem o Makefile)</summary>

```bash
mkdir -p build
conan install . --build=missing --settings=build_type=Debug

meson setup --reconfigure \
    --backend ninja \
    --buildtype debug \
    --native-file build/conan_meson_native.ini \
    --prefix="$(pwd)/dist" \
    --libdir="$(pwd)/dist/lib" \
    -Dpkg_config_path="$(pwd)/dist/lib/pkgconfig:$(pwd)/build" \
    build/ .

meson compile -C build
```
</details>

---

## Executar

```bash
# Padrão: 60 segundos → output/flight.acmi
make run

# Personalizado
make run ARGS="120 output/minha_missao.acmi"

# ou, diretamente:
./build/poc-mixr 120 output/minha_missao.acmi
```

Depois abra `output/flight.acmi` no Tacview.

---

## Formato ACMI gerado

```
FileType=text/acmi/tacview
FileVersion=2.2
0,ReferenceTime=2024-01-01T12:00:00Z
0,Title=poc-mixr simulation

#0.000
1,T=-46.467|−23.417|4572.0|0.0|2.1|90.0,Name=alpha,Type=Air+FixedWing,Color=Blue
2,T=-46.120|−23.383|3048.0|0.0|0.5|260.0,Name=bravo,Type=Air+FixedWing,Color=Red

#0.100
1,T=-46.466|−23.417|4573.2|0.3|2.0|90.1
2,T=-46.121|−23.383|3048.1|0.0|0.4|260.0
...
```

Propriedades estáticas (`Name`, `Type`, `Color`) são escritas **uma única vez**
por player. Frames subsequentes contêm apenas `T=lon|lat|alt|roll|pitch|yaw`.

---

## Ajuste do JSBSim

Em `config/sim.edl`, altere:

```edl
dynamicsModel: ( MyDynamics
    model:   "f16"                    // nome do modelo
    rootDir: "/usr/share/JSBSim"      // seu diretório JSBSim
)
```

Modelos disponíveis tipicamente em `/usr/share/JSBSim/aircraft/`.

---

## Estrutura do projeto

```
poc-mixr/
├── Makefile                ← orquestra conan + meson (configure/build/install/run)
├── conanfile.py             ← resolve o mixr/1.0.5 (e deps transitivas) via Conan
├── meson.build
├── meson_options.txt
├── config/sim.edl
├── CORRECOES.md          ← detalhe de todos os bugs de build corrigidos
├── include/poc/
│   ├── factory.hpp
│   ├── AcmiWriter.hpp      ← gravador ACMI
│   ├── MyStation.hpp
│   ├── MySimulation.hpp
│   ├── MyAircraft.hpp
│   ├── MyNavigation.hpp
│   └── MyRadar.hpp
└── src/
    ├── meson.build
    ├── main.cpp             ← laço principal + integração AcmiWriter
    ├── factory/factory.cpp
    ├── station/
    ├── simulation/
    ├── player/
    ├── navigation/
    ├── sensor/
    └── tacview/
        └── AcmiWriter.cpp  ← implementação ACMI 2.2
```

> `include/poc/MyDynamics.hpp` e `src/dynamics/MyDynamics.cpp` foram
> removidos — ver `CORRECOES.md`, item 3.
