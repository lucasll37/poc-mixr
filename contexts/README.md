# `contexts/` — material de consulta sobre MIXR e BehaviorTree.CPP

Três documentos técnicos de referência (não são material de RAG só — RAG, *Retrieval Augmented
Generation*, é a técnica de buscar e injetar só o trecho relevante de um acervo grande num prompt;
estes documentos servem também como leitura contínua para quem está aprendendo os dois frameworks
do zero), mais o fonte vendorizado que os sustenta:

| documento | pergunta que responde | leia |
|---|---|---|
| [`MIXR-CONTEXT.md`](MIXR-CONTEXT.md) | como o MIXR funciona **por dentro** — classes, macros, ciclo de vida, EDL, recorder | primeiro |
| [`MIXR-PATTERN-CONTEXT.md`](MIXR-PATTERN-CONTEXT.md) | como se **escreve** uma aplicação MIXR — os padrões que os exemplos oficiais repetem (§0 lista o que do fork empacotado aqui **não** existe) | depois, é o documento-irmão do anterior |
| [`BTCPP-CONTEXT.md`](BTCPP-CONTEXT.md) | como funciona a **BehaviorTree.CPP v3.5.6** — nada nele vale para a v4 | quando for mexer em árvore de comportamento |

`MIXR-CONTEXT.md` descreve o mecanismo; `MIXR-PATTERN-CONTEXT.md` descreve o emprego — quando um
padrão deste segundo depende de um mecanismo do primeiro, ele aponta a seção correspondente em vez
de repeti-la (ver o próprio topo de `MIXR-PATTERN-CONTEXT.md`, "COMO USAR ESTE DOCUMENTO"). Os
dois são sobre o MIXR; `BTCPP-CONTEXT.md` é o único sobre o BehaviorTree.CPP.

**`src/`** é a árvore de fontes completa por trás dos três documentos acima: `src/mixr/` (fonte do
fork v1.0.5, o mesmo que gera o pacote Conan consumido aqui) e `src/BehaviorTree.CPP/` (fonte da
3.5.6, com `examples/`/`sample_nodes/`/`tests/`). É onde confirmar qualquer coisa que a destilação
nos três `.md` não cobre ou que pareça contraditória — ler o `.cpp` do framework, não adivinhar
pelo header. Se a pasta não existir na sua cópia do repositório, os headers instalados pelo Conan
(`~/.conan2/p/b/mixr*/p/include/mixr/...`, `<prefix>/include/behaviortree_cpp_v3/`) são o
substituto. Em caso de divergência entre `src/` e o pacote Conan, quem vale é o pacote — é ele que
está de fato linkado.

Ver `CLAUDE.md`, seção "Onde consultar o framework", para mais detalhe.
