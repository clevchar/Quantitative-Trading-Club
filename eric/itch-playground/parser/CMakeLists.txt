add_library(itch_parser STATIC src/itch_parser.cpp)
target_include_directories(itch_parser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(parser_bin src/main.cpp)
target_link_libraries(parser_bin PRIVATE itch_parser)

add_executable(consumer_bin src/udp_consumer.cpp)
target_link_libraries(consumer_bin PRIVATE itch_parser)

target_compile_options(itch_parser PRIVATE -O3)
 
