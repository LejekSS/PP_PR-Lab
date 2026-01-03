#include "main.h"
#include "util.h"
MPI_Datatype MPI_PAKIET_T;

/* 
 * w util.h extern state_t stan (czyli zapowiedź, że gdzieś tam jest definicja
 * tutaj w util.c state_t stan (czyli faktyczna definicja)
 */
state_t stan=InRun;

/* zamek wokół zmiennej współdzielonej między wątkami. 
 * Zwróćcie uwagę, że każdy proces ma osobą pamięć, ale w ramach jednego
 * procesu wątki współdzielą zmienne - więc dostęp do nich powinien
 * być obwarowany muteksami
 */
pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;

/* DODANE: Definicje zmiennych globalnych */
int lamport_clock = 0;
pthread_mutex_t clockMut = PTHREAD_MUTEX_INITIALIZER;
int ackCount = 0;
pthread_mutex_t ackMut = PTHREAD_MUTEX_INITIALIZER;
/* DODANE: Definicja muteksu tablicy */
pthread_mutex_t tablicaMut = PTHREAD_MUTEX_INITIALIZER;
int *tablica_zasobow;

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

int *tablica_zadan;

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

    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    /* Jeśli NITEMS == 4, to znaczy że dodałeś resource_id w util.h */
    if (NITEMS >= 3) offsets[2] = offsetof(packet_t, resource_id); // Zakładam, że zmieniłeś nazwę data na resource_id lub dodałeś nowe pole
    /* Uwaga: Dostosuj offsety do swojej struktury packet_t w util.h! */
    /* Poniżej bezpieczniejsza wersja, która zakłada, że masz 4 pola w struct packet_t */

    // Upewnij się, że ta sekcja pasuje do struct packet_t w util.h:
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, resource_id); // Jeśli masz to pole
    offsets[3] = offsetof(packet_t, data);        // Jeśli masz to pole

    MPI_Type_create_struct(NITEMS, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);
}

void sendPacket(packet_t *pkt, int destination, int tag)
{
    int freepkt=0;
    if (pkt==0) {
        // ZMIANA: używamy calloc zamiast malloc
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

    // Zmieniam debug na println zeby widziec zegar (jesli makro to obsluguje)
    // Ale na razie zostawmy debug
    debug("Wysyłam %s do %d z zegarem %d", tag2string(tag), destination, pkt->ts);

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
