#ifndef __xair_TacticalAlert_H__
#define __xair_TacticalAlert_H__

#include "mixr/base/Object.hpp"

#include <string>

namespace mixr {
namespace xair {

//------------------------------------------------------------------------------
// Class: TacticalAlert
//
// Description: Carga util do evento TACTICAL_ALERT_EVENT -- "achei um
//              intruso aqui". E um mixr::base::Object de verdade
//              (ref-contado, com RTTI do framework), como o Emission do
//              radar nativo ou a mensagem do Datalink: e assim que o MIXR
//              transporta dados em eventos, nao com um struct solto.
//
// Factory name: TacticalAlert
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

} // namespace xair
} // namespace mixr

#endif
