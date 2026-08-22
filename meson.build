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

# Emite DT_RPATH em vez do DT_RUNPATH moderno. Necessário porque RPATH é
# herdado pelas dependências transitivas e RUNPATH não: sem isso, o
# loader acha as libs nomeadas no executável, mas falha ao resolver as
# dependências entre elas (libmixr_models.so -> libmixr_base.so).
rpath_link_args = ['-Wl,--disable-new-dtags']

# ============================================
# Includes
# ============================================
inc_dir = include_directories('include')

# ============================================
# Subdiretórios
# ============================================
subdir('./src')

# Subprojetos incrementais da PoC (cada um coexiste como um executável
# próprio). Adicionar um novo subdir() aqui para cada poc/NN-slug novo.
subdir('./poc/01-flying-aircraft')
subdir('./poc/02-behavior-tree')
subdir('./poc/03-bt-autopilot')
subdir('./poc/04-jsbsim-6dof')
subdir('./poc/05-formation-flight')
subdir('./poc/06-radar-detection')
subdir('./poc/07-radar-intercept')
subdir('./poc/08-event-relay')
subdir('./poc/09-chaff-flare')
subdir('./poc/10-satellite-constellation')

# ============================================
# Summary
# ============================================
summary({
    'Build Type':   get_option('buildtype'),
    'C++ Standard': get_option('cpp_std'),
    'Prefix':       get_option('prefix'),
}, section: 'Configuration')

summary({
    'poc-mixr (hello)':        hello.full_path(),
    'poc/01-flying-aircraft':  flying_aircraft.full_path(),
    'poc/02-behavior-tree':    behavior_tree_poc.full_path(),
    'poc/03-bt-autopilot':     bt_autopilot.full_path(),
    'poc/04-jsbsim-6dof':      jsbsim_6dof.full_path(),
    'poc/05-formation-flight': formation_flight.full_path(),
    'poc/06-radar-detection':  radar_detection.full_path(),
    'poc/07-radar-intercept':  radar_intercept.full_path(),
    'poc/08-event-relay':      event_relay.full_path(),
    'poc/09-chaff-flare':      chaff_flare.full_path(),
    'poc/10-satellite-constellation': satellite_constellation.full_path(),
}, section: 'Build Artifacts')