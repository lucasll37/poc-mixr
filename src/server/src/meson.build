# API REST de simulacao -- DOIS executaveis, ver src/server/README.md.
#
# Declarados AQUI (nao no meson.build raiz), mesmo precedente de
# app/src/meson.build com ftxui_dep: sao os UNICOS consumidores.
json_dep = dependency('nlohmann_json', method: 'pkg-config', required: true)
httplib_dep = dependency('cpp-httplib', method: 'pkg-config', required: true)

local_inc_dir = include_directories('../include')

# ------------------------------------------------------------------------
# sim-runner -- o UNICO dos dois que linka mixr_dep. Roda UMA Station por
# processo (ver o cabecalho de main_runner.cpp) -- sem xtacview/xclock/
# xjoystick/xmsg/xlog: nada disso e usado (sem Tacview, sem controle de
# tempo, sem joystick, sem mensagens, sem log em arquivo).
# ------------------------------------------------------------------------
runner_sources = files(
   'main_runner.cpp',
   'mixr_factory.cpp',
   'app/RunnerOptions.cpp',
   'app/TerrainData.cpp',
   'app/ScenarioAssembler.cpp',
   'app/StationBuilder.cpp',
   'app/Fleet.cpp',
   'app/TelemetryJson.cpp',
)

sim_runner = executable('sim-runner',
   runner_sources,
   cpp_args            : asan_cpp_args,
   link_args           : asan_link_args + rpath_link_args,
   include_directories : local_inc_dir,
   dependencies        : [thread_dep, mixr_dep, xboard_dep, xtrack_dep, xplugin_dep, json_dep],
   install             : true,
   install_dir         : get_option('bindir'),
   install_rpath       : mixr_libdir + ':' + own_libs_rpath,
   build_rpath         : mixr_libdir,
)

# ------------------------------------------------------------------------
# server -- HTTP puro. NAO linka mixr_dep: so texto, arquivo e processo
# (dispara sim-runner por fork+execv, nunca por shell -- ver
# app/Subprocess.cpp).
# ------------------------------------------------------------------------
server_sources = files(
   'main_server.cpp',
   'app/ServerOptions.cpp',
   'app/ScenarioUpload.cpp',
   'app/Subprocess.cpp',
   'app/HttpServer.cpp',
)

server = executable('server',
   server_sources,
   include_directories : local_inc_dir,
   dependencies        : [thread_dep, httplib_dep, json_dep],
   install             : true,
   install_dir         : get_option('bindir'),
)
