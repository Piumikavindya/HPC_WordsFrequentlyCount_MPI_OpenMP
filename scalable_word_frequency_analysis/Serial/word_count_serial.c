#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_WORD_LEN 100
#define HASH_SIZE 10007
#define MAX_WORDS 10000000

typedef struct Word {
    char word[MAX_WORD_LEN];
    int count;
    struct Word *next;
} Word;

Word* global_hash_table[HASH_SIZE];

// Hash function
int hash(char *word) {
    int h = 0;
    for (int i = 0; word[i]; i++)
        h = (h * 31 + word[i]) % HASH_SIZE;
    return h;
}

// Insert into global hash table
void insert_word(char *word) {
    int index = hash(word);
    Word *entry = global_hash_table[index];

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
    new_word->next = global_hash_table[index];
    global_hash_table[index] = new_word;
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
int load_words_and_insert(char *filename) {
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
            insert_word(buffer);
            count++;
        }
    }

    fclose(file);
    return count;
}

// Save final global hash table
void save_results() {
    FILE *fp = fopen("word_counts_serial.txt", "w");
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

    double start_time = (double)clock() / CLOCKS_PER_SEC;

    int total_words = load_words_and_insert(argv[1]);
    if (total_words < 0) return 1;

    double end_time = (double)clock() / CLOCKS_PER_SEC;
    double duration = end_time - start_time;

    printf("Word count complete. Time taken: %.4f seconds\n", duration);
    printf("Total words processed: %d\n", total_words);

    save_results();

    // Log performance
    FILE *log = fopen("performance_log_serial.txt", "w");
    if (log) {
        fprintf(log, "Execution time: %.4f seconds\n", duration);
        fprintf(log, "Total words processed: %d\n", total_words);
        fclose(log);
    } else {
        perror("Failed to open performance log file");
    }

    return 0;
}
