cmake_minimum_required(VERSION 3.16)
project(smartcar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Widgets Test)
find_package(GTest REQUIRED)

pkg_check_modules(GPIOD REQUIRED libgpiod)
pkg_check_modules(LIRC REQUIRED lirc)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

include_directories(include)
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${GPIOD_INCLUDE_DIRS})
include_directories(${LIRC_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMER_VIDEO_INCLUDE_DIRS})

add_library(car_logic STATIC
    src/motion/MotionController.cpp
    src/rt/Scheduler.cpp
    src/motor/GpiodMotorDriver.cpp
    src/monitor/MonitorService.cpp
    src/monitor/CameraService.cpp
    src/gimbal/GimbalService.cpp
    src/ir/IrRemote.cpp
)

target_link_libraries(car_logic PRIVATE
    ${OpenCV_LIBS}
    ${GPIOD_LIBRARIES}
    ${LIRC_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
    pthread
)

add_executable(smartcar
    src/main.cpp
)

target_link_libraries(smartcar PRIVATE
    car_logic
    ${OpenCV_LIBS}
    pthread
)

add_executable(smartcar_gui
    src/gui/main_gui.cpp
    src/gui/MainWindow.cpp
    src/gui/SystemFacade.cpp
    include/gui/MainWindow.hpp
    include/gui/SystemFacade.hpp
)

target_link_libraries(smartcar_gui PRIVATE
    car_logic
    ${OpenCV_LIBS}
    Qt5::Widgets
    pthread
)

enable_testing()

add_executable(unit_tests
    tests/MotionTest.cpp
    tests/SchedulerTest.cpp
    tests/GimbalTest.cpp
    tests/MonitorTest.cpp
    tests/CameraTest.cpp
    tests/IrRemoteTest.cpp
)

target_link_libraries(unit_tests PRIVATE
    car_logic
    GTest::gtest_main
    pthread
)

add_executable(gui_test
    tests/GuiTest.cpp
    src/gui/MainWindow.cpp
    src/gui/SystemFacade.cpp
    include/gui/MainWindow.hpp
)

target_link_libraries(gui_test PRIVATE
    car_logic
    Qt5::Widgets
    Qt5::Test
    pthread
)

add_test(NAME AllLogicTests COMMAND unit_tests)
add_test(NAME GuiSystemTest COMMAND gui_test)