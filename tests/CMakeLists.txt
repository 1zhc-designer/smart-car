include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01d3170949999c06ade657fb89da597c63.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    MotionControllerTest.cpp
    SchedulerTest.cpp
)

target_link_libraries(unit_tests 
    PRIVATE 
    GTest::gtest_main 
    GTest::gmock
    car_logic
    pthread
)

include(GoogleTest)
gtest_discover_tests(unit_tests)