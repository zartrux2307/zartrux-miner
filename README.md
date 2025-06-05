# Zartrux Miner

Zartrux Miner es un prototipo de minero escrito en C++20 que integra modulos de 
IA, comunicación por ZeroMQ y monitorización a través de Prometheus. Incluye un
backend web en `zarbackend/` y utilidades para entrenamiento de modelos bajo
`ia-modules/`.

## Requisitos

- CMake >= 3.20
- Compilador C++20 (GCC, Clang o MSVC)
- [Qt 6](https://www.qt.io/) y [vcpkg](https://github.com/microsoft/vcpkg) para
  resolver dependencias (OpenSSL, fmt, nlohmann_json, cpr, yaml-cpp, etc.)

## Compilación rapida

```bash
# Clonar el repositorio
git clone https://github.com/zartrux2307/zartrux-miner.git
cd zartrux-miner

# Crear directorio de build
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
cmake --build .
```

El ejecutable final `zartrux-miner` se generará dentro de `build/`.
Para personalizar la ejecución modifica `src/config/miner_config.json`.

## Documentación adicional

En la carpeta `docs/` se detallan las arquitecturas disponibles y el modo
distribuido. Consulta `ZARTRUX_DISTRIBUTED.md` para una descripción de la
arquitectura distribuida y los scripts de ejemplo.# zartrux-miner

This repository relies on the [RandomX](https://github.com/tevador/RandomX) library.
The source is expected under `src/randomxzar`. If the directory is empty, fetch
it with:

```bash
# Clone submodules
git submodule update --init --recursive
# or clone manually
git clone https://github.com/tevador/RandomX.git src/randomxzar
```

After obtaining the RandomX sources, proceed with the usual build steps using
CMake.
This directory should contain the RandomX source code used by Zartrux Miner.
Since the sources are not included, fetch them with:

```bash
git clone https://github.com/tevador/RandomX.git src/randomxzar
```

Alternatively, initialize the submodule if `.gitmodules` is configured:

```bash
git submodule update --init --recursive
```