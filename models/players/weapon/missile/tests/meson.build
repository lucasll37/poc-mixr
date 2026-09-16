# ==============================================================================
# Suite deste modelo -- tres camadas: a regra pura (domain, a lei de
# navegacao proporcional + a espoleta de proximidade), a classe MIXR propria
# SEM Station (native, achado por auditoria -- ver abaixo) e a FORMA do
# artefato (contract). Nao ha suite 'tree' -- este modelo nao tem arvore de
# comportamento (guiagem e controle continuo, nao decisao discreta, ver
# docs/ARCHITECTURE.md).
#
# ATUALIZACAO (achado por auditoria, nao redescobrir): este comentario dizia
# que nao valia a pena ter 'native' porque "levantar uma Station so' pra
# instanciar um Missile isolado nao provaria nada que 'domain' + o cenario
# sandbox/A4-6DOF-MISSILE ja nao provem". Isso e verdade para um native
# PESADO (com Station/EDL) -- mas GuidedMissile::atReleaseInit()/
# weaponDynamics() (a integracao de rumo/arfagem/velocidade, o clamp de
# taxa de giro) moram SO' em xnative/GuidedMissile.cpp, nunca em
# domain/Guidance.cpp, e nao precisam de Station nenhuma pra testar -- so'
# de um WorldModel+Player de bancada (mesmo padrao 'Bench' ja usado em
# models/players/air/A-4/tests/native/test_flight_state_action.cpp). O caso real
# que motivou isto: um bug medido e corrigido (atReleaseInit() nao existia)
# ficou sem NENHUM teste travando a correcao -- so' um comentario avisando
# "nao redescobrir". 'native' aqui e' essa camada leve, nao a pesada que o
# paragrafo original rejeitava.
#
# 'model_inc' e 'missile_lib' vem do escopo do meson.build PAI (Meson
# compartilha o namespace entre um project() e os subdir() dele) -- nao sao
# redeclarados aqui.
#
# ARMADILHA MEDIDA (documentada em CLAUDE.md, raiz, e em todo meson.build de
# modelo deste repositorio): 'meson test' devolve rc=0 para suite VAZIA
# ("No tests defined."), e o default de -Dtests e false. O Makefile confere
# a contagem com 'meson introspect --tests' antes de rodar -- ver o alvo
# 'test' dele.
# ==============================================================================
gtest_dep      = dependency('gtest', method: 'pkg-config')
gtest_main_dep = dependency('gtest_main', method: 'pkg-config')

# Camada 1 -- a regra pura: sem MIXR, sem rpath nenhum. Acrescente um
# 'test_XXX.cpp' aqui ao lado a cada regra nova em domain/. 'domain_sources'
# vem do meson.build PAI -- os MESMOS arquivos que o .so compila, nunca uma
# copia.
test_domain = executable('test-domain',
    files('domain/test_Guidance.cpp') + domain_sources,
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('domain', test_domain, suite: 'domain', timeout: 60)

# Camada nova -- xnative/GuidedMissile.cpp SEM Station (WorldModel+Player de
# bancada, ver o "ATUALIZACAO" acima). Precisa de mixr_dep/sdk_dep (xlog) e
# do rpath ate as .so do MIXR -- diferente de 'domain', que roda sem
# nenhuma das duas.
test_native = executable('test-native',
    files('native/test_GuidedMissile.cpp') + domain_sources + files(
        '../src/xnative/GuidedMissile.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, sdk_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('native', test_native, suite: 'native', timeout: 60)

# Camada 2 -- a FORMA do artefato (nao o comportamento): exatamente um
# simbolo T exportado (o ponto de entrada de MIXR_PLUGIN_DEFINE) e nenhuma
# dependencia dinamica nao resolvida -- os dois criterios que
# CONTRIBUTING.md secao 5.1 (raiz) ja documentava a mao.
check_contract = find_program('check_contract.sh')
test('contrato-simbolo-unico-e-deps-resolvidas', check_contract,
    args    : [missile_lib],
    suite   : 'contract',
    timeout : 30,
)
