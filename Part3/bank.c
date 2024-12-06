#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "account.h"
#include "string_parser.h"

#define NUM_WORKERS 10
#define CHECK_BALANCE_THRESHOLD 5000

typedef struct {
    Account *accounts;
    int num_accounts;
    FILE *input;
    pthread_mutex_t *file_mutex;
    pthread_mutex_t *account_mutexes;
} WorkerArgs;

// Global synchronization variables
pthread_barrier_t start_barrier;
pthread_mutex_t transaction_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t update_cond = PTHREAD_COND_INITIALIZER;
int transaction_count = 0;
int update_ready = 0;

void* process_transactions_thread(void *arg);
void* bank_thread(void *arg);
void apply_rewards(Account *accounts, int num_accounts);
void write_output_logs(Account *accounts, int num_accounts);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *input = fopen(argv[1], "r");
    if (!input) {
        perror("Error opening input file");
        return 1;
    }

    int num_accounts;
    fscanf(input, "%d\n", &num_accounts);

    Account *accounts = malloc(sizeof(Account) * num_accounts);
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

    pthread_t workers[NUM_WORKERS];
    pthread_t bank;
    WorkerArgs args = {accounts, num_accounts, input, &file_mutex, account_mutexes};

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&workers[i], NULL, process_transactions_thread, &args);
    }
    pthread_create(&bank, NULL, bank_thread, accounts);

    pthread_barrier_wait(&start_barrier);

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }
    pthread_cancel(bank);

    pthread_mutex_destroy(&file_mutex);
    for (int i = 0; i < num_accounts; i++) {
        pthread_mutex_destroy(&account_mutexes[i]);
    }
    pthread_barrier_destroy(&start_barrier);
    free(account_mutexes);
    free(accounts);
    fclose(input);

    return 0;
}

void* process_transactions_thread(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;
    char buffer[256];

    pthread_barrier_wait(&start_barrier);

    while (1) {
        pthread_mutex_lock(args->file_mutex);
        if (!fgets(buffer, sizeof(buffer), args->input)) {
            pthread_mutex_unlock(args->file_mutex);
            break;
        }
        pthread_mutex_unlock(args->file_mutex);

        // Process transaction...
        pthread_mutex_lock(&transaction_mutex);
        transaction_count++;
        if (transaction_count >= CHECK_BALANCE_THRESHOLD) {
            update_ready = 1;
            pthread_cond_signal(&update_cond);
        }
        pthread_mutex_unlock(&transaction_mutex);

        pthread_mutex_lock(&transaction_mutex);
        while (update_ready) {
            pthread_cond_wait(&update_cond, &transaction_mutex);
        }
        pthread_mutex_unlock(&transaction_mutex);
    }

    pthread_exit(NULL);
}

void* bank_thread(void *arg) {
    Account *accounts = (Account *)arg;

    while (1) {
        pthread_mutex_lock(&transaction_mutex);
        while (!update_ready) {
            pthread_cond_wait(&update_cond, &transaction_mutex);
        }
        update_ready = 0;
        pthread_mutex_unlock(&transaction_mutex);

        apply_rewards(accounts, 10);
        write_output_logs(accounts, 10);

        pthread_mutex_lock(&transaction_mutex);
        pthread_cond_broadcast(&update_cond);
        pthread_mutex_unlock(&transaction_mutex);
    }

    pthread_exit(NULL);
}

void apply_rewards(Account *accounts, int num_accounts) {
    for (int i = 0; i < num_accounts; i++) {
        accounts[i].balance += accounts[i].transaction_tracter * accounts[i].reward_rate;
        accounts[i].transaction_tracter = 0.0;
    }
}

void write_output_logs(Account *accounts, int num_accounts) {
    for (int i = 0; i < num_accounts; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "act_%s.txt", accounts[i].account_number);
        FILE *fp = fopen(filename, "a");
        if (fp) {
            fprintf(fp, "Current Balance:\t%.2f\n", accounts[i].balance);
            fclose(fp);
        }
    }
}
