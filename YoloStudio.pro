QT += core gui widgets sql

CONFIG += c++17
TARGET = YoloStudio
TEMPLATE = app

SOURCES += \
    main.cpp \
    src/MainWindow.cpp \
    src/ProjectManager.cpp \
    src/LabelIO.cpp \
    src/Database.cpp \
    src/PythonRunner.cpp \
    src/DashboardPage.cpp \
    src/AnnotationPage.cpp \
    src/AnnotationView.cpp \
    src/TrainingPage.cpp \
    src/ValidationPage.cpp \
    src/InferenceWorker.cpp \
    src/TrainingCurveWidget.cpp \
    src/ExportPage.cpp \
    src/PreprocessDialog.cpp

HEADERS += \
    src/MainWindow.h \
    src/ProjectManager.h \
    src/Annotation.h \
    src/LabelIO.h \
    src/Database.h \
    src/PythonRunner.h \
    src/TrainConfig.h \
    src/DashboardPage.h \
    src/AnnotationPage.h \
    src/AnnotationView.h \
    src/TrainingPage.h \
    src/ValidationPage.h \
    src/InferenceWorker.h \
    src/TrainingCurveWidget.h \
    src/ExportPage.h \
    src/PreprocessDialog.h

INCLUDEPATH += $$PWD/src
INCLUDEPATH += $$PWD/../deps/opencv4.10/include

# Debug/Release 各自链接对应的 OpenCV（d 后缀为 Debug 版）
CONFIG(debug, debug|release) {
    LIBS += -L$$PWD/../deps/opencv4.10/x64/vc16/lib -lopencv_world4100d
} else {
    LIBS += -L$$PWD/../deps/opencv4.10/x64/vc16/lib -lopencv_world4100
}

# 复制 OpenCV 运行时 DLL 到可执行文件目录（Windows 需 DLL 与 exe 同目录或加入 PATH）
win32 {
    COPY_DLL = $$clean_path($$PWD/../scripts/copy_dlls.bat)
    QMAKE_POST_LINK += $$shell_path($$COPY_DLL) $$shell_path($$OUT_PWD/debug) $$escape_expand(\\n\\t)
    QMAKE_POST_LINK += $$shell_path($$COPY_DLL) $$shell_path($$OUT_PWD/release) $$escape_expand(\\n\\t)
}
