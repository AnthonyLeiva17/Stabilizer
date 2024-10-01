#!/bin/bash

# Ruta a la carpeta con los videos
VIDEO_FOLDER="./videos/"
# Nombre del ejecutable de tu programa
EXECUTABLE="./build/stabilizer"

# Lista de algoritmos (1 = LK Sparse, 2 = LK Dense, 3 = Farneback)
ALGORITHMS=("1" "2" "3")

# Repetir la ejecución para cada video 5 veces
for video in "$VIDEO_FOLDER"*.avi; do
    for algorithm in "${ALGORITHMS[@]}"; do
        for ((i=1;i<=5;i++)); do
            echo "Ejecutando iteración $i/5 con video $video usando algoritmo $algorithm"
            $EXECUTABLE "$video" "$algorithm"
        done
    done
done

