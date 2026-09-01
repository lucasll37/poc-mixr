project('poc-mixr', 'cpp',
    version: '1.0.0',
    license: 'MIT',
    default_options: [
        'cpp_std=c++17',
        'warning_level=1',
        'werror=false',
    ],
)

message('POC-MIXR - Meson Build')
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

# Usado diretamente por shared/xtacview: o recorder nativo entrega os dados
# como mensagens protobuf (DataRecord.pb.h).
protobuf_dep = dependency('protobuf', method: 'pkg-config', required: true)


# Resolve o libdir do MIXR via pkg-config. As libs dele são COMPARTILHADAS
# (.so) e vivem no cache do Conan, fora de qualquer caminho padrão do loader —
# então todo executável precisa carregá-lo no rpath, senão roda a partir de
# build/ mas falha a partir de dist/ com "error while loading shared libraries".
mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')

# NAO ha behavior_tree_dep nem jsbsim_dep aqui, e a ausencia e deliberada:
# depois que o modelo saiu para models/flight-model (projeto meson proprio),
# NENHUM alvo deste projeto linka BehaviorTree.CPP ou JSBSim. Quem precisa
# delas e o modelo, e ele declara as suas.
#
# Se um subprojeto novo do HOST algum dia precisar de uma das duas, e sinal de
# que ele esta fazendo trabalho de modelo -- reconsidere antes de acrescentar
# a linha de volta.



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

# Consulta ao radar nativo -- disputada pelo host (dump) e pelo modelo
# (percepcao), por isso .so e por isso no SDK.
subdir('./shared/xtrack')

# Mensagens configuraveis por EDL (telemetria e eventos). Tambem antes dos
# subprojetos, que consomem xmsg_dep.
subdir('./shared/xmsg')

# Carga de modelos em tempo de execucao (dlopen). Exporta xplugin_abi_dep (o
# contrato, header-only, usado pelos plugins) e xplugin_dep (o registro,
# usado pelos executaveis). Precisa vir ANTES de src/.
subdir('./shared/xplugin')


# ==============================================================================
# O SDK DE PLUGIN
#
# Tudo o que um TERCEIRO precisa para compilar um modelo sem ter este
# repositorio: o contrato (header-only) e as tres .so que atravessam a
# fronteira. Publicado em dist/{include,lib,lib/pkgconfig}.
#
# COMO INSTALAR SO O SDK (e a etapa 'sdk' do Makefile):
#     ninja -C build xboard xlog xtrack        # por NOME de alvo
#     meson install -C build --no-rebuild --tags sdk,devel
#
# DUAS ARMADILHAS MEDIDAS, nao supostas:
#
#   1. 'meson compile -C build shared/xboard/libxboard.so' NAO funciona --
#      "target not found". O meson compile resolve por NOME de alvo; quem
#      resolve por caminho de saida e o ninja. Dai o 'ninja' acima.
#
#   2. '--tags sdk' SOZINHO instala so as .so. install_headers() nao aceita
#      install_tag no meson 1.2, entao os headers ficam com a tag automatica
#      'devel' e sao EXCLUIDOS. O sintoma aparece longe: o build do modelo
#      morre com "xplugin/PluginAbi.hpp: No such file or directory", dois
#      alvos depois. Por isso '--tags sdk,devel'.
#
# O SDK NAO exporta -DMIXR_PLUGIN_PKG_VERSION / -DMIXR_PLUGIN_BUILD_ID pelo
# .pc, embora isso funcione tecnicamente (medido: as aspas escapadas
# atravessam intactas ate o compile_commands.json). E deliberado: a impressao
# digital descreve QUEM CONSTRUIU o plugin, entao cada lado calcula a sua e o
# registro compara as duas na carga. Um -D vindo do .pc faria os dois lados
# reportarem o mesmo numero por construcao, e a checagem viraria tautologia.
# O .pc expoe 'mixr_pkg_version' como VARIAVEL, para conferencia cruzada.
# ==============================================================================
install_headers('shared/xplugin/PluginAbi.hpp', subdir: 'xplugin')
install_headers('shared/xboard/Board.hpp',      subdir: 'xboard')
install_headers('shared/xlog/Log.hpp',          subdir: 'xlog')
install_headers('shared/xtrack/TrackQuery.hpp', subdir: 'xtrack')

pkg = import('pkgconfig')
pkg.generate(
    name        : 'poc-mixr-sdk',
    description : 'SDK para compilar um modelo (plugin) do poc-mixr',
    libraries   : [xboard_lib, xlog_lib, xtrack_lib],
    requires    : ['mixr'],
    variables   : ['mixr_pkg_version=' + mixr_dep.version()],
)

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