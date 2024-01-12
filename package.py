# Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
# SPDX-License-Identifier: Apache-2.0

# -*- coding: utf-8 -*-
import sys

name = 'arras4_core'

@early()
def version():
    """
    Increment the build in the version.
    """
    _version = '4.10.3'
    from rezbuild import earlybind
    return earlybind.version(this, _version)

description = 'Arras Core'

authors = ['Dreamworks Animation R&D - JoSE Team',
           'psw-jose@dreamworks.com',
           'rob.wilson@dreamworks.com']

help = ('For assistance, '
        "please contact the folio's owner at: psw-jose@dreamworks.com")

if 'scons' in sys.argv:
    build_system = 'scons'
    build_system_pbr = 'bart_scons-10'
else:
    build_system = 'cmake'
    build_system_pbr = 'cmake_modules'

variants = [
    ['os-CentOS-7', 'refplat-vfx2019.3'],
    ['os-CentOS-7', 'refplat-vfx2020.3'],
    ['os-CentOS-7', 'refplat-vfx2021.0'],
    ['os-CentOS-7', 'refplat-vfx2022.0'],
    ['os-rocky-9', 'refplat-vfx2021.0'],
    ['os-rocky-9', 'refplat-vfx2022.0'],
    ['os-rocky-9', 'refplat-vfx2023.0'],
]

sconsTargets = {}
for variant in variants:
    sconsTargets[variant[0]] = ['@install', '@doxygen']

private_build_requires = [
    build_system_pbr,
    'cppunit', 
    'gcc-6.3.x|9.3.x|11.x'
]

if build_system is 'cmake':
    def commands():
        prependenv('CMAKE_MODULE_PATH', '{root}/lib64/cmake')
        prependenv('CMAKE_PREFIX_PATH', '{root}')
        prependenv('LD_LIBRARY_PATH', '{root}/lib64')
        prependenv('PATH', '{root}/bin')
        prependenv('ARRAS_SESSION_PATH', '{root}/sessions')
else:
    def commands():
        prependenv('LD_LIBRARY_PATH', '{root}/lib')
        prependenv('PATH', '{root}/bin')
        prependenv('ARRAS_SESSION_PATH', '{root}/sessions')

uuid = '2625d822-f8b4-4e3d-8106-007c2b9f42c2'

config_version = 0

@early()
def requires():
    # Requirements that apply to both pipeX and job environments
    requires = [
        'uuid-1.0.0',
        'boost',
        'curl_no_ldap-7.49.1.x.2',
        'jsoncpp-1.9.5.x',
        'libmicrohttpd-0.9.71.x.1',
        'breakpad-1499'
    ]
    if building:
        from rezbuild import earlybind
        requires.extend(earlybind.get_compiler(build_variant_requires))

    return requires
