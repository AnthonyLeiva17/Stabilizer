cmake_minimum_required(VERSION 3.10)

# Nombre del proyecto
project(Stabilizer)

# Establecer el estándar de C++
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Encontrar la librería OpenCV
find_package(OpenCV REQUIRED)

# Incluir los directorios de cabecera
include_directories(${OpenCV_INCLUDE_DIRS})

# Añadir el ejecutable
add_executable(stabilizer stabilizer.cpp)

# Enlazar OpenCV con el ejecutable
target_link_libraries(stabilizer ${OpenCV_LIBS})

# Mensaje para indicar que OpenCV se encontró correctamente
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
