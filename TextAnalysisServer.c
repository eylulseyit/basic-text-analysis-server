#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define INPUT_CHARACTER_LIMIT 30
#define OUTPUT_CHARACTER_LIMIT 200
#define PORT_NUMBER 60000
#define LEVENSHTEIN_LIST_LIMIT 5
#define DICTIONARY_FILE "basic_english_2000.txt"
#define UPDATED_DICTIONARY_FILE "new.txt"

char **dictionary = NULL;
int wordCount = 0;

pthread_mutex_t dictLock;
pthread_mutex_t questionLock;

void saveDictionary() {
    FILE *file = fopen(UPDATED_DICTIONARY_FILE, "w");
    if (!file) {
        fprintf(stderr, "Error: Failed to open file '%s' for writing.\n", UPDATED_DICTIONARY_FILE);
        return;
    }

    pthread_mutex_lock(&dictLock);
    for (int i = 0; i < wordCount; i++) {
        fprintf(file, "%s\n", dictionary[i]);
    }
    pthread_mutex_unlock(&dictLock);

    fclose(file);
    printf("Dictionary saved to '%s'.\n", UPDATED_DICTIONARY_FILE);
}

void loadDictionary(char ***dict, int *wordCount) {
    FILE *file = fopen(DICTIONARY_FILE, "r");
    if (!file) {
        fprintf(stderr, "Error: Failed to open dictionary file '%s'.\n", DICTIONARY_FILE);
        exit(EXIT_FAILURE);
    }

    int capacity = 100;
    *dict = malloc(capacity * sizeof(char *));
    if (!*dict) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    char word[INPUT_CHARACTER_LIMIT];
    *wordCount = 0;

    while (fscanf(file, "%s", word) != EOF) {
        if (*wordCount >= capacity) {
            capacity *= 2;
            *dict = realloc(*dict, capacity * sizeof(char *));
        }
        (*dict)[*wordCount] = strdup(word);
        (*wordCount)++;
    }

    fclose(file);
    printf("Loaded %d words into dictionary.\n", *wordCount);
}

/*void trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    *(end + 1) = 0;
}*/


int levenshtein(const char *str1, const char *str2) {
    trim(str1);
    trim(str2);

    int length1 = strlen(str1);
    int length2 = strlen(str2);
    int matrix[length1 + 1][length2 + 1];

    for(int i = 0; i <= length1; i++) {
        matrix[i][0] = i;
    }

    for(int j = 0; j <= length2; j++) {
        matrix[0][j] = j;
    }

    for (int i = 1; i <= length1; i++) {
        for (int j = 1; j <= length2; j++) {
            int cost;
            if (str1[i - 1] == str2[j - 1]) {
                cost = 0;
            } else {
                cost = 1;
            }

            int delete_cost = matrix[i - 1][j] + 1;
            int insert_cost = matrix[i][j - 1] + 1;
            int replace_cost = matrix[i - 1][j - 1] + cost;

            if(delete_cost <= insert_cost && delete_cost <= replace_cost) {
                matrix[i][j] = delete_cost;
            }
            else if(insert_cost <= delete_cost && insert_cost <= replace_cost) {
                matrix[i][j] = insert_cost;
            }
            else {
                matrix[i][j] = replace_cost;
            }
        }
    }
    return matrix[length1][length2];
}

void addWordToDictionary(const char *word) {
    pthread_mutex_lock(&dictLock);
    dictionary = realloc(dictionary, (wordCount + 1) * sizeof(char *));
    dictionary[wordCount] = strdup(word);
    wordCount++;
    pthread_mutex_unlock(&dictLock);
}

struct WordProcessArgs {
    char word[INPUT_CHARACTER_LIMIT];
    int clientSocket;
};

