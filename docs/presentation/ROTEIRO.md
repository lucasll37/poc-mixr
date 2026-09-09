=I. Abertura — o problema que nenhuma ferramenta resolve

Antes de falar de código, de repositório ou de processo, eu quero falar de uma coisa que não vai mudar com nada do que eu vou apresentar hoje — e é importante começar por ela, porque ela é a régua contra a qual tudo o resto deve ser medido.

Simulação aeroespacial é um domínio difícil por natureza. Não é a dificuldade de "esse código está mal escrito" ou "essa arquitetura está confusa" — é uma dificuldade  **inerente ao domínio** . Estamos falando de muitos agentes — aeronaves, sensores, armas, sistemas de navegação — cada um com uma dinâmica própria, e que precisam reagir e causar efeitos uns nos outros em tempo real. Um radar que detecta, um piloto automático que reage, um alerta que se propaga para outras aeronaves, um terreno que impõe um piso de segurança. Tudo isso acontecendo ao mesmo tempo, com dependências cruzadas.

E aqui está o ponto que eu quero deixar bem claro: quando alguém implementa um modelo novo, o trabalho não termina em implementar a dinâmica desse modelo. O trabalho real é  **revisitar todos os modelos que já existem** , para especificar o efeito que essa nova dinâmica tem sobre eles — e vice-versa. Um míssil novo precisa saber como reagir a um chaff que já existia. Uma aeronave nova precisa ser vista corretamente por um sistema de alerta que foi escrito antes dela existir. Isso não é acidental, é o próprio significado de "simular um domínio com agentes que interagem".

Parte disso já é facilitada pelo framework que usamos — o MIXR. Ele já vem com uma dinâmica de eventos e um conjunto de classes-base pensadas exatamente para resolver esse tipo de acoplamento sem que cada par de modelos precise se conhecer diretamente. Mas — e esse "mas" é importante — isso só funciona se usarmos o framework do jeito que ele foi desenhado para ser usado. Exige entender as abstrações dele, entender o vocabulário dele, usá-lo de forma idiomática. Um MIXR mal utilizado não entrega nada disso de graça.

Então, para deixar bem concreto o que eu quero que fique desta primeira parte: a dificuldade de domínio é indissociável do trabalho. A proficiência em modelagem — em entender a física, o comportamento, as interações do domínio que estamos simulando — é uma barreira que **nenhuma reestruturação de processo, nenhuma ferramenta nova, nenhum framework** vai amenizar. Ela é intrínseca ao que estamos simulando. Isso não é pessimismo — é honestidade sobre onde está o verdadeiro custo do nosso trabalho, para que a gente não gaste energia tentando eliminar por engenharia de software algo que é, na raiz, um problema de domínio.

[pausa]

O que dá para fazer — e é sobre isso que eu vou falar no resto desta apresentação — é garantir que **tudo o que não é essa dificuldade intrínseca** deixe de atrapalhar. Que a gente não precise brigar com o ambiente, com a organização do código, com processo sem rastreabilidade, para só então chegar ao problema de verdade, que já é difícil o suficiente sozinho.

---

### II. A proposta — uma reestruturação para devolver produtividade e escala

Foi com esse diagnóstico que o time de framework propôs uma reestruturação de como a base da nossa aplicação se organiza hoje. A expectativa não é remover a dificuldade de modelar — como eu disse, isso não é removível — é recuperar a produtividade e a capacidade de escalar o desenvolvimento de modelos novos, atacando tudo que é atrito evitável. São várias frentes, e eu vou passar por cada uma.

**Uso idiomático das ferramentas.** MIXR, BehaviorTree.CPP e o Groot — a ferramenta de edição e monitoramento de árvores de comportamento — não são bibliotecas genéricas, são frameworks com opiniões fortes sobre como o código deveria se organizar. A aposta aqui é usá-los da forma como foram desenhados para ser usados, em vez de reconstruir por fora aquilo que eles já resolvem por dentro. Isso é o que faz a dinâmica de eventos que eu mencionei realmente funcionar a nosso favor.

