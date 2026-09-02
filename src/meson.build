# As duas pocs são o MESMO modelo; o nome diz só onde a decisão do UBF roda
# (SimAgent nativo, em updateData(), x FlightAgentTC próprio, na fase 3 do
# frame). A simulação em si é multithread nas duas — ver src/*/README.md.
subdir('./single-thread')
subdir('./multi-thread')

# bandit1 num processo proprio (joystick/Autopilot + emissao DIS) -- as duas
# pocs acima recebem esse player so pela rede, sem declara-lo localmente.
subdir('./bandit-dis')

# O dashboard (TUI de controle/monitoramento) NAO mora aqui -- ver
# ./app/meson.build, subdir'ado direto da raiz. E o UNICO ocupante de
# ./app/, por isso o alvo se chama 'app', nao 'dashboard'.
