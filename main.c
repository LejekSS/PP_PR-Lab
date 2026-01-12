#include "main.h"
#include "watek_glowny.h"
#include "watek_komunikacyjny.h"


int rank, size;
int* deferred_ack = NULL;
int* tablica_zadan = NULL;
int* tablica_zasobow = NULL;
pthread_mutex_t deferredMut = PTHREAD_MUTEX_INITIALIZER;

pthread_t threadKom;

void finalizuj()
{
    pthread_mutex_destroy( &stateMut);
    /* Czekamy, aż wątek potomny się zakończy */
    println("czekam na wątek \"komunikacyjny\"\n" );
    pthread_join(threadKom,NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    
    if (tablica_zadan) free(tablica_zadan);
    if (tablica_zasobow) free(tablica_zasobow);
    if (deferred_ack) free(deferred_ack);
    
    MPI_Finalize();
}
void check_thread_support(int provided)
{
    printf("THREAD SUPPORT: chcemy %d. Co otrzymamy?\n", provided);
    switch (provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
            MPI_Finalize();
            exit(-1);
            break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	    break;
        case MPI_THREAD_SERIALIZED: 
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	    break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n");
	    break;
        default: printf("Nikt nic nie wie\n");
    }
}


int main(int argc, char **argv)
{
    MPI_Status status;
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);
    srand(rank);
    inicjuj_typ_pakietu(); 
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //Inicjalizacja tablicy żądań
    tablica_zadan = (int*)malloc(sizeof(int) * size);
    tablica_zasobow = (int*)malloc(sizeof(int) * size);
    deferred_ack = (int*)malloc(sizeof(int) * size);
    for (int i = 0; i < size; i++) deferred_ack[i] = 0;
    for(int i=0; i<size; i++) tablica_zasobow[i] = -999; // Coś co nie jest ani Pyrkonem, ani warsztatem
    for(int i=0; i<size; i++) tablica_zadan[i] = -2; // -1 oznacza: ten proces nie ubiega się o zasób

    pthread_create( &threadKom, NULL, startKomWatek , 0);

    //wywolujemy glowna petle z pliku watek_glowny.c
    mainLoop(); 
    
    finalizuj();
    return 0;
}
