# As duas pocs são o MESMO modelo; o nome diz só onde a decisão do UBF roda
# (SimAgent nativo, em updateData(), x FlightAgentTC próprio, na fase 3 do
# frame). A simulação em si é multithread nas duas — ver src/*/README.md.
subdir('./single-thread')
subdir('./multi-thread')

# bandit1 num processo proprio (joystick/Autopilot + emissao DIS) -- as duas
# pocs acima recebem esse player so pela rede, sem declara-lo localmente.
subdir('./bandit-dis')

# TUI de controle/monitoramento (estilo btop) -- a MESMA pilha nativa de
# single-thread, so trocando o laco de tempo real por um painel FTXUI. Tres
# cenarios PROPRIOS, herméticos (ver src/dashboard/configs/).
subdir('./dashboard')
