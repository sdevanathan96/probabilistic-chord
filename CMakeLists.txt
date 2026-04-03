cmake_minimum_required(VERSION 3.10)
project(chord-filter-routing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(LIB_SOURCES
    src/transport/message.cpp
    src/transport/sim_transport.cpp
    src/transport/sim_network.cpp
    src/transport/ipc_transport.cpp
    src/chord/node.cpp
    src/chord/ring_utils.cpp
    src/chord/finger_table.cpp
    # src/filter/cuckoo_filter.cpp
    # src/filter/quotient_filter.cpp
)

add_library(chord_lib ${LIB_SOURCES})
target_include_directories(chord_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)


add_executable(chord_node src/main.cpp)
target_link_libraries(chord_node chord_lib pthread)

enable_testing()


add_executable(test_message test/test_message.cpp)
target_link_libraries(test_message chord_lib)
add_test(NAME test_message COMMAND test_message)

add_executable(test_sim_transport test/test_sim_transport.cpp)
target_link_libraries(test_sim_transport chord_lib pthread)
add_test(NAME test_sim_transport COMMAND test_sim_transport)

add_executable(test_ipc_transport test/test_ipc_transport.cpp)
target_link_libraries(test_ipc_transport chord_lib pthread)
add_test(NAME test_ipc_transport COMMAND test_ipc_transport)

add_executable(test_chord test/test_chord.cpp)
target_link_libraries(test_chord chord_lib pthread)
add_test(NAME test_chord COMMAND test_chord)

add_executable(test_finger_table test/test_finger_table.cpp)
target_link_libraries(test_finger_table chord_lib pthread)
add_test(NAME test_finger_table COMMAND test_finger_table)