@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0.."

echo ============================================================
echo   YoloStudio - training environment setup
echo   Creates isolated runtime\ venv with CUDA PyTorch
echo ============================================================
echo.

REM ---------- 1. Locate a Python interpreter ----------
set "PY_CMD="
for %%v in (3.12 3.11 3.13 3.14) do (
    py -%%v -c "import sys" >nul 2>nul
    if !errorlevel! equ 0 (
        set "PY_CMD=py -%%v"
        echo [OK] Using Python %%v
        goto :found
    )
)
python -c "import sys" >nul 2>nul
if !errorlevel! equ 0 (
    set "PY_CMD=python"
    echo [OK] Using system python
    goto :found
)
echo [ERROR] Python not found. Install Python 3.11 - 3.14 first.
echo         Download: https://www.python.org/downloads/
pause
exit /b 1

:found

REM ---------- 2. Create virtual environment ----------
set "VENVPY=runtime\Scripts\python.exe"
if not exist "%VENVPY%" (
    echo Creating virtual environment runtime\ ...
    %PY_CMD% -m venv runtime
    if errorlevel 1 (
        echo [ERROR] Failed to create venv.
        pause
        exit /b 1
    )
)

echo.
echo ---------- 3. Install PyTorch (CUDA 12.6, for RTX 40-series) ----------
"%VENVPY%" -m pip install --upgrade pip
"%VENVPY%" -m pip install torch torchvision --index-url https://download.pytorch.org/whl/cu126
if errorlevel 1 (
    echo [WARN] CUDA install failed, falling back to CPU build...
    "%VENVPY%" -m pip install torch torchvision
)

echo.
echo ---------- 4. Install ultralytics ----------
"%VENVPY%" -m pip install ultralytics

echo.
echo ---------- 5. Verify GPU ----------
"%VENVPY%" -c "import torch; print('torch', torch.__version__); print('cuda_available', torch.cuda.is_available()); print('gpu', torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'NONE')"

echo.
echo ============================================================
echo   Done!
echo   - If cuda_available=False: upgrade NVIDIA driver and rerun.
echo   - RTX 50-series machines: edit this file, change cu126 to cu128.
echo ============================================================
pause
exit /b 0
