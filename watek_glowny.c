#include "main.h"
#include "watek_glowny.h"
#include "util.h"

/* Funkcja sprawdza, na którym miejscu w kolejce jesteśmy */
/* resource_id = o co my walczymy (-1 to Pyrkon, 0..N to warsztaty) */
int check_priority(int my_ts, int my_resource) {
    int position = 0;

    for (int i = 0; i < size; i++) {
        if (i == rank) continue;
        if (tablica_zadan[i] == -1) continue; // Proces nie bierze udziału

        int other_resource = tablica_zasobow[i];
        int other_ts = tablica_zadan[i];

        // WARUNEK 1: Walczymy o WEJŚCIE NA PYRKON
        if (my_resource == REQ_PYRKON) {
            // Konkurujemy z tymi, co chcą wejść na Pyrkon (REQ_PYRKON)
            // ORAZ z tymi, co są na warsztatach (id >= 0), bo oni zajmują miejsce na Pyrkonie!

            if (other_resource == REQ_PYRKON) {
                // Standardowy Lamport
                if (other_ts < my_ts || (other_ts == my_ts && i < rank)) {
                    position++;
                }
            } else if (other_resource >= 0) {
                // Ktoś chce/jest na warsztacie -> czyli jest już na Pyrkonie.
                // On ZAWSZE blokuje miejsce komuś, kto dopiero chce wejść.
                position++;
            }
        }
            // WARUNEK 2: Walczymy o konkretny WARSZTAT
        else {
            // Konkurujemy TYLKO z tymi, co chcą TEN SAM warsztat
            if (other_resource == my_resource) {
                // Standardowy Lamport
                if (other_ts < my_ts || (other_ts == my_ts && i < rank)) {
                    position++;
                }
            }
        }
    }
    return position;
}

