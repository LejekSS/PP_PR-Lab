#!/bin/bash

# Domyślna komenda uruchomienia
CMD="make run"
UTIL_FILE="util.h"

# --- ZCZYTYWANIE LIMITÓW ---
if [ ! -f "$UTIL_FILE" ]; then
    if [ -f "PP_PR-Lab/$UTIL_FILE" ]; then
        UTIL_FILE="PP_PR-Lab/$UTIL_FILE"
    fi
fi

L_PYRKON=5
L_WARSZTAT=2

if [ -f "$UTIL_FILE" ]; then
    FOUND_PYRKON=$(grep "#define PYRKON_SLOTS" "$UTIL_FILE" | awk '{print $3}')
    FOUND_WARSZTAT=$(grep "#define WARSZTAT_SLOTS" "$UTIL_FILE" | awk '{print $3}')

    if [[ "$FOUND_PYRKON" =~ ^[0-9]+$ ]]; then L_PYRKON=$FOUND_PYRKON; fi
    if [[ "$FOUND_WARSZTAT" =~ ^[0-9]+$ ]]; then L_WARSZTAT=$FOUND_WARSZTAT; fi
fi

if [ "$#" -gt 0 ]; then CMD="$@"; fi

echo "Uruchamiam: $CMD"
sleep 1

# --- GŁÓWNA PĘTLA AWK ---
$CMD 2>&1 | awk -v LIMIT_PYRKON="$L_PYRKON" -v LIMIT_WARSZTAT="$L_WARSZTAT" '
BEGIN {
    # Konfiguracja
    TIMEOUT_SEC = 5
    START_TIME = systime()
    ERR_COUNT = 0

    # Kolory
    C_RESET = "\033[0m"
    C_RED = "\033[31m"
    C_GREEN = "\033[32m"
    C_YELLOW = "\033[33m"
    C_BLUE = "\033[34m"
    C_BOLD = "\033[1m"
    C_CYAN = "\033[36m"
    C_GRAY = "\033[90m"

    # Inicjalizacja
    split("", seen)
    split("", states)
    split("", resources)
    split("", clocks)
    split("", current_workshop)
    split("", error_history)
    
    # Flagi błędów
    err_pyrkon_active = 0
    split("", err_workshop_active)
    split("", err_zombie_active)
}

function add_error(msg) {
    curr_time = systime() - START_TIME
    time_str = sprintf("[+%ds]", curr_time)
    ERR_COUNT++
    error_history[ERR_COUNT] = sprintf("%s %s", time_str, msg)
}

