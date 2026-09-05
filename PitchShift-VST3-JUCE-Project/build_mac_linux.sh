#!/bin/bash
set -e

echo "====================================================================="
echo "   Compilador Automático PitchShift VST3 (macOS / Linux)"
echo "====================================================================="

# Comprobar CMake
if ! command -v cmake &> /dev/null; then
    echo "[ERROR] CMake no está instalado."
    echo "  - En macOS: brew install cmake"
    echo "  - En Ubuntu/Debian: sudo apt update && sudo apt install -y cmake build-essential libasound2-dev libx11-dev"
    exit 1
fi

echo "[1/2] Configurando CMake y descargando JUCE 7..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo "[2/2] Compilando plugin VST3..."
cmake --build build --config Release --parallel

echo ""
echo "====================================================================="
echo "   ¡COMPILACIÓN EXITOSA!"
echo "====================================================================="
echo "El plugin compilado se encuentra en:"
echo "  build/PitchShiftVST3_artefacts/Release/VST3/"
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo ""
    echo "Para instalarlo en tu Mac:"
    echo "  cp -R "build/PitchShiftVST3_artefacts/Release/VST3/PitchShift Guitar & Bass Transposer.vst3" ~/Library/Audio/Plug-Ins/VST3/"
fi
echo "====================================================================="
