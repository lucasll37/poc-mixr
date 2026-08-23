project('mixr-hello', 'cpp',
    version: '1.0.0',
    license: 'MIT',
    default_options: [
        'cpp_std=c++17',
        'warning_level=1',
        'werror=false',
    ],
)

message('mixr-hello - Meson Build')
message('Version: ' + meson.project_version())
message('Build Type: ' + get_option('buildtype'))

# ============================================
# Dependências
# ============================================
thread_dep = dependency('threads')

mixr_dep = dependency('mixr', method: 'pkg-config', required: true)
behavior_tree_dep = dependency('behaviortree.cpp.asa', method: 'pkg-config', required: true)

# Usado diretamente por shared/xtacview: o recorder nativo entrega os dados
# como mensagens protobuf (DataRecord.pb.h).
protobuf_dep = dependency('protobuf', method: 'pkg-config', required: true)

# Usado diretamente por poc/12: a dinâmica 6-DOF fala com a JSBSim::FGFDMExec
# sem passar pelo adaptador do MIXR (o pacote vem como dependência transitiva
# do mixr, e o pkg-config já expõe include/JSBSim).
jsbsim_dep = dependency('jsbsim', method: 'pkg-config', required: true)

# Resolve o libdir de cada dependência via pkg-config. Ambas entregam
# bibliotecas COMPARTILHADAS (.so) que vivem no cache do Conan, fora de
# qualquer caminho padrão do loader — então todo executável precisa
# carregar o(s) libdir(s) das dependências que usa no seu rpath, senão
# roda a partir de build/ mas falha a partir de dist/ com
# "error while loading shared libraries".
mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')
bt_libdir = behavior_tree_dep.get_variable(pkgconfig: 'libdir')

# Para os alvos que usam as duas (poc/03, poc/05): rpath aceita vários
# caminhos separados por ':'.
mixr_bt_libdir = ':'.join([mixr_libdir, bt_libdir])

# poc/12 usa as três (MIXR + BehaviorTree.CPP + JSBSim).
jsbsim_libdir = jsbsim_dep.get_variable(pkgconfig: 'libdir')
mixr_bt_jsbsim_libdir = ':'.join([mixr_libdir, bt_libdir, jsbsim_libdir])

# Emite DT_RPATH em vez do DT_RUNPATH moderno. Necessário porque RPATH é
# herdado pelas dependências transitivas e RUNPATH não: sem isso, o
# loader acha as libs nomeadas no executável, mas falha ao resolver as
# dependências entre elas (libmixr_models.so -> libmixr_base.so).
rpath_link_args = ['-Wl,--disable-new-dtags']

# Depuração temporária (poc/13): -Dasan=true liga o AddressSanitizer só no
# alvo que precisa, sem recompilar o resto do projeto.
asan_cpp_args = get_option('asan') ? ['-fsanitize=address', '-fno-omit-frame-pointer', '-g'] : []
asan_link_args = get_option('asan') ? ['-fsanitize=address'] : []

# ============================================
# Includes
# ============================================
inc_dir = include_directories('include')

# ============================================
# Subdiretórios
# ============================================

# Bibliotecas compartilhadas entre subprojetos (padrao shared/x<nome> do
# MIXR). Precisa vir ANTES dos poc/, que consomem xtacview_dep.
subdir('./shared/xtacview')

subdir('./src')

# ============================================
# Summary
# ============================================
summary({
    'Build Type':   get_option('buildtype'),
    'C++ Standard': get_option('cpp_std'),
    'Prefix':       get_option('prefix'),
}, section: 'Configuration')

# summary({
#     'poc/01-flying-aircraft':  flying_aircraft.full_path(),
#     'poc/02-behavior-tree':    behavior_tree_poc.full_path(),
#     'poc/03-bt-autopilot':     bt_autopilot.full_path(),
#     'poc/04-jsbsim-6dof':      jsbsim_6dof.full_path(),
#     'poc/05-formation-flight': formation_flight.full_path(),
#     'poc/06-radar-detection':  radar_detection.full_path(),
#     'poc/07-radar-intercept':  radar_intercept.full_path(),
#     'poc/08-event-relay':      event_relay.full_path(),
#     'poc/09-chaff-flare':      chaff_flare.full_path(),
#     'poc/10-satellite-constellation': satellite_constellation.full_path(),
#     'poc/11-custom-models':    custom_models.full_path(),
#     'poc/12-jsbsim-ubf':       jsbsim_ubf.full_path(),
#     'poc/13-native-stack':     native_stack.full_path(),
#     'poc/14-tc-agent':         tc_agent.full_path(),
# }, section: 'Build Artifacts')