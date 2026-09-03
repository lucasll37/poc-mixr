// A producao 'arglist' do edl_parser real e recursiva a ESQUERDA: forms
// irmaos sao construidos na ordem do TEXTO, e o parser chama
// factory+setSlotByName+isValid() no fecha-parenteses de cada um. A carga de
// um PluginModule acontece no isValid() do PluginLoader -- portanto
// qualquer uso de uma classe que ele fornece (BtBehavior, FlightAgentTC...)
// tem que aparecer DEPOIS dele no texto gerado, mesmo que a arvore de edicao
// do usuario tenha outra ordem (ex: o usuario editou o PluginLoader por
// ultimo). Ver CLAUDE.md / src/poc/single-thread/configs/scenario.epp.in:51-61.
//
// A regra real do repo e simples o bastante pra nao precisar de um
// topological sort generico sobre um grafo de dependencias: so
// 'PluginLoader' precisa vir primeiro entre os irmaos de uma mesma
// namedList (hoje, sempre components: da Station). Isso resolve o unico
// caso de ordem que os .epp reais do repositorio de fato exercitam.
import type { NamedEntry } from '../model/SlotValue';

export function reorderPluginLoaderFirst(entries: NamedEntry[]): NamedEntry[] {
  const isPluginLoader = (e: NamedEntry) => e.value.kind === 'form' && e.value.node.factoryName === 'PluginLoader';
  const loaders = entries.filter(isPluginLoader);
  const rest = entries.filter((e) => !isPluginLoader(e));
  return [...loaders, ...rest];
}
