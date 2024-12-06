#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "account.h"
#include "string_parser.h"

#define NUM_WORKERS 10
#define CHECK_BALANCE_THRESHOLD 500
#define MAX_CHECK_LOGS 20
#define REQUESTS_THRESHOLD 5000
#define FILENAME_SIZE 20 // Increased size of filename buffer

typedef struct {
    Account *accounts;
    int num_accounts;
    FILE *input;
    pthread_mutex_t *file_mutex;
    pthread_mutex_t *account_mutexes;
    pthread_barrier_t *barrier;
    pthread_mutex_t *request_mutex;
    pthread_cond_t *request_cond;
    int total_requests;
} WorkerArgs;

int pipe_fd[2];
pthread_mutex_t check_count_mutex = PTHREAD_MUTEX_INITIALIZER;
int check_balance_count = 0;   // Global count of check balance commands
int logged_checks = 0;         // Number of check balance logs written

void* process_transactions_thread(void *arg);
void apply_rewards(Account *accounts, int num_accounts);
void write_output(Account *accounts, int num_accounts);
void auditor_process(int read_fd);
void log_to_pipe(const char *message);
void log_interest_application(Account *accounts, int num_accounts);
void bank_thread(WorkerArgs *args);

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

    pthread_barrier_t barrier;
    pthread_mutex_t request_mutex;
    pthread_cond_t request_cond;
    pthread_barrier_init(&barrier, NULL, NUM_WORKERS);
    pthread_mutex_init(&request_mutex, NULL);
    pthread_cond_init(&request_cond, NULL);

    pthread_mutex_t file_mutex;
    pthread_mutex_init(&file_mutex, NULL);

    WorkerArgs args = {accounts, num_accounts, input, &file_mutex, account_mutexes, &barrier, &request_mutex, &request_cond, 0};

    pthread_t workers[NUM_WORKERS];
    pthread_t bank_thread_id;

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&workers[i], NULL, process_transactions_thread, &args);
    }

    pthread_create(&bank_thread_id, NULL, (void*(*)(void*))bank_thread, &args);

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(workers[i], NULL);
    }

    pthread_join(bank_thread_id, NULL);

    close(pipe_fd[1]);
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
    char buffer[256];

    pthread_barrier_wait(args->barrier);

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
        char log_entry[256];

        if (strcmp(type, "D") == 0) { // Deposit
            char *account_num = cmd.command_list[1];
            char *password = cmd.command_list[2];
            double amount = atof(cmd.command_list[3]);

            for (int i = 0; i < args->num_accounts; i++) {
                if (strcmp(args->accounts[i].account_number, account_num) == 0 &&
                    strcmp(args->accounts[i].password, password) == 0) {
                    pthread_mutex_lock(&args->account_mutexes[i]);
                    args->accounts[i].balance += amount;
                    args->accounts[i].transaction_tracter += amount;
                    pthread_mutex_unlock(&args->account_mutexes[i]);
                    break;
                }
            }
        } else if (strcmp(type, "W") == 0) { // Withdraw
            char *account_num = cmd.command_list[1];
            char *password = cmd.command_list[2];
            double amount = atof(cmd.command_list[3]);

            for (int i = 0; i < args->num_accounts; i++) {
                if (strcmp(args->accounts[i].account_number, account_num) == 0 &&
                    strcmp(args->accounts[i].password, password) == 0) {
                    pthread_mutex_lock(&args->account_mutexes[i]);
                    args->accounts[i].balance -= amount;
                    args->accounts[i].transaction_tracter += amount;
                    pthread_mutex_unlock(&args->account_mutexes[i]);
                    break;
                }
            }
        } else if (strcmp(type, "T") == 0) { // Transfer
            char *src_account = cmd.command_list[1];
            char *password = cmd.command_list[2];
            char *dest_account = cmd.command_list[3];
            double amount = atof(cmd.command_list[4]);

            for (int i = 0; i < args->num_accounts; i++) {
                if (strcmp(args->accounts[i].account_number, src_account) == 0 &&
                    strcmp(args->accounts[i].password, password) == 0) {
                    pthread_mutex_lock(&args->account_mutexes[i]);
                    args->accounts[i].balance -= amount;
                    args->accounts[i].transaction_tracter += amount;
                    pthread_mutex_unlock(&args->account_mutexes[i]);

                    for (int j = 0; j < args->num_accounts; j++) {
                        if (strcmp(args->accounts[j].account_number, dest_account) == 0) {
                            pthread_mutex_lock(&args->account_mutexes[j]);
                            args->accounts[j].balance += amount;
                            pthread_mutex_unlock(&args->account_mutexes[j]);
                            break;
                        }
                    }
                    break;
                }
            }
        } else if (strcmp(type, "C") == 0) { // Check Balance
            char *account_num = cmd.command_list[1];
            for (int i = 0; i < args->num_accounts; i++) {
                if (strcmp(args->accounts[i].account_number, account_num) == 0) {
                    pthread_mutex_lock(&args->account_mutexes[i]);
                    pthread_mutex_lock(&check_count_mutex);

                    check_balance_count++;
                    if (check_balance_count % CHECK_BALANCE_THRESHOLD == 0 && logged_checks < MAX_CHECK_LOGS) {
                        logged_checks++;
                        time_t now = time(NULL);
                        snprintf(log_entry, sizeof(log_entry),
                                 "Worker checked balance of Account %s. Balance is $%.2f. Check occured at %s",
                                 account_num, args->accounts[i].balance, ctime(&now));
                        log_to_pipe(log_entry);
                    }

                    pthread_mutex_unlock(&check_count_mutex);
                    pthread_mutex_unlock(&args->account_mutexes[i]);
                    break;
                }
            }
        }

        pthread_mutex_lock(args->request_mutex);
        args->total_requests++;
        if (args->total_requests >= REQUESTS_THRESHOLD) {
            pthread_cond_signal(args->request_cond);
            pthread_mutex_unlock(args->request_mutex);
            pthread_cond_wait(args->request_cond, args->request_mutex);
        } else {
            pthread_mutex_unlock(args->request_mutex);
        }

        free_command_line(&cmd);
    }

    pthread_exit(NULL);
}

void bank_thread(WorkerArgs *args) {
    while (1) {
        pthread_mutex_lock(args->request_mutex);
        while (args->total_requests < REQUESTS_THRESHOLD) {
            pthread_cond_wait(args->request_cond, args->request_mutex);
        }

        apply_rewards(args->accounts, args->num_accounts);
        write_output(args->accounts, args->num_accounts);
        log_interest_application(args->accounts, args->num_accounts);

        args->total_requests = 0;
        pthread_mutex_unlock(args->request_mutex);
        pthread_cond_broadcast(args->request_cond);
    }
}

void apply_rewards(Account *accounts, int num_accounts) {
    for (int i = 0; i < num_accounts; i++) {
        accounts[i].balance += accounts[i].transaction_tracter * accounts[i].reward_rate;
    }
}

void write_output(Account *accounts, int num_accounts) {
    for (int i = 0; i < num_accounts; i++) {
        char filename[FILENAME_SIZE];
        snprintf(filename, FILENAME_SIZE, "act_%02d.txt", i);
        FILE *output = fopen(filename, "w");
        if (output) {
            fprintf(output, "Current Balance: %.2f\n", accounts[i].balance);
            fclose(output);
        }
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