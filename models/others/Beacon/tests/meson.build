# ==============================================================================
# Suite deste modelo -- so' DUAS camadas, porque Beacon so' tem UMA classe de
# comportamento (sem domain/ puro, sem arvore -- ver o cabecalho de
# ../src/plugin.cpp): as classes MIXR proprias (native, SEM levantar
# Station) e a FORMA do .so publicado (contract).
#
# ARMADILHA MEDIDA (documentada em CLAUDE.md, raiz, e em todo meson.build de
# modelo deste repositorio): 'meson test' devolve rc=0 para suite VAZIA
# ("No tests defined."), e o default de -Dtests e false. O Makefile confere
# a contagem com 'meson introspect --tests' antes de rodar -- ver o alvo
# 'test' dele.
#
# 'model_inc'/'beacon_lib' vem do escopo do meson.build PAI (Meson
# compartilha o namespace entre um project() e os subdir() dele) -- nao sao
# redeclarados aqui.
# ==============================================================================
gtest_dep      = dependency('gtest', method: 'pkg-config')
gtest_main_dep = dependency('gtest_main', method: 'pkg-config')

# Camada 1 -- a classe Beacon de verdade, SEM levantar Station: um
# mixr::models::WorldModel de bancada com dois Beacon direto em
# players: (sem EDL), exercitando emitir/tratar o evento pela API publica
# que Beacon::updateData()/event() ja chamam. Reusa o MESMO fonte que vai
# para o .so, nao uma copia.
test_native = executable('test-native',
    files('native/test_beacon.cpp')
        + files('../src/Beacon.cpp', '../src/xnative/factory.cpp'),
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, sdk_dep],
    link_args           : ['-Wl,--disable-new-dtags'],
    install             : false,
    build_rpath         : mixr_libdir + ':' + sdk_libdir,
)
test('native', test_native, suite: 'native', timeout: 60)

# Camada 2 -- a FORMA do artefato (nao o comportamento): exatamente um
# simbolo T exportado (o ponto de entrada de MIXR_PLUGIN_DEFINE) e nenhuma
# dependencia dinamica nao resolvida -- os dois criterios que
# CONTRIBUTING.md secao 5.1 (raiz) ja documentava a mao. Convencao herdada
# de models/template/ (e de todo modelo gerado por 'make new-model' depois
# dela).
check_contract = find_program('check_contract.sh')
test('contrato-simbolo-unico-e-deps-resolvidas', check_contract,
    args    : [beacon_lib],
    suite   : 'contract',
    timeout : 30,
)
