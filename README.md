# HPC_WordsFrequentlyCount_MPI_OpenMP

# README.md
# Scalable Word Frequency Analysis

This project implements a scalable word frequency analysis tool using multiple parallel programming models: Serial, OpenMP, MPI, and Hybrid (MPI+OpenMP). The goal is to efficiently count word frequencies in large text files and compare the performance and accuracy of different parallelization strategies.

## Project Structure

```
scalable_word_frequency_analysis/
├── Serial/
│   ├── word_count_serial.c
│   ├── input.txt
│   └── performance_log_serial.txt
├── openmp/
│   ├── word_count_openmp_v2.c
│   ├── input.txt
│   └── performance_log_thread*.txt
├── mpi/
│   ├── word_count_mpi.c
│   ├── input.txt
│   ├── mpi_output.txt
│   └── mpi_execution_time.txt
├── hybrid/
│   ├── word_count_hybrid.c
│   ├── input.txt
│   └── mpi_openmp_output.txt
├── accuracy/
│   ├── accuracy.c
│   ├── accuracy.txt
│   └── accuracy
└── README.md
```

## How to Build

Each implementation can be built using `gcc` or `mpicc` as appropriate.

### Serial

```sh
gcc -o word_count_serial word_count_serial.c
```

### OpenMP

```sh
gcc -fopenmp -o word_count_openmp_v2 word_count_openmp_v2.c
```

### MPI

```sh
mpicc -o word_count_mpi word_count_mpi.c
```

### Hybrid (MPI + OpenMP)

```sh
mpicc -fopenmp -o word_count_hybrid word_count_hybrid.c
```

### Accuracy Checker

```sh
gcc -o accuracy accuracy.c
```

## How to Run

### Serial

```sh
./word_count_serial input.txt
```

### OpenMP

```sh
./word_count_openmp_v2 input.txt
```

### MPI

```sh
mpirun -np <num_processes> ./word_count_mpi input.txt
```

### Hybrid

```sh
mpirun -np <num_processes> ./word_count_hybrid input.txt
```

### Accuracy Comparison

After running all implementations, run:

```sh
./accuracy
```

This will generate `accuracy.txt` comparing RMSE of OpenMP, MPI, and Hybrid results against the Serial version.

## Output Files

- `word_counts_serial.txt` / `mpi_output.txt` / `mpi_openmp_output.txt`: Word frequency results for each implementation.
- `performance_log_serial.txt` / `performance_log_thread*.txt` / `mpi_execution_time.txt`: Performance logs.
- `accuracy.txt`: RMSE accuracy comparison.

## Notes

- Input file should be placed in each implementation's folder as `input.txt`.
- The hash table size and maximum word length are defined in each source file.
- The project is designed for educational purposes to compare parallel programming models.

