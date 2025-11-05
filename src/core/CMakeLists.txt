add_library(seb_core STATIC
    core.cpp
    Config.cpp
    ConfigLoader.cpp
    IdleInhibitor.cpp
)

target_link_libraries(seb_core PUBLIC
    Qt6::Core
)

if(QT_FEATURE_dbus)
    target_link_libraries(seb_core PUBLIC Qt6::DBus)
endif()

target_include_directories(seb_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Optional: Test executable for ConfigLoader
add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE seb_core Qt6::Core)

