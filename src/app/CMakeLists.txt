add_executable(seb-linux
    main.cpp
)

target_link_libraries(seb-linux PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    seb_core
    seb_web
)

