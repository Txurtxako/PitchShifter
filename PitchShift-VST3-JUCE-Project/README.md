# PitchShift VST3 - Guitar & Bass Transposer

Plugin VST3 / AU nativo en C++ para subir y bajar semitonos en guitarra y bajo en tiempo real, con algoritmos optimizados para acordes polifónicos y notas subsónicas de bajo (hasta Si grave B0 a 30Hz).

---

## ⚡ OPCIÓN 1: Compilación 100% en la Nube (Sin instalar nada en tu PC)

Puedes hacer que los servidores de **GitHub Actions** compilen el plugin VST3 por ti de forma totalmente gratuita:

1. Crea una cuenta gratuita en **GitHub.com** (si no tienes una).
2. Crea un nuevo repositorio (puede ser privado).
3. Sube todos los archivos de este archivo ZIP descomprimido a tu repositorio.
4. GitHub detectará automáticamente el archivo `.github/workflows/build-vst3.yml`.
5. Haz clic en la pestaña **"Actions"** arriba en tu repositorio.
6. Verás la tarea **"Build VST3 Plugins"** ejecutándose en máquinas reales de Windows, macOS y Linux.
7. Al cabo de 2-3 minutos, entra en la ejecución completada y en la sección **Artifacts** descarga:
   - `PitchShift_Windows_x64_VST3.zip` (para Windows)
   - `PitchShift_macOS_Universal_VST3.zip` (para Mac)
   - `PitchShift_Linux_x64_VST3.zip` (para Linux)
8. ¡Listo! Ya tienes el plugin compilado sin haber tenido que configurar compiladores.

---

## 💻 OPCIÓN 2: Compilación en tu PC (1 Clic)

### En Windows:
1. Asegúrate de tener instalado:
   - **CMake** (https://cmake.org/download/ - marca la casilla "Add CMake to system PATH").
   - **Visual Studio 2022 Community** (gratuito) con el paquete "Desarrollo para el escritorio con C++".
2. Haz doble clic sobre el archivo **`build_windows.bat`**.
3. El script descargará la librería JUCE y compilará el archivo `.vst3` automáticamente en modo optimizado.
4. Al terminar, el archivo estará en:
   `build\PitchShiftVST3_artefacts\Release\VST3\PitchShift Guitar & Bass Transposer.vst3`

### En macOS:
1. Abre la Terminal en la carpeta del proyecto.
2. Asegúrate de tener Xcode y CMake (`brew install cmake`).
3. Ejecuta:
   ```bash
   chmod +x build_mac_linux.sh
   ./build_mac_linux.sh
   ```
4. El archivo `.vst3` se creará en:
   `build/PitchShiftVST3_artefacts/Release/VST3/`

---

## 📁 Dónde instalar el archivo .vst3 para que tu DAW lo reconozca

Copia la carpeta/archivo `PitchShift Guitar & Bass Transposer.vst3` en la ruta correspondiente a tu sistema:

- **Windows 10 / 11:**
  `C:\Program Files\Common Files\VST3\`
- **macOS:**
  `~/Library/Audio/Plug-Ins/VST3/`  (o `/Library/Audio/Plug-Ins/VST3/`)
- **Linux:**
  `~/.vst3/`  (o `/usr/lib/vst3/`)

---

## 🎛️ Cómo usarlo en tu DAW (Reaper, FL Studio, Ableton, Cubase...)

1. Abre tu DAW y ejecuta un **"Scan / Rescan Plugins"**.
2. En tu pista de guitarra o bajo, inserta **"PitchShift Guitar & Bass Transposer"** como **PRIMER plugin de la cadena de efectos** (antes de tu emulador de amplificador como Neural DSP Archetype, Helix Native, Amplitube, Bias FX o Guitar Rig).
3. Selecciona el modo (**Guitar** o **Bass**) y ajusta los semitonos deseados (ej. -2 para D Standard, -5 para Si estándar, +1 para cejilla virtual en traste 1).
