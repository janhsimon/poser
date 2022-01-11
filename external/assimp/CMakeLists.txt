set(TARGET_NAME assimp)

add_library(${TARGET_NAME} SHARED IMPORTED GLOBAL)
target_include_directories(${TARGET_NAME} INTERFACE "include")

if(WIN32)
  set_target_properties(${TARGET_NAME} PROPERTIES IMPORTED_IMPLIB "${CMAKE_CURRENT_LIST_DIR}/lib/win/assimp.lib")
  target_link_libraries(${TARGET_NAME} INTERFACE "${CMAKE_CURRENT_LIST_DIR}/lib/win/zlibstatic.lib")
  install(FILES "lib/win/assimp.dll" DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()