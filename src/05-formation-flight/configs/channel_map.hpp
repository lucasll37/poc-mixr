// -----------------------------------------------------------------------------
// Canais discretos do KeyboardDevice -- usado tanto pelo EDL (linkage.epp,
// via #include + pre-processador C, mesmo padrao de examples/mainSim2/
// configs/linkage/channel_map.hpp) quanto implicitamente pelo C++
// (include/linkage/KeyboardDevice.hpp declara o enum keyboard::Channel com
// os MESMOS valores -- mantenha os dois em sincronia se adicionar canais).
// -----------------------------------------------------------------------------
#ifndef __poc05_channel_map_H__
#define __poc05_channel_map_H__

#define ALT_UP      0
#define ALT_DOWN    1
#define HDG_LEFT    2
#define HDG_RIGHT   3
#define SPD_DOWN    4
#define SPD_UP      5
#define FORM_1      6
#define FORM_2      7
#define FORM_3      8
#define FORM_4      9
#define RTB         10

#endif
