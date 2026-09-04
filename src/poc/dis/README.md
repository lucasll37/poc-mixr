# poc/dis — as três que só fazem sentido juntas

Não é uma poc; é o **grupo** delas. O que as junta não é a pilha —
`single-thread` e `multi-thread` são o mesmo modelo e o `bandit` não tem modelo
nenhum — e sim o fato de só fazerem sentido **juntas**, em processos separados
trocando **DIS nativo do MIXR**: o intruso mora no `bandit` e chega nas outras
duas apenas pela rede (`networks:`), enquanto `falcon1..4` fazem o caminho de
volta. Rodar qualquer uma sozinha é meia demonstração.

| pasta | o que isola | Tacview | DIS (emite de) |
|---|---|---|---|
| `bandit/` | uma aeronave só, sem decisão: joystick físico ou `Autopilot` de fallback | 1235 | 3001 |
| `single-thread/` | decisão no `( SimAgent )` nativo da `Station`, em `updateData()` | 1234 | 3002 |
| `multi-thread/` | a mesma pilha decidindo na **fase 3** do frame de tempo crítico | 1234 | 3003 |

Todos escutam na porta **3000**; cada um ignora a própria porta de origem para
não ouvir o próprio eco.

```bash
# em três terminais, a partir da raiz do repositório
make run-bandit
make run-single-thread     # ou run-multi-thread
```

**Não há executável por poc.** Cada pasta aqui é só `configs/` + `data/` +
`README.md`; quem executa é o `./app`, o runner único
(`app -scenario <chave>`). Ver [src/poc/meson.build](../meson.build) para o
porquê.
