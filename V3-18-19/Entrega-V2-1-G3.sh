#!/bin/bash

# //Grupo 1 

echo "//Programa llamado: programaEntrega-V2-1 
60 
1 
ADD 5 4 
WRITE 50
ADD 7 -16
WRITE 49
TRAP 4 
TRAP 4 
TRAP 4 
TRAP 4 
TRAP 3 
" > programaEntrega-V2-1

echo "//Programa llamado: programaEntrega-V2-2 
41 
123 
ADD 120 -6
SHIFT -12
INC 2047
INC 11
SHIFT -12
WRITE 8
ADD 6 -3
WRITE 40
TRAP 4 
TRAP 4
INC -1
ZJUMP 2
JUMP -3
TRAP 4 
READ 40
TRAP 4 
INC -1
ZJUMP 2
JUMP -3
TRAP 4 
READ 40
TRAP 4 
TRAP 4
TRAP 3 
" > programaEntrega-V2-2


echo "//Programa llamado: programaEntrega-V2-3 
41 
123 
ADD 120 -6
SHIFT -12
INC 2047
INC 72
SHIFT -12
WRITE 8
ADD 3 0
WRITE 40
TRAP 4
TRAP 4 
INC 1
ZJUMP 2
JUMP -3
TRAP 4 
READ 40
TRAP 4 
INC -1
ZJUMP 2
JUMP -3
TRAP 4 
READ 40
TRAP 4 
TRAP 4
TRAP 3 
" > programaEntrega-V2-3


./Simulator a programaEntrega-V2-1 programaEntrega-V2-2 programaEntrega-V2-3 2>&1 | head -n 1500 > V2-4-salidaSimulador-Ivan.log

