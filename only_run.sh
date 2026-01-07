!/bin/bash

# Verifica se foi passado o nome do programa
if [ -z "$1" ]; then
    echo "Erro: Indique o nome do programa."
    echo "Uso: ./so_correr.sh <nome_programa> [argumentos]"
    exit 1
fi

PROGRAMA=$1
shift

echo "A correr a simulação '$PROGRAMA' (sem recompilar)..."

# A flag --no-build salta o passo de build do CMake/Waf
./ns3 run "$PROGRAMA" --no-build -- "$@"