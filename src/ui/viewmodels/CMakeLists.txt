# ViewModels 模块

add_library(EasyKiConverterViewModels STATIC
    ComponentListViewModel.cpp
    ExportSettingsViewModel.cpp
    ExportProgressViewModel.cpp
    ThemeSettingsViewModel.cpp
    SystemTrayManager.cpp
    ThumbnailUpdateManager.cpp
    ValidationStateManager.cpp
)

target_include_directories(EasyKiConverterViewModels
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(EasyKiConverterViewModels
    PUBLIC
        Qt6::Core
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        EasyKiConverterServices
        EasyKiConverterModels
)