#ifndef __xtacview_ExposedDataRecorder_H__
#define __xtacview_ExposedDataRecorder_H__

#include "mixr/recorder/DataRecorder.hpp"

namespace mixr {
namespace xtacview {

//------------------------------------------------------------------------------
// Class: ExposedDataRecorder
// Description: mixr::recorder::DataRecorder identico -- so republica
//              getOutputHandler() como PUBLICO (no framework nativo e
//              'protected').
//
// Factory name: ExposedDataRecorder
//
// POR QUE EXISTE -- app/StationBuilder::tacviewOutputOf() precisa achar o
// TacviewOutput dentro de dataRecorder->outputHandler->components a partir
// do main.cpp, fora da cadeia do recorder (ver TacviewOutput::updateRadarScan()).
// O getter nativo existe mas e protected: nao da pra alcancar de fora sem
// isso. Mesmo raciocinio do xjoystick::JoystickIoHandler -- subclassear
// para expor o que o framework nao da pronto, sem reimplementar nada.
//
// Trocar 'dataRecorder: ( DataRecorder ... )' por '( ExposedDataRecorder
// ... )' no .epp continua rodando IDENTICO -- e o mesmo objeto nativo, so
// mais acessivel; nenhum slot novo, nenhum comportamento novo.
//------------------------------------------------------------------------------
class ExposedDataRecorder : public recorder::DataRecorder
{
   DECLARE_SUBCLASS(ExposedDataRecorder, recorder::DataRecorder)

public:
   ExposedDataRecorder();

   using recorder::DataRecorder::getOutputHandler;
};

}
}

#endif
