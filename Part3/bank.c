#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "account.h"
#include "string_parser.h"

#define NUM_WORKERS 10
#define THRESHOLD 5000

typedef struct {
    Account *accounts;
    int num_accounts;
    FILE *input;
    pthread_mutex_t *file_mutex;
    pthread_mutex_t *account_mutexes;
} WorkerArgs;

// Shared variables and synchronization tools
pthread_barrier_t start_barrier;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t check_count_mutex = PTHREAD_MUTEX_INITIALIZER;
int check_balance_count = 0;
int processed_requests = 0;
int pause_processing = 0;
int pipe_fd[2];

// Function prototypes
void* process_transactions_thread(void *arg);
void* bank_thread_function(void *arg);
void apply_rewards(Account *accounts, int num_accounts);
void write_output(Account *accounts, int num_accounts);
void log_interest_application(Account *accounts, int num_accounts);
void log_to_pipe(const char *message);
void auditor_process(int read_fd);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    if (pipe(pipe_fd) == -1) {
        perror("Pipe creation failed");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Forking error");
        return 1;
    }

    if (pid == 0) {
        close(pipe_fd[1]);
        auditor_process(pipe_fd[0]);
        exit(0);
    }

    close(pipe_fd[0]);

    FILE *input = fopen(argv[1], "r");
    if (!input) {
        perror("Error opening file");
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

    pthread_t workers[NUM_WORKERS];
    pthread_t bank_thread;
    WorkerArgs args = {accounts, num_accounts, input, &file_mutex, account_mutexes};

    // Initialize barrier
    pthread_barrier_init(&start_barrier, NULL, NUM_WORKERS + 1);

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&workers[i], NULL, process_transactions_thread, &args);
    }

    pthread_create(&bank_thread, NULL, bank_thread_function, &args);

    pthread_barrier_wait(&start_barrier); // Synchronize all threads

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }

    pthread_cancel(bank_thread);
    pthread_join(bank_thread, NULL);

    apply_rewards(accounts, num_accounts);
    write_output(accounts, num_accounts);
    log_interest_application(accounts, num_accounts);

    close(pipe_fd[1]);

    pthread_barrier_destroy(&start_barrier);
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&cond_mutex);
    pthread_mutex_destroy(&file_mutex);
    for (int i = 0; i < num_accounts; i++) {
        pthread_mutex_destroy(&account_mutexes[i]);
    }
    free(account_mutexes);
    free(accounts);
    fclose(input);
    return 0;
}

void* process_transactions_thread(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;

    pthread_barrier_wait(&start_barrier); // Wait for all threads to start

    char buffer[256];

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

        if (strcmp(type, "D") == 0 || strcmp(type, "W") == 0 || strcmp(type, "T") == 0) {
            pthread_mutex_lock(&cond_mutex);
            processed_requests++;
            if (processed_requests >= THRESHOLD) {
                pause_processing = 1;
                pthread_cond_signal(&cond);
                while (pause_processing) {
                    pthread_cond_wait(&cond, &cond_mutex);
                }
            }
            pthread_mutex_unlock(&cond_mutex);
        }

        // Existing transaction logic (e.g., deposit, withdraw, transfer)

        free_command_line(&cmd);
    }

    pthread_exit(NULL);
}

void* bank_thread_function(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;

    while (1) {
        pthread_mutex_lock(&cond_mutex);
        while (!pause_processing) {
            pthread_cond_wait(&cond, &cond_mutex);
        }

        // Apply rewards and log balances
        apply_rewards(args->accounts, args->num_accounts);
        log_interest_application(args->accounts, args->num_accounts);
        write_output(args->accounts, args->num_accounts);

        processed_requests = 0;
        pause_processing = 0;
        pthread_cond_broadcast(&cond); // Notify worker threads
        pthread_mutex_unlock(&cond_mutex);
    }

    return NULL;
}

void apply_rewards(Account *accounts, int num_accounts) {
    for (int i = 0; i < num_accounts; i++) {
        accounts[i].balance += accounts[i].transaction_tracter * accounts[i].reward_rate;
    }
}

void log_interest_application(Account *accounts, int num_accounts) {
    time_t now = time(NULL);
    char log_entry[256];
    
    for (int i = 0; i < num_accounts; i++) {
        snprintf(log_entry, sizeof(log_entry),
                 "Applied interest to account %s. New Balance: $%.2f. Time of Update: %s",
                 accounts[i].account_number, accounts[i].balance, ctime(&now));
        log_to_pipe(log_entry);
    }
}

void write_output(Account *accounts, int num_accounts) {
    for (int i = 0; i < num_accounts; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "act_%s.txt", accounts[i].account_number);
        FILE *output = fopen(filename, "w");
        if (output) {
            fprintf(output, "Account: %s\nCurrent Balance: %.2f\n", accounts[i].account_number, accounts[i].balance);
            fclose(output);
        }
    }
}

void auditor_process(int read_fd) {
    FILE *ledger = fopen("ledger.txt", "w");
    if (!ledger) {
        perror("Failed to open ledger.txt");
        exit(1);
    }

    char buffer[256];
    ssize_t bytes_read;
    while ((bytes_read = read(read_fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        fprintf(ledger, "%s", buffer);
    }

    fclose(ledger);
}

void log_to_pipe(const char *message) {
    write(pipe_fd[1], message, strlen(message));
}
