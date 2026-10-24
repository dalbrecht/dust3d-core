# SPDX-License-Identifier: MIT
if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Supply SOURCE_ROOT")
endif()
file(READ "${SOURCE_ROOT}/application/application.pro" reference_manifest)
# Existing reference catalog callers require the newly extracted metadata definition.
set(registration "SOURCES += ../dust3d/animation/biped/clip_metadata.cc")
string(FIND "${reference_manifest}" "${registration}" position)
if(position EQUAL -1)
    message(FATAL_ERROR "Reference animation build is missing biped/clip_metadata.cc")
endif()
if(NOT EXISTS "${SOURCE_ROOT}/dust3d/animation/biped/clip_metadata.cc")
    message(FATAL_ERROR "Registered reference metadata source does not exist")
endif()
