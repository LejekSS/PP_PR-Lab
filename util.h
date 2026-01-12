#ifndef UTILH
#define UTILH
#include "main.h"

/* Typ pakietu przesyłany przez MPI */
typedef struct {
    int ts;           /* timestamp (zegar Lamporta) */
    int src;          /* źródło wiadomości (rank) */
    int resource_id;  /* -1 = Pyrkon, 0..M = Warsztaty */
    int data;         /* pole pomocnicze */
} packet_t;

/* Liczba pól w packet_t (używana przy tworzeniu typu MPI) */
#define NITEMS 4

/* Stany procesu */
typedef enum {
    InRun,
    InMonitor,
    InWant,
    InSection,      /* Na Pyrkonie (korytarz) */
    InFinish,
    InWantWorkshop, /* Chcę wejść na warsztat */
    InWorkshop      /* Jestem na warsztacie */
} state_t;

/* Stałe konfiguracyjne */
#define REQ_PYRKON -1
#define WARSZTATY_COUNT 3   /* Liczba warsztatów (np. 0..2) */
#define PYRKON_SLOTS 5      /* Maksymalna liczba miejsc na Pyrkonie */
#define WARSZTAT_SLOTS 2    /* Maksymalna liczba uczestników jednego warsztatu */
#define PYRKON_TURY 10      /* Maksymalna liczba tur symulacji */

/* Typy pakietów (MPI_TAG) */
#define ACK     1
#define REQUEST 2
#define RELEASE 3
#define APP_PKT 4
#define FINISH  5

/* Tablice i typ MPI */
extern int *tablica_zasobow; /* resource_id dla każdego procesu */
extern int *tablica_zadan;   /* timestamp żądania lub -1 */
extern MPI_Datatype MPI_PAKIET_T;

/* Funkcje/zmienne udostępniane */
void inicjuj_typ_pakietu();
void sendPacket(packet_t *pkt, int destination, int tag);

extern state_t stan;
extern pthread_mutex_t stateMut;

/* Zegar Lamporta i muteksy */
extern int lamport_clock;
extern pthread_mutex_t clockMut;
extern int ackCount;
extern pthread_mutex_t ackMut;
extern pthread_mutex_t tablicaMut; /* chroni tablica_zadan i tablica_zasobow */

/* zmiana stanu, obwarowana muteksem */
void changeState( state_t );

#endif

