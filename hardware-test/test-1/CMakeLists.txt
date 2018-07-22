cmake_minimum_required(VERSION 2.8)

project(test-1)

#add_library(DBFTDIEvent dbftdievent.cpp)
#add_library(ftdi SHARED IMPORTED)

add_executable(${PROJECT_NAME} "main.cpp" "dbftdi.cpp" "dbftdievent.cpp")
#target_link_libraries(${PROJECT_NAME} -lftdi)
target_link_libraries(${PROJECT_NAME} ftdi1 usb-1.0)

#target_link_libraries(DBFTDI -l)
