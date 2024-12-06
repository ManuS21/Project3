#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "account.h"
#include "string_parser.h"

#define NUM_WORKERS 10
#define TRANSACTION_THRESHOLD 5000

typedef struct {
    account *accounts;
    int num_accounts;
    FILE *input;
    pthread_mutex_t *file_mutex;
    pthread_mutex_t *account_mutexes;
    int *global_transaction_count;
    pthread_mutex_t *global_mutex;
    pthread_cond_t *update_cond;
    pthread_barrier_t *start_barrier;
    int *active_threads;
} WorkerArgs;

pthread_mutex_t bank_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bank_cond = PTHREAD_COND_INITIALIZER;
pthread_barrier_t start_barrier;
int global_transaction_count = 0;
int active_threads = 0;

void* process_transactions_thread(void *arg);
void* bank_thread(void *arg);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *input = fopen(argv[1], "r");
    if (!input) {
        perror("Error opening file");
        return 1;
    }

    int num_accounts;
    fscanf(input, "%d\n", &num_accounts);

    account *accounts = malloc(sizeof(account) * num_accounts);
    pthread_mutex_t *account_mutexes = malloc(sizeof(pthread_mutex_t) * num_accounts);
    for (int i = 0; i < num_accounts; i++) {
        fscanf(input, "index %*d\n");
        fscanf(input, "%s\n", accounts[i].account_number);
        fscanf(input, "%s\n", accounts[i].password);
        fscanf(input, "%lf\n", &accounts[i].balance);
        fscanf(input, "%lf\n", &accounts[i].reward_rate);
        accounts[i].transaction_tracter = 0.0;
        pthread_mutex_init(&account_mutexes[i], NULL);
    }

    pthread_mutex_t file_mutex;
    pthread_mutex_init(&file_mutex, NULL);

    pthread_barrier_init(&start_barrier, NULL, NUM_WORKERS + 1);

    pthread_t workers[NUM_WORKERS], bank;
    WorkerArgs args = {
        accounts,
        num_accounts,
        input,
        &file_mutex,
        account_mutexes,
        &global_transaction_count,
        &bank_mutex,
        &bank_cond,
        &start_barrier,
        &active_threads
    };

    pthread_create(&bank, NULL, bank_thread, &args);

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&workers[i], NULL, process_transactions_thread, &args);
    }

    pthread_barrier_wait(&start_barrier); // Signal all threads to start processing

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }

    pthread_mutex_lock(&bank_mutex);
    active_threads = 0; // Signal the bank thread for the final update
    pthread_cond_signal(&bank_cond);
    pthread_mutex_unlock(&bank_mutex);

    pthread_join(bank, NULL);

    fclose(input);
    pthread_barrier_destroy(&start_barrier);
    pthread_mutex_destroy(&file_mutex);
    for (int i = 0; i < num_accounts; i++) {
        pthread_mutex_destroy(&account_mutexes[i]);
    }
    free(account_mutexes);
    free(accounts);
    return 0;
}

void* process_transactions_thread(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;
    char buffer[256];
    int local_transaction_count = 0;

    pthread_barrier_wait(args->start_barrier);

    while (1) {
        pthread_mutex_lock(args->file_mutex);
        if (!fgets(buffer, sizeof(buffer), args->input)) {
            pthread_mutex_unlock(args->file_mutex);
            break;
        }
        pthread_mutex_unlock(args->file_mutex);

        command_line cmd = str_filler(buffer, " ");
        if (cmd.num_token == 0) {
            free_command_line(&cmd);
            continue;
        }

        char *type = cmd.command_list[0];
        if (strcmp(type, "C") != 0) { // Exclude Check Balance
            local_transaction_count++;
            pthread_mutex_lock(args->global_mutex);
            *(args->global_transaction_count) += 1;
            if (*(args->global_transaction_count) >= TRANSACTION_THRESHOLD) {
                pthread_cond_signal(args->update_cond);
                pthread_cond_wait(&bank_cond, args->global_mutex);
            }
            pthread_mutex_unlock(args->global_mutex);
        }
        // Handle transactions here...
        free_command_line(&cmd);
    }

    pthread_mutex_lock(&bank_mutex);
    *(args->active_threads) -= 1;
    pthread_cond_signal(&bank_cond);
    pthread_mutex_unlock(&bank_mutex);

    pthread_exit(NULL);
}

void* bank_thread(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;

    pthread_mutex_lock(&bank_mutex);
    while (*(args->active_threads) > 0) {
        pthread_cond_wait(&bank_cond, &bank_mutex);
        if (*(args->global_transaction_count) >= TRANSACTION_THRESHOLD || *(args->active_threads) == 0) {
            // Apply rewards logic
            *(args->global_transaction_count) = 0; // Reset counter
            pthread_cond_broadcast(args->update_cond); // Resume worker threads
        }
    }
    pthread_mutex_unlock(&bank_mutex);
    pthread_exit(NULL);
}
