# ViewModels 模块

add_library(EasyKiConverterViewModels STATIC
    ComponentListViewModel.cpp
    ComponentListServiceConnectionCoordinator.cpp
    ComponentListServiceConnectionCoordinator.h
    ComponentListTimerCoordinator.cpp
    ComponentListTimerCoordinator.h
    ComponentListIndex.cpp
    ComponentListIndex.h
    ComponentListStateTracker.cpp
    ComponentListStateTracker.h
    ComponentListPreviewUpdateBuffer.cpp
    ComponentListPreviewUpdateBuffer.h
    ComponentListItemUpdateBuffer.cpp
    ComponentListItemUpdateBuffer.h
    ComponentListDataCoordinator.cpp
    ComponentListDataCoordinator.h
    ComponentListBatchCoordinator.cpp
    ComponentListBatchCoordinator.h
    ComponentValidationQueue.cpp
    ComponentValidationQueue.h
    ComponentValidationCoordinator.cpp
    ComponentValidationCoordinator.h
    ComponentValidationErrorPolicy.cpp
    ComponentValidationErrorPolicy.h
    ExportSettingsViewModel.cpp
    ExportTargetModel.cpp
    ExportProgressViewModel.cpp
    ExportProgressResultsCoordinator.cpp
    ExportProgressResultsCoordinator.h
    ThemeSettingsViewModel.cpp
    SystemTrayManager.cpp
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
