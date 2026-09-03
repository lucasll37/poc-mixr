# As duas pocs são o MESMO modelo; o nome diz só onde a decisão do UBF roda
# (SimAgent nativo, em updateData(), x FlightAgentTC próprio, na fase 3 do
# frame). A simulação em si é multithread nas duas — ver src/poc/*/README.md.
subdir('./single-thread')
subdir('./multi-thread')

# bandit1 num processo proprio (joystick/Autopilot + emissao DIS) -- as duas
# pocs acima recebem esse player so pela rede, sem declara-lo localmente.
subdir('./bandit-dis')

# A MESMA pilha do multi-thread, com as leis de voo em PYTHON: a arvore chama
# ( PyDecide ), que roda um decide(obs) de configs/policy/*.py dentro da fase 3
# do frame. Editar a politica deixa de ser recompilar -- ver o README.
subdir('./python-flight')

# A MESMA pilha do multi-thread, com a decisao numa REDE NEURAL: a arvore tem
# uma folha so, ( OnnxPolicy ), que infere um .onnx dentro da fase 3 do frame.
# Trocar as regras pela rede foi trocar o 'treeFile:' -- ver o README.
subdir('./onnx-policy')
