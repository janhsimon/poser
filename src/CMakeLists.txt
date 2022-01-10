set(TARGET_NAME poser)

add_executable(${TARGET_NAME})
target_sources(${TARGET_NAME} PRIVATE "Main.cpp")
target_link_libraries(${TARGET_NAME} PRIVATE glad glfw glm)
set_target_properties(${TARGET_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_INSTALL_PREFIX}")
install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}")