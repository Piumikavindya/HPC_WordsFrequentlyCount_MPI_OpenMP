#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_WORD_LEN 100
#define HASH_SIZE 10007

typedef struct Word {
    char word[MAX_WORD_LEN];
    int count;
    struct Word *next;
} Word;

int hash(const char *word) {
    int h = 0;
    for (int i = 0; word[i]; i++) {
        h = (h * 31 + word[i]) % HASH_SIZE;
    }
    return h;
}

void insert_word(Word **table, const char *word, int count) {
    int index = hash(word);
    Word *entry = table[index];
    while (entry) {
        if (strcmp(entry->word, word) == 0) {
            entry->count = count;
            return;
        }
        entry = entry->next;
    }

    Word *new_word = malloc(sizeof(Word));
    strcpy(new_word->word, word);
    new_word->count = count;
    new_word->next = table[index];
    table[index] = new_word;
}

int get_count(Word **table, const char *word) {
    int index = hash(word);
    Word *entry = table[index];
    while (entry) {
        if (strcmp(entry->word, word) == 0) {
            return entry->count;
        }
        entry = entry->next;
    }
    return 0;
}

void load_counts(const char *filename, Word **table) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Could not open file: %s\n", filename);
        exit(1);
    }

    char word[MAX_WORD_LEN];
    int count;

    while (fscanf(file, "%s: %d", word, &count) == 2) {
        insert_word(table, word, count);
    }

    fclose(file);
}

double calculate_rmse(Word **serial_table, Word **other_table) {
    double sum = 0.0;
    int total_words = 0;

    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = serial_table[i];
        while (entry) {
            int serial_count = entry->count;
            int other_count = get_count(other_table, entry->word);
            double diff = serial_count - other_count;
            sum += diff * diff;
            total_words++;
            entry = entry->next;
        }
    }

    return (total_words == 0) ? 0.0 : sqrt(sum / total_words);
}

void free_table(Word **table) {
    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = table[i];
        while (entry) {
            Word *next = entry->next;
            free(entry);
            entry = next;
        }
    }
}

int main() {
    Word *serial_table[HASH_SIZE] = {0};
    Word *openmp_table_t2[HASH_SIZE] = {0};
    Word *openmp_table_t4[HASH_SIZE] = {0};
    Word *mpi_table_p2[HASH_SIZE] = {0};
    Word *mpi_table_p4[HASH_SIZE] = {0};
    Word *hybrid_table[HASH_SIZE] = {0};

    // Load data
    load_counts("../Serial/word_counts_serial.txt", serial_table);
    load_counts("../openmp/word_counts_Thread2.txt", openmp_table_t2);
    load_counts("../openmp/word_counts_Thread4.txt", openmp_table_t4);
    load_counts("../mpi/mpi_output_p2.txt", mpi_table_p2);
    load_counts("../mpi/mpi_output_p4.txt", mpi_table_p4);
    load_counts("../hybrid/mpi_openmp_output.txt", hybrid_table);

    // Calculate RMSEs    load_counts("../mpi/mpi_output.txt", mpi_table);
    double rmse_openmp_t2 = calculate_rmse(serial_table, openmp_table_t2);
    double rmse_openmp_t4 = calculate_rmse(serial_table, openmp_table_t4);
    double rmse_mpi_p2 = calculate_rmse(serial_table, mpi_table_p2);
    double rmse_mpi_p4 = calculate_rmse(serial_table, mpi_table_p4);
    double rmse_hybrid = calculate_rmse(serial_table, hybrid_table);

    // Save to accuracy.txt
    FILE *fout = fopen("accuracy.txt", "w");
    if (!fout) {
        perror("Cannot write to accuracy.txt");
        return 1;
    }

    fprintf(fout, "Accuracy Comparison (RMSE w.r.t Serial Version)\n");
    fprintf(fout, "-----------------------------------------------\n");
    fprintf(fout, "OpenMP T2   RMSE: %.6f\n", rmse_openmp_t2);
    fprintf(fout, "OpenMP T4  RMSE: %.6f\n", rmse_openmp_t4);
    fprintf(fout, "MPI P2     RMSE: %.6f\n", rmse_mpi_p2);
    fprintf(fout, "MPI P4     RMSE: %.6f\n", rmse_mpi_p4);
    fprintf(fout, "Hybrid   RMSE: %.6f\n", rmse_hybrid);
    fclose(fout);

    printf("Accuracy metrics saved to accuracy/accuracy.txt\n");

    // Clean up
    free_table(serial_table);
    free_table(openmp_table_t2);
    free_table(openmp_table_t4);
    free_table(mpi_table_p2);
    free_table(mpi_table_p4);
    free_table(hybrid_table);

    return 0;
}
