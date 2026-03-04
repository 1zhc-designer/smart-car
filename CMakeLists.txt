cmake_minimum_required(VERSION 3.16)
project(smartcar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(smartcar
    main.cpp
    motor/WiringPiMotorDriver.cpp
    motion/MotionController.cpp
    rt/Scheduler.cpp
    ir/IrRemote.cpp
    monitor/MonitorService.cpp
)

target_include_directories(smartcar PRIVATE .)

target_link_libraries(smartcar PRIVATE wiringPi lirc_client pthread m)