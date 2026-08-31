#ifndef __xmsg_MsgFileSink_H__
#define __xmsg_MsgFileSink_H__

#include "xmsg/MsgSink.hpp"

#include <fstream>
#include <string>

namespace mixr {
namespace base { class String; class Number; }

namespace xmsg {

//------------------------------------------------------------------------------
// Class: MsgFileSink
//
// Description: Grava as mensagens em NDJSON -- um objeto JSON por linha.
//
// Factory name: MsgFileSink
//
// Slots:
//    fileName    <String>   ! caminho do arquivo (obrigatorio)
//    flushEvery  <Time>     ! intervalo entre flushes (default: 2 s)
//    messages    <PairStream> (herdado)
//
// POR QUE NAO REUSA O recorder::PrintHandler, ao contrario do shared/xlog.
// PrintHandler::printToOutput() termina em std::endl, que e um flush POR
// LINHA. O xlog escreve poucas linhas e nao se importa; aqui a taxa e de
// dezenas a centenas de linhas por segundo, e um flush por linha vira
// tempestade de syscall no mesmo laco que tambem drena o gravador do Tacview.
// Por isso o ofstream e proprio e o flush e periodico.
//
// O DIRETORIO TEM DE EXISTIR: openFile() do framework nao cria diretorio, e o
// TacviewOutput ja falha assim quando falta data/recordings/. Aqui a falha e
// LOG(ERROR) com o caminho, nunca silencio.
//------------------------------------------------------------------------------
class MsgFileSink : public MsgSink
{
   DECLARE_SUBCLASS(MsgFileSink, MsgSink)

public:
   MsgFileSink();

   bool open() override;
   void write(const char* line, std::size_t len) override;
   void tick(double dt) override;
   void close() override;

private:
   bool setSlotFileName(const base::String* const);
   bool setSlotFlushEvery(const base::Number* const);

   std::string fileName_;
   double flushEvery_{2.0};
   double sinceFlush_{};
   std::ofstream out_;
   bool opened_{};
};

} // namespace xmsg
} // namespace mixr

#endif
