#ifndef UTILH
#define UTILH
#include "main.h"

/* typ pakietu */
typedef struct {
    int ts;       /* timestamp (zegar lamporta */
    int src;
    int resource_id; /* -1 = Pyrkon, 0...M = Warsztaty */
    int data;     /* przykładowe pole z danymi; można zmienić nazwę na bardziej pasującą */
} packet_t;
/* packet_t ma trzy pola, więc NITEMS=3. Wykorzystane w inicjuj_typ_pakietu */
#define NITEMS 4

/* Zmieniamy enum state_t */
typedef enum {
    InRun,
    InMonitor,
    InWant,
    InSection,      /* "Jestem na Pyrkonie / Odpoczywam" */
    InFinish,
    InWantWorkshop, /* Nowy stan: Chcę wejść na warsztat */
    InWorkshop      /* Nowy stan: Jestem na warsztacie */
} state_t;

/* Dodaj definicje limitów */
#define REQ_PYRKON -1
#define WARSZTATY_COUNT 3   // Mamy warsztaty 0, 1, 2
#define PYRKON_SLOTS 2      // Max 2 osoby na terenie Pyrkonu
#define WARSZTAT_SLOTS 2    // Max 1 osoba na konkretnym warsztacie

/* Typy wiadomości */
/* TYPY PAKIETÓW */
#define ACK     1
#define REQUEST 2
#define RELEASE 3
#define APP_PKT 4
#define FINISH  5

extern int *tablica_zasobow; // Przechowuje resource_id dla danego procesu
#define REQ_PYRKON -1
#define WARSZTATY_COUNT 3 // Np. 3 różne warsztaty

extern MPI_Datatype MPI_PAKIET_T;
void inicjuj_typ_pakietu();

/* wysyłanie pakietu, skrót: wskaźnik do pakietu (0 oznacza stwórz pusty pakiet), do kogo, z jakim typem */
void sendPacket(packet_t *pkt, int destination, int tag);

extern state_t stan;
extern pthread_mutex_t stateMut;

/* DODANE: Zegar Lamporta i Mutexy */
extern int lamport_clock;
extern pthread_mutex_t clockMut;
extern int ackCount;
extern pthread_mutex_t ackMut;

/* zmiana stanu, obwarowana muteksem */
void changeState( state_t );
#endif

extern int *tablica_zadan;