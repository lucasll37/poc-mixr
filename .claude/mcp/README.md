# MCP — nenhum servidor configurado hoje

Nenhum `.mcp.json` foi criado na reestruturação da camada de extensão (sessão de 2026-09-06): o
levantamento da Fase 1 não achou evidência de serviço externo em ponto nenhum do grafo de
build/teste deste repositório — é tudo Meson/Ninja/Conan/venvs Python locais, sem banco de dados,
issue tracker, API cloud ou serviço de terceiro que um agente precisasse consultar em runtime.

## Quando adicionar um servidor MCP

Crie `.mcp.json` na **raiz** do repositório (não aqui dentro — este arquivo é só a nota; o
`.mcp.json` de verdade não pode morar em `.claude/`) quando o projeto passar a depender de um
serviço externo que um agente precise acessar — por exemplo: um rastreador de issues onde bugs
deste repositório passem a ser abertos (hoje é só o histórico de commits + CLAUDE.md), um painel
de telemetria além do Tacview local, ou uma API de CI (hoje não existe nenhuma — o Makefile é a
única automação, ver CLAUDE.md "Estado atual").

Use `claude mcp add <nome> <comando>` (ou edite `.mcp.json` direto) e documente o motivo no
próprio commit que o introduz. Apague este arquivo nesse momento — uma nota de "nada aqui ainda"
ao lado de um `.mcp.json` com conteúdo real só fica desatualizada.
