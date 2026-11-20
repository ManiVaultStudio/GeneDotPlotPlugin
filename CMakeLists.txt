cmake_minimum_required(VERSION 3.22)

option(MV_UNITY_BUILD "Combine target source files into batches for faster compilation" OFF)

# -----------------------------------------------------------------------------
# Gene Dot Plot Plugin
# -----------------------------------------------------------------------------
set(PROJECT "GeneDotPlotPlugin")

PROJECT(${PROJECT}
        DESCRIPTION "Gene expression dot plot plugin"
        VERSION 1.0.0
        LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# CMake Options
# -----------------------------------------------------------------------------
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

set(GENEDOTPLOT_AUTO_INSTALL_DEFAULT OFF)
if(TARGET MV_Application)
    set(GENEDOTPLOT_AUTO_INSTALL_DEFAULT ON)
endif()

option(GENEDOTPLOT_AUTO_INSTALL "Install the plugin after building" ${GENEDOTPLOT_AUTO_INSTALL_DEFAULT})

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------
# Network and WebEngineWidgets are required by the ManiVault public headers.
find_package(Qt6 COMPONENTS Widgets Network WebEngineWidgets REQUIRED)

find_package(ManiVault COMPONENTS Core PointData ClusterData CONFIG QUIET)

# -----------------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------------
set(PLUGIN
    src/GeneDotPlotPlugin.h
    src/GeneDotPlotPlugin.cpp
)

set(UI
    src/GeneDotPlotWidget.h
    src/GeneDotPlotWidget.cpp
)

set(Actions
    src/Actions/SettingsAction.h
    src/Actions/SettingsAction.cpp
    src/Actions/MarkerSelectionAction.h
    src/Actions/MarkerSelectionAction.cpp
    src/Actions/ExportAction.h
    src/Actions/ExportAction.cpp
)

set(Utilities
    src/DotPlotStatistics.h
    src/DotPlotMembership.h
    src/DotPlotListState.h
)

set(AUX
    PluginInfo.json
)

set(SOURCES ${PLUGIN} ${UI} ${Actions} ${Utilities})

source_group(Plugin FILES ${PLUGIN})
source_group(UI FILES ${UI})
source_group(Actions FILES ${Actions})
source_group(Utilities FILES ${Utilities})
source_group(Aux FILES ${AUX})

# -----------------------------------------------------------------------------
# CMake Target
# -----------------------------------------------------------------------------
add_library(${PROJECT} SHARED ${SOURCES} ${AUX})

# -----------------------------------------------------------------------------
# Target include directories
# -----------------------------------------------------------------------------
target_include_directories(${PROJECT} SYSTEM PRIVATE "${ManiVault_INCLUDE_DIR}")

# -----------------------------------------------------------------------------
# Target properties
# -----------------------------------------------------------------------------
target_compile_features(${PROJECT} PRIVATE cxx_std_20)

set_target_properties(${PROJECT} PROPERTIES
    AUTOMOC ON
    UNITY_BUILD ${MV_UNITY_BUILD}
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${PROJECT} PRIVATE /EHsc /permissive- /Zc:__cplusplus /utf-8 /W4)
    target_compile_definitions(${PROJECT} PRIVATE NOMINMAX)
endif()

# -----------------------------------------------------------------------------
# Target library linking
# -----------------------------------------------------------------------------
target_link_libraries(${PROJECT} PRIVATE Qt6::Widgets)

target_link_libraries(${PROJECT} PRIVATE Qt6::Network)

target_link_libraries(${PROJECT} PRIVATE Qt6::WebEngineWidgets)

target_link_libraries(${PROJECT} PRIVATE ManiVault::Core)

target_link_libraries(${PROJECT} PRIVATE ManiVault::PointData)

target_link_libraries(${PROJECT} PRIVATE ManiVault::ClusterData)

# -----------------------------------------------------------------------------
# Target installation
# -----------------------------------------------------------------------------
install(TARGETS ${PROJECT}
    RUNTIME DESTINATION Plugins COMPONENT PLUGINS # Windows .dll
    LIBRARY DESTINATION Plugins COMPONENT PLUGINS # Linux/Mac .so
)

if(GENEDOTPLOT_AUTO_INSTALL)
    add_custom_command(TARGET ${PROJECT} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            --install "${CMAKE_CURRENT_BINARY_DIR}"
            --config $<CONFIGURATION>
            --component PLUGINS
            --prefix "${ManiVault_INSTALL_DIR}/$<CONFIGURATION>"
    )
endif()

mv_handle_plugin_config(${PROJECT})

# -----------------------------------------------------------------------------
# Miscellaneous
# -----------------------------------------------------------------------------
# Automatically set the debug environment (command + working directory) for MSVC.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_property(TARGET ${PROJECT} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${ManiVault_INSTALL_DIR}/$<CONFIG>")
    set_property(TARGET ${PROJECT} PROPERTY VS_DEBUGGER_COMMAND "${ManiVault_INSTALL_DIR}/$<CONFIG>/ManiVault Studio.exe")
endif()
