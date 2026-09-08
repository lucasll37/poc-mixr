# `src/node` — runner headless e independente de um cenário

Um binário **super enxuto**: roda **uma simulação passada por argumento**, em tempo real, sem
TUI, sem status line — na tela só aparecem as linhas de log (`LOG(...)` de `libs/xlog`, já
ligado no console por padrão). É um peer de `./app` (a TUI de controle/monitoramento) e de
`src/rl` (o wrapper Gymnasium), não uma variação de nenhum dos dois.

## Uso

```bash
./dist/bin/node <arquivo.edl|.edl.in>
```

Como todo binário deste repositório, precisa ser executado a partir da **raiz** do repositório
(lê `configs/`/`data/`/`shared/` por caminho relativo). Aceita qualquer cenário `.edl`/`.edl.in`
de `src/poc/**` — não há `-folder`/`-scenario`/`-threads`/`-deterministic`: é a leitura mais
direta de "roda uma simulação passada por argumento", mesma convenção já usada por
`dist/bin/edlcheck <arquivo>`. `Ctrl+C` (`SIGINT`) ou `SIGTERM` encerram de forma limpa.