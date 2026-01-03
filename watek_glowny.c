#include "main.h"
#include "watek_glowny.h"
#include "util.h"

/* Funkcja sprawdza, na którym miejscu w kolejce jesteśmy */
int check_priority(int my_ts, int my_resource) {
    int position = 0;

    /* ZMIANA: Zamykamy dostęp do tablic na czas sprawdzania */
    pthread_mutex_lock(&tablicaMut);

    for (int i = 0; i < size; i++) {
        if (i == rank) continue;
        if (tablica_zadan[i] == -1) continue;

        int other_resource = tablica_zasobow[i];
        int other_ts = tablica_zadan[i];

        // WARUNEK 1: Walczymy o WEJŚCIE NA PYRKON
        if (my_resource == REQ_PYRKON) {
            if (other_resource == REQ_PYRKON) {
                if (other_ts < my_ts || (other_ts == my_ts && i < rank)) {
                    position++;
                }
            } else if (other_resource >= 0) {
                // Ktoś na warsztacie też zajmuje miejsce na Pyrkonie
                position++;
            }
        }
            // WARUNEK 2: Walczymy o konkretny WARSZTAT
        else {
            if (other_resource == my_resource) {
                if (other_ts < my_ts || (other_ts == my_ts && i < rank)) {
                    position++;
                }
            }
        }
    }

    /* ZMIANA: Zwalniamy zamek */
    pthread_mutex_unlock(&tablicaMut);

    return position;
}

/* ... funkcja print_history bez zmian ... */

void mainLoop()
{
    srandom(rank);
    /* ... zmienne lokalne bez zmian ... */
    int my_request_time = -1;
    int current_resource = -999;
    int wybrany_warsztat = 0;
    int liczba_odwiedzonych = 0;
    int historia_warsztatow[10];

    int tury = 0; // Licznik przeprowadzonych tur Pyrkonu

    while (stan != InFinish) {
        switch (stan) {
            case InRun:
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
                liczba_odwiedzonych = 0;
                for(int i=0; i<10; i++) historia_warsztatow[i] = -1;
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

    


                println("Chcę wejść na PYRKON (tura %d)", tury);
                println("Stan: InRun. Zgłaszam chęć wejścia na PYRKON.");

                current_resource = REQ_PYRKON;
                ackCount = 0;

                pthread_mutex_lock(&clockMut);
                lamport_clock++;
                my_request_time = lamport_clock;
                pthread_mutex_unlock(&clockMut);

                /* ZMIANA: Zabezpieczamy zapis własnego żądania do tablicy współdzielonej */
                pthread_mutex_lock(&tablicaMut);
                tablica_zadan[rank] = my_request_time;
                tablica_zasobow[rank] = current_resource;
                pthread_mutex_unlock(&tablicaMut);
                /* KONIEC ZMIANY */

                debug("Wysyłam REQ o Pyrkon (Zegar: %d)", my_request_time);

                packet_t *pkt = calloc(1, sizeof(packet_t));
                pkt->ts = my_request_time;
                pkt->resource_id = current_resource;
                for (int i=0;i<size;i++) if (i!=rank) sendPacket( pkt, i, REQUEST);
                free(pkt);
                changeState( InWant );
                break;

            case InWant:
                /* ... (bez zmian - check_priority ma już w środku mutex) ... */
                if (ackCount == size - 1) {
                    int pos = check_priority(my_request_time, REQ_PYRKON);
                    if (pos < PYRKON_SLOTS) {
                        println("Stan: InWant -> InSection. Sukces! Wchodzę na teren PYRKONU.");
                        changeState(InSection);
                    }
                }
                break;

            case InSection:
                wybrany_warsztat = random() % WARSZTATY_COUNT;
                if (liczba_odwiedzonych > 0) print_history(historia_warsztatow, liczba_odwiedzonych);

                println("Stan: InSection. Wybieram warsztat nr %d.", wybrany_warsztat);

                ackCount = 0;
                current_resource = wybrany_warsztat;

                pthread_mutex_lock(&clockMut);
                lamport_clock++;
                my_request_time = lamport_clock;
                pthread_mutex_unlock(&clockMut);

                /* ZMIANA: Zabezpieczamy zmianę własnego zasobu w tablicy */
                pthread_mutex_lock(&tablicaMut);
                tablica_zadan[rank] = my_request_time;
                tablica_zasobow[rank] = current_resource;
                pthread_mutex_unlock(&tablicaMut);
                /* KONIEC ZMIANY */

                debug("Wysyłam REQ o warsztat %d (Zegar: %d)", wybrany_warsztat, my_request_time);

                packet_t *pkt_w = calloc(1, sizeof(packet_t));
                pkt_w->ts = my_request_time;
                pkt_w->resource_id = current_resource;
                for (int i=0;i<size;i++) if (i!=rank) sendPacket( pkt_w, i, REQUEST);
                free(pkt_w);

                changeState(InWantWorkshop);
                break;

            case InWantWorkshop:
                /* ... (bez zmian) ... */
                if (ackCount == size - 1) {
                    int pos = check_priority(my_request_time, current_resource);
                    if (pos < WARSZTAT_SLOTS) {
                        println("Stan: InWantWorkshop -> InWorkshop. Dostałem się na WARSZTAT nr %d.", current_resource);
                        changeState(InWorkshop);
                    }
                }
                break;

            case InWorkshop:
                /* ... (logika warsztatu bez zmian) ... */
                historia_warsztatow[liczba_odwiedzonych] = current_resource;
                liczba_odwiedzonych++;

                /* --- LOGIKA PĘTLI WARSZTATOWEJ --- */
                // Jeśli odwiedziliśmy mniej niż 2 warsztaty, idziemy na kolejny
                if (liczba_odwiedzonych < 2  || ((random() % 100) < 50) && (liczba_odwiedzonych < WARSZTATY_COUNT)) {
                    println("Chcę iść na kolejny warsztat!");

                if (liczba_odwiedzonych < 2) {
                    println("Decyzja: Wracam na korytarz.");
                    changeState(InSection);
                }
                else {
                    println("Decyzja: Koniec wycieczki. Opuszczam Pyrkon.");

                    /* ZMIANA: Zabezpieczamy czyszczenie własnego wpisu */
                    pthread_mutex_lock(&tablicaMut);
                    tablica_zadan[rank] = -1;
                    tablica_zasobow[rank] = -999;
                    pthread_mutex_unlock(&tablicaMut);
                    /* KONIEC ZMIANY */

                    packet_t *pkt_rel = calloc(1, sizeof(packet_t));
                    pkt_rel->ts = lamport_clock;
                    pkt_rel->resource_id = -1;
                    for (int i=0;i<size;i++) if (i!=rank) sendPacket( pkt_rel, i, RELEASE);
                    free(pkt_rel);

                    changeState(InRun);
                }
                break;

            default: break;
        }
    }
}