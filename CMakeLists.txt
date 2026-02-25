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
    main.cpp
    motor/WiringPiMotorDriver.cpp
    motion/MotionController.cpp
    rt/Scheduler.cpp

    sensors/DHT11.cpp
    sensors/DS18B20.cpp
    sensors/RainSensor.cpp
)

target_link_libraries(smart_car wiringPi pthread)