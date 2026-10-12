cmake_minimum_required(VERSION 3.20)
project(SaDiscordPresence LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Classic GTA San Andreas needs a 32-bit Windows build. Configure with -A Win32.")
endif()

set(PSDK_DIR "" CACHE PATH "Path to the plugin-sdk checkout")
set(PSDK_LIB "" CACHE FILEPATH "Path to the built 32-bit Release Plugin-SDK SA library")
if(NOT EXISTS "${PSDK_DIR}/shared/plugin.h")
    message(FATAL_ERROR "Set PSDK_DIR to a plugin-sdk checkout.")
endif()
if(NOT EXISTS "${PSDK_LIB}")
    message(FATAL_ERROR "Set PSDK_LIB to the built 32-bit SA Plugin-SDK library.")
endif()

set(PRESENCE_INI "${CMAKE_CURRENT_SOURCE_DIR}/SaDiscordPresence.ini.example")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SaDiscordPresence.ini")
    set(PRESENCE_INI "${CMAKE_CURRENT_SOURCE_DIR}/SaDiscordPresence.ini")
endif()

include(FetchContent)
FetchContent_Declare(discord_rpc
    URL https://github.com/discord/discord-rpc/archive/refs/tags/v3.4.0.zip)
FetchContent_Declare(rapidjson
    URL https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.zip)
FetchContent_GetProperties(discord_rpc)
if(NOT discord_rpc_POPULATED)
    FetchContent_Populate(discord_rpc)
endif()
FetchContent_GetProperties(rapidjson)
if(NOT rapidjson_POPULATED)
    FetchContent_Populate(rapidjson)
endif()

add_library(discord-rpc STATIC
    ${discord_rpc_SOURCE_DIR}/src/discord_rpc.cpp
    ${discord_rpc_SOURCE_DIR}/src/rpc_connection.cpp
    ${discord_rpc_SOURCE_DIR}/src/serialization.cpp
    ${discord_rpc_SOURCE_DIR}/src/connection_win.cpp
    ${discord_rpc_SOURCE_DIR}/src/discord_register_win.cpp)
target_compile_features(discord-rpc PUBLIC cxx_std_14)
target_compile_definitions(discord-rpc PRIVATE DISCORD_WINDOWS)
target_include_directories(discord-rpc PUBLIC ${discord_rpc_SOURCE_DIR}/include
    PRIVATE ${rapidjson_SOURCE_DIR}/include)
target_link_libraries(discord-rpc PUBLIC psapi advapi32)

add_library(SaDiscordPresence SHARED src/main.cpp)
target_compile_features(SaDiscordPresence PRIVATE cxx_std_23)
target_compile_definitions(SaDiscordPresence PRIVATE _DX9_SDK_INSTALLED GTASA PLUGIN_SGV_10US RW WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(SaDiscordPresence PRIVATE
    ${PSDK_DIR}/shared/dxsdk
    ${PSDK_DIR}/plugin_sa
    ${PSDK_DIR}/plugin_sa/game_sa
    ${PSDK_DIR}/plugin_sa/game_sa/enums
    ${PSDK_DIR}/plugin_sa/game_sa/rw
    ${PSDK_DIR}/shared
    ${PSDK_DIR}/shared/game
    ${PSDK_DIR}/safetyhook)
target_link_libraries(SaDiscordPresence PRIVATE "${PSDK_LIB}" discord-rpc)
if(MINGW)
    target_link_options(SaDiscordPresence PRIVATE -static)
endif()
set_target_properties(SaDiscordPresence PROPERTIES PREFIX "" SUFFIX ".asi")

add_custom_command(TARGET SaDiscordPresence POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PRESENCE_INI}
        $<TARGET_FILE_DIR:SaDiscordPresence>/SaDiscordPresence.ini
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/SaDiscordPresence.missions.ini
        $<TARGET_FILE_DIR:SaDiscordPresence>/SaDiscordPresence.missions.ini)
