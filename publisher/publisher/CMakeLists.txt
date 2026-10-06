cmake_minimum_required(VERSION 3.16)
project(lidar_csv_someip_publisher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(vsomeip3 REQUIRED)
find_package(Threads REQUIRED)

add_executable(lidar_csv_publisher
    LidarCSV_SOMEIP_Publisher.cpp
)

target_link_libraries(lidar_csv_publisher
    vsomeip3
    Threads::Threads
)