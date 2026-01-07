!/bin/bash

# Verifica se foi passado o nome do programa
if [ -z "$1" ]; then
    echo "Erro: Por favor indique o nome do programa a correr."
    echo "Uso: ./build_and_run.sh <nome_programa> [argumentos]"
    echo "Exemplo: ./build_and_run.sh mesh1 --EnableFloodAndPrune=true"
    exit 1
fi

PROGRAMA=$1
shift # Remove o nome do programa da lista de argumentos para passar o resto

echo "A compilar o ns-3..."
./ns3 build

if [ $? -eq 0 ]; then
    echo "Compilação terminada com sucesso."
    echo "A correr a simulação '$PROGRAMA'..."
    
    # Executa o programa passando todos os argumentos extra ("$@")
    ./ns3 run "$PROGRAMA" -- "$@"
else
    echo "Erro na compilação. Corrija os erros e tente novamente."
    exit 1
fi