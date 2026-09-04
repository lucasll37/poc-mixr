#ifndef __events_TacticalAlert_H__
#define __events_TacticalAlert_H__

#include "mixr/base/Object.hpp"

#include <string>

namespace mixr {
namespace events {

//------------------------------------------------------------------------------
// Class: TacticalAlert
//
// Description: Carga util de um alerta tatico -- "achei um intruso aqui". E
//              um mixr::base::Object de verdade (ref-contado, com RTTI do
//              framework), como o Emission do radar nativo ou a mensagem do
//              Datalink: e assim que o MIXR transporta dados em eventos, nao
//              com um struct solto.
//
// Factory name: TacticalAlert
//
// Movida de models/flight/xnative/ para ca (ver events/README.md): um
// payload que precisa ser dynamic_cast de DENTRO DE OUTRO PLUGIN (.so
// carregado por dlopen) so e seguro se a classe morar numa shared library
// de verdade, linkada por AMBOS os lados -- caso contrario cada .so
// enxergaria um type_info diferente para o "mesmo" tipo (mesmo motivo
// documentado no CLAUDE.md para RLBridgeBehavior ter ficado dentro de
// models/flight em vez de virar plugin proprio). Continua com o MESMO nome
// de classe e de fabrica de antes -- so o namespace (xnative -> events) e o
// local mudaram -- para nao mexer em 'provides:' de nenhum cenario.
//
// Mora em events/payloads/, dentro de ./events/ na RAIZ do repositorio --
// nao em shared/xevents/, ao contrario das outras seis shared_library() de
// fronteira de plugin (xboard/xlog/xtrack/xrlbridge/xinfer/xpyembed).
// Excecao deliberada: evento e o eixo central da modelagem deste projeto
// (ver events/README.md), entao ganhou pasta propria em destaque em vez de
// ficar mais um `shared/x<nome>` entre outros. Continua publicado pelo
// MESMO SDK, do MESMO jeito -- so o endereco do FONTE mudou, nao o
// mecanismo (subdir()+shared_library() no meson.build raiz, instalado em
// dist/lib+dist/include via pkgconfig).
//
// events/payloads/<TOKEN>/ e o lugar de TODO payload novo -- uma pasta por
// EVENTO (nomeada IGUAL ao token que carrega, aqui EID_ALERT -- ver
// events/EventTokens.hpp), com um par .hpp/.cpp dentro. O nome da pasta
// bate com o nome do token de proposito: quem le "events::EID_ALERT" acha o
// payload correspondente so pelo nome, sem precisar grepar. Se um evento
// precisar de mais que um par .hpp/.cpp (helpers, sub-tipos), eles entram
// na MESMA pasta, sem afetar os outros eventos.
//
// O REGISTRO de tokens (events/EventTokens.hpp) fica FORA de payloads/, de
// proposito -- e um ledger UNICO (nao um-por-evento), porque o valor dele
// esta em ser o unico lugar onde dois eventos poderiam colidir de numero;
// espalhar essa lista destruiria essa garantia.
//
// Os campos sao deliberadamente CRUS (posicao no plano NED do cenario, em
// metros): quem recebe decide o que fazer, e o alerta nao carrega nenhuma
// ordem. Isso mantem o emissor ignorante sobre o comportamento do receptor.
//------------------------------------------------------------------------------
class TacticalAlert : public base::Object
{
   DECLARE_SUBCLASS(TacticalAlert, base::Object)

public:
   TacticalAlert();

   int getSenderId() const              { return senderId; }
   const std::string& getSenderName() const { return senderName; }
   const std::string& getContactName() const { return contactName; }
   double getNorthM() const             { return northM; }
   double getEastM() const              { return eastM; }
   double getAltitudeM() const          { return altitudeM; }
   double getRangeM() const             { return rangeM; }

   void setSender(const int id, const std::string& name)   { senderId = id; senderName = name; }
   void setContactName(const std::string& name)            { contactName = name; }
   void setPosition(const double n, const double e, const double alt)
   {
      northM = n; eastM = e; altitudeM = alt;
   }
   void setRangeM(const double r)       { rangeM = r; }

private:
   int senderId{};
   std::string senderName;
   std::string contactName;
   double northM{};
   double eastM{};
   double altitudeM{};
   double rangeM{};
};

} // namespace events
} // namespace mixr

#endif
