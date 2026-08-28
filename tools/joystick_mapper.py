#!/usr/bin/env python3
"""
Mapeador de eixos/botoes do joystick -- ferramenta TEMPORARIA, fora do build.

Por que existe: para configurar o 'channel:' de cada ( AnalogInput ) no
scenario.epp.in (ver shared/xjoystick/JoystickIoHandler.hpp e a secao
'shared/xjoystick' do CLAUDE.md) e preciso saber, no SEU joystick fisico,
qual numero de canal corresponde a qual eixo/botao fisico -- e esse mapeamento
muda de aparelho para aparelho.

Le o MESMO protocolo cru que o mixr::linkage::UsbJoystick (Linux) usa --
'/dev/input/jsX' via <linux/joystick.h> (struct js_event de 8 bytes, sem
biblioteca nenhuma) -- entao o numero de canal que este script mostra e
EXATAMENTE o numero que vai no 'channel:' do EDL, sem tradução nenhuma.
Ver contexts/src/mixr/src/linkage/platform/UsbJoystick_linux.cpp, que este
script espelha de proposito.

Uso:
    python3 tools/joystick_mapper.py                 # /dev/js0 ou /dev/input/js0
    python3 tools/joystick_mapper.py --index 1        # segundo joystick
    python3 tools/joystick_mapper.py --path /dev/input/js2

Mova UM eixo de cada vez, do batente a batente, e observe qual linha "AI"
se move -- esse numero e o 'channel:' do eixo. Aperte cada botao para achar
o 'channel:' dos DIs. Ctrl+C sai.

WSL2: o dispositivo so aparece em /dev/input/jsX depois de anexado com
usbipd-win (`usbipd attach --wsl --busid <id>` no host Windows) -- ver a
secao 'shared/xjoystick' do CLAUDE.md.
"""

import argparse
import ctypes
import fcntl
import os
import struct
import sys
import time

# ------------------------------------------------------------------------
# Protocolo /dev/input/jsX (linux/joystick.h) -- reimplementado aqui porque
# o modulo nao existe no Python stdlib.
# ------------------------------------------------------------------------

EVENT_STRUCT = struct.Struct("=IhBB")   # __u32 time; __s16 value; __u8 type; __u8 number;
EVENT_SIZE = EVENT_STRUCT.size          # 8 bytes

JS_EVENT_BUTTON = 0x01
JS_EVENT_AXIS = 0x02
JS_EVENT_INIT = 0x80                    # ORed no replay do estado inicial

# Codificacao _IOR('j', nr, size) do <asm-generic/ioctl.h> -- reimplementada
# aqui para nao depender de nenhum binding externo.
_IOC_READ = 2
_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = 8
_IOC_SIZESHIFT = 16
_IOC_DIRSHIFT = 30


def _ior(type_char: str, nr: int, size: int) -> int:
    return (
        (_IOC_READ << _IOC_DIRSHIFT)
        | (ord(type_char) << _IOC_TYPESHIFT)
        | (nr << _IOC_NRSHIFT)
        | (size << _IOC_SIZESHIFT)
    )


JSIOCGVERSION = _ior('j', 0x01, 4)   # __u32
JSIOCGAXES = _ior('j', 0x11, 1)      # __u8
JSIOCGBUTTONS = _ior('j', 0x12, 1)   # __u8


def jsiocgname(length: int) -> int:
    return _ior('j', 0x13, length)


# ------------------------------------------------------------------------
# Descoberta do device -- MESMA ordem do UsbJoystick_linux.cpp::reset().
# ------------------------------------------------------------------------

def find_device(index: int) -> str:
    for template in ("/dev/js{}", "/dev/input/js{}"):
        path = template.format(index)
        if os.path.exists(path):
            return path
    raise FileNotFoundError(
        f"nenhum joystick em /dev/js{index} nem /dev/input/js{index}.\n"
        "  Linux nativo: confira `lsmod | grep joydev` e `ls /dev/input/js*`.\n"
        "  WSL2: o USB nao passa por padrao -- anexe com usbipd-win antes\n"
        "  (`usbipd attach --wsl --busid <id>` no host Windows; ver a secao\n"
        "  'shared/xjoystick' do CLAUDE.md)."
    )


