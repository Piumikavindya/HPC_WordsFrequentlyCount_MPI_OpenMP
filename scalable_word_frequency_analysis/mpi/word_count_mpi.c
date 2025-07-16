#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mpi.h>
#include <unistd.h> // for getcwd()

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

void save_results(Word **table, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Error: Could not open file %s for writing results.\n", filename);
        return;
    }
    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = table[i];
        while (entry) {
            fprintf(f, "%s: %d\n", entry->word, entry->count);
            entry = entry->next;
        }
    }
    fclose(f);
}

void save_execution_time(double time, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (f) {
        fprintf(f, "Execution Time: %.6f seconds\n", time);
        fclose(f);
        printf("Execution time saved to file: %s\n", filename);
    } else {
        fprintf(stderr, "Error: Could not open file %s for writing execution time.\n", filename);
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double start_time = MPI_Wtime();

    if (argc < 2) {
        if (rank == 0) printf("Usage: %s input.txt\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    MPI_File file;
    MPI_Offset file_size;

    MPI_File_open(MPI_COMM_WORLD, argv[1], MPI_MODE_RDONLY, MPI_INFO_NULL, &file);
    MPI_File_get_size(file, &file_size);

    MPI_Offset chunk_size = file_size / size;
    MPI_Offset offset = rank * chunk_size;

    // Each rank reads its chunk + MAX_WORD_LEN extra to avoid word truncation
    char *buffer = malloc(chunk_size + MAX_WORD_LEN + 1);
    MPI_File_read_at(file, offset, buffer, chunk_size + MAX_WORD_LEN, MPI_CHAR, MPI_STATUS_IGNORE);
    buffer[chunk_size + MAX_WORD_LEN] = '\0';

    MPI_File_close(&file);

    // Tokenize words and count locally
    Word *local_table[HASH_SIZE] = {0};
    char temp[MAX_WORD_LEN];
    int i = 0, j = 0;
    while (buffer[i]) {
        if (isalpha(buffer[i])) {
            temp[j++] = tolower(buffer[i]);
        } else if (j > 0) {
            temp[j] = '\0';
            insert_word(local_table, temp, 1);
            j = 0;
        }
        i++;
    }
    if (j > 0) {
        temp[j] = '\0';
        insert_word(local_table, temp, 1);
    }
    free(buffer);

    // Serialize local table
    int local_count = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = local_table[i];
        while (entry) {
            local_count++;
            entry = entry->next;
        }
    }

    char *flat_words = malloc(local_count * MAX_WORD_LEN);
    int *flat_counts = malloc(local_count * sizeof(int));

    int index = 0;
    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = local_table[i];
        while (entry) {
            strncpy(flat_words + index * MAX_WORD_LEN, entry->word, MAX_WORD_LEN);
            flat_counts[index] = entry->count;
            index++;
            entry = entry->next;
        }
    }

    // Gather word counts
    int *recv_counts = NULL, *displs = NULL;
    int *word_recvcounts = NULL, *word_displs = NULL;
    int total_recv = 0;

    if (rank == 0) {
        recv_counts = malloc(size * sizeof(int));
    }

    MPI_Gather(&local_count, 1, MPI_INT, recv_counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

    char *all_words = NULL;
    int *all_counts = NULL;

    if (rank == 0) {
        word_recvcounts = malloc(size * sizeof(int));
        word_displs = malloc(size * sizeof(int));
        displs = malloc(size * sizeof(int));

        displs[0] = 0;
        word_recvcounts[0] = recv_counts[0] * MAX_WORD_LEN;
        word_displs[0] = 0;
        total_recv = recv_counts[0];

        for (int i = 1; i < size; i++) {
            displs[i] = displs[i - 1] + recv_counts[i - 1];
            word_recvcounts[i] = recv_counts[i] * MAX_WORD_LEN;
            word_displs[i] = word_displs[i - 1] + word_recvcounts[i - 1];
            total_recv += recv_counts[i];
        }

        all_words = malloc(total_recv * MAX_WORD_LEN);
        all_counts = malloc(total_recv * sizeof(int));
    }

    MPI_Gatherv(flat_words, local_count * MAX_WORD_LEN, MPI_CHAR,
                all_words, word_recvcounts, word_displs, MPI_CHAR,
                0, MPI_COMM_WORLD);

    MPI_Gatherv(flat_counts, local_count, MPI_INT,
                all_counts, recv_counts, displs, MPI_INT,
                0, MPI_COMM_WORLD);

    if (rank == 0) {
        Word *global_table[HASH_SIZE] = {0};

        for (int i = 0; i < total_recv; i++) {
            insert_word(global_table, all_words + i * MAX_WORD_LEN, all_counts[i]);
        }

        save_results(global_table, "mpi_output_p4.txt");

        double end_time = MPI_Wtime();
        double elapsed = end_time - start_time;
        printf("MPI Word Count Completed in %.4f seconds\n", elapsed);

        save_execution_time(elapsed, "mpi_execution_time_p4.txt");

        free(recv_counts);
        free(displs);
        free(word_recvcounts);
        free(word_displs);
        free(all_words);
        free(all_counts);
    }

    free(flat_words);
    free(flat_counts);

    MPI_Finalize();
    return 0;
}
