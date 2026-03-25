cmake_minimum_required(VERSION 3.16)
project(smartcar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

enable_testing()

find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Widgets Test)

pkg_check_modules(GPIOD REQUIRED libgpiod)
pkg_check_modules(LIRC REQUIRED lirc)

include_directories(
    include
    tests
    ${OpenCV_INCLUDE_DIRS}
    ${GPIOD_INCLUDE_DIRS}
    ${LIRC_INCLUDE_DIRS}
)

add_library(car_logic STATIC
    src/motion/MotionController.cpp
    src/rt/MotionCommandService.cpp
    src/motor/GpiodMotorDriver.cpp
    src/monitor/MonitorService.cpp
    src/monitor/CameraService.cpp
    src/gimbal/GimbalService.cpp
    src/ir/IrRemote.cpp
    src/gui/SystemFacade.cpp
    src/gui/MainWindow.cpp
    include/gui/MainWindow.hpp
)

target_link_libraries(car_logic PRIVATE
    ${OpenCV_LIBS}
    ${GPIOD_LIBRARIES}
    ${LIRC_LIBRARIES}
    Qt5::Widgets
    pthread
)

add_executable(smartcar src/main.cpp)
target_link_libraries(smartcar PRIVATE car_logic)

add_executable(smartcar_gui
    src/main_gui.cpp
)
target_link_libraries(smartcar_gui PRIVATE car_logic Qt5::Widgets)

macro(add_car_test name src)
    add_executable(${name} tests/${src})
    target_link_libraries(${name} PRIVATE car_logic pthread)
    add_test(NAME ${name} COMMAND ${name})
endmacro()

add_car_test(bus_test BusTest.cpp)
add_car_test(motion_test MotionTest.cpp)
add_car_test(monitor_test MonitorTest.cpp)
add_car_test(gimbal_test GimbalTest.cpp)

add_executable(camera_test tests/CameraTest.cpp)
target_link_libraries(camera_test PRIVATE car_logic ${OpenCV_LIBS})
add_test(NAME camera_test COMMAND camera_test)

add_executable(ir_test tests/IrRemoteTest.cpp)
target_link_libraries(ir_test PRIVATE car_logic ${LIRC_LIBRARIES})
add_test(NAME ir_test COMMAND ir_test)

add_executable(gui_test tests/GuiTest.cpp include/gui/MainWindow.hpp)
target_link_libraries(gui_test PRIVATE car_logic Qt5::Widgets Qt5::Test)
add_test(NAME gui_test COMMAND gui_test)