cmake_minimum_required(VERSION 4.2.3)

if(MEDIAPROXY_INNER_BUILD)
    project(MediaProxy LANGUAGES CXX)
    include(cmake/Application.cmake)
    return()
endif()

project(MediaProxySuperbuild LANGUAGES NONE)

include(CTest)
include(cmake/Superbuild.cmake)
