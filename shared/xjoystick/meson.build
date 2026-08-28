# Controle do ownship por joystick fisico, compartilhado entre os
# subprojetos -- mesmo padrao 'shared/x<nome>' de shared/xtacview/xclock
# (factory propria + classes no namespace mixr::x<nome>).
#
# So fala com mixr::linkage (UsbJoystick/IoData/AnalogInput, ja parte da
# mixr_dep via 'Requires: mixr-linkage' do mixr.pc) e com a Station/
# WorldModel/AirVehicle nativos -- nada de dependencia nova (SDL/evdev): o
# UsbJoystick le /dev/input/jsX com ioctl cru.

xjoystick_inc_dir = include_directories('..')

xjoystick_lib = static_library('xjoystick',
    files(
        'JoystickIoHandler.cpp',
        'factory.cpp',
    ),
    include_directories : xjoystick_inc_dir,
    dependencies        : [mixr_dep],
)

xjoystick_dep = declare_dependency(
    link_with           : xjoystick_lib,
    include_directories : xjoystick_inc_dir,
    dependencies        : [mixr_dep],
)