**Organização de código.** Até pouco tempo atrás, o que hoje é um projeto só estava espalhado em repositórios diferentes — `asa-models`, `asa-libs`, `asa-models-r`, o próprio MIXR, cada um com seu próprio ciclo, sua própria forma de versionar, sua própria distância dos outros. Hoje isso está tudo em um único repositório integrado. Não é só conveniência: é a diferença entre conseguir enxergar o efeito de uma mudança de ponta a ponta, ou não.

**Uma esteira de CI forte.** Testes automatizados em várias camadas — da regra de domínio mais isolada até o binário rodando de verdade — e diretivas de Makefile que deixam claro, no dia a dia, o que fazer para configurar, compilar, testar e rodar um modelo. Isso é o que transforma "eu acho que funciona" em "eu tenho como provar que funciona", e isso é rastreável, não depende de tribal knowledge de quem já mexeu naquilo antes.

**Um contrato forte para o desenvolvimento de modelos.** Todo modelo, para entrar nesse repositório, carrega consigo a mesma espinha dorsal: documentação de arquitetura em `docs/`, e um `CHANGELOG.md` que registra o que mudou e quando — por data de commit, não por data da mensagem, porque mensagem de commit se engana, data de commit não. Isso é gestão de conhecimento de verdade: o conhecimento não fica só na cabeça de quem escreveu, fica registrado de um jeito que sobrevive à saída da pessoa.

**Ferramentas que assistem o desenvolvimento e o debug de modelos.** Isso inclui um painel de controle e observação em tempo real — o `./app` — que permite ver o que uma simulação está fazendo por dentro enquanto ela roda: qual comportamento está ativo, em qual thread, com qual estado de memória. Inclui também um manual interativo, que navega pela estrutura do framework e pela árvore de decisão de verdade que os modelos usam. E inclui um editor visual para montar cenários, sem precisar decorar sintaxe.

**Documentação bem escrita, com apoio de inteligência artificial.** Isso não é terceirizar a explicação para uma IA e aceitar o que sai — é usar IA como alavanca para produzir e manter atualizada uma documentação que, historicamente, é a primeira coisa a envelhecer mal em qualquer projeto de engenharia.

**A filosofia de poc-first.** Em vez de desenhar a arquitetura ideal no papel antes de escrever qualquer linha, a aposta é começar simples, com uma prova de conceito pequena. O ganho não é só "ver funcionando rápido" — embora isso já valha muito. O ganho real é que uma poc vira **contrato** e vira  **exemplo de como fazer** : quem for escrever o próximo modelo tem um caminho já percorrido para copiar e adaptar, em vez de um documento de intenções.

**Padronização de ambiente.** Extensões utilitárias do VS Code que fazem o dia a dia menos artesanal: realce de sintaxe para os arquivos de cenário — e, mais importante, uma validação de verdade desses arquivos, que roda o mesmo parser que o framework usa em produção, antes mesmo de tentar rodar a simulação. E `clangd` para C++, no lugar do IntelliSense padrão — indexação melhor, navegação melhor, menos fricção para entender um código que não é seu.

E, olhando para frente: essa base é pensada para ser **integrável com o asa-engine** — o conjunto que transforma o binário de simulação num serviço de verdade, com node, manager, handler, e que se conecta ao `asa-py` e à `webstation`. E, num segundo momento, com o ambiente virtual. Eu quero ser preciso aqui: isso é roadmap, não é algo que já está pronto neste repositório hoje. Mas é a razão pela qual a reestruturação foi desenhada do jeito que foi — para que, quando essa integração acontecer, ela encontre uma base organizada, e não um monte de repositórios espalhados para costurar.

---

### III. Onde estamos — o primeiro entregável

Reescrever modelos que já existem é, admitidamente, um desafio grande — e um desafio que já sabíamos, desde o início, que teria de ser encarado. Não é uma surpresa desagradável no meio do caminho. E esse desafio vem com um benefício concreto: reescrever é a oportunidade de recuperar um conhecimento que hoje só existe dentro do próprio código — e código é, por natureza, uma forma difícil de guardar conhecimento, mesmo quando esse código está em uma linguagem tão legível quanto Python.

E esse é, sinceramente, o melhor momento possível para fazer esse trabalho. Temos hoje ferramentas de IA que tornam a tarefa de ler um código antigo, entender sua intenção, e reescrevê-lo com documentação e testes junto, muito mais viável do que era há pouco tempo.

