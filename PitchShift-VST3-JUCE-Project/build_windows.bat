@echo off
echo =====================================================================
echo    Compilador Automatico PitchShift VST3 para Windows
echo =====================================================================
echo.

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake no esta instalado o no se encuentra en el PATH.
    echo Por favor descarga e instala CMake (marcando "Add to PATH"):
    echo https://cmake.org/download/
    echo.
    echo Asegurate tambien de tener instalado Visual Studio 2022 Community
    echo con la carga de trabajo "Desarrollo para el escritorio con C++".
    pause
    exit /b 1
)

echo [1/3] Creando carpeta de compilacion 'build'...
if not exist build mkdir build

echo [2/3] Configurando CMake y descargando JUCE 7 automaticamente...
cmake -B build -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] Fallo la configuracion con CMake.
    pause
    exit /b 1
)

echo [3/3] Compilando plugin VST3 en modo Release optimizado...
cmake --build build --config Release --parallel
if %errorlevel% neq 0 (
    echo [ERROR] Ocurrio un error durante la compilacion.
    pause
    exit /b 1
)

echo.
echo =====================================================================
echo   COMPILACION EXITOSA!
echo =====================================================================
echo Tu plugin VST3 listo para usar esta en:
echo   build\PitchShiftVST3_artefacts\Release\VST3\PitchShift Guitar ^& Bass Transposer.vst3
echo.
echo Para instalarlo en tu DAW:
echo   1. Copia la carpeta .vst3 anterior a:
echo      C:\Program Files\Common Files\VST3\
echo   2. Abre tu DAW (Reaper, FL Studio, Ableton, Cubase) y haz "Rescan Plugins".
echo =====================================================================
pause
