#include "main.h"
#include "util.h"
MPI_Datatype MPI_PAKIET_T;

/* State definition */
state_t stan=InRun;

/* Mutex for state */
pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;

/* Global variable definitions */
int lamport_clock = 0;
pthread_mutex_t clockMut = PTHREAD_MUTEX_INITIALIZER;
int ackCount = 0;
pthread_mutex_t ackMut = PTHREAD_MUTEX_INITIALIZER;

/* MUTEX: chroni dostęp do tablica_zadan i tablica_zasobow */
pthread_mutex_t tablicaMut = PTHREAD_MUTEX_INITIALIZER;

struct tagNames_t{
    const char *name;
    int tag;
} tagNames[] = { { "pakiet aplikacyjny", APP_PKT }, { "finish", FINISH}, 
                { "potwierdzenie", ACK}, {"prośbę o sekcję krytyczną", REQUEST}, {"zwolnienie sekcji krytycznej", RELEASE} };

const char *const tag2string( int tag )
{
    for (int i=0; i <sizeof(tagNames)/sizeof(struct tagNames_t);i++) {
	if ( tagNames[i].tag == tag )  return tagNames[i].name;
    }
    return "<unknown>";
}
/* tworzy typ MPI_PAKIET_T
*/

void inicjuj_typ_pakietu()
{
    /* Używamy MPI_INT dla wszystkich pól - ts, src, resource_id, data */
    /* Definiujemy tablice o rozmiarze NITEMS (zdefiniowanym w util.h) */
    int blocklengths[NITEMS];
    MPI_Datatype typy[NITEMS];
    MPI_Aint offsets[NITEMS];

    /* Wypełniamy tablice ręcznie, aby uniknąć problemów z inicjalizacją */
    for (int i = 0; i < NITEMS; i++) {
        blocklengths[i] = 1;
        typy[i] = MPI_INT; // Wszędzie przesyłamy inty
    }

    // Obliczamy offsety pól w strukturze packet_t
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, resource_id);
    offsets[3] = offsetof(packet_t, data);  

    MPI_Type_create_struct(NITEMS, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);
}

/* funkcja pomocnicza: wykonaj wysłanie wszystkich odłożonych ACK-ów do procesu src (wywoływane przy RELEASE) */
void send_deferred_acks_for(int src)
{
    pthread_mutex_lock(&deferredMut);
    if (deferred_ack && deferred_ack[src]) {
        deferred_ack[src] = 0;
        pthread_mutex_unlock(&deferredMut);
        sendPacket(0, src, ACK);
    } else {
        pthread_mutex_unlock(&deferredMut);
    }
}

void sendPacket(packet_t *pkt, int destination, int tag)
{
    int freepkt=0;
    if (pkt==0) {
        // używamy calloc ponieważ chcemy mieć pewność, że wszystkie pola są zerowe
        pkt = calloc(1, sizeof(packet_t));
        freepkt=1;
    }

    pkt->src = rank;

    // POPRAWKA: Podbijamy zegar tylko jeśli to nowa wiadomość (ts == 0 lub pusty pakiet)
    // Ale dla REQUEST musimy to zrobić ręcznie przed wysłaniem!
    pthread_mutex_lock(&clockMut);
    if(pkt->ts == 0) { // Zakładamy, że 0 to "pusty/nieustawiony"
        lamport_clock++;
        pkt->ts = lamport_clock;
    }
    pthread_mutex_unlock(&clockMut);

    // Ustawiamy domyślny resource_id jeśli pakiet był pusty (np. dla ACK)
    // Ale w pełnej implementacji będziemy to ustawiać ręcznie przed wywołaniem
    if(freepkt) pkt->resource_id = -999;

    MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);

    debug("Wysyłam %s do %d", tag2string(tag), destination);

    if (freepkt) free(pkt);
}

void changeState( state_t newState )
{
    pthread_mutex_lock( &stateMut );
    if (stan==InFinish) { 
	pthread_mutex_unlock( &stateMut );
        return;
    }
    stan = newState;
    pthread_mutex_unlock( &stateMut );
}
