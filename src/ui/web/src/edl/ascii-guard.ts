// Um unico caractere nao-ASCII num comentario `//` ja quebrou o parser real
// do MIXR (bison/flex) com "syntax error" opaco, apontando a linha certa mas
// sem dizer o motivo -- documentado no CLAUDE.md (secao "src/poc/bandit-dis",
// armadilha 5). O editor NUNCA pode gerar esse arquivo: toda string escrita
// pelo serializer passa por aqui antes de sair.

export class NonAsciiError extends Error {
  constructor(
    public readonly context: string,
    public readonly value: string,
  ) {
    super(`valor nao-ASCII em ${context}: "${value}" -- o parser EDL real quebra com isso`);
    this.name = 'NonAsciiError';
  }
}

const ASCII_ONLY = /^[\x00-\x7F]*$/;

export function assertAscii(value: string, context: string): void {
  if (!ASCII_ONLY.test(value)) {
    throw new NonAsciiError(context, value);
  }
}

export function isAscii(value: string): boolean {
  return ASCII_ONLY.test(value);
}
