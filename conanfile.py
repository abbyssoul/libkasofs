#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""Conan recipe package for libkasofs
"""
from conans import ConanFile, CMake


class LibkasofsConan(ConanFile):
    name = "libkasofs"
    version = "0.1"
    license = "Apache-2.0"
    author = "Ivan Ryabov <abbyssoul@gmail.com>"
    url = "https://github.com/abbyssoul/%s.git" % name
    homepage = "https://github.com/abbyssoul/%s" % name
    description = "Virtual filesystem"
    topics = ("vfs", "Modern C++")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = "shared=False", "fPIC=True"
    generators = "cmake"

    #exports_sources = "src/*"
    scm = {
       "type": "git",
       "subfolder": "libkasofs",
       "url": "auto",
       "revision": "auto"
    }
    requires = "libsolace/0.1.1@abbyssoul/stable"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def build(self):
        cmake = CMake(self, parallel=True)
        cmake.configure(source_folder=self.name)
        cmake.build()
        cmake.test()
        cmake.install()

    def package(self):
        self.copy("*.hpp", dst="include", src="include")
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.dylib*", dst="lib", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["kasofs"]