def query_device(fd: int):
    version_buf = ctypes.create_string_buffer(4)
    fcntl.ioctl(fd, JSIOCGVERSION, version_buf)
    version = struct.unpack("=I", version_buf.raw)[0]

    name_buf = ctypes.create_string_buffer(128)
    fcntl.ioctl(fd, jsiocgname(128), name_buf)
    name = name_buf.value.decode(errors="replace")

    axes_buf = ctypes.create_string_buffer(1)
    fcntl.ioctl(fd, JSIOCGAXES, axes_buf)
    num_axes = axes_buf.raw[0]

    buttons_buf = ctypes.create_string_buffer(1)
    fcntl.ioctl(fd, JSIOCGBUTTONS, buttons_buf)
    num_buttons = buttons_buf.raw[0]

    return name, num_axes, num_buttons, version


def format_version(version: int) -> str:
    return f"{version >> 16}.{(version >> 8) & 0xff}.{version & 0xff}"


def bar(value: float, width: int = 21) -> str:
    value = max(-1.0, min(1.0, value))
    pos = int(round((value + 1.0) / 2.0 * (width - 1)))
    return "[" + ("-" * pos) + "|" + ("-" * (width - 1 - pos)) + "]"


def redraw(path: str, name: str, version: int, axes, buttons, last_touch: str):
    lines = []
    lines.append("=== Mapeador de joystick (mesmo protocolo do mixr::linkage::UsbJoystick) ===")
    lines.append(f"Dispositivo : {path}")
    lines.append(f"Nome        : {name}")
    lines.append(f"Driver      : {format_version(version)}")
    lines.append(f"Eixos (AI)  : {len(axes)}      Botoes (DI): {len(buttons)}")
    lines.append("")
    lines.append("Mova UM eixo de cada vez ate o batente e veja qual linha 'AI' se move --")
    lines.append("esse numero e o 'channel:' do ( AnalogInput ) no scenario.epp.in (0-based).")
    lines.append("Aperte cada botao para achar o 'channel:' dos DIs. Ctrl+C sai.")
    lines.append("")
    for i, v in enumerate(axes):
        lines.append(f"  AI  channel={i:<2d}  valor={v:+.3f}  {bar(v)}")
    lines.append("")
    for i, pressed in enumerate(buttons):
        estado = "PRESSIONADO" if pressed else "solto      "
        lines.append(f"  DI  channel={i:<2d}  {estado}")
    lines.append("")
    lines.append(f"Ultimo evento: {last_touch or '(nenhum ainda -- mexa o joystick)'}")

    sys.stdout.write("\x1b[H\x1b[J")
    sys.stdout.write("\n".join(lines) + "\n")
    sys.stdout.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("--index", type=int, default=0, help="indice do device (default: 0)")
    parser.add_argument("--path", type=str, default=None, help="caminho explicito (ex.: /dev/input/js1)")
    args = parser.parse_args()

    path = args.path if args.path else find_device(args.index)

    try:
        fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
    except OSError as exc:
        print(f"erro ao abrir {path}: {exc}", file=sys.stderr)
        return 1

    try:
        name, num_axes, num_buttons, version = query_device(fd)
    except OSError as exc:
        print(f"erro consultando {path} (ioctl): {exc}", file=sys.stderr)
        os.close(fd)
        return 1

    axes = [0.0] * num_axes
    buttons = [False] * num_buttons
    last_touch = ""

    try:
        while True:
            got_event = False
            while True:
                try:
                    data = os.read(fd, EVENT_SIZE)
                except BlockingIOError:
                    break
                if len(data) < EVENT_SIZE:
                    break

                _time, value, ev_type, number = EVENT_STRUCT.unpack(data)
                kind = ev_type & ~JS_EVENT_INIT
                initial = bool(ev_type & JS_EVENT_INIT)

                if kind == JS_EVENT_AXIS and number < len(axes):
                    axes[number] = value / 32767.0
                    if not initial:
                        last_touch = f"AXIS  channel={number}  valor={axes[number]:+.3f}  (raw={value})"
                elif kind == JS_EVENT_BUTTON and number < len(buttons):
                    buttons[number] = bool(value)
                    if not initial:
                        estado = "PRESSIONADO" if value else "solto"
                        last_touch = f"BOTAO channel={number}  {estado}"

                got_event = True

            redraw(path, name, version, axes, buttons, last_touch)
            time.sleep(0.03)
    except KeyboardInterrupt:
        pass
    finally:
        os.close(fd)

    print("\nEncerrado.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
