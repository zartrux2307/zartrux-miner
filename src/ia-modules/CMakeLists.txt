cmake_minimum_required(VERSION 3.15)
project(zartrux_ia_modules LANGUAGES NONE)

# Detectar Python para instalar scripts y usar herramientas si es necesario
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Archivos por categoría a instalar
set(LAUNCHER_DIR     launcher)
set(BRIDGE_DIR       bridge)
set(TRAINING_DIR     training)
set(ANALYTICS_DIR    analytics)
set(EVALUATION_DIR   evaluation)
set(UTILS_DIR        utils)
set(LOGS_DIR         logs)
set(DATA_DIR         data)
set(MODELS_DIR       models)

# Instalación de scripts por módulos
install(DIRECTORY ${BRIDGE_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/ia/bridge
    FILES_MATCHING PATTERN "*.py"
)

install(DIRECTORY ${TRAINING_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/ia/training
    FILES_MATCHING PATTERN "*.py"
)

install(DIRECTORY ${ANALYTICS_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/ia/analytics
    FILES_MATCHING PATTERN "*.py"
)

install(DIRECTORY ${EVALUATION_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/ia/evaluation
    FILES_MATCHING PATTERN "*.py"
)

install(DIRECTORY ${UTILS_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/ia/utils
    FILES_MATCHING PATTERN "*.py"
)

install(DIRECTORY ${LAUNCHER_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/ia/launcher
    FILES_MATCHING PATTERN "*.py"
)

# Logs y datos históricos
install(DIRECTORY ${LOGS_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/ia/logs
    FILES_MATCHING PATTERN "*.log" PATTERN "*.txt" PATTERN "*.csv"
)

# Data sets de entrenamiento
install(DIRECTORY ${DATA_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/ia/data
    FILES_MATCHING PATTERN "*.csv"
)

# Modelos IA (joblib, onnx, etc.)
install(DIRECTORY ${MODELS_DIR}/
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/zartrux/ia/models
    FILES_MATCHING PATTERN "*.joblib" PATTERN "*.onnx" PATTERN "*.tflite"
)

message(STATUS "✅ Módulo 'ia-modules' instalado correctamente.")
