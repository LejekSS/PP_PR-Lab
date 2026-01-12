#include "main.h"
#include "watek_glowny.h"
#include "util.h"

// zwraca pozycję w kolejce do zasobu
int check_priority(int my_ts, int my_resource) {
    int position = 0;

    pthread_mutex_lock(&tablicaMut);

    for (int i = 0; i < size; i++) {
        if (i == rank) continue;
        if (tablica_zadan[i] == -1) continue;

        int other_resource = tablica_zasobow[i];
        int other_ts = tablica_zadan[i];

        //pyrkon
        if (my_resource == REQ_PYRKON) {
            //ci co chca pyrkon
            if (other_resource == REQ_PYRKON) {
                if (other_ts < my_ts || (other_ts == my_ts && i < rank)) {
                    position++;
                }
            } 
            //ci co chca warsztat
            else if (other_resource >= 0) {
                position++;
            }
        }
        //warsztat
        else {
            if (other_resource == my_resource) {
                if (other_ts < my_ts || (other_ts == my_ts && i < rank)) {
                    position++;
                }
            }
        }
    }
    pthread_mutex_unlock(&tablicaMut);

    return position;
}

void mainLoop()
{
    srandom(rank);

    int my_request_time = -1;
    int current_resource = -999;
    int wybrany_warsztat = 0;
    int liczba_odwiedzonych = 0; //warsztatów
    int tury = 0;

    while (stan != InFinish) {
        switch (stan) {
            case InRun:
                
                MPI_Barrier(MPI_COMM_WORLD);

                
                // Resetujemy tablice zadan i zasobow
                pthread_mutex_lock(&tablicaMut);
                for (int i = 0; i < size; i++) {
                    tablica_zadan[i] = -1;      // Zakładamy, że nikt nic nie chce
                    tablica_zasobow[i] = -999;  // Zakładamy, że nikt nigdzie nie jest
                }
                pthread_mutex_unlock(&tablicaMut);
                liczba_odwiedzonych = 0;
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


                // Pyrkon start
                

                println("Chcę wejść na PYRKON (tura %d)", tury);

                current_resource = REQ_PYRKON;
                pthread_mutex_lock(&ackMut);
                ackCount = 0;
                pthread_mutex_unlock(&ackMut);

                pthread_mutex_lock(&clockMut);
                lamport_clock++;
                my_request_time = lamport_clock;
                pthread_mutex_lock(&tablicaMut);
                tablica_zadan[rank] = my_request_time;
                tablica_zasobow[rank] = current_resource;
                pthread_mutex_unlock(&tablicaMut);
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
                pthread_mutex_lock(&ackMut);
                int localAck = ackCount;
                pthread_mutex_unlock(&ackMut);
                if (localAck == size - 1) {
                    if (check_priority(my_request_time, REQ_PYRKON) < PYRKON_SLOTS) {
                        // println("Wszedłem na teren PYRKONU!");
                        changeState(InSection);
                    }
                }
                break;

            case InSection:
                // wybieramy losowy warsztat
                wybrany_warsztat = random() % WARSZTATY_COUNT;
                println("Jestem na Pyrkonie. Chcę iść na warsztat nr %d", wybrany_warsztat);

                pthread_mutex_lock(&ackMut);
                ackCount = 0;
                pthread_mutex_unlock(&ackMut);
                current_resource = wybrany_warsztat;

                pthread_mutex_lock(&clockMut);
                lamport_clock++;
                my_request_time = lamport_clock;
                pthread_mutex_lock(&tablicaMut);
                tablica_zadan[rank] = my_request_time;
                tablica_zasobow[rank] = current_resource;
                pthread_mutex_unlock(&tablicaMut);
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
                pthread_mutex_lock(&ackMut);
                localAck = ackCount;
                pthread_mutex_unlock(&ackMut);
                if (localAck == size - 1) {
                    if (check_priority(my_request_time, current_resource) < WARSZTAT_SLOTS) {
                        println("Wszedłem na WARSZTAT nr %d", current_resource);
                        changeState(InWorkshop);
                    }
                }
                break;

            case InWorkshop:
                sleep(1); //warsztatujemy
                liczba_odwiedzonych++;
                println("Koniec warsztatu %d. Odwiedziłem już %d.", current_resource, liczba_odwiedzonych);

                // decyzja czy iść na kolejny warsztat czy opuścić Pyrkon
                if (liczba_odwiedzonych < 2  || ((random() % 100) < 50) && (liczba_odwiedzonych < WARSZTATY_COUNT)) {
                    println("Chcę iść na kolejny warsztat!");
                    changeState(InSection);
                }
                else {
                    println("Opuszczam Pyrkon (zaliczyłem %d warsztatów).", liczba_odwiedzonych);

                    pthread_mutex_lock(&tablicaMut);
                    tablica_zadan[rank] = -1;
                    tablica_zasobow[rank] = -999;
                    pthread_mutex_unlock(&tablicaMut);

                    packet_t *pkt_rel = calloc(1, sizeof(packet_t));
                    pkt_rel->ts = lamport_clock;
                    pkt_rel->resource_id = -1;

                    println("Wysyłam RELEASE (zwalniam miejsce)");

                    for (int i=0;i<size;i++)
                        if (i!=rank) sendPacket( pkt_rel, i, RELEASE);
                    free(pkt_rel);

                    changeState(InRun);
                }
                break;

            default:
                break;
        }
    }
}