O primeiro entregável dessa reestruturação é exatamente este projeto: um metaprojeto  **open-source** , com todo o esqueleto do que pode vir a ser, no futuro, uma refatoração completa da ASA. Hoje, com um único modelo de produção pronto de ponta a ponta: a aeronave  **A-4 Skyhawk** . E, junto dele, provas de conceito que mostram até onde essa estrutura pode ir — uma decisão tomada por uma rede neural treinada via ONNX, e uma decisão escrita inteiramente em Python, sem recompilar nada.

E aqui tem uma decisão de arquitetura que eu acho que vale a pena destacar, porque ela é o que torna essas duas provas de conceito possíveis sem quebrar nada: a árvore de comportamento é um  **contrato inegociável** . Tudo roda dentro de folhas dessa árvore. O que muda é o que essas folhas podem ser: um nó em C++ tradicional, um script em Python interpretado dentro do próprio frame de simulação, ou uma política inteira, treinada, rodando como uma única folha ONNX. A estrutura de decisão é sempre a mesma — o que varia é como cada folha decide.

E por trás dessa árvore — porque nem tudo aqui é BehaviorTree.CPP — tem uma segunda arquitetura de decisão, essa nativa do próprio MIXR, que vale a pena explicar por um instante, porque é onde a diferença entre "usar o framework direito" e "usar o framework por cima" fica mais visível: o  **UBF** , o *Unified Behavior Framework*. A ideia central dele é separar três responsabilidades que, num código mal estruturado, tendem a se misturar: percepção — um Estado que lê o ator e constrói uma visão do mundo —, decisão — um Comportamento que, a partir desse estado, decide uma ação —, e atuação — uma Ação que sabe executar aquela decisão sobre o ator, sem guardar referência nenhuma para ele. Um Agente amarra essas três pontas: a cada ciclo, ele atualiza o estado, pede uma ação ao comportamento, e executa essa ação.

E aqui está o ponto que eu quero destacar, porque é sutil e é fácil de fazer errado sem perceber que está errado: uma versão anterior deste trabalho já se aproveitava desses conceitos do UBF — estado, comportamento, ação —, mas não fazia o uso correto das classes abstratas que o próprio framework oferece. O agente usado herdava de `Agent`, a classe base, cujo ciclo roda dentro de `updateData()` — a thread de background da simulação, a uns 10 Hz. A diferença que fizemos foi trocar essa herança para `AgentTC`: o mesmo ciclo — a mesma percepção, decisão, atuação —, mas agora rodando dentro de `updateTC()`, ou seja, na fase 3 do frame de tempo crítico, a fase de processamento, junto com o resto da lógica de decisão da simulação, na mesma cadência da física — até 50 Hz em produção. É uma diferença de uma palavra na herança, mas é a diferença entre o comportamento decidir "por fora" da simulação, numa thread separada e mais lenta, e decidir "por dentro" dela, na fase certa do frame.

O principal impacto disso é que a simulação passa a poder usar os vários núcleos da máquina — com garantia de determinismo. Cada aeronave decide na sua própria thread do pool de tempo crítico, em paralelo, e o resultado sai byte-idêntico rodando com uma, duas ou quatro threads. E eu quero ser preciso sobre onde esse ganho aparece: ele é maior justamente em  **simulações isoladas** — uma simulação só, com várias entidades decidindo ao mesmo tempo dentro dela —, e não em cenários de simulação em lote, rodando várias instâncias em paralelo. É paralelismo de *dentro* de uma simulação, não *entre* simulações.

[pausa]

E, da forma como isso está proposto agora, a gente já tem testes automatizados em várias camadas cobrindo isso de ponta a ponta — eu falei da esteira de CI na parte II. O que eu quero deixar claro aqui é a divisão de responsabilidade por trás dela: os testes de infraestrutura, do host, já vêm prontos e valem para qualquer modelo que entrar no repositório. Os testes específicos de UM modelo — a regra de domínio daquela aeronave, daquele comportamento — ficam naturalmente a cargo de quem escreve o modelo, o modelista. E é exatamente aí que a IA brilha: escrever bateria de teste de regra de domínio, com casos de borda, é um trabalho repetitivo — e é exatamente o tipo de trabalho onde um assistente de IA multiplica a velocidade sem abrir mão de cobertura.

