@echo off
REM Copy OpenCV runtime DLLs to the given directory.
REM Usage: copy_dlls.bat <target_dir>
REM deps\opencv4.10 lives beside the Qt project (workspace root) or one level up.
setlocal
if "%~1"=="" (
    echo Usage: copy_dlls.bat ^<target_dir^>
    exit /b 1
)
set "SRC=%~dp0..\deps\opencv4.10\x64\vc16\bin"
if not exist "%SRC%\opencv_world4100.dll" set "SRC=%~dp0..\..\deps\opencv4.10\x64\vc16\bin"
if not exist "%SRC%\opencv_world4100.dll" (
    echo [WARN] OpenCV DLLs not found. Expected deps\opencv4.10\x64\vc16\bin
    exit /b 1
)
copy /y "%SRC%\opencv_world4100.dll" "%~1\" >nul
copy /y "%SRC%\opencv_world4100d.dll" "%~1\" >nul
copy /y "%SRC%\opencv_videoio_ffmpeg4100_64.dll" "%~1\" >nul
copy /y "%SRC%\opencv_videoio_msmf4100_64.dll" "%~1\" >nul
echo Copied OpenCV DLLs to "%~1"
endlocal
