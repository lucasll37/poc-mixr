#pragma once

#include <string>

namespace app {

//------------------------------------------------------------------------------
// Preparacao do banco de elevacao antes do parse do cenario.
//
// Uma unica questao: garantir que o arquivo que o EDL vai nomear existe em
// disco e tem o tamanho EXATO que o SrtmHgtFile aceita. Sao duas armadilhas
// do framework, as duas silenciosas:
//
//   1) SrtmHgtFile nao le .gz -- ele abre um std::ifstream cru. O que fica
//      versionado aqui e o .gz (12 MB); o .hgt (25 MB) e artefato gerado.
//
//   2) determineSrtmInfo() decide a resolucao por um switch sobre o tamanho
//      do arquivo EM BYTES (2884802 = SRTM3, 25934402 = SRTM1) e nao tem
//      nenhuma tolerancia. Um arquivo truncado no download, ou descomprimido
//      pela metade, cai no 'default' e a unica mensagem que aparece e
//      "ERROR in determining SRTM type" -- sem dizer qual arquivo nem por
//      que. Conferir o tamanho aqui e o antidoto: o erro passa a apontar
//      exatamente o que esta errado, antes de a simulacao comecar.
//
// Encerra o processo se nao conseguir produzir um arquivo valido. Seguir sem
// terreno seria pior do que parar: o Player::updateElevation() nativo IGNORA
// o retorno de getElevation(), entao um banco ausente vira elevacao zero com
// o flag de validade LIGADO -- e toda a decisao passa a raciocinar sobre um
// mundo ao nivel do mar sem nenhum aviso.
//------------------------------------------------------------------------------
void ensureTerrainData(const std::string& dir, const std::string& baseName);

} // namespace app