void mainLoop()
{
    srandom(rank);

    int my_request_time = -1;
    int current_resource = -999;
    int wybrany_warsztat = 0;
    int liczba_odwiedzonych = 0; // Licznik odwiedzonych warsztatów

    int tury = 0; // Licznik przeprowadzonych tur Pyrkonu

    while (stan != InFinish) {
        switch (stan) {
            case InRun:
                // --- SYNCHRONIZACJA TURY ---

                // 1. Czekamy, aż WSZYSCY zakończą poprzedni cykl i wrócą do InRun
                MPI_Barrier(MPI_COMM_WORLD);

                // 2. Tylko ROOT czeka na znak od użytkownika
                // if (rank == 0) {
                //     println("=== Wciśnij ENTER, aby rozpocząć PYRKON (Start Tury) ===");
                //     getchar();
                // }
                // println("Czekam na sygnał do rozpoczęcia tury...");
                for (int i = 0; i < size; i++) {
                    tablica_zadan[i] = -1;      // Zakładamy, że nikt nic nie chce
                    tablica_zasobow[i] = -999;  // Zakładamy, że nikt nigdzie nie jest
                }
                // 3. Czekamy, aż ROOT da sygnał (wciśnie Enter)
                MPI_Barrier(MPI_COMM_WORLD);

                // Zwiększamy licznik tur i sprawdzamy limit
                tury++;
                if (tury > PYRKON_TURY) {
                    println("Osiągnięto maksymalną liczbę tur (%d). Kończę symulację.", PYRKON_TURY);
                    changeState(InFinish);
                    break;
                }else{
                    println("=== Rozpoczynam TURĘ PYRKON %d ===", tury);
                }

                // --- START LOGIKI ---

                // Resetujemy licznik na nową turę
                liczba_odwiedzonych = 0;

                println("Chcę wejść na PYRKON (tura %d)", tury);

                current_resource = REQ_PYRKON;
                ackCount = 0;

                pthread_mutex_lock(&clockMut);
                lamport_clock++;
                my_request_time = lamport_clock;
                tablica_zadan[rank] = my_request_time;
                tablica_zasobow[rank] = current_resource;
                pthread_mutex_unlock(&clockMut);

                packet_t *pkt = calloc(1, sizeof(packet_t));
                pkt->ts = my_request_time;
                pkt->resource_id = current_resource;

                for (int i=0;i<size;i++)
                    if (i!=rank) sendPacket( pkt, i, REQUEST);

                free(pkt);
                changeState( InWant );
                break;

            case InWant:
                if (ackCount == size - 1) {
                    if (check_priority(my_request_time, REQ_PYRKON) < PYRKON_SLOTS) {
                        println("Wszedłem na teren PYRKONU!");
                        changeState(InSection);
                    }
                }
                break;

            case InSection:
                // Jesteśmy na Pyrkonie (w korytarzu), wybieramy warsztat.
                wybrany_warsztat = random() % WARSZTATY_COUNT;
                println("Jestem na Pyrkonie. Chcę iść na warsztat nr %d", wybrany_warsztat);

                ackCount = 0;
                current_resource = wybrany_warsztat;

                pthread_mutex_lock(&clockMut);
                lamport_clock++;
                my_request_time = lamport_clock;
                // WAŻNE: Aktualizujemy tablicę, ale NIE zwalniamy miejsca na Pyrkonie (nadpisujemy wpis)
                tablica_zadan[rank] = my_request_time;
                tablica_zasobow[rank] = current_resource;
                pthread_mutex_unlock(&clockMut);

                packet_t *pkt_w = calloc(1, sizeof(packet_t));
                pkt_w->ts = my_request_time;
                pkt_w->resource_id = current_resource;

                for (int i=0;i<size;i++)
                    if (i!=rank) sendPacket( pkt_w, i, REQUEST);
                free(pkt_w);

                changeState(InWantWorkshop);
                break;

            case InWantWorkshop:
                if (ackCount == size - 1) {
                    if (check_priority(my_request_time, current_resource) < WARSZTAT_SLOTS) {
                        println("Wszedłem na WARSZTAT nr %d", current_resource);
                        changeState(InWorkshop);
                    }
                }
                break;

            case InWorkshop:
                sleep(1); // Krótka symulacja pracy na warsztacie
                liczba_odwiedzonych++;
                println("Koniec warsztatu %d. Odwiedziłem już %d.", current_resource, liczba_odwiedzonych);

                /* --- LOGIKA PĘTLI WARSZTATOWEJ --- */
                // Jeśli odwiedziliśmy mniej niż 2 warsztaty, idziemy na kolejny
                if (liczba_odwiedzonych < 2  || ((random() % 100) < 50) && (liczba_odwiedzonych < WARSZTATY_COUNT)) {
                    println("Chcę iść na kolejny warsztat!");

                    // Wracamy do InSection, aby wylosować nowy warsztat i wysłać nowe żądanie.
                    // Nowe żądanie (REQUEST) nadpisze stare w tablicach innych procesów,
                    // co automatycznie zwolni nas z obecnego warsztatu, ale zachowa miejsce na Pyrkonie.
                    changeState(InSection);
                }
                else {
                    // Odwiedziliśmy wystarczająco dużo, wychodzimy z Pyrkonu
                    println("Opuszczam Pyrkon (zaliczyłem %d warsztatów).", liczba_odwiedzonych);

                    tablica_zadan[rank] = -1;
                    tablica_zasobow[rank] = -999;

                    packet_t *pkt_rel = calloc(1, sizeof(packet_t));
                    pkt_rel->ts = lamport_clock;
                    pkt_rel->resource_id = -1;

                    for (int i=0;i<size;i++)
                        if (i!=rank) sendPacket( pkt_rel, i, RELEASE);
                    free(pkt_rel);

                    // Po zwolnieniu wracamy do InRun, gdzie trafimy na Barierę
                    changeState(InRun);
                }
                break;

            default:
                break;
        }
    }
}