{
    # 1. Parsowanie
    gsub(/\x1b\[[0-9;]*m/, "", $0) # usuwanie kolorów z outputu C

    if (match($0, /\[([0-9]+)\]:/, arr)) {
        rank = arr[1]
        seen[rank] = 1
        last_update[rank] = systime()
        
        msg_content = substr($0, RSTART + RLENGTH)
        sub(/^ */, "", msg_content)
        last_msg[rank] = msg_content
    }

    if (match($0, /zegar(em)? ([0-9]+)/, m)) { clocks[rank] = m[2] }

    # Maszyna stanów
    if ($0 ~ /Chcę wejść na PYRKON/) {
        states[rank] = "WANT_PYRKON"; resources[rank] = "Brama"
    } else if ($0 ~ /Wszedłem na teren PYRKONU/) {
        states[rank] = "IN_PYRKON"; resources[rank] = "Korytarz"
    } else if (match($0, /Chcę iść na warsztat nr ([0-9]+)/, m)) {
        states[rank] = "WANT_WORKSHOP"; resources[rank] = "Warsztat " m[1]
    } else if (match($0, /Wszedłem na WARSZTAT nr ([0-9]+)/, m)) {
        states[rank] = "IN_WORKSHOP"; resources[rank] = "Warsztat " m[1]; current_workshop[rank] = m[1]
    } else if ($0 ~ /Koniec warsztatu/) {
        states[rank] = "IN_PYRKON"; resources[rank] = "Korytarz"; delete current_workshop[rank]
    } else if ($0 ~ /Opuszczam Pyrkon/) {
        states[rank] = "RELEASED"; resources[rank] = "-"; delete current_workshop[rank]
    } else if ($0 ~ /Kończę symulację/ || $0 ~ /finish/) {
        states[rank] = "FINISHED"; resources[rank] = "DOM"
    }

    # 2. Logika i Limity
    count_pyrkon = 0
    delete count_workshop_users

    for (r in seen) {
        if (states[r] == "IN_PYRKON" || states[r] == "WANT_WORKSHOP" || states[r] == "IN_WORKSHOP") {
            count_pyrkon++
        }
        if (states[r] == "IN_WORKSHOP") {
            w_id = current_workshop[r]
            count_workshop_users[w_id]++
        }
        
        if (systime() - last_update[r] > TIMEOUT_SEC && states[r] != "FINISHED") {
            if (err_zombie_active[r] == 0) {
                add_error(sprintf("TIMEOUT: Proces %d nie odpowiada!", r))
                err_zombie_active[r] = 1
            }
        } else { err_zombie_active[r] = 0 }
    }

    if (count_pyrkon > LIMIT_PYRKON) {
        if (err_pyrkon_active == 0) {
            add_error(sprintf("PRZEPEŁNIENIE PYRKONU! Jest %d/%d", count_pyrkon, LIMIT_PYRKON))
            err_pyrkon_active = 1
        }
    } else { err_pyrkon_active = 0 }

    for (w_id in count_workshop_users) {
        if (count_workshop_users[w_id] > LIMIT_WARSZTAT) {
            if (err_workshop_active[w_id] == 0) {
                add_error(sprintf("PRZEPEŁNIENIE WARSZTATU %d! Jest %d/%d", w_id, count_workshop_users[w_id], LIMIT_WARSZTAT))
                err_workshop_active[w_id] = 1
            }
        } else { err_workshop_active[w_id] = 0 }
    }

    # 3. RYSOWANIE
    printf "\033[2J\033[H"
    
    # --- TABELA 1: GŁÓWNA ---
    printf "%sMONITOR PYRKONU (P=%d, W=%d)%s\n", C_BOLD, LIMIT_PYRKON, LIMIT_WARSZTAT, C_RESET
    print "----------------------------------------------------------------"
    printf "%-6s | %-15s | %-12s | %-6s | %s\n", "RANK", "STAN", "ZASÓB", "ZEGAR", "OSTATNIA AKCJA"
    print "----------------------------------------------------------------"

    n = asorti(seen, sorted_ranks)
    for (i = 1; i <= n; i++) {
        r = sorted_ranks[i]
        color = C_RESET
        if (states[r] == "IN_WORKSHOP") color = C_GREEN
        else if (states[r] == "IN_PYRKON") color = C_BLUE
        else if (states[r] == "WANT_PYRKON" || states[r] == "WANT_WORKSHOP") color = C_YELLOW
        else if (states[r] == "FINISHED") color = C_GRAY
        else if (states[r] == "RELEASED") color = "\033[37m"
        
        if (err_zombie_active[r] == 1) color = C_RED

        msg_disp = substr(last_msg[r], 1, 35)
        printf "%s%-6d | %-15s | %-12s | %-6s | %s%s\n", 
            color, r, states[r], resources[r], clocks[r], msg_disp, C_RESET
    }
    print "----------------------------------------------------------------"

    # --- TABELA 2: WARSZTATY (ZGRUPOWANE I ŁADNE) ---
    # Zbieranie danych
    delete workshop_groups
    has_users = 0
    for (i = 1; i <= n; i++) {
        r = sorted_ranks[i]
        if (states[r] == "IN_WORKSHOP") {
            w_id = current_workshop[r]
            if (workshop_groups[w_id] == "") {
                workshop_groups[w_id] = r
            } else {
                workshop_groups[w_id] = workshop_groups[w_id] ", " r
            }
            has_users = 1
        }
    }

    print ""
    printf "%s>>> AKTYWNE WARSZTATY <<<%s\n", C_CYAN, C_RESET
    print "----------------------------------------------------------------"
    # Nagłówek dopasowany stylem do tabeli wyżej (szerokość kolumn dobrana wizualnie)
    printf "%-15s | %s\n", "NR WARSZTATU", "LISTA PROCESÓW (RANK)"
    print "----------------------------------------------------------------"

    if (has_users == 0) {
         printf "%s%-15s | %s%s\n", C_GRAY, "-", "Brak aktywnych warsztatów", C_RESET
    } else {
        # Sortowanie kluczy (ID warsztatów)
        num_w = asorti(workshop_groups, sorted_wids)
        for (j = 1; j <= num_w; j++) {
            w_id = sorted_wids[j]
            # Formatowanie wiersza
            printf "%s%-15s | %s%s\n", C_GREEN, w_id, workshop_groups[w_id], C_RESET
        }
    }
    print "----------------------------------------------------------------"

    # --- TABELA 3: HISTORIA BŁĘDÓW ---
    if (ERR_COUNT > 0) {
        print ""
        printf "%s>>> HISTORIA BŁĘDÓW <<<%s\n", C_RED C_BOLD, C_RESET
        print "----------------------------------------------------------------"
        for (i = 1; i <= ERR_COUNT; i++) {
            print C_RED error_history[i] C_RESET
        }
        print "----------------------------------------------------------------"
    } else {
        print ""
        print C_GREEN "System stabilny." C_RESET
    }

    # Stopka
    printf "\nBieżące obłożenie Pyrkonu: "
    if (count_pyrkon > LIMIT_PYRKON) printf "%s%d/%d (PRZEPEŁNIENIE!)%s\n", C_RED, count_pyrkon, LIMIT_PYRKON, C_RESET
    else printf "%s%d/%d%s\n", C_GREEN, count_pyrkon, LIMIT_PYRKON, C_RESET
}'