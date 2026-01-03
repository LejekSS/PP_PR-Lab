#!/bin/bash

# Konfiguracja limitów (zgodnie z Twoim opisem)
PYRKON_LIMIT=5
WARSZTAT_LIMIT=2

# Liczba procesów MPI do uruchomienia
MPI_PROCS=8

# Limit czasu na jedno uruchomienie (w sekundach) - zabezpieczenie przed deadlockiem
TIMEOUT_SEC=30

if [ -z "$1" ]; then
  echo "Użycie: $0 <liczba_powtorzen>"
  exit 1
fi

REPEAT_COUNT=$1

# 1. Kompilacja
echo "=== Kompilacja projektu ==="
make
if [ $? -ne 0 ]; then
    echo "Błąd kompilacji! Prerywam."
    exit 1
fi

# 2. Tworzenie skryptu weryfikującego w Pythonie (analiza logów)
cat <<EOF > verify_log.py
import sys
import re

# Limity
PYRKON_MAX = $PYRKON_LIMIT
WORKSHOP_MAX = $WARSZTAT_LIMIT

# Stan aktualny
on_pyrkon = 0
on_workshop = {} # słownik: {id_warsztatu: liczba_osob}

# Regexy dopasowujące komunikaty z Twojego kodu
# Ignorujemy kolory i znaczniki czasu, szukamy kluczowych fraz
re_enter_pyrkon = re.compile(r"Wszedłem na teren PYRKONU")
re_leave_pyrkon = re.compile(r"Opuszczam Pyrkon")
re_enter_workshop = re.compile(r"Wszedłem na WARSZTAT nr (\d+)")
re_leave_workshop = re.compile(r"Koniec warsztatu (\d+)")

error_found = False

for line in sys.stdin:
    # Wypisujemy linię na ekran, żebyś widział co się dzieje
    sys.stdout.write(line)
    
    # Analiza wejścia na Pyrkon
    if re_enter_pyrkon.search(line):
        on_pyrkon += 1
        if on_pyrkon > PYRKON_MAX:
            print(f"\n[BŁĄD] Przekroczono limit Pyrkonu! Jest {on_pyrkon}/{PYRKON_MAX}")
            error_found = True

    # Analiza wyjścia z Pyrkonu
    elif re_leave_pyrkon.search(line):
        on_pyrkon -= 1
        if on_pyrkon < 0:
            print(f"\n[BŁĄD] Ujemna liczba osób na Pyrkonie!")
            error_found = True

    # Analiza wejścia na Warsztat
    match_enter = re_enter_workshop.search(line)
    if match_enter:
        w_id = int(match_enter.group(1))
        current = on_workshop.get(w_id, 0) + 1
        on_workshop[w_id] = current
        if current > WORKSHOP_MAX:
            print(f"\n[BŁĄD] Przekroczono limit na warsztacie {w_id}! Jest {current}/{WORKSHOP_MAX}")
            error_found = True

    # Analiza końca warsztatu
    match_leave = re_leave_workshop.search(line)
    if match_leave:
        w_id = int(match_leave.group(1))
        current = on_workshop.get(w_id, 0) - 1
        on_workshop[w_id] = current
        if current < 0:
            print(f"\n[BŁĄD] Ujemna liczba osób na warsztacie {w_id}!")
            error_found = True

if error_found:
    sys.exit(1) # Kod błędu dla Basha
else:
    sys.exit(0)
EOF

# 3. Pętla testowa
echo "=== Rozpoczynam testy ($REPEAT_COUNT powtórzeń) ==="

for ((i=1; i<=REPEAT_COUNT; i++)); do
    echo "---------------------------------------------------"
    echo "TEST #$i / $REPEAT_COUNT"
    
    # Uruchamiamy mpirun z timeoutem, a wyjście przekazujemy do skryptu Pythona
    # 2>&1 łączy stderr z stdout, żeby analizować też błędy
    timeout $TIMEOUT_SEC mpirun -oversubscribe -np $MPI_PROCS ./main 2>&1 | python3 verify_log.py
    
    # Sprawdzamy kody wyjścia
    PIPE_STATUS=("${PIPESTATUS[@]}")
    MPI_EXIT=${PIPE_STATUS[0]} # Kod wyjścia timeout/mpirun
    VERIFY_EXIT=${PIPE_STATUS[1]} # Kod wyjścia skryptu python

    if [ $MPI_EXIT -eq 124 ]; then
        echo -e "\n[TIMEOUT] Program zawiesił się (deadlock)!"
        rm verify_log.py
        exit 1
    elif [ $MPI_EXIT -ne 0 ]; then
        echo -e "\n[MPI ERROR] Program zakończył się błędem (kod $MPI_EXIT)."
        rm verify_log.py
        exit 1
    fi

    if [ $VERIFY_EXIT -ne 0 ]; then
        echo -e "\n[LOGIC ERROR] Wykryto naruszenie zasad dostępu do zasobów!"
        rm verify_log.py
        exit 1
    fi

    echo "Test #$i ZALICZONY."
done

echo "==================================================="
echo "Wszystkie testy zakończone sukcesem!"
rm verify_log.py