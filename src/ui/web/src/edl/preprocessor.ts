// Pre-processamento textual de "@include:arquivo@", especifico do `app/`
// (ver app/src/app/ScenarioTemplate.cpp) -- NAO faz parte da gramatica EDL
// real, e resolvido como substituicao de texto ANTES do parser tokenizar.
//
// "@NUM_TC_THREADS@" e qualquer outro "@TOKEN@" (ex: "@SCENARIO_ID@",
// "@MODEL_MAP@") nao precisam de tratamento aqui -- '@' ja faz parte da
// classe de caracteres de identifier do lexer EDL real (e do nosso), entao
// "@NUM_TC_THREADS@" sozinho ja tokeniza como um IDENT normal e vira
// { kind: 'identifier', value: '@NUM_TC_THREADS@' } no parser -- so precisa
// de tratamento especial a UI (mostrar "placeholder de template, nao
// resolvido"), nunca no lexer/parser.
//
// So "@include:NOME@" precisa de pre-processamento de verdade, porque o
// NOME do arquivo (ex: "tacview_recorder.epp.frag") contem '.', que NAO faz
// parte da classe de caracteres de identifier -- sem resolver isso antes, o
// lexer rejeitaria com "caractere inesperado: .".

const INCLUDE_RE = /@include:([^@]+)@/g;

export interface PreprocessResult {
  text: string;
  /** nomes de fragmento encontrados no arquivo original, resolvidos ou nao */
  includesFound: string[];
  /** nomes que NAO foram resolvidos (fragmentContents nao tinha essa chave) */
  includesUnresolved: string[];
}

/**
 * Substitui cada "@include:NOME@" pelo conteudo em `fragmentContents[NOME]`,
 * se presente; caso contrario remove a diretiva (deixa o arquivo parseavel,
 * so perde aquele trecho -- ver README.md, limitacao conhecida).
 */
export function preprocessIncludes(source: string, fragmentContents: Record<string, string> = {}): PreprocessResult {
  const includesFound: string[] = [];
  const includesUnresolved: string[] = [];

  const text = source.replace(INCLUDE_RE, (_match, name: string) => {
    includesFound.push(name);
    const content = fragmentContents[name];
    if (content === undefined) {
      includesUnresolved.push(name);
      return '';
    }
    return content;
  });

  return { text, includesFound, includesUnresolved };
}

/** Varre o texto e devolve so os nomes de fragmento referenciados (sem substituir nada) --
 *  usado pelo chamador (App.tsx) pra saber quais buscar via /api/scenarios/fragment antes de parsear. */
export function findIncludeNames(source: string): string[] {
  const names = new Set<string>();
  for (const m of source.matchAll(INCLUDE_RE)) names.add(m[1]!);
  return [...names];
}
