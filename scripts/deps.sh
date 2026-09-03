#!/usr/bin/env bash
#
# Constroi, do FONTE, as quatro dependencias que este repositorio empacota ele
# mesmo (as demais -- protobuf, boost, ftxui, onnxruntime... -- vem prontas do
# conancenter e sao resolvidas pelo 'conan install' da raiz).
#
# A ORDEM IMPORTA: mixr requer jsbsim e openrti, entao as duas tem de estar no
# cache antes dele. behaviortree e independente e vai por ultimo.
#
# Debug E Release: o projeto configura em Debug por padrao (BUILD_TYPE do
# Makefile da raiz), mas o perfil default do conan e Release -- ter os dois
# evita um rebuild surpresa ao trocar de build_type.

set -x
set -e

# Roda a partir da RAIZ do repositorio, independente de onde foi chamado --
# os caminhos './deps/...' abaixo dependem disso.
cd "$(dirname "${BASH_SOURCE[0]}")/.."

for BUILD_TYPE in Debug Release; do
    echo "Building mixr dependencies in ${BUILD_TYPE} mode"

    conan create ./deps/jsbsim  --build=missing --settings=build_type="${BUILD_TYPE}"
    conan create ./deps/openrti --build=missing --settings=build_type="${BUILD_TYPE}"

    # cppstd=gnu11 e o que o proprio repositorio do mixr usa no alvo 'package'
    # do Makefile dele, e e com o que os pacotes mixr/1.0.5 em cache foram
    # construidos: o fonte e C++11 ('cpp_std=c++11' no project() do meson.build
    # do mixr, compilado com -fpermissive). O consumidor continua em gnu17 --
    # quem casa os dois e o plugin de compatibilidade do conan, que aceita um
    # binario de cppstd menor.
    conan create ./deps/mixr \
        --build=missing \
        --settings=build_type="${BUILD_TYPE}" \
        --settings=compiler.cppstd=gnu11

    # shared=False e o default da receita, repetido aqui porque e requisito do
    # projeto, nao preferencia: o plugin do modelo linka a BT.CPP ESTATICA (ver
    # o comentario em ./deps/behaviortree/conanfile.py e o conanfile.py da
    # raiz, que repete o override do lado do consumidor).
    conan create ./deps/behaviortree \
        --build=missing \
        --settings=build_type="${BUILD_TYPE}" \
        --options='behaviortree.cpp.asa/*:shared=False'
done
