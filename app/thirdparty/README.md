# `app/thirdparty/` — código de terceiro vendorizado, fora do Conan

Um único arquivo hoje: **`stb_image.h`** (v2.30, [nothings/stb](https://github.com/nothings/stb),
domínio público / MIT à escolha — cabeçalho de licença completo dentro do próprio arquivo).
Decodifica PNG/JPG em memória, sem dependência de sistema nenhuma além de um compilador C++.

**Por que vendorizado aqui, e não uma receita Conan em `deps/`** — o mesmo raciocínio já usado
para `libs/xrandom` (header-only, sem estado compartilhado entre core e plugin): é um único
header, sem build próprio, sem versão a reconciliar contra um pacote binário. Puxar o Conan pra
isto seria peso desproporcional ao que a biblioteca faz.

**Único consumidor: `app/src/app/BannerImage.cpp`** — decodifica `app/assets/banner.{png,jpg}`
(a imagem de fundo da tela de seleção de cenário, `./app -folder <pasta>` sem `-scenario`) em
tempo de execução, nunca embutida no binário em tempo de compilação — trocar o arquivo em disco
e rodar `./app` de novo já mostra a imagem nova, sem rebuild.

**Não editar o conteúdo do arquivo** — é cópia vendorizada, byte a byte, de
`https://raw.githubusercontent.com/nothings/stb/master/stb_image.h`. Para atualizar a versão,
baixar de novo e substituir o arquivo inteiro.

`BannerImage.cpp` é a ÚNICA translation unit que define `STB_IMAGE_IMPLEMENTATION` antes de
incluir o header (a convenção do próprio stb: incluído sem a macro em qualquer outro lugar, viraria
apenas a declaração) — mas hoje nenhum outro arquivo o inclui.
