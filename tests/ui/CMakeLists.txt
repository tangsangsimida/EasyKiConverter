
find_package(Qt6 REQUIRED COMPONENTS QuickTest Quick Qml)

# QML 文件列表 (从根目录复制，确保测试可以访问)
set(QML_FILES
    ${CMAKE_SOURCE_DIR}/src/ui/qml/Main.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/MainWindow.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/Card.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ComponentListItem.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ResultListItem.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/Icon.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ModernButton.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/StatItem.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/StepItem.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/TitleBar.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/HeaderSection.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ComponentInputCard.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/BomImportCard.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ComponentListCard.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ExportSettingsCard.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ExportProgressCard.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ExportResultsCard.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ExportStatisticsCard.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/ExportButtonsSection.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/WindowResizeHandles.qml
    ${CMAKE_SOURCE_DIR}/src/ui/qml/styles/AppStyle.qml
    ${CMAKE_CURRENT_SOURCE_DIR}/tst_BasicComponents.qml
)

set(QML_RESOURCES
    ${CMAKE_SOURCE_DIR}/src/ui/qml/styles/qmldir
    ${CMAKE_SOURCE_DIR}/src/ui/qml/components/qmldir
)

# 为每个 QML 文件和资源文件设置资源别名
foreach(FILE ${QML_FILES} ${QML_RESOURCES})
    # 如果文件在源码目录中
    string(FIND "${FILE}" "${CMAKE_SOURCE_DIR}/src/" POS)
    if(POS EQUAL 0)
        file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}" "${FILE}")
        set_source_files_properties("${FILE}" PROPERTIES QT_RESOURCE_ALIAS "${REL_PATH}")
    else()
        # 对于当前目录的测试文件
        string(FIND "${FILE}" "${CMAKE_CURRENT_SOURCE_DIR}" POS_TEST)
        if(POS_TEST EQUAL 0)
            file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}" "${FILE}")
            set_source_files_properties("${FILE}" PROPERTIES QT_RESOURCE_ALIAS "${REL_PATH}")
        endif()
    endif()
endforeach()

qt_add_executable(test_ui
    main.cpp
)

qt_add_qml_module(test_ui
    URI EasyKiconverter_Cpp_Version
    VERSION 1.0
    QML_FILES ${QML_FILES}
    RESOURCES ${QML_RESOURCES}
)

target_link_libraries(test_ui PRIVATE
    Qt6::QuickTest
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
    EasyKiConverterLib
)

target_include_directories(test_ui PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(test_ui PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_test(NAME test_ui COMMAND test_ui)
