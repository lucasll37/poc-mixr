#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Conan recipe package for BehaviorTree.CPP (fork ASA, v3)
"""
import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class BehaviorTreeCppAsaConan(ConanFile):
    name = "behaviortree.cpp.asa"
    version = "3.5.6"
    license = "MIT"
    url = "https://github.com/BehaviorTree/BehaviorTree.CPP"
    author = "Davide Faconti <davide.faconti@gmail.com>"
    topics = ("behaviortree", "ai", "robotics", "games", "coordination")
    description = (
        "This C++ library provides a framework to create BehaviorTrees. "
        "It was designed to be flexible, easy to use and fast."
    )

    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": True, "fPIC": True}

    exports_sources = (
        "CMakeLists.txt",
        "cmake/*",
        "include/*",
        "src/*",
        "3rdparty/*",
        "LICENSE",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        tc.cache_variables["BUILD_EXAMPLES"] = False
        tc.cache_variables["BUILD_UNIT_TESTS"] = False
        tc.cache_variables["BUILD_TOOLS"] = False
        tc.cache_variables["BUILD_WITH_CURSES"] = False
        # Determinismo: o CMakeLists procura estes pacotes no sistema e, se os
        # achar, muda o conteudo/ABI do pacote (coroutines com Boost, logger
        # ZMQ, layout de instalacao do ament) sem que a receita os declare.
        tc.cache_variables["CMAKE_DISABLE_FIND_PACKAGE_Boost"] = True
        tc.cache_variables["CMAKE_DISABLE_FIND_PACKAGE_ZMQ"] = True
        tc.cache_variables["CMAKE_DISABLE_FIND_PACKAGE_ament_cmake"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder,
             os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["behaviortree_cpp_v3"]
        # Sem Boost a lib e compilada com -DBT_NO_COROUTINES; o mesmo define
        # precisa chegar ao consumidor, senao action_node.h declara
        # CoroActionNode, que nao existe no binario.
        self.cpp_info.defines = ["BT_NO_COROUTINES"]
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs = ["pthread", "dl"]
