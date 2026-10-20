cmake_minimum_required(VERSION 3.21)
project(YoloStudio VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt 6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql)

# OpenCV 4.10（隔离依赖，直接链接 opencv_world4100）
set(OPENCV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../deps/opencv4.10")

set(SOURCES
    main.cpp
    src/MainWindow.cpp
    src/ProjectManager.cpp
    src/LabelIO.cpp
    src/Database.cpp
    src/PythonRunner.cpp
    src/DashboardPage.cpp
    src/AnnotationPage.cpp
    src/AnnotationView.cpp
    src/TrainingPage.cpp
    src/ValidationPage.cpp
    src/InferenceWorker.cpp
    src/TrainingCurveWidget.cpp
    src/ExportPage.cpp
    src/PreprocessDialog.cpp
)

set(HEADERS
    src/MainWindow.h
    src/ProjectManager.h
    src/Annotation.h
    src/LabelIO.h
    src/Database.h
    src/PythonRunner.h
    src/TrainConfig.h
    src/DashboardPage.h
    src/AnnotationPage.h
    src/AnnotationView.h
    src/TrainingPage.h
    src/ValidationPage.h
    src/InferenceWorker.h
    src/TrainingCurveWidget.h
    src/ExportPage.h
    src/PreprocessDialog.h
)

qt_add_executable(YoloStudio WIN32 ${SOURCES} ${HEADERS})

target_link_libraries(YoloStudio PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql
    debug     "${OPENCV_ROOT}/x64/vc16/lib/opencv_world4100d.lib"
    optimized "${OPENCV_ROOT}/x64/vc16/lib/opencv_world4100.lib"
)
target_include_directories(YoloStudio PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${OPENCV_ROOT}/include"
)

# 复制 OpenCV 运行时 DLL 到可执行文件目录（Debug 与 Release 都拷，按需加载）
foreach(dll opencv_world4100.dll opencv_world4100d.dll opencv_videoio_ffmpeg4100_64.dll opencv_videoio_msmf4100_64.dll)
    add_custom_command(TARGET YoloStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPENCV_ROOT}/x64/vc16/bin/${dll}"
            "$<TARGET_FILE_DIR:YoloStudio>"
    )
endforeach()
