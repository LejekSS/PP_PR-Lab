#ifndef MAINH
#define MAINH
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "util.h"
/* Boolean values */
#define TRUE 1
#define FALSE 0
#define SEC_IN_STATE 1
#define STATE_CHANGE_PROB 10

#define ROOT 0

/* Declarations only - definitions in main.c */
extern int rank;
extern int size;
extern int ackCount;
extern pthread_t threadKom;
extern int* deferred_ack;
extern int* tablica_zadan;
extern int* tablica_zasobow;
extern pthread_mutex_t deferredMut;


// debug macro
// [rank] [Lamport clock]: wiadomosc
#ifdef DEBUG
#define debug(FORMAT,...) do { \
    pthread_mutex_lock(&clockMut); \
    int _clock = lamport_clock; \
    pthread_mutex_unlock(&clockMut); \
    printf("%c[%d;%dm [%d] [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, _clock, ##__VA_ARGS__, 27,0,37); \
} while(0);
#else
#define debug(...) ;
#endif

//to samo co wyżej tylko wyswietla zawsze
#define println(FORMAT,...) do { \
    pthread_mutex_lock(&clockMut); \
    int _clock = lamport_clock; \
    pthread_mutex_unlock(&clockMut); \
    printf("%c[%d;%dm [%d] [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, _clock, ##__VA_ARGS__, 27,0,37); \
} while(0);


#endif
