#ifndef __xair_event_tokens_H__
#define __xair_event_tokens_H__

namespace mixr {
namespace xair {

// Eventos proprios da aplicacao comecam em USER_EVENTS (2000) --
// <conan>/include/mixr/base/eventTokens.hpp. Abaixo disso a faixa e do
// framework (e ate MAX_KEY_EVENT=999 sao eventos de tecla, os unicos que
// sobem sozinhos para o container).
const int TACTICAL_ALERT_EVENT{2001};

} // namespace xair
} // namespace mixr

#endif
