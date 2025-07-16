#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <omp.h>

#define MAX_WORD_LEN 100
#define HASH_SIZE 10007
#define MAX_WORDS 10000000
#define MAX_THREADS 16

typedef struct Word {
    char word[MAX_WORD_LEN];
    int count;
    struct Word *next;
} Word;

Word* global_hash_table[HASH_SIZE];
Word* thread_local_tables[MAX_THREADS][HASH_SIZE];

// Hash function
int hash(char *word) {
    int h = 0;
    for (int i = 0; word[i]; i++)
        h = (h * 31 + word[i]) % HASH_SIZE;
    return h;
}

// Insert into thread local hash table
void insert_word_local(Word **table, char *word) {
    int index = hash(word);
    Word *entry = table[index];

    while (entry) {
        if (strcmp(entry->word, word) == 0) {
            entry->count++;
            return;
        }
        entry = entry->next;
    }

    Word *new_word = (Word *)malloc(sizeof(Word));
    strcpy(new_word->word, word);
    new_word->count = 1;
    new_word->next = table[index];
    table[index] = new_word;
}

// Merge one threads hash table into global hash table
void merge_local_to_global(Word **local) {
    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = local[i];
        while (entry) {
            Word *next = entry->next;

            int index = hash(entry->word);
            Word *g_entry = global_hash_table[index];
            int found = 0;
            while (g_entry) {
                if (strcmp(g_entry->word, entry->word) == 0) {
                    g_entry->count += entry->count;
                    found = 1;
                    break;
                }
                g_entry = g_entry->next;
            }

            if (!found) {
                Word *new_word = (Word *)malloc(sizeof(Word));
                strcpy(new_word->word, entry->word);
                new_word->count = entry->count;
                new_word->next = global_hash_table[index];
                global_hash_table[index] = new_word;
            }

            free(entry);
            entry = next;
        }
    }
}

// Lowercase, remove punctuation
void clean_word(char *word) {
    int j = 0;
    for (int i = 0; word[i]; i++) {
        if (isalpha(word[i])) {
            word[j++] = tolower(word[i]);
        }
    }
    word[j] = '\0';
}

// Load all words into array
int load_words(char *filename, char words[][MAX_WORD_LEN]) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("File open failed");
        return -1;
    }

    int count = 0;
    char buffer[MAX_WORD_LEN];
    while (fscanf(file, "%s", buffer) != EOF && count < MAX_WORDS) {
        clean_word(buffer);
        if (strlen(buffer) > 0) {
            strcpy(words[count++], buffer);
        }
    }

    fclose(file);
    return count;
}

// Save final global hash table
void save_results() {
    FILE *fp = fopen("word_counts_Thread4.txt", "w");
    if (!fp) {
        perror("Failed to open output file");
        return;
    }

    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = global_hash_table[i];
        while (entry) {
            fprintf(fp, "%s: %d\n", entry->word, entry->count);
            entry = entry->next;
        }
    }

    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s input.txt\n", argv[0]);
        return 1;
    }

    int num_threads = 4;
    omp_set_num_threads(num_threads);

    char (*words)[MAX_WORD_LEN] = malloc(sizeof(char[MAX_WORD_LEN]) * MAX_WORDS);
    if (!words) {
        printf("Memory allocation failed\n");
        return 1;
    }

    int total_words = load_words(argv[1], words);
    if (total_words < 0) return 1;

    int *word_counts = calloc(num_threads, sizeof(int));
    double *thread_times = calloc(num_threads, sizeof(double));

    double start_time = omp_get_wtime();

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        Word **local_table = thread_local_tables[tid];

        double local_start = omp_get_wtime();

        #pragma omp for
        for (int i = 0; i < total_words; i++) {
            insert_word_local(local_table, words[i]);
            word_counts[tid]++;
        }

        double local_end = omp_get_wtime();
        thread_times[tid] = local_end - local_start;
    }

    // Merging thread-local tables into global table
    for (int t = 0; t < num_threads; t++) {
        merge_local_to_global(thread_local_tables[t]);
    }

    double end_time = omp_get_wtime();
    double duration = end_time - start_time;


    printf("Word count complete. Time taken: %.4f seconds with %d threads\n\n", duration, num_threads);
    for (int i = 0; i < num_threads; i++) {
        printf("Thread %d processed %d words in %.4f seconds\n", i, word_counts[i], thread_times[i]);
    }

    save_results();

    // Log thread performance
    FILE *log = fopen("performance_log_thread4.txt", "w");
    if (log) {
        fprintf(log, "Execution time: %.4f seconds\n", duration);
        fprintf(log, "Threads used: %d\n\n", num_threads);
        for (int i = 0; i < num_threads; i++) {
            fprintf(log, "Thread %d processed %d words in %.4f seconds\n",
                    i, word_counts[i], thread_times[i]);
        }
        fclose(log);
    } else {
        perror("Failed to open performance log file");
    }

    free(words);
    free(word_counts);
    free(thread_times);

    return 0;
}
