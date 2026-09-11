# `src/ui/presets/` — convenção pros presets próprios do editor gráfico

Destino sugerido pra quem salva um preset pela seção **"Meus presets"** de
`src/ui/edl-builder.html` (`PresetsSection`, em `src/ui/edl_builder.jsx`).
Em Chrome/Edge (File System Access API), o botão "Escolher pasta…" da seção
pede permissão UMA vez por sessão pra escrever DIRETO nesta pasta (ou
qualquer outra que você apontar) — depois disso, "Salvar como preset" grava
o arquivo sem download nenhum. Em navegadores sem essa API (Firefox) — ou
antes de escolher uma pasta — "Salvar como preset" cai no download de
sempre; mover o arquivo baixado pra cá continua sendo passo manual, opcional.

Não confundir com o botão **"Carregar preset"** da barra de ferramentas: esse
carrega o único cenário de exemplo embutido no próprio build
(`built-in_mixr_1`, ver `src/ui/README.md`). Os arquivos aqui são os SEUS —
tantos quantos você quiser, salvos a partir do cenário que você montou na
ferramenta.

## Convenção

- Nome de arquivo: **`preset_<nome>.edl`**, ou **`preset_<nome>.edl.in`** se o
  cenário ainda tinha placeholder de template (`@TOKEN@`) pendente no momento
  de salvar — mesma regra de extensão que "Exportar .edl" usa (ver o
  comentário de `edlExtension()` em `edl_builder.jsx`). `<nome>` sai do campo
  "nome do preset" (ou do nome do cenário, se aquele campo ficar em branco),
  passado por `slugify()` (minúsculo, só `[a-z0-9-]`).
- Conteúdo: um `.edl`/`.edl.in` de verdade — o mesmo texto que "Exportar .edl"
  produziria a partir da árvore atual. Reabra com "Carregar .edl" a qualquer
  momento, de qualquer pasta — a ferramenta não exige que o arquivo esteja
  aqui especificamente.

## Não são versionados

`.gitignore` ignora `src/ui/presets/*.edl` e `src/ui/presets/*.edl.in` — cada
preset é trabalho local de quem está usando o editor, não fonte deste
repositório. Só este `README.md` fica rastreado.
