#!/usr/bin/env python3
"""Gera src/ui/edl_catalog.generated.json: o catalogo COMPLETO (todas as
factories nativas -- base/models/terrain/recorder/interop/simulation/linkage
-- mais libs/x* e os plugins deste repositorio) com tipo/unidade/
cardinalidade por slot, para dirigir a paleta e os widgets do editor grafico
de .edl (src/ui/edl_builder.jsx).

Historico: ate a passada que automatizou o build deste editor num unico
comando, isto era o modo '--edl-catalog' de tools/extract_execution_chain.py
(o mesmo arquivo que ainda serve docs/manual/ via '--catalog'). Foi extraido
para ca porque so' interessa a ESTE editor -- docs/manual/ nunca usou este
modo -- e porque 'nao ser mais rotina de tools/' era pedido explicito. As
primitivas de varredura de C++ que os dois ainda compartilham (mask_source,
build_inheritance, build_factory_map, extract_slots, find_matching_brace)
moram em tools/mixr_source_scan.py, importado por AMBOS -- nao duplicadas.

Alem do scan ESTATICO (regex sobre .cpp/.hpp), este gerador tambem tenta
introspeccao de RUNTIME sobre qualquer .so solto em plugins/ (o deposito
flat de terceiro -- ver a secao 'plugins/' do CLAUDE.md): um .so sem fonte
neste repositorio nao tem texto pra escanear, entao a UNICA forma de saber
que classes ele publica e' carrega-lo e perguntar (ver o binario
'plugininfo', app/src/plugininfo_main.cpp). Isso e' ADITIVO e BEST-EFFORT --
sem o binario compilado (precisa de 'make build'), o catalogo sai
IDENTICO ao de sempre, so' com o que o fonte deste repositorio de fato tem;
nenhum erro, nenhuma dependencia nova pra quem so' quer editar cenarios
com os plugins ja' conhecidos.

Uso:
    python3 src/ui/scripts/generate_edl_catalog.py > src/ui/edl_catalog.generated.json

Chamado automaticamente por src/ui/scripts/build.js (o unico jeito
"suportado" de gerar o catalogo hoje -- nao ha mais alvo `make edl-catalog`
solto; ver src/ui/README.md).
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))
import mixr_source_scan as scan  # noqa: E402

PLUGINS_DIR = REPO_ROOT / "plugins"
# Preferimos o binario INSTALADO (dist/bin, criado por 'make install') sobre
# o cru de build/ (existe so' depois de 'meson compile', sem 'install') --
# na duvida, o mais recente/completo dos dois. Nenhum dos dois e' garantido
# existir; ver o comentario de introspect_thirdparty_plugins() abaixo.
PLUGININFO_CANDIDATES = [
    REPO_ROOT / "dist" / "bin" / "plugininfo",
    REPO_ROOT / "build" / "app" / "src" / "plugininfo",
]

MIXR_INCLUDE = REPO_ROOT / scan.MIXR_INCLUDE_REL
MIXR_SRC = REPO_ROOT / scan.MIXR_SRC_REL
MODELS_DIR = REPO_ROOT / scan.MODELS_REL
LIBS_DIR = REPO_ROOT / scan.LIBS_REL

# As tres subpastas de categoria que 'scripts/models.sh' (CATEGORIA_DIR) usa
# como destino de 'make new-model' -- nao ha constante Python compartilhada
# com aquele script (bash), entao esta lista e' o espelho dela aqui. Usada
# tanto para descobrir factory.cpp de modelo (dispatch_factory_cpp_paths())
# quanto para rotular a origem de uma classe (origin_of()) -- as duas
# assumiam "models/players/" ate um modelo real aparecer em
# models/others/ pela primeira vez (Navstar-3) e expor a lacuna.
MODEL_CATEGORY_DIRS = {"players", "systems", "others"}

mask_source = scan.mask_source
iter_files = scan.iter_files
build_inheritance = scan.build_inheritance
build_factory_map = scan.build_factory_map
resolve_chain = scan.resolve_chain
extract_slots = scan.extract_slots
find_matching_brace = scan.find_matching_brace
# Promovidas para tools/mixr_source_scan.py quando tools/extract_class_diagram.py
# virou o terceiro consumidor a precisar da mesma classificacao de tipo/papel
# primario -- ver o comentario delas la para o raciocinio completo.
build_descendants = scan.build_descendants
collect_descendants = scan.collect_descendants
classify_slot_type = scan.classify_slot_type
PRIMARY_COMPONENT_RE = scan.PRIMARY_COMPONENT_RE
role_name_from_base_class = scan.role_name_from_base_class

# Restringe o universo aos MESMOS modulos que app/src/mixr_factory.cpp de
# fato encadeia em mixrFactoryBuiltin() -- nao "tudo sob
# contexts/src/mixr/{src,include}", que tambem tem interop/common,
# interop/hla, interop/rprfom e linearsystem/, NENHUM encadeado ali (o dis so
# entra via mixr::dis::factory(), que e mixr::interop::dis por baixo -- ver o
# comentario da secao "src/poc/dis/bandit" no CLAUDE.md). Isso nao e so escopo:
# interop/hla e interop/rprfom tem classes com o MESMO NOME BARRA (sem
# namespace) de classes DIFERENTES e de fato usadas -- Aircraft, GroundVehicle,
# NetIO, Nib, Ntm -- confirmado rodando: build_inheritance()/extract_slots()
# indexam por nome de classe SEM namespace (build_inheritance() so guarda "o
# ultimo segmento"), entao "Aircraft" (mixr::models, 45 slots reais) e
# "Aircraft" (mixr::rprfom::RprFom.cpp, um NIB sem relacao nenhuma) colidem na
# mesma chave, e quem "ganha" depende so da ordem alfabetica de arquivo -- um
# risco real de corromper a entrada MAIS importante do catalogo (Aircraft)
# dependendo de qual arquivo e varrido por ultimo. Restringir aos modulos de
# fato encadeados elimina a colisao (nenhuma classe realmente usada neste
# repositorio reaparece com o mesmo nome barra em dois desses sete modulos) e
# tambem tira do catalogo fabricas que o parser deste repositorio nunca
# alcancaria de qualquer forma (HlaNetIO, RprFomNetIO, ...).
#
# Limitacao conhecida, restante mesmo com este recorte -- nao redescobrir:
# base::FileReader (fabrica "FileReader") e recorder::FileReader (fabrica
# "RecorderFileReader") sao DUAS classes diferentes com o MESMO nome barra
# "FileReader", uma em base/ e outra em recorder/ -- as duas ficam no
# catalogo (nenhuma fabrica e perdida, confirmado rodando), mas ambas mostram
# a cadeia/slots de qualquer uma que a varredura processar por ultimo
# (build_inheritance()/extract_slots() indexam por classe, sem namespace).
# Caso raro (so este par, dos 16 nomes colidentes originais, sobrevive a
# exclusao de interop/hla e interop/rprfom) e de baixo impacto (nenhum
# cenario real deste repositorio usa FileReader/RecorderFileReader) --
# resolver de verdade exigiria chave qualificada por namespace em
# build_inheritance()/extract_slots(), fora do escopo desta extensao.
EDL_CATALOG_MIXR_MODULES = [
    "base", "models", "terrain", "interop/dis", "linkage", "recorder", "simulation",
]
EDL_CATALOG_INCLUDE_ROOTS = [MIXR_INCLUDE / m for m in EDL_CATALOG_MIXR_MODULES] + [
    MODELS_DIR, LIBS_DIR,
]
EDL_CATALOG_SRC_ROOTS = [MIXR_SRC / m for m in EDL_CATALOG_MIXR_MODULES] + [
    MODELS_DIR, LIBS_DIR,
]

SLOT_MAP_RE = re.compile(
    r"BEGIN_SLOT_MAP\s*\(\s*(\w+)\s*\)(.*?)END_SLOT_MAP\s*\(\s*\)", re.S
)
# END_SLOT_MAP() nao repete o nome da classe (macros.hpp:313-315, ao contrario
# de END_SLOTTABLE(ThisType)) -- por isso o casamento e so pelo par mais
# proximo, nao por backreference.
ON_SLOT_RE = re.compile(r"\bON_SLOT\s*\(\s*(\d+)\s*,\s*(\w+)\s*,\s*([\w:]+)\s*\)")

def extract_slot_types(cpp_roots):
    """Classe -> {indice: [tipos C++ aceitos]}, a partir de ON_SLOT dentro de
    BEGIN_SLOT_MAP(Classe)/END_SLOT_MAP() (macros.hpp:302-324). Mais de um
    ON_SLOT no mesmo indice = tipo uniao (ex.: Antenna.beamWidth aceita Angle
    OU Number; RfSensor.modes aceita PairStream OU RfSensor).

    SEMPRE roda sobre texto MASCARADO -- sem isso, ON_SLOT comentado (ex.:
    IrSensor.cpp:54-56, dois slots do indice 7 desativados no fonte real)
    vira falso positivo e o indice 7 pareceria aceitar 3 tipos em vez de 1.
    Confirmado rodando sem o mascaramento antes de escrever esta versao.

    PRIMEIRO achado vence, por classe inteira (mesmo motivo/mesma ordem de
    varredura de extract_slots() em mixr_source_scan.py -- 'models/players/A-4'
    antes de 'models/template'): sem isso, os INDICES de ON_SLOT
    de dois arquivos diferentes para a mesma classe se MISTURARIAM num so
    dicionario, o que faz ainda menos sentido que so perder um dos dois --
    os indices de um arquivo nao tem relacao nenhuma com os do outro."""
    out: dict[str, dict[int, list[str]]] = {}
    seen_classes = set()
    for f in iter_files(cpp_roots, {".cpp"}):
        masked = mask_source(f.read_text(encoding="utf-8", errors="replace"))
        for m in SLOT_MAP_RE.finditer(masked):
            cls, body = m.group(1), m.group(2)
            if cls in seen_classes:
                continue
            seen_classes.add(cls)
            per_idx = out.setdefault(cls, {})
            for sm in ON_SLOT_RE.finditer(body):
                idx = int(sm.group(1))
                bare = sm.group(3).rsplit("::", 1)[-1]
                lst = per_idx.setdefault(idx, [])
                if bare not in lst:
                    lst.append(bare)
    return out


def dispatch_factory_cpp_paths():
    """Todos os factory.cpp cujo despacho real conta para 'concrete' no
    catalogo -- reunidos por GLOB, nunca lista fixa (o repositorio pode
    ganhar mais plugins/libs com o tempo, e o usuario pediu que isso seja
    automatico, sem tratamento especial por nome): os 7 factory.cpp nativos
    (os MESMOS EDL_CATALOG_MIXR_MODULES que ja restringem o resto deste
    gerador -- ver o comentario deles), mais
    'models/<categoria>/*/src/xnative/factory.cpp' para toda categoria de
    MODEL_CATEGORY_DIRS (A-4/C-130/paratrooper em players/, e qualquer modelo
    futuro em systems/ ou others/), MAIS 'models/*/src/xnative/factory.cpp'
    para um modelo de PRIMEIRO nivel -- hoje so' models/template/, que nao
    mora em categoria nenhuma e entra sem exclusao, como qualquer outro --
    e 'libs/*/factory.cpp' (resolve sozinho quais libs tem
    fabrica EDL -- hoje xtacview/xclock/xjoystick/xmsg/xplugin; uma lib sem
    fabrica, como xboard/xlog, so' nao tem o arquivo, sem tabela de exclusao
    curada). Caminhos inexistentes sao tolerados por
    find_dispatch_reachable_classes() (mesma tolerancia de iter_files)."""
    paths = [MIXR_SRC / m / "factory.cpp" for m in EDL_CATALOG_MIXR_MODULES]
    for categoria in sorted(MODEL_CATEGORY_DIRS):
        paths += sorted(MODELS_DIR.glob(f"{categoria}/*/src/xnative/factory.cpp"))
    paths += sorted(MODELS_DIR.glob("*/src/xnative/factory.cpp"))
    paths += sorted(LIBS_DIR.glob("*/factory.cpp"))
    return paths


def build_class_files(cpp_roots):
    """Classe -> arquivo .cpp onde IMPLEMENT_*SUBCLASS(Classe, ...) aparece --
    um passe so sobre todos os arquivos (ao contrario de uma busca refeita por
    classe -- caro demais para rodar uma vez por classe do catalogo inteiro)."""
    out: dict[str, str] = {}
    for f in iter_files(cpp_roots, {".cpp"}):
        masked = mask_source(f.read_text(encoding="utf-8", errors="replace"))
        for m in scan.IMPLEMENT_RE.finditer(masked):
            out.setdefault(m.group(1), str(f.relative_to(REPO_ROOT)))
    return out


def origin_of(impl_file):
    """De onde uma classe vem, pelo caminho do .cpp que a implementa: uma
    factory nativa do fork (agrupada pelo primeiro nivel sob
    contexts/src/mixr/src/, com interop/dis e interop/rprfom mantidos
    distintos), uma lib propria deste repo (libs/x*), ou algo do USUARIO
    sob ./models/ -- QUALQUER coisa la (nao so models/players/<nome>/): um
    plugin de player (inclusive o mirror de contrato em
    models/template/src/mirror.cpp), ou um payload de evento
    (models/events/<nome>/...). Generico de proposito -- um modelo novo em
    models/<qualquer-coisa>/ ja aparece rotulado na hora, sem precisar
    editar este arquivo.

    Confirmado rodando, duas vezes: (1) antes da generalizacao original,
    TacticalAlert (o payload real do evento de alerta tatico, implementado
    em models/events/payloads/EID_ALERT/TacticalAlert.cpp -- nao um
    'models/players/<nome>/') caia no 'return "builtin"' por engano, como
    se fosse uma classe nativa do MIXR; (2) antes do ajuste de
    MODEL_CATEGORY_DIRS, uma classe de Navstar-3 (o primeiro modelo real
    fora de models/players/) saia rotulada 'plugin:others' -- a CATEGORIA,
    nao o modelo -- porque o codigo so' tratava 'models/players/' como
    especial e caia no ramo generico para qualquer outra coisa sob
    models/."""
    if impl_file is None:
        return "unknown"
    if impl_file.startswith("models/"):
        rest = impl_file[len("models/"):].split("/")
        # 'models/<categoria>/<nome>/...' (players/others/systems) rotula
        # pelo NOME do modelo (rest[1]), nao pela categoria -- e' o que
        # distingue "plugin:Navstar-3" de "plugin:others" (o bug que existia
        # antes de um modelo real aparecer fora de players/ pela primeira
        # vez). Qualquer outra coisa sob models/ (ex.:
        # models/events/payloads/EID_ALERT/TacticalAlert.cpp) rotula pelo
        # PRIMEIRO nivel (rest[0], "events") -- nao e' uma categoria de
        # modelo, e' um projeto proprio.
        if rest[0] in MODEL_CATEGORY_DIRS and len(rest) > 1:
            return f"plugin:{rest[1]}"
        return f"plugin:{rest[0]}"
    if impl_file.startswith("libs/"):
        return "libs"
    prefix = "contexts/src/mixr/src/"
    if impl_file.startswith(prefix):
        parts = impl_file[len(prefix):].split("/")
        if parts[0] == "interop" and len(parts) > 1:
            return f"interop/{parts[1]}"
        return parts[0]
    return "builtin"


def load_edl_catalog_overrides():
    path = REPO_ROOT / "src" / "ui" / "edl_catalog_overrides.json"
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


# Alguns slots-lista (acceptsChildList) tem o setter C++ declarado como um
# 'base::PairStream' generico (ON_SLOT sem um segundo tipo de objeto unico) --
# o dynamic_cast/isClassType() de verdade mora DENTRO do corpo do setter, que
# nenhuma regex sobre a ASSINATURA alcanca (ver o comentario de isCompatible()
# em edl_builder_core.js). Investigado lendo o .cpp de cada um (nao adivinhado)
# antes de escrever esta tabela -- 36 casos no total nesta versao do fork
# (23 em LIST_SLOT_TYPE_OVERRIDES + 13 em TEXT_ONLY_LIST_SLOTS -- achado por
# auditoria, corrigido: o comentario dizia 35, nao redescobrir),
# nenhum sobrando (conferido: 'python3 -c' sobre o catalogo gerado, filtrando
# acceptsChildList=true e objectTypes vazio, some da lista depois deste
# override ser aplicado). Duas categorias:
#
# 'listSlotTypes' -- o setter FAZ dynamic_cast/isClassType() pra um tipo real
# (ex.: Station::setSlotNetworks() -> dynamic_cast<AbstractNetIO*>,
# simulation/Station.cpp:1007) -- a classe (ou uniao de classes, como
# Stores.stores aceitando Weapon OU ExternalStore) que a lista de fato aceita.
#
# 'textOnlySlots' -- o setter NUNCA espera um objeto MIXR ali: ou e' uma
# tabela de nomes/strings (ex.: TacviewOutput.typeMap, PluginModule.provides),
# ou e' uma mascara de bit por nome de categoria (Gimbal.playerOfInterestTypes:
# "air"/"ground"/"weapon"/...), ou e' uma lista de VETORES brutos '[ x y ]'
# (Navigation.feba, ScanGimbal.pseudoRandomPattern) -- nenhuma classe do
# catalogo jamais serviria ali; o caminho certo e' sempre o item de TEXTO
# ("+texto"), nunca uma classe arrastada.
LIST_SLOT_TYPE_OVERRIDES = {
    "Station.networks": ["AbstractNetIO"],
    "Station.igHosts": ["AbstractIgHost"],
    "IoHandler.devices": ["AbstractIoDevice"],
    "IoDevice.adapters": ["AbstractAdapter"],
    "NetIO.inputEntityTypes": ["Ntm"],
    "NetIO.outputEntityTypes": ["Ntm"],
    "NetIO.maxTimeDR": ["Time"],
    "NetIO.maxAge": ["Time"],
    "NetIO.maxPositionError": ["Distance"],
    "NetIO.maxEntityRange": ["Distance"],
    "NetIO.maxOrientationError": ["Angle"],
    "NetIO.emissionPduHandlers": ["EmissionPduHandler"],
    "Stores.stores": ["AbstractWeapon", "ExternalStore"],
    "Radio.channels": ["Frequency"],
    "MultiActorAgent.agentList": ["AbstractBehavior"],
    "Simulation.players": ["AbstractPlayer"],
    "Arbiter.behaviors": ["AbstractBehavior"],
    "MockDevice.generators": ["AbstractGenerator"],
    "MsgFeed.sinks": ["MsgSink"],
    "MsgFeed.messages": ["MsgReport"],
    "MsgReport.when": ["Condition"],
    "PluginLoader.modules": ["PluginModule"],
    "StateMachine.stateMachines": ["StateMachine"],
}
TEXT_ONLY_LIST_SLOTS = {
    "Gimbal.playerOfInterestTypes",
    "ScanGimbal.pseudoRandomPattern",
    "CollisionDetect.playerTypes",
    "Navigation.feba",
    "MsgSink.messages",
    "MsgReport.players",
    "MsgReport.labels",
    "MsgReport.fields",
    "PluginLoader.searchPaths",
    "PluginModule.provides",
    "TacviewOutput.typeMap",
    "TacviewOutput.colorMap",
    "TacviewOutput.modelMap",
}


def apply_list_slot_override(declared_in, slot_name, object_types):
    """Devolve (objectTypes, textOnly) -- object_types e' o override quando o
    par (declared_in, slot_name) esta' em LIST_SLOT_TYPE_OVERRIDES, [] com
    textOnly=True quando esta' em TEXT_ONLY_LIST_SLOTS, ou os valores
    recebidos sem mudanca (textOnly sempre False) caso contrario -- a MAIORIA
    dos slots-lista ja' vem com objectTypes correto do proprio ON_SLOT, e
    estas duas tabelas so' cobrem os casos onde a assinatura sozinha nao
    decide."""
    key = f"{declared_in}.{slot_name}"
    if key in LIST_SLOT_TYPE_OVERRIDES:
        return list(LIST_SLOT_TYPE_OVERRIDES[key]), False
    if key in TEXT_ONLY_LIST_SLOTS:
        return [], True
    return object_types, False


# ---------------------------------------------------------------------------
# 'papeis primarios' de Player -- dynamicsModel/pilot/navigation/datalink/
# radio/gimbal/rfSensor/irSystem/onboardComputer/storesMgr NAO sao slots (o
# catalogo de slots, acima, nao tem entrada nenhuma para eles): Player.hpp
# documenta explicitamente que esses nomes de chave em EDL sao COSMETICOS --
# quem resolve de verdade e Player::updateSystemPointers() (Player.cpp),
# varrendo a lista generica 'components:' por TIPO C++ (findByType()), nunca
# por nome. Sem esta extracao, o editor grafico nao tem como saber que um
# Aircraft "espera" um DynamicsModel/Pilot/etc. em algum lugar dos
# componentes -- e o que possibilita o placeholder "nenhum DynamicsModel
# encontrado" na arvore.
#
# A extracao mecanica em si (PRIMARY_COMPONENT_RE/role_name_from_base_class)
# mora em tools/mixr_source_scan.py (aliasada acima) -- este wrapper so'
# fixa a raiz (MIXR_SRC, resolvida do jeito proprio deste script) para
# manter o call site de baixo (e o de tests/tools/test_edl_catalog.py, que
# chama 'ext.extract_primary_components()' sem argumento) inalterado.
def extract_primary_components():
    """[{role, baseClass}], extraido do corpo de Player::updateSystemPointers()
    (contexts/src/mixr/src/models/player/Player.cpp) -- ver o comentario
    acima para o porque disso nao estar no catalogo de slots. Se o framework
    algum dia deixar de ter essa funcao nesse formato, isto devolve lista
    vazia (silencioso) -- e' o teste de integracao em
    tests/tools/test_edl_catalog.py, nao este script, quem trava contra essa
    regressao."""
    return scan.extract_primary_components(MIXR_SRC)


def build_edl_catalog():
    """Catalogo completo para o editor grafico de .edl: por classe (de TODAS
    as factories, nao so mixr::models), slots ACHATADOS por heranca, com
    tipo(s)/unidade/cardinalidade por slot -- derivados mecanicamente do
    fonte. Nao ha lista fixa de 'familias de unidade': uma familia e qualquer
    tipo que deriva de Number e tem descendente concreto (classify_slot_type),
    e os proprios descendentes (collect_descendants) viram as opcoes do
    dropdown. A curadoria manual e minima: src/ui/edl_catalog_overrides.json
    (os poucos slots 'referencia por nome' que o TIPO sozinho, String, igual
    a qualquer rotulo comum, nao distingue) -- 'primaryComponents' (abaixo)
    tambem NAO e curado, e extraido mecanicamente do fonte."""
    # Inclui tambem as raizes de SRC (nao so INCLUDE) na varredura de
    # heranca: dez headers deste fork moram junto do .cpp em vez de em
    # include/ (ex.: linkage/platform/UsbJoystick_linux.hpp) -- sem isso,
    # UsbJoystick saia sem base nenhuma (chain=['UsbJoystick'], sem herdar
    # 'adapters' de IoDevice), confirmado rodando contra o cenario real do
    # 'bandit' (que declara justamente 'adapters:' num UsbJoystick).
    # iter_files() so pega .hpp/.h aqui, entao os .cpp destas mesmas
    # pastas nao sao lidos de novo por engano.
    inheritance = build_inheritance(EDL_CATALOG_INCLUDE_ROOTS + EDL_CATALOG_SRC_ROOTS)
    factory_map = build_factory_map(EDL_CATALOG_SRC_ROOTS)   # nome-de-fabrica -> classe
    slot_names = extract_slots(EDL_CATALOG_SRC_ROOTS)        # classe -> [{name, comment}]
    slot_types = extract_slot_types(EDL_CATALOG_SRC_ROOTS)   # classe -> {indice: [tipos]}
    class_files = build_class_files(EDL_CATALOG_SRC_ROOTS)   # classe -> arquivo .cpp
    primary_components = extract_primary_components()        # [{role, baseClass}], so' de Player
    # Nomes de classe com despacho REAL num factory.cpp (nao so' declarada
    # via IMPLEMENT_*SUBCLASS em algum .cpp) -- ver find_dispatch_reachable_classes()
    # em tools/mixr_source_scan.py. Uma unica varredura para o catalogo
    # inteiro; cada entrada abaixo so' testa participacao no set (cls in
    # dispatch_reachable), sem repetir o parse de factory.cpp por classe.
    dispatch_reachable = scan.find_dispatch_reachable_classes(dispatch_factory_cpp_paths())

    # `dis::NetIO`/`dis::Ntm`/`dis::Nib` tem o MESMO nome barra que a propria
    # classe BASE delas (`DECLARE_SUBCLASS(NetIO, interop::NetIO)` -- o
    # segundo argumento vira bare "NetIO" apos build_inheritance() descartar
    # o namespace, IGUAL ao nome da propria classe) -- um self-loop que
    # resolve_chain() nao consegue atravessar (confirmado rodando:
    # inheritance["NetIO"] == "NetIO"). Sintoma medido, antes deste
    # remendo: o catalogo perdia 'inputEntityTypes'/'outputEntityTypes'
    # (declarados em interop/common/NetIO.cpp, nunca alcancados) -- e o
    # lint (src/ui/scripts/edl_lint.py) acusava "slot desconhecido" em
    # cenarios de PRODUCAO reais que usam exatamente esses dois slots.
    # Corrigido varrendo interop/common (que NAO entra no restante do
    # catalogo -- ver o comentario de EDL_CATALOG_MIXR_MODULES: nada de la
    # e encadeado pelo `app/src/mixr_factory.cpp` deste repositorio, entao
    # nao deveria aparecer na PALETA) so para EMENDAR o nivel-base que
    # falta, sob uma chave sintetica que quebra o self-loop sem contaminar
    # o restante do catalogo. Generico (detecta QUALQUER self-loop, nao so
    # estes tres) -- se um dia mais uma classe de dis/ ganhar o mesmo
    # padrao, o remendo ja cobre sem precisar de outra entrada hardcoded.
    self_loop = sorted(c for c, b in inheritance.items() if b == c)
    if self_loop:
        common_roots_inc = [MIXR_INCLUDE / "interop/common"]
        common_roots_src = [MIXR_SRC / "interop/common"]
        common_inheritance = build_inheritance(common_roots_inc)
        common_slot_names = extract_slots(common_roots_src)
        common_slot_types = extract_slot_types(common_roots_src)
        for c in self_loop:
            common_base = common_inheritance.get(c)
            if not common_base or common_base == c:
                continue  # nao achou base de verdade em interop/common -- deixa o self-loop como esta
            alias = f"{c}@interop_common"
            slot_names[alias] = common_slot_names.get(c, [])
            slot_types[alias] = common_slot_types.get(c, {})
            inheritance[alias] = common_base
            for k, v in common_inheritance.items():
                inheritance.setdefault(k, v)  # o resto da cadeia acima (AbstractNetIO -> ... -> Object)
            inheritance[c] = alias  # quebra o self-loop

    descendants = build_descendants(inheritance)
    reference_slots = set(load_edl_catalog_overrides().get("referenceSlots", {}))

    # Classe -> UM nome de fabrica (o primeiro encontrado), usado so para
    # rotular as OPCOES de um dropdown de unidade (ex.: a classe Decibel vira
    # a opcao "dB" -- fatoria::factory_name diverge do nome da classe C++,
    # confirmado no fonte). NAO e a fonte das entradas do catalogo: uma classe
    # pode ter MAIS DE UM nome de fabrica de verdade (ex.: base::FileReader
    # registra tanto "FileReader" quanto -- via recorder::FileReader, MESMO
    # nome de classe, classe DIFERENTE -- "RecorderFileReader"; colapsar por
    # classe aqui perderia esse segundo, real, nome de fabrica).
    class_to_factory: dict[str, str] = {}
    for factory_name, cls in factory_map.items():
        class_to_factory.setdefault(cls, factory_name)

    def clean(name):
        # Descarta o sufixo sintetico '@interop_common' do remendo de
        # self-loop acima -- nunca deve vazar pro JSON de saida, so serviu
        # pra achatar corretamente por dentro.
        return name.split("@", 1)[0] if name else name

    entries = []
    for factory_name, cls in sorted(factory_map.items()):
        chain = resolve_chain(cls, inheritance)

        flat_slots_raw = []
        # Da RAIZ (perto de Object) para a FOLHA (cls): slots de classe-base
        # primeiro -- mesma ordem de numeracao encadeada do SlotTable real
        # (SlotTable::baseTable, ver libs/xplugin/README.md).
        for lvl in reversed(chain):
            lvl_names = slot_names.get(lvl, [])
            lvl_types = slot_types.get(lvl, {})
            for i, item in enumerate(lvl_names, start=1):
                name = item["name"]
                types = lvl_types.get(i, [])
                kinds = [classify_slot_type(t, inheritance, descendants) for t in types]
                unit_families = sorted({k[1] for k in kinds if k[0] == "unit"})
                object_types = sorted({k[1] for k in kinds if k[0] == "object"})
                object_types, text_only = apply_list_slot_override(clean(lvl), name, object_types)
                flat_slots_raw.append({
                    "name": name,
                    "declaredIn": clean(lvl),
                    "comment": item["comment"],
                    "acceptsNumber": any(k[0] == "number" for k in kinds),
                    "acceptsBoolean": any(k[0] == "boolean" for k in kinds),
                    "acceptsText": any(k[0] == "text" for k in kinds),
                    "acceptsVector": any(k[0] == "vector" for k in kinds),
                    "acceptsChildList": any(k[0] == "list" for k in kinds),
                    "unitFamilies": [
                        {
                            "family": fam,
                            "options": [
                                class_to_factory.get(d, d)
                                for d in collect_descendants(fam, descendants)
                                if d in class_to_factory
                            ],
                        }
                        for fam in unit_families
                    ],
                    "objectTypes": object_types,
                    "isReference": name in reference_slots,
                    "textOnly": text_only,
                })

        # Uma subclasse pode REDECLARAR um slot com o mesmo nome que a base
        # ja tinha (ex.: dis::NetIO redeclara 'maxTimeDR' etc., que ja vem
        # de interop::common::NetIO, com o mesmo par de tipos) -- mantem a
        # posicao da PRIMEIRA aparicao (ordem base->folha) mas os DADOS da
        # ULTIMA (a redeclaracao mais derivada vence, mesma semantica de
        # SlotTable/setSlotByIndex real).
        by_name, order = {}, []
        for s in flat_slots_raw:
            if s["name"] not in by_name:
                order.append(s["name"])
            by_name[s["name"]] = s
        flat_slots = [by_name[n] for n in order]

        # primaryComponents e' de Player, herdado por QUALQUER descendente
        # (Aircraft, AirVehicle, GroundVehicle, SpaceVehicle, ...) que nao
        # redeclare updateSystemPointers() -- por isso e' so' presente se
        # 'Player' aparece na propria cadeia de heranca da classe, e vem
        # sempre da lista extraida de Player.cpp (nao ha override conhecido
        # neste fork, confirmado por grep antes de escrever este gerador).
        primary = list(primary_components) if primary_components and "Player" in chain else []

        entries.append({
            "class": cls,
            "factory": factory_name,
            "baseClass": clean(inheritance.get(cls)),
            "chain": [clean(lvl) for lvl in chain],
            "origin": origin_of(class_files.get(cls)),
            "concrete": cls in dispatch_reachable,
            "slots": flat_slots,
            "primaryComponents": primary,
        })

    known_names = set(factory_map.keys())
    entries.extend(introspect_thirdparty_plugins(
        known_names, inheritance, slot_names, slot_types, descendants,
        class_to_factory, reference_slots, primary_components,
    ))
    return entries


def find_plugininfo_binary():
    for candidate in PLUGININFO_CANDIDATES:
        if candidate.exists():
            return candidate
    return None


def thirdparty_plugin_label(so_path):
    """'libAcme.so' -> 'Acme' -- mesma convencao de 'plugin:<nome>' que o
    resto do catalogo ja usa para models/players/<nome>/ (origin_of() acima),
    so que aqui o <nome> vem do ARQUIVO, nao de um caminho fonte (nao ha
    fonte pra um .so de terceiro)."""
    name = so_path.stem
    if name.startswith("lib"):
        name = name[3:]
    return name or so_path.stem


def slot_def_from_known_ancestor(slot_name, chain, slot_names, slot_types, inheritance, descendants, class_to_factory, reference_slots):
    """Acha slot_name em algum ancestral da CADEIA (mais derivado primeiro)
    que o scan ESTATICO ja conhece (ex.: 'components', herdado de
    base::Component, que toda classe de terceiro relevante tambem herda) e
    devolve a MESMA forma de entrada de slot que o resto do catalogo usa --
    com tipo/unidade de verdade, nao um palpite. Devolve None se nenhum
    ancestral conhecido declara esse nome (ai' o slot e' PROPRIO da classe
    de terceiro, sem fonte -- so' o nome, nunca o tipo, ver o cabecalho de
    plugininfo_main.cpp para o porque disso ser um limite estrutural, nao
    uma lacuna deste script)."""
    for ancestor in chain:
        names = slot_names.get(ancestor, [])
        for idx, item in enumerate(names, start=1):
            if item["name"] != slot_name:
                continue
            types = slot_types.get(ancestor, {}).get(idx, [])
            kinds = [classify_slot_type(t, inheritance, descendants) for t in types]
            unit_families = sorted({k[1] for k in kinds if k[0] == "unit"})
            object_types = sorted({k[1] for k in kinds if k[0] == "object"})
            object_types, text_only = apply_list_slot_override(ancestor, slot_name, object_types)
            return {
                "name": slot_name, "declaredIn": ancestor, "comment": item["comment"],
                "acceptsNumber": any(k[0] == "number" for k in kinds),
                "acceptsBoolean": any(k[0] == "boolean" for k in kinds),
                "acceptsText": any(k[0] == "text" for k in kinds),
                "acceptsVector": any(k[0] == "vector" for k in kinds),
                "acceptsChildList": any(k[0] == "list" for k in kinds),
                "unitFamilies": [
                    {
                        "family": fam,
                        "options": [
                            class_to_factory.get(d, d)
                            for d in collect_descendants(fam, descendants)
                            if d in class_to_factory
                        ],
                    }
                    for fam in unit_families
                ],
                "objectTypes": object_types,
                "isReference": slot_name in reference_slots,
                "textOnly": text_only,
            }
    return None


def introspect_thirdparty_plugins(known_factory_names, inheritance, slot_names, slot_types,
                                   descendants, class_to_factory, reference_slots, primary_components):
    """Roda o binario 'plugininfo' sobre cada .so de plugins/ (o deposito
    flat, proprio OU de terceiro -- ver a secao 'plugins/' do CLAUDE.md) e
    devolve entradas de catalogo SO' para nomes de fabrica que o scan
    estatico ainda nao conhece. Um .so DESTE proprio repositorio (flight/
    template/template_mirror, tambem depositados ali por 'make models')
    contribui ZERO entradas novas -- todo nome dele ja veio do fonte, mais
    completo (com tipo/unidade de verdade), entao e' descartado aqui por
    colisao de nome, de proposito.

    Best-effort em CADA etapa: sem o binario compilado, sem plugins/, um
    .so que nao carrega (ABI incompativel, nao e' plugin MIXR), ou uma
    saida que nao parseia como JSON -- tudo silenciosamente ignorado. Isto
    NUNCA pode fazer o catalogo inteiro falhar por causa de um .so ruim
    largado em plugins/."""
    binary = find_plugininfo_binary()
    if binary is None or not PLUGINS_DIR.exists():
        return []

    entries = []
    for so_path in sorted(PLUGINS_DIR.glob("*.so")):
        try:
            proc = subprocess.run(
                [str(binary), str(so_path)],
                capture_output=True, text=True, timeout=15,
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        if proc.returncode != 0:
            continue
        try:
            data = json.loads(proc.stdout)
        except json.JSONDecodeError:
            continue

        plugin_label = thirdparty_plugin_label(so_path)
        for cls in data.get("classes", []):
            factory_name = cls.get("factory")
            if not factory_name or factory_name in known_factory_names:
                continue  # ja conhecido pelo scan estatico (com tipo de verdade) -- nada a acrescentar
            chain = cls.get("chain") or [cls.get("class", factory_name)]

            slot_list = []
            for slot_name in cls.get("slots", []):
                known = slot_def_from_known_ancestor(
                    slot_name, chain, slot_names, slot_types, inheritance,
                    descendants, class_to_factory, reference_slots,
                )
                if known:
                    slot_list.append(known)
                else:
                    # Slot PROPRIO da classe de terceiro -- sem fonte, sem
                    # tipo conhecido. Vira campo texto-ou-numero (o mais
                    # permissivo que o editor ja sabe desenhar -- mesmo
                    # padrao de Component.select), marcado 'typeUnknown'
                    # pra UI avisar "isto e um palpite, confira".
                    slot_list.append({
                        "name": slot_name, "declaredIn": chain[0], "comment": "",
                        "acceptsNumber": True, "acceptsBoolean": False, "acceptsText": True,
                        "acceptsVector": False, "acceptsChildList": False,
                        "unitFamilies": [], "objectTypes": [], "isReference": False,
                        "typeUnknown": True, "textOnly": False,
                    })

            # SlotTable::name(i) e' POSICIONAL, nao deduplicado -- uma
            # subclasse pode legitimamente REDECLARAR o nome de um slot da
            # base (mesma situacao ja tratada no scan estatico, acima,
            # 'by_name'/'order'). Mesma regra aqui: mantem a PRIMEIRA
            # posicao mas os DADOS da ULTIMA aparicao.
            by_name, order = {}, []
            for s in slot_list:
                if s["name"] not in by_name:
                    order.append(s["name"])
                by_name[s["name"]] = s
            slot_list = [by_name[n] for n in order]

            entries.append({
                "class": cls.get("class", factory_name),
                "factory": factory_name,
                "baseClass": chain[1] if len(chain) > 1 else None,
                "chain": chain,
                "origin": f"plugin:{plugin_label}",
                # Sempre True, incondicional: um .so que chegou ate aqui ja'
                # passou pela validacao de carga do PluginRegistry (guardas
                # de ABI -- ver o cabecalho de plugininfo_main.cpp) e o
                # binario 'plugininfo' so' lista classes que o proprio
                # MIXR_PLUGIN_DEFINE(...) publicou -- nao ha' deteccao nova
                # de despacho a fazer aqui (nao ha' factory.cpp pra ler).
                "concrete": True,
                "slots": slot_list,
                "primaryComponents": list(primary_components) if primary_components and "Player" in chain else [],
                "runtimeOnly": True,
            })
    return entries


def main():
    print(json.dumps(build_edl_catalog(), indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
