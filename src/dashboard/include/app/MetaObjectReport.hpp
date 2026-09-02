#pragma once

//------------------------------------------------------------------------------
// Contagem de instancias por classe -- deteccao de vazamento SEM ferramenta
// externa, usando o metadado que o proprio MIXR mantem.
//
// Toda classe que usa DECLARE_SUBCLASS/IMPLEMENT_SUBCLASS carrega um
// base::MetaObject estatico com tres contadores publicos, alimentados pelas
// macros STANDARD_CONSTRUCTOR() (++count, atualiza mc, tc++) e
// STANDARD_DESTRUCTOR() (count--):
//
//    count   instancias VIVAS agora
//    mc      pico de instancias simultaneas
//    tc      total ja criado desde o inicio do processo
//
// O proprio Object.hpp diz para que isso existe: "to spot potential memory
// leaks". Nao e instrumentacao nossa -- e um recurso do framework que estava
// sem uso aqui.
//
// COMO SE LE UM VAZAMENTO. Um retrato sozinho nao prova nada: 'count' alto
// pode ser retencao legitima. O que prova e COMPARAR duas execucoes de
// duracoes diferentes -- se 'tc' cresce com os frames mas 'count' fica no
// mesmo lugar, os objetos estao sendo criados E destruidos. Se 'count'
// cresce junto com 'tc', vazou. E o que tests/memory/run_leak_test.py faz.
//
// O prefixo 'meta=' das linhas nao e decorativo: os checks de determinismo
// filtram '^frame=', entao este relatorio passa ao largo deles sem que nada
// precise mudar la.
//
// ARMADILHA: os contadores NAO sao atomicos (macros.hpp:249 faz
// '++metaObject.count' em int cru). Com os agentes decidindo em paralelo no
// pool de tempo critico os incrementos correm entre si e o numero perde
// exatidao -- por isso o teste de vazamento roda com '-threads 1'.
//------------------------------------------------------------------------------

namespace app {

// Imprime uma linha 'meta=' por classe vigiada, em stdout.
void printMetaObjectReport();

} // namespace app
