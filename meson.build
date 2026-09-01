project('poc-mixr', 'cpp',
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

cpp = meson.get_compiler('cpp')

# dlopen/dlsym, para a carga de modelos em tempo de execucao
# (shared/xplugin). Em glibc >= 2.34 as duas moram na libc e este
# find_library nao acha nada -- por isso required: false, que cobre as duas
# eras. NAO vem de graca por nenhum .pc: conferido que o da
# behaviortree.cpp.asa traz so '-lbehaviortree_cpp_v3 -lpthread'.
dl_dep = cpp.find_library('dl', required: false)

mixr_dep = dependency('mixr', method: 'pkg-config', required: true)
behavior_tree_dep = dependency('behaviortree.cpp.asa', method: 'pkg-config', required: true)

# Usado diretamente por shared/xtacview: o recorder nativo entrega os dados
# como mensagens protobuf (DataRecord.pb.h).
protobuf_dep = dependency('protobuf', method: 'pkg-config', required: true)

# Nenhum subprojeto atual fala com a JSBSim::FGFDMExec direto (todos usam o
# JSBSimModel do próprio MIXR); fica declarado para o rpath de um subprojeto
# futuro que precise (ver mixr_bt_jsbsim_libdir abaixo e a seção "Ao
# adicionar um subprojeto novo" do CLAUDE.md).
jsbsim_dep = dependency('jsbsim', method: 'pkg-config', required: true)

# Resolve o libdir de cada dependência via pkg-config. Ambas entregam
# bibliotecas COMPARTILHADAS (.so) que vivem no cache do Conan, fora de
# qualquer caminho padrão do loader — então todo executável precisa
# carregar o(s) libdir(s) das dependências que usa no seu rpath, senão
# roda a partir de build/ mas falha a partir de dist/ com
# "error while loading shared libraries".
mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')
bt_libdir = behavior_tree_dep.get_variable(pkgconfig: 'libdir')

# Para os alvos que usam as duas (single-thread, multi-thread): rpath aceita
# vários caminhos separados por ':'.
mixr_bt_libdir = ':'.join([mixr_libdir, bt_libdir])

# Para um subprojeto futuro que use as três (MIXR + BehaviorTree.CPP + JSBSim).
jsbsim_libdir = jsbsim_dep.get_variable(pkgconfig: 'libdir')
mixr_bt_jsbsim_libdir = ':'.join([mixr_libdir, bt_libdir, jsbsim_libdir])

# Emite DT_RPATH em vez do DT_RUNPATH moderno. Necessário porque RPATH é
# herdado pelas dependências transitivas e RUNPATH não: sem isso, o
# loader acha as libs nomeadas no executável, mas falha ao resolver as
# dependências entre elas (libmixr_models.so -> libmixr_base.so).
rpath_link_args = ['-Wl,--disable-new-dtags']

# As nossas proprias .so (xlog, xboard) instalam em <prefix>/lib e os
# executaveis em <prefix>/bin, entao o rpath instalado precisa do caminho
# RELATIVO ao binario -- os libdir do Conan sao absolutos e nao cobrem isto.
# Sem esta entrada, roda a partir de build/ e falha a partir de dist/ com
# "error while loading shared libraries", que e o gotcha ja documentado no
# CLAUDE.md.
own_libs_rpath = '$ORIGIN/../lib'

# -Dasan=true liga o AddressSanitizer só nos alvos que consomem estas flags
# (single-thread e multi-thread), sem recompilar o resto do projeto.
asan_cpp_args = get_option('asan') ? ['-fsanitize=address', '-fno-omit-frame-pointer', '-g'] : []
asan_link_args = get_option('asan') ? ['-fsanitize=address'] : []

# ============================================
# Subdiretórios
# ============================================

# Bibliotecas compartilhadas entre subprojetos (padrao shared/x<nome> do
# MIXR). Precisa vir ANTES de src/, que consome xtacview_dep.
subdir('./shared/xtacview')

# Controle de velocidade do tempo (acelerar/frear/pausar). Tambem precisa vir
# ANTES dos subprojetos, que consomem xclock_dep.
subdir('./shared/xclock')

# Controle do ownship por joystick fisico. Tambem precisa vir ANTES dos
# subprojetos, que consomem xjoystick_dep.
subdir('./shared/xjoystick')

# Sistema de log (LOG(NIVEL) << ...;). Tambem precisa vir ANTES dos
# subprojetos, que consomem xlog_dep.
subdir('./shared/xlog')

# O quadro de leitura entre o host e o modelo. E a UNICA peca que o executavel
# e o plugin de modelo compartilham -- por isso e shared_library() de verdade,
# e nao static como as demais de shared/ (ver shared/xboard/Board.hpp).
subdir('./shared/xboard')

# Mensagens configuraveis por EDL (telemetria e eventos). Tambem antes dos
# subprojetos, que consomem xmsg_dep.
subdir('./shared/xmsg')

# Carga de modelos em tempo de execucao (dlopen). Exporta xplugin_abi_dep (o
# contrato, header-only, usado pelos plugins) e xplugin_dep (o registro,
# usado pelos executaveis). Precisa vir ANTES de src/.
subdir('./shared/xplugin')

subdir('./src')

# Suite de testes. Fica DEPOIS de src/ porque as camadas de cenario, memoria
# e determinismo rodam os executaveis das pocs. Atras da opcao -Dtests=true
# para que um build comum nao precise do gtest resolvido.
if get_option('tests')
    subdir('./tests')
endif

# ============================================
# Summary
# ============================================
summary({
    'Build Type':   get_option('buildtype'),
    'C++ Standard': get_option('cpp_std'),
    'Prefix':       get_option('prefix'),
}, section: 'Configuration')