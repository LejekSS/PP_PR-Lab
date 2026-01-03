#include "main.h"
#include "watek_komunikacyjny.h"
#include "util.h"

/* ... nagłówki ... */

void *startKomWatek(void *ptr)
{
    MPI_Status status;
    packet_t pakiet;

    while ( stan!=InFinish ) {
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        // Aktualizacja zegara Lamporta
        pthread_mutex_lock(&clockMut);
        if (pakiet.ts > lamport_clock) lamport_clock = pakiet.ts;
        lamport_clock++;
        pthread_mutex_unlock(&clockMut);

        switch ( status.MPI_TAG ) {
            case REQUEST:
                // SEKCJA KRYTYCZNA: Zapis do tablic
                pthread_mutex_lock(&tablicaMut);
                tablica_zadan[pakiet.src] = pakiet.ts;
                tablica_zasobow[pakiet.src] = pakiet.resource_id;
                pthread_mutex_unlock(&tablicaMut);
                // KONIEC SEKCJI

                sendPacket( 0, status.MPI_SOURCE, ACK );
                break;

            case RELEASE:
                // SEKCJA KRYTYCZNA: Zapis do tablic
                pthread_mutex_lock(&tablicaMut);
                tablica_zadan[pakiet.src] = -1;
                pthread_mutex_unlock(&tablicaMut);
                // KONIEC SEKCJI
                break;

            case ACK:
                pthread_mutex_lock(&ackMut);
                ackCount++;
                pthread_mutex_unlock(&ackMut);
                break;

            default:
                break;
        }
    }
}