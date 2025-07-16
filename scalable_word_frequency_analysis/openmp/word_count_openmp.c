#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <omp.h>
#include <time.h>

#define MAX_WORD_LEN 100
#define HASH_SIZE 10007
#define MAX_WORDS 1000000

typedef struct Word {
    char word[MAX_WORD_LEN];
    int count;
    struct Word *next;
} Word;

Word* hash_table[HASH_SIZE];

// Hash function
int hash(char *word) {
    int h = 0;
    for (int i = 0; word[i]; i++)
        h = (h * 31 + word[i]) % HASH_SIZE;
    return h;
}

// Insert word into hash table
void insert_word(char *word) {
    int index = hash(word);
    int found = 0;

    #pragma omp critical
    {
        Word *entry = hash_table[index];
        while (entry) {
            if (strcmp(entry->word, word) == 0) {
                entry->count++;
                found = 1;
                break;
            }
            entry = entry->next;
        }

        if (!found) {
            Word *new_word = (Word *)malloc(sizeof(Word));
            strcpy(new_word->word, word);
            new_word->count = 1;
            new_word->next = hash_table[index];
            hash_table[index] = new_word;
        }
    }
}


// Lowercase and clean punctuation
void clean_word(char *word) {
    int j = 0;
    for (int i = 0; word[i]; i++) {
        if (isalpha(word[i])) {
            word[j++] = tolower(word[i]);
        }
    }
    word[j] = '\0';
}

// Store all words first
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

// Print result to file
void save_results() {
    FILE *fp = fopen("word_counts.txt", "w");
    if (!fp) {
        perror("Failed to open output file");
        return;
    }

    for (int i = 0; i < HASH_SIZE; i++) {
        Word *entry = hash_table[i];
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

    omp_set_num_threads(4);

    char (*words)[MAX_WORD_LEN] = malloc(sizeof(char[MAX_WORD_LEN]) * MAX_WORDS);
    if (!words) {
        printf("Memory allocation failed\n");
        return 1;
    }

    int total_words = load_words(argv[1], words);
    if (total_words < 0) return 1;

    double start_time = omp_get_wtime();

    int max_threads = omp_get_max_threads();
    int *word_counts = calloc(max_threads, sizeof(int));
    double *thread_times = calloc(max_threads, sizeof(double));

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();  
        double local_start = omp_get_wtime();

        #pragma omp for
        for (int i = 0; i < total_words; i++) {
            insert_word(words[i]);
            word_counts[tid]++; 
        }

        double local_end = omp_get_wtime();
        thread_times[tid] = local_end - local_start;
    }

    double end_time = omp_get_wtime();
    double duration = end_time - start_time;

    printf("Word count complete. Time taken: %.4f seconds\n", duration);
    printf("No Threads used: %d\n\n", omp_get_max_threads());
    for (int i = 0; i < max_threads; i++) {
        printf("No of Thread %d processed %d words in %.4f seconds\n", i, word_counts[i], thread_times[i]);
    }

    // Save results
    save_results();

      FILE *log = fopen("performance_log.txt", "w");
    if (log) {
        fprintf(log, "Execution time: %.4f seconds\n", duration);
        for (int i = 0; i < max_threads; i++) {
            fprintf(log, "Thread %d processed %d words in %.4f seconds\n", i, word_counts[i], thread_times[i]);
        }
        fclose(log);
    }

    free(words);
    free(word_counts);
    free(thread_times);
    return 0;
}
