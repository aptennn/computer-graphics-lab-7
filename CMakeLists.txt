cmake_minimum_required(VERSION 4.2)
project(computer_graphics7)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED)

# Егор: подключение модели многогранника и каркасной отрисовки к приложению.
add_executable(computer_graphics6
        main.cpp
        geometry.cpp geometry.h
        wireframe_renderer.cpp wireframe_renderer.h
        wireframe_canvas.cpp wireframe_canvas.h
        mat4.cpp mat4.h
        projection.cpp projection.h
        mainwindow.cpp mainwindow.h)
target_link_libraries(computer_graphics6
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

include(CTest)
if(BUILD_TESTING)
    add_executable(lab6_transformations_test
            tests/transformations.cpp geometry.cpp mat4.cpp projection.cpp)
    target_include_directories(lab6_transformations_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(lab6_transformations_test PROPERTIES
            AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    add_test(NAME lab6_transformations COMMAND lab6_transformations_test)

    add_executable(lab6_window_test
            tests/window.cpp mainwindow.cpp mainwindow.h
            wireframe_canvas.cpp wireframe_renderer.cpp
            geometry.cpp mat4.cpp projection.cpp)
    target_include_directories(lab6_window_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(lab6_window_test PRIVATE Qt6::Widgets)
    add_test(NAME lab6_window COMMAND lab6_window_test)
    set_tests_properties(lab6_window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
