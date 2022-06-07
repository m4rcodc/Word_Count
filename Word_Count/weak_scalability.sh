#!/bin/bash

# Delete older performance file
rm -rf results/weak_scalability.csv

# Compile the program
mpicc distribuited-game-of-life.c -o distribuited-game-of-life

# Add collumns names to CSV file
echo "TIME,PROCESSES,ROWS,COLS,GENERATIONS" >> results/weak_scalability.csv

# Weak Scalability Test
size=1000
for ((p=1; p<=24; p=p+1))
do
    echo "Start program with $p process and ($((size*p)), 24000) matrix"
    mpirun --allow-run-as-root -np $p --hostfile host distribuited-game-of-life $((size*p)) 24000 100 >> results/weak_scalability.csv
done

# Show performance
cat results/weak_scalability.csv
