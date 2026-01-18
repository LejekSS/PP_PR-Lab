#ifndef UTILH
#define UTILH
#include "main.h"

// pakiet komunikacyjny MPI
typedef struct {
    int ts;           // timestamp Lamporta
    int src;          // rank nadawcy
    int resource_id;  // -1 = pyrkon reszta warsztaty
    int data;        // wiadomosc
} packet_t;

// liczba elementów w pakiecie
#define NITEMS 4

//stany
typedef enum {
    InRun,
    InWant,
    InSection,      // na pyrkonie, wybiera warsztat
    InFinish,
    InWantWorkshop,
    InWorkshop,
    DecideNext
} state_t;

//konfiguracja
#define REQ_PYRKON -1
#define WARSZTATY_COUNT 3   // liczba warsztatów liczona od zera
#define PYRKON_SLOTS 5      
#define WARSZTAT_SLOTS 2   
#define PYRKON_TURY 10   //ile pyrkonow bedzie

//typy wiadomości MPI
#define ACK     1
#define REQUEST 2
#define RELEASE 3

extern int *tablica_zasobow; // resource_id dla każdego procesu
extern int *tablica_zadan;   // timestamp żądania lub -1 
extern MPI_Datatype MPI_PAKIET_T;

// funkcje pomocnicze
void inicjuj_typ_pakietu();
void sendPacket(packet_t *pkt, int destination, int tag);

extern state_t stan;
extern pthread_mutex_t stateMut;

// zegar Lamporta
extern int lamport_clock;
extern int ackCount;
// mutexy
extern pthread_mutex_t clockMut;
extern pthread_mutex_t ackMut;
extern pthread_mutex_t tablicaMut; //tablica zadan i tablica zasobow

//funkcja zmiany stanu
void changeState( state_t );

#endif