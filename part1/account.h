#ifndef ACCOUNT_H_
#define ACCOUNT_H_
#include <pthread.h>

typedef struct
{
char account_number[17];
char password[20];
double balance;
double reward_rate;
double transaction_tracter;
char out_file[64];
pthread_mutex_t ac_lock;
}Account;
#endif /* ACCOUNT_H_ */
