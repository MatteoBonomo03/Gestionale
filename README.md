ESEMPIO DI GESTIONALE DI MAGAZZINO

Per compilare:
g++ Gestionale.cpp -o Gestionale -lws2_32 -std=c++11 sqlite3.o

Per il progetto sono importate le librerie: sqlite3.h per la gestione del database e json.hpp (nlohmann-json)
per la gestione delle richieste JSON da parte dei clients.
I socket sono gestiti utilizzando WinSock2.h (fornita di default da windows).

L'applicazione consiste in un server creato in c++ programmato per rispondere a specifiche richieste da parte dei clients.
Eseguito il programma permette a tutti gli utenti appartenenti ad una rete locale di accedere al database condiviso attraverso
un'interfaccia grafica disponibile all'indirizzo del server alla porta 8080.

Le operazioni possibili per la gestione del database sono: inserimento, eliminazione, modifica e lettura di dati.

Per accedere alla piattaforma client:
Username: Impiegato1
Password: 1234
