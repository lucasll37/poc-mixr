// Tokenizador EDL, espelhando as regras relevantes de
// contexts/src/mixr/src/base/edl_parser/edl_scanner.l (quando disponivel --
// e git-ignored, entao isto tambem se apoia nos exemplos reais do repo e nas
// regras ja documentadas no CLAUDE.md).
//
// Nao pretende ser um reimplementacao completa e generica da gramatica EDL
// (hex/octal, continuacao de linha, marcadores de preprocessador GCC) --
// cobre exatamente o subconjunto que os .epp/.epp.in REAIS deste repo usam.
// Um arquivo fora desse subconjunto deve falhar com um erro claro, nao
// silenciosamente produzir uma AST errada.

export type TokenKind =
  | 'lparen' | 'rparen' | 'lbrace' | 'rbrace' | 'lbracket' | 'rbracket'
  | 'slotId' | 'ident' | 'string' | 'number' | 'bool' | 'eof';

export interface Token {
  kind: TokenKind;
  line: number;
  col: number;
  /** presente para slotId/ident/string */
  text?: string;
  /** presente para number */
  num?: number;
  /** presente para bool */
  bool?: boolean;
}

export class EdlLexError extends Error {
  constructor(message: string, public readonly line: number, public readonly col: number) {
    super(`${message} (linha ${line}, coluna ${col})`);
    this.name = 'EdlLexError';
  }
}

// L = [a-zA-Z0-9~!@#$%^&*\-_+=<>?/] -- edl_scanner.l:48
const IDENT_CHAR = /[a-zA-Z0-9~!@#$%^&*\-_+=<>?/]/;
const NUMBER_RE = /^[+-]?\d+(\.\d+)?([eE][+-]?\d+)?/;

export function tokenize(source: string): Token[] {
  const tokens: Token[] = [];
  let i = 0;
  let line = 1;
  let col = 1;

  function advance(n = 1) {
    for (let k = 0; k < n; k++) {
      if (source[i] === '\n') {
        line += 1;
        col = 1;
      } else {
        col += 1;
      }
      i += 1;
    }
  }

  while (i < source.length) {
    const ch = source[i]!;

    // espacos/separadores -- espaco, tab, virgula, \v, \f, \n (edl_scanner.l:105)
    if (ch === ' ' || ch === '\t' || ch === ',' || ch === '\v' || ch === '\f' || ch === '\n' || ch === '\r') {
      advance();
      continue;
    }

    // comentario // ate fim de linha -- nao existe /* */ na gramatica real
    if (ch === '/' && source[i + 1] === '/') {
      while (i < source.length && source[i] !== '\n') advance();
      continue;
    }

    const startLine = line;
    const startCol = col;

    if (ch === '(') { tokens.push({ kind: 'lparen', line: startLine, col: startCol }); advance(); continue; }
    if (ch === ')') { tokens.push({ kind: 'rparen', line: startLine, col: startCol }); advance(); continue; }
    if (ch === '{') { tokens.push({ kind: 'lbrace', line: startLine, col: startCol }); advance(); continue; }
    if (ch === '}') { tokens.push({ kind: 'rbrace', line: startLine, col: startCol }); advance(); continue; }
    if (ch === '[') { tokens.push({ kind: 'lbracket', line: startLine, col: startCol }); advance(); continue; }
    if (ch === ']') { tokens.push({ kind: 'rbracket', line: startLine, col: startCol }); advance(); continue; }

    if (ch === '"') {
      let value = '';
      advance();
      while (i < source.length && source[i] !== '"') {
        if (source[i] === '\\' && i + 1 < source.length) {
          value += source[i + 1];
          advance(2);
        } else {
          value += source[i];
          advance();
        }
      }
      if (source[i] !== '"') throw new EdlLexError('string nao fechada', startLine, startCol);
      advance();
      tokens.push({ kind: 'string', line: startLine, col: startCol, text: value });
      continue;
    }

    // numero -- tentado ANTES de identifier pra "-5.0"/"1750.0" nao virarem
    // identifier fragmentado (digitos tambem fazem parte do conjunto L).
    // Excecao: uma chave numerica pura como "1:" (ex: stores: { 1: (...) })
    // e um SLOT_ID valido na gramatica real (L inclui digito) -- se o
    // caractere logo apos o numero for ':', cai pro caminho de
    // identifier/slotId em vez de virar NUMBER solto seguido de ':' orfao.
    const rest = source.slice(i);
    const numMatch = NUMBER_RE.exec(rest);
    if (numMatch && numMatch[0].length > 0) {
      const matched = numMatch[0];
      const nextChar = rest[matched.length];
      // so aceita como NUMBER se o que vem depois nao continua um identifier
      // nem abre um slot (evita interpretar "1:" como NUMBER 1 + ':' orfao)
      if (!nextChar || (!IDENT_CHAR.test(nextChar) && nextChar !== ':')) {
        tokens.push({ kind: 'number', line: startLine, col: startCol, num: Number(matched) });
        advance(matched.length);
        continue;
      }
    }

    if (IDENT_CHAR.test(ch)) {
      let value = '';
      while (i < source.length && IDENT_CHAR.test(source[i]!)) {
        value += source[i];
        advance();
      }
      if (source[i] === ':') {
        advance();
        tokens.push({ kind: 'slotId', line: startLine, col: startCol, text: value });
        continue;
      }
      const lower = value.toLowerCase();
      if (lower === 'true' || lower === 'false') {
        tokens.push({ kind: 'bool', line: startLine, col: startCol, bool: lower === 'true' });
        continue;
      }
      tokens.push({ kind: 'ident', line: startLine, col: startCol, text: value });
      continue;
    }

    throw new EdlLexError(`caractere inesperado: "${ch}"`, startLine, startCol);
  }

  tokens.push({ kind: 'eof', line, col });
  return tokens;
}
