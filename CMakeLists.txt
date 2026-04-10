cmake_minimum_required(VERSION 3.16)
project(TransportAdvisor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network WebEngineWidgets WebChannel)

qt_add_executable(TransportAdvisor
    main.cpp
    MainWindow.cpp
    MainWindow.h
    TransportBridge.cpp
    TransportBridge.h
    RouteService.cpp
    RouteService.h
    RouteRanker.cpp
    RouteRanker.h
    DestinationValidator.cpp
    DestinationValidator.h
    LocalBusCsvModel.cpp
    LocalBusCsvModel.h
    HkGridToWgs84.h
    resources.qrc
)

target_link_libraries(TransportAdvisor PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)

if(APPLE)
    set_target_properties(TransportAdvisor PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# Text-mode graph journey planner (coursework model: stops/segments CSV, DFS, preferences)
qt_add_executable(pt_advisor
    pt_advisor_main.cpp
    PtGraphAdvisor.cpp
    PtGraphAdvisor.h
    PtGraphTypes.cpp
    PtGraphTypes.h
)
target_link_libraries(pt_advisor PRIVATE Qt6::Core)
set_target_properties(pt_advisor PROPERTIES WIN32_EXECUTABLE FALSE)
if(APPLE)
    set_target_properties(pt_advisor PROPERTIES MACOSX_BUNDLE FALSE)
endif()
