
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
aux_source_directory(. DIR_SRCS)

add_executable(wifi_monitor ${DIR_SRCS})

target_link_libraries(wifi_monitor wpa_client)
target_link_libraries(wifi_monitor wpa_ctrl)
target_link_libraries(wifi_monitor rkamsex)
