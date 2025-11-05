add_library(seb_web STATIC
    web.cpp
    RequestInterceptor.cpp
    MainWindow.cpp
    SecureWebEnginePage.cpp
)

target_link_libraries(seb_web PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
)

target_include_directories(seb_web PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# seb_web depends on seb_core
target_link_libraries(seb_web PRIVATE
    seb_core
)

