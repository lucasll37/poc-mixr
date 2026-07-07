#pragma once

#include "mixr/models/system/System.hpp"

#include <string>

namespace events { class RadarContactMessage; }

namespace events {

//------------------------------------------------------------------------------
// Class: AlertReceiver
//
// System generico que reage a CONTACT_EVENT via BEGIN_EVENT_HANDLER (o
// "destino apropriado" do padrao de eventos -- so processa a mensagem
// quando ela realmente chega nele, endereçada por nome, nao importa se
// veio de um componente irmao (mesmo player) ou de um RadarContactRelay
// de OUTRO player). Usado tanto no proprio "hunter" (alerta local) quanto
// no "controller" (alerta remoto), mesma classe, duas instancias.
//
// main.cpp consome o texto do ultimo alerta (consumePendingAlert()) so
// pra log/Tacview -- a decisao de "recebi, processei" ja aconteceu aqui,
// dentro do event handler nativo, nao no main.cpp.
//------------------------------------------------------------------------------
class AlertReceiver final : public mixr::models::System
{
   DECLARE_SUBCLASS(AlertReceiver, mixr::models::System)

public:
   AlertReceiver();

   std::string consumePendingAlert();

   bool event(const int event, mixr::base::Object* const obj = nullptr) final;

private:
   bool onContactEvent(RadarContactMessage* const msg);

   std::string pendingAlert;
   int numAlertsReceived{};
};

} // namespace events