Uma peça pequena do dia a dia que ajuda a explorar tudo isso é o realce de sintaxe de verdade que os arquivos `.edl` ganharam no editor — o mesmo vocabulário que o parser real da simulação entende, não uma aproximação. E um dos lugares onde isso ajuda a explorar é justamente trocar a física da aeronave sem tocar em uma linha de C++.

Isso fica registrado como exploração, não como algo em produção ainda, mas eu acho que vale mostrar: testamos trocar o modelo de dinâmica da mesma aeronave — a mesma árvore de comportamento, a mesma rota — só mudando o slot `dynamicsModel:` no `.edl`. De ponta a ponta: do JSBSim de seis graus de liberdade que já usamos em produção, até um modelo linear de quatro graus, até um modelo cinemático de três canais comandados — rumo, altitude e velocidade, sem simular rotação nenhuma. Nenhuma das três trocas exige recompilar nada. E o motivo de eu achar isso importante não é só "dá para trocar" — é que isso dá fôlego para cenários grandes: nem toda aeronave de um cenário precisa da fidelidade completa do JSBSim o tempo inteiro. Um modelo de dinâmica mais barato, para quem está só de pano de fundo, libera orçamento de CPU para escalar o número de entidades.

---

### IV. O convite — comece pelo TOUR.md

Então, qual é o primeiro passo prático, para quem está nesta sala e quer participar disso?

É simples: seguir o `TOUR.md`. É um roteiro guiado, pensado para não exigir conhecimento tribal do repositório — que é exatamente o problema que ele existe para resolver. Ele leva você a subir o ambiente, rodar um cenário de verdade, ver a simulação acontecendo no Tacview, explorar o painel de controle, abrir o manual interativo. E, para quem quiser ir mais fundo, uma segunda trilha: o editor visual de cenários, o Groot, a decisão em Python, o ambiente de treino de aprendizado por reforço.

O convite não é só "leia a documentação". É:  **estresse o metaprojeto** . Rode, quebre, proponha melhorias, reporte erros. Ele foi feito com a filosofia de ser amigável, plug-and-play — mas isso não significa que ele esteja livre de erros. Significa que encontrar esses erros, e reportá-los, já é parte esperada e valiosa do processo. Um sistema assim só amadurece com gente de verdade tentando usá-lo.

E, para quem quiser se capacitar mais formalmente antes ou durante esse processo, existe conteúdo próprio para isso: em `docs/books`, há dois livros escritos com apoio de IA, um sobre o MIXR e outro sobre o BehaviorTree.CPP — pensados como material de capacitação para quem está entrando no framework agora.

---

### V. O que se espera — e como fechamos isso

Para encerrar, eu quero deixar claro qual é a expectativa por trás de tudo isso — não é só "reorganizar pastas".

A expectativa é que cada pessoa que passar por esse processo deixe esse framework com a cara do que gostaria de trabalhar no futuro. Que a gente consiga, com o tempo, chegar a um núcleo o mais próximo possível do imutável — uma base estável o suficiente para que o esforço de cada equipe vá para o que é específico do modelo dela, não para reinventar a fundação de novo a cada projeto.

E, talvez o ganho mais silencioso de tudo isso: gerar um vocabulário e um contexto comuns. Quando todo mundo fala de "slot", de "plugin", de "folha de árvore de comportamento" com o mesmo significado, uma reunião técnica deixa de gastar metade do tempo alinhando terminologia e passa a gastar o tempo discutindo o que realmente importa: o comportamento do modelo.

Simulação aeroespacial vai continuar sendo difícil — isso eu disse no começo, e repito agora, porque é verdade e vai continuar sendo. Mas a forma como escrevemos, testamos, documentamos e conversamos sobre os modelos que simulam esse domínio — essa parte, sim, está em nossas mãos. E é exatamente isso que estamos reconstruindo, um eixo de cada vez, com um modelo, hoje, e o convite aberto para o próximo.

Obrigado.