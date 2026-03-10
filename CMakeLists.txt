cmake_minimum_required(VERSION 3.16)
project(smartcar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

include_directories(include)
include_directories(${OpenCV_INCLUDE_DIRS})

add_library(car_logic STATIC
    src/motion/MotionController.cpp
    src/rt/Scheduler.cpp
    src/monitor/MonitorService.cpp
    src/monitor/CameraService.cpp
    src/gimbal/GimbalService.cpp
)

target_link_libraries(car_logic PRIVATE
    ${OpenCV_LIBS}
)

add_executable(smartcar
    src/main.cpp
    src/motor/WiringPiMotorDriver.cpp
    src/ir/IrRemote.cpp
)

target_link_libraries(smartcar PRIVATE
    car_logic
    wiringPi
    lirc_client
    pthread
    ${OpenCV_LIBS}
)