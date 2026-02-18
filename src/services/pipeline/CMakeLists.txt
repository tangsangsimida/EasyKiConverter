# Pipeline 子模块

set(PIPELINE_SOURCES
    pipeline/stats/PipelineStatistics.cpp
    pipeline/cleanup/PipelineCleanup.cpp
    pipeline/queue/PipelineQueueManager.cpp
    pipeline/stage/FetchStageHandler.cpp
    pipeline/stage/ProcessStageHandler.cpp
    pipeline/stage/WriteStageHandler.cpp
)

set(PIPELINE_HEADERS
    pipeline/stats/PipelineStatistics.h
    pipeline/cleanup/PipelineCleanup.h
    pipeline/queue/PipelineQueueManager.h
    pipeline/stage/FetchStageHandler.h
    pipeline/stage/ProcessStageHandler.h
    pipeline/stage/StageHandler.h
    pipeline/stage/WriteStageHandler.h
)

# 将源文件和头文件提升到父目录作用域
# 注意：路径已经是相对于 services/ 的，所以父目录可以直接用
set(PIPELINE_SOURCES ${PIPELINE_SOURCES} PARENT_SCOPE)
set(PIPELINE_HEADERS ${PIPELINE_HEADERS} PARENT_SCOPE)

# 包含目录
target_include_directories(EasyKiConverterServices
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/stats
        ${CMAKE_CURRENT_SOURCE_DIR}/cleanup
        ${CMAKE_CURRENT_SOURCE_DIR}/queue
        ${CMAKE_CURRENT_SOURCE_DIR}/stage
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../models
        ${CMAKE_CURRENT_SOURCE_DIR}/../../workers
        ${CMAKE_CURRENT_SOURCE_DIR}/../../utils
)
