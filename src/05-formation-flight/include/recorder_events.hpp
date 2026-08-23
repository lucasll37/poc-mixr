#pragma once

// IDs de evento customizados desta PoC, dentro da faixa reservada pelo
// proprio framework para uso da aplicacao (ver
// mixr/include/mixr/simulation/dataRecorderTokens.hpp:
// REID_FIRST_USER_EVENT..REID_LAST_USER_EVENT, 1000..9999). Gravados via
// AbstractDataRecorder::recordData() (ver main.cpp).
#define REID_FORMATION_CHANGED 1000
#define REID_RTB_ENGAGED       1001