void *processWord(void *args) {
    struct WordProcessArgs *wpArgs = (struct WordProcessArgs *)args;
    char *input_word = wpArgs->word;
    int clientSocket = wpArgs->clientSocket;

    pthread_mutex_lock(&questionLock);
    char message[OUTPUT_CHARACTER_LIMIT];
    snprintf(message, sizeof(message), "\nProcessing word: '%s'\n", input_word);
    write(clientSocket, message, strlen(message));

    int exactMatch = 0;
    int distance[LEVENSHTEIN_LIST_LIMIT];
    char *best_words[LEVENSHTEIN_LIST_LIMIT];

    for (int i = 0; i < LEVENSHTEIN_LIST_LIMIT; i++) {
        distance[i] = 9999;
        best_words[i] = NULL;
    }

    pthread_mutex_lock(&dictLock);
    for (int i = 0; i < wordCount; i++) {
        int dist = levenshtein(input_word, dictionary[i]);

        if (dist == 0) {
            exactMatch = 1;
        }

        for (int j = 0; j < LEVENSHTEIN_LIST_LIMIT; j++) {
            if (dist < distance[j]) {
                for (int k = LEVENSHTEIN_LIST_LIMIT - 1; k > j; k--) {
                    distance[k] = distance[k - 1];
                    best_words[k] = best_words[k - 1];
                }
                distance[j] = dist;
                best_words[j] = dictionary[i];
                break;
            }
        }
    }
    pthread_mutex_unlock(&dictLock);

    snprintf(message, sizeof(message), "Levenshtein results for '%s':\n", input_word);
    write(clientSocket, message, strlen(message));

    for (int i = 0; i < LEVENSHTEIN_LIST_LIMIT && best_words[i] != NULL; i++) {
        snprintf(message, sizeof(message), "%s (%d)\n", best_words[i], distance[i]);
        write(clientSocket, message, strlen(message));
    }

    if (exactMatch) {
        snprintf(message, sizeof(message), "\nThe word '%s' already exists in the dictionary.\nPress 'y' to continue: ", input_word);
        write(clientSocket, message, strlen(message));

        char response[10];
        read(clientSocket, response, sizeof(response));
        while (response[0] != 'y' && response[0] != 'Y') {
            snprintf(message, sizeof(message), "Invalid input. Press 'y' to continue: ");
            write(clientSocket, message, strlen(message));
            read(clientSocket, response, sizeof(response));
        }
    } else {
        snprintf(message, sizeof(message), "\nThe word '%s' was not found in the dictionary.\nDo you want to add it? (y/n): ", input_word);
        write(clientSocket, message, strlen(message));

        char response[10];
        read(clientSocket, response, sizeof(response));
        if (response[0] == 'y' || response[0] == 'Y') {
            addWordToDictionary(input_word);
            snprintf(message, sizeof(message), "The word '%s' has been added to the dictionary.\n", input_word);
            write(clientSocket, message, strlen(message));
        } else {
            // Replace the word with the closest match
            if (best_words[0] != NULL) {
                snprintf(message, sizeof(message), "The word '%s' was not added. Replacing with closest match: '%s'.\n", input_word, best_words[0]);
                write(clientSocket, message, strlen(message));

                // Replace the word in the input string with the closest match
                strncpy(input_word, best_words[0], INPUT_CHARACTER_LIMIT); // Modify the original word in the input string
            }
        }
    }

    pthread_mutex_unlock(&questionLock);
    free(wpArgs);
    pthread_exit(NULL);
}


int isValidInput(char *input) {
    // Remove any trailing newline or spaces
    trim(input);

    // Check if the input is empty or contains non-alphabetic characters
    for (int i = 0; input[i] != '\0'; i++) {
        if (!(isalpha(input[i]) || input[i] == ' ')) {
            return 0; // Invalid input if it contains non-alphabetic or non-space character
        }
    }

    return 1; // Valid input
}

void trim(char *str) {
    // Remove leading and trailing spaces/newlines
    char *end;

    // Remove leading spaces
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return; // If string is empty after trimming

    // Remove trailing spaces
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    *(end + 1) = '\0'; // Null-terminate the string
}



void handleClientInput(char *input, int clientSocket) {
    // Check if the input exceeds the character limit
    if (strlen(input) >= INPUT_CHARACTER_LIMIT) {
        char errorMessage[] = "Error: Input exceeds the character limit.\n";
        write(clientSocket, errorMessage, strlen(errorMessage));
        close(clientSocket);
        return; // Exit the function early
    }

    // Validate if the input contains only alphabetic characters and spaces
    if (!isValidInput(input)) {
        char errorMessage[] = "Error: Input contains invalid characters. Only alphabetic characters and spaces are allowed.\n";
        write(clientSocket, errorMessage, strlen(errorMessage));
        close(clientSocket);
        return; // Exit the function early
    }

    // If input is valid, proceed with processing
    char *token = strtok(input, " ");
    pthread_t threads[INPUT_CHARACTER_LIMIT];
    int threadCount = 0;

    while (token != NULL) {
        struct WordProcessArgs *args = malloc(sizeof(struct WordProcessArgs));
        strncpy(args->word, token, INPUT_CHARACTER_LIMIT);
        args->clientSocket = clientSocket;

        pthread_create(&threads[threadCount], NULL, processWord, (void *)args);
        threadCount++;
        token = strtok(NULL, " ");
    }

    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], NULL);
    }

    saveDictionary();
    close(clientSocket);
    exit(0);
}

void *clientHandler(void *clientSocket) {
    int socket = *(int *)clientSocket;
    char buffer[INPUT_CHARACTER_LIMIT];
    int bytesRead;

    if ((bytesRead = read(socket, buffer, sizeof(buffer))) > 0) {
        buffer[bytesRead] = '\0';
        handleClientInput(buffer, socket);
    }

    free(clientSocket);
    pthread_exit(NULL);
}

void startServer() {
    int serverSocket, clientSocket, *newClientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT_NUMBER);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    listen(serverSocket, 5);
    printf("Server listening on port %d...\n", PORT_NUMBER);
    printf("\n");
    printf("Hello, this is Text Analysis Server! \n");
    printf("\n");

    while (1) {
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            perror("Error accepting connection");
            continue;
        }

        pthread_t clientThread;
        newClientSocket = malloc(sizeof(int));
        *newClientSocket = clientSocket;
        pthread_create(&clientThread, NULL, clientHandler, (void *)newClientSocket);
        pthread_detach(clientThread);
    }

    close(serverSocket);
}

int main() {
    loadDictionary(&dictionary, &wordCount);
    pthread_mutex_init(&dictLock, NULL);
    pthread_mutex_init(&questionLock, NULL);
    startServer();

    for (int i = 0; i < wordCount; i++) {
        free(dictionary[i]);
    }
    free(dictionary);

    pthread_mutex_destroy(&dictLock);
    pthread_mutex_destroy(&questionLock);
    return 0;
}