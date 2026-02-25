cmake_minimum_required(VERSION 3.16)
project(smart_car LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/motor
    ${CMAKE_SOURCE_DIR}/motion
    ${CMAKE_SOURCE_DIR}/rt
    ${CMAKE_SOURCE_DIR}/sensors
)

add_executable(smart_car
    ${CMAKE_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/motor/WiringPiMotorDriver.cpp
    ${CMAKE_SOURCE_DIR}/motion/MotionController.cpp
    ${CMAKE_SOURCE_DIR}/rt/Scheduler.cpp
    ${CMAKE_SOURCE_DIR}/sensors/DHT11.cpp
    ${CMAKE_SOURCE_DIR}/sensors/DS18B20.cpp
    ${CMAKE_SOURCE_DIR}/sensors/RainSensor.cpp
)

target_link_libraries(smart_car wiringPi pthread)