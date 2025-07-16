#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mpi.h>
#include <omp.h>

#define MAX_WORD_LEN 100
#define HASH_SIZE 10007

typedef struct Word {
    char word[MAX_WORD_LEN];
    int count;
    struct Word *next;
} Word;

int hash(char *word) {
    int h = 0;
    for (int i = 0; word[i]; i++) {
        h = (h * 31 + word[i]) % HASH_SIZE;
    }
    return h;
}

void insert_word(Word **table, char *word, int count) {
    int index = hash(word);
    Word *entry = table[index];

    while (entry) {
        if (strcmp(entry->word, word) == 0) {
            entry->count += count;
            return;
        }
        entry = entry->next;
    }

    Word *new_word = (Word *)malloc(sizeof(Word));
    strcpy(new_word->word, word);
    new_word->count = count;
    new_word->next = table[index];
    table[index] = new_word;
}

void clean_word(char *word) {
    int j = 0;
    for (int i = 0; word[i]; i++) {
        if (isalpha(word[i]))
            word[j++] = tolower(word[i]);
    }
    word[j] = '\0';
}

void save_results(Word **table, const char *filename, double exec_time) {
    FILE *f = fopen(filename, "w");
    if (!f) return;

    fprintf(f, "Execution Time: %.4f seconds\n\n", exec_time);

    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = table[i];
        while (entry) {
            fprintf(f, "%s: %d\n", entry->word, entry->count);
            entry = entry->next;
        }
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    double start_time = MPI_Wtime();

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2) {
        if (rank == 0) printf("Usage: %s input.txt\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    char *filename = argv[1];
    MPI_File file;
    MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &file);

    MPI_Offset file_size;
    MPI_File_get_size(file, &file_size);
    file_size--;  // remove EOF

    MPI_Offset chunk_start = rank * (file_size / size);
    MPI_Offset chunk_end = (rank == size - 1) ? file_size : (rank + 1) * (file_size / size);
    MPI_Offset chunk_size = chunk_end - chunk_start;

    // Read chunk
    char *buffer = malloc(chunk_size + MAX_WORD_LEN);  // buffer for overflow word
    MPI_File_read_at(file, chunk_start, buffer, chunk_size, MPI_CHAR, MPI_STATUS_IGNORE);
    buffer[chunk_size] = '\0';

    // Extend to read full word at the end
    if (rank != size - 1) {
        int i = 0;
        char c;
        while (chunk_end + i < file_size && i < MAX_WORD_LEN - 1) {
            MPI_File_read_at(file, chunk_end + i, &c, 1, MPI_CHAR, MPI_STATUS_IGNORE);
            buffer[chunk_size + i] = c;
            if (isspace(c)) break;
            i++;
        }
        buffer[chunk_size + i + 1] = '\0';
    }

    MPI_File_close(&file);

    // Allocate per-thread local tables
    int num_threads = 2;
    omp_set_num_threads(num_threads);
    Word *local_tables[2][HASH_SIZE] = {{0}};

    // Parse and count words in parallel
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        Word **local_table = local_tables[tid];

        char word[MAX_WORD_LEN];
        int j = 0;
        #pragma omp for
        for (int i = 0; i < (int)strlen(buffer); i++) {
            if (isalpha(buffer[i])) {
                if (j < MAX_WORD_LEN - 1)
                    word[j++] = tolower(buffer[i]);
            } else {
                if (j > 0) {
                    word[j] = '\0';
                    insert_word(local_table, word, 1);
                    j = 0;
                }
            }
        }
        if (j > 0) {
            word[j] = '\0';
            insert_word(local_table, word, 1);
        }
    }

    // Merge local thread tables
    Word *merged_table[HASH_SIZE] = {0};
    for (int t = 0; t < num_threads; t++) {
        for (int i = 0; i < HASH_SIZE; i++) {
            Word *entry = local_tables[t][i];
            while (entry) {
                insert_word(merged_table, entry->word, entry->count);
                entry = entry->next;
            }
        }
    }

    // Serialize merged table
    int local_total = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = merged_table[i];
        while (entry) {
            local_total++;
            entry = entry->next;
        }
    }

    char *flat_words = malloc(local_total * MAX_WORD_LEN);
    int *counts = malloc(sizeof(int) * local_total);
    int index = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = merged_table[i];
        while (entry) {
            strncpy(flat_words + index * MAX_WORD_LEN, entry->word, MAX_WORD_LEN);
            counts[index] = entry->count;
            index++;
            entry = entry->next;
        }
    }

    if (rank == 0) {
        Word *global_table[HASH_SIZE] = {0};
        for (int i = 0; i < local_total; i++) {
            insert_word(global_table, flat_words + i * MAX_WORD_LEN, counts[i]);
        }

        for (int src = 1; src < size; src++) {
            int recv_count;
            MPI_Recv(&recv_count, 1, MPI_INT, src, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            char *recv_words = malloc(recv_count * MAX_WORD_LEN);
            int *recv_counts = malloc(sizeof(int) * recv_count);
            MPI_Recv(recv_words, recv_count * MAX_WORD_LEN, MPI_CHAR, src, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(recv_counts, recv_count, MPI_INT, src, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (int i = 0; i < recv_count; i++) {
                insert_word(global_table, recv_words + i * MAX_WORD_LEN, recv_counts[i]);
            }

            free(recv_words);
            free(recv_counts);
        }

        double end_time = MPI_Wtime();
        save_results(global_table, "mpi_openmp_output.txt", end_time - start_time);
        printf("Hybrid MPI + OpenMP Word Count Completed in %.4f seconds\n", end_time - start_time);

    } else {
        MPI_Send(&local_total, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send(flat_words, local_total * MAX_WORD_LEN, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
        MPI_Send(counts, local_total, MPI_INT, 0, 2, MPI_COMM_WORLD);
    }

    free(flat_words);
    free(counts);
    free(buffer);

    MPI_Finalize();
    return 0;
}
