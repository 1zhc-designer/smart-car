cmake_minimum_required(VERSION 3.16)
project(smartcar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header search path
include_directories(include)

# Core logic library
add_library(car_logic STATIC
    src/motion/MotionController.cpp
    src/rt/Scheduler.cpp
    src/monitor/MonitorService.cpp
)

# Main executable
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
)

# Tests temporarily disabled
# enable_testing()
# add_subdirectory(tests)