cmake_minimum_required(VERSION 3.9.2)

project(cthreadpool VERSION 0.1.0 LANGUAGES C)

set(sources cthreadpool.c)

option(CTHREADPOOL_SHARED "Build shared library" ON)
option(CTHREADPOOL_STATIC "Build static library" ON)

if(CTHREADPOOL_SHARED)
	add_library(cthreadpool_shared SHARED ${sources})
	target_include_directories(cthreadpool_shared PUBLIC .)
	set_target_properties(cthreadpool_shared PROPERTIES OUTPUT_NAME cthreadpool)
endif()
if(CTHREADPOOL_STATIC)
	add_library(cthreadpool STATIC ${sources})
	target_include_directories(cthreadpool PUBLIC .)
endif()
if(NOT (CTHREADPOOL_STATIC OR CTHREADPOOL_SHARED))
	add_library(cthreadpool OBJECT ${sources})
endif()
