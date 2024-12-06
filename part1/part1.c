#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "account.h"
#include "string_parser.h"

void process_transactions(FILE *input, Account *accounts, int num_accounts) {
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), input)) {
        command_line cmd = str_filler(buffer, " ");
        if (cmd.num_token == 0) {
            free_command_line(&cmd);
            continue;
        }

        char *type = cmd.command_list[0];

        if (strcmp(type, "D") == 0) { // Deposit
            char *account_num = cmd.command_list[1];
            char *password = cmd.command_list[2];
            double amount = atof(cmd.command_list[3]);

            for (int i = 0; i < num_accounts; i++) {
                if (strcmp(accounts[i].account_number, account_num) == 0 &&
                    strcmp(accounts[i].password, password) == 0) {
                    accounts[i].balance += amount;
                    accounts[i].transaction_tracter += amount;
                    break;
                }
            }
        } else if (strcmp(type, "W") == 0) { // Withdraw
            char *account_num = cmd.command_list[1];
            char *password = cmd.command_list[2];
            double amount = atof(cmd.command_list[3]);

            for (int i = 0; i < num_accounts; i++) {
                if (strcmp(accounts[i].account_number, account_num) == 0 &&
                    strcmp(accounts[i].password, password) == 0) {
                    accounts[i].balance -= amount;
                    accounts[i].transaction_tracter += amount;
                    break;
                }
            }
        } else if (strcmp(type, "T") == 0) { // Transfer
            char *src_account = cmd.command_list[1];
            char *password = cmd.command_list[2];
            char *dest_account = cmd.command_list[3];
            double amount = atof(cmd.command_list[4]);

            for (int i = 0; i < num_accounts; i++) {
                if (strcmp(accounts[i].account_number, src_account) == 0 &&
                    strcmp(accounts[i].password, password) == 0) {
                    accounts[i].balance -= amount;
                    accounts[i].transaction_tracter += amount;

                    for (int j = 0; j < num_accounts; j++) {
                        if (strcmp(accounts[j].account_number, dest_account) == 0) {
                            accounts[j].balance += amount;
                            break;
                        }
                    }
                    break;
                }
            }
        }

        free_command_line(&cmd);
    }
}

void apply_rewards(Account *accounts, int num_accounts) {
    for (int i = 0; i < num_accounts; i++) {
        accounts[i].balance += accounts[i].transaction_tracter * accounts[i].reward_rate;
    }
}

void write_output(FILE *output, Account *accounts, int num_accounts) {
    for (int i = 0; i < num_accounts; i++) {
        fprintf(output, "%d balance:\t%.2f\n\n", i, accounts[i].balance); // Add blank line for formatting
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    FILE *input = fopen(argv[1], "r");
    FILE *output = fopen(argv[2], "w");
    if (!input || !output) {
        perror("Error opening file");
        return 1;
    }

    int num_accounts;
    fscanf(input, "%d\n", &num_accounts);

    Account *accounts = malloc(sizeof(Account) * num_accounts);

    for (int i = 0; i < num_accounts; i++) {
        fscanf(input, "index %*d\n");
        fscanf(input, "%s\n", accounts[i].account_number);
        fscanf(input, "%s\n", accounts[i].password);
        fscanf(input, "%lf\n", &accounts[i].balance);
        fscanf(input, "%lf\n", &accounts[i].reward_rate);
        accounts[i].transaction_tracter = 0.0;
    }

    process_transactions(input, accounts, num_accounts);
    apply_rewards(accounts, num_accounts);
    write_output(output, accounts, num_accounts);

    free(accounts);
    fclose(input);
    fclose(output);

    return 0;
}
