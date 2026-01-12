#include "main.h"
#include "watek_komunikacyjny.h"
#include "util.h"

void *startKomWatek(void *ptr)
{
    MPI_Status status;
    packet_t pakiet;

    while ( stan!=InFinish ) {
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        // Aktualizacja zegara Lamporta (Max + 1)
        pthread_mutex_lock(&clockMut);
        if (pakiet.ts > lamport_clock) lamport_clock = pakiet.ts;
        lamport_clock++;
        pthread_mutex_unlock(&clockMut);

        switch ( status.MPI_TAG ) {
            case REQUEST:
                // debug("Otrzymałem REQ od %d...", pakiet.src);

                // Aktualizujemy tablice wiedzy o innych
                // ZABEZPIECZAMY ZAPIS DO TABLIC
                pthread_mutex_lock(&tablicaMut);
                tablica_zadan[pakiet.src] = pakiet.ts;
                tablica_zasobow[pakiet.src] = pakiet.resource_id;
                pthread_mutex_unlock(&tablicaMut);

                // Odsyłamy TYLKO JEDEN pakiet ACK
                debug("Otrzymano REQUEST od %d (res=%d, ts=%d)", pakiet.src, pakiet.resource_id, pakiet.ts);
                sendPacket( 0, status.MPI_SOURCE, ACK );
                break;

            case RELEASE:
                // Ktoś wyszedł, więc czyścimy jego wpisy w tablicy
                // ZABEZPIECZAMY CZYSZCZENIE WPISU (zarówno żądanie jak i zasób)
                debug("Otrzymano RELEASE od %d (res=%d, ts=%d)", pakiet.src, pakiet.resource_id, pakiet.ts);
                pthread_mutex_lock(&tablicaMut);
                tablica_zadan[pakiet.src] = -1;
                tablica_zasobow[pakiet.src] = -999;
                pthread_mutex_unlock(&tablicaMut);
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