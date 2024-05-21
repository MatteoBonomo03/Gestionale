#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma comment(lib, "ws2_32.lib")
#include <iostream>
#include <WinSock2.h>
#include <string>
#include <fstream>
#include <sstream>
#include "lib/sqlite3.h"
#include "lib/json.hpp"
#include<cstdlib>

using namespace std;

int connessioni = 0;
int sessione[300] = { };
string database = "Dati.db";
const char* DB = database.c_str();

struct rispostaJSON {
    string risposta;
    string header = "HTTP/1.1 200 OK\n";
    string content = "Content-Type: application/json\r\n\r\n";
    string OK = header + content + "{\"mssg\":\"OK\"}";
};

struct matrici {
    string risultato[200] = { "" };
    string prodotti[8] = {"tabella", "codice", "descriz", "quantita", "prezunit", "aliq", "categoria", "id"};
    string vendite[9] = {"tabella", "codice", "quantven", "nomeimp", "data", "ora", "cliente", "status", "id"};
    string clienti[10] = {"tabella", "nome", "piva", "paese", "citt", "indir", "civix", "tel", "email", "id"};
    string utenti[6] = {"tabella", "username", "password", "nome", "cogn", "id"};
    string log[4] = {"user", "pass"};
    string primari[2] = {"azione", "token"};
    string chiaviPrim[4] = {"state", "aliq", "stato", "categoria"};
    string entSin[5] = {"tabella", "valore", "id", "chiv", "rowid"};
    string entit[8] = {"prodotti", "utenti", "clienti", "vendite", "status", "aliquote", "paesi", "categorie"};
    string mod[10] = { "" };
    string tmpQuery;
    string guidaInser;
    int id;
};

rispostaJSON ris;
matrici mat;

void DefRis(SOCKET& nuovoWSocket, string ris) {
    int datiInviati = send(nuovoWSocket, ris.c_str(), ris.size(), 0);
    if (datiInviati < 0) {
        cerr << "Risposta non inviata\n";
    }
}

void httpStatico(SOCKET& nuovoWSocket) {
    ifstream in_file("index.html");
    stringstream html;
    html << in_file.rdbuf();
    in_file.close();
    string statico = html.str();

    DefRis(nuovoWSocket, statico);
}

string riumoviHeader(string& buff, string trova) {
    string completo = buff;
    size_t nohead = completo.find(trova);
    if (nohead != string::npos)
        completo = completo.substr(nohead);

    return completo;
}

void errore(SOCKET& nuovoWSocket, string chiave) {
    string erroreMess = "HTTP/1.1 200 OK\nContent-Type: application/json\r\n\r\n{\""+chiave+"\":\"err\"}";
    DefRis(nuovoWSocket, erroreMess);
}

bool leggiUniversale(string& buff, matrici* mat, int numChiav, string* pARR) {
    string completo = riumoviHeader(buff, "{");

    nlohmann::json j = nlohmann::json::parse(completo);

    for (int i = 0; i < numChiav; ++i) {
        nlohmann::json pre = j[*(pARR + i)];
        mat->risultato[i] = (string)pre;
    }
    return true;
}

bool eseguiUniversale(string esegui) {
    char* err;
    sqlite3* db;
    sqlite3_open(DB, &db);

    int rc = sqlite3_exec(db, esegui.c_str(), NULL, NULL, &err);
    sqlite3_close(db);
    if (rc != SQLITE_OK) {
        cerr << "Errore: " << err << endl;
        return false;

    }
    return true;
}

static int leggiDB(void* data, int numeroAttributi, char** valori, char** colonne) {
    rispostaJSON *ris = (rispostaJSON *)data;
    ris->risposta += "{";

    for (int i = 0; i < numeroAttributi; i++) {
        ris->risposta += "\""+ (string)colonne[i]+"\":\""+ (string)valori[i] +"\",";
    }
    ris->risposta = ris->risposta.substr(0, ris->risposta.length() - 1);
    ris->risposta = ris->risposta +"},";
    
    return 0; 
}

int generaToken(int& connessioni, int (&sessione)[300]) {

    srand((unsigned) time(NULL));
    int nuovoToken = rand();

    sessione[connessioni] = nuovoToken;

    connessioni++;
    return nuovoToken;
}

bool controlloCredenziali(string* pARR) {

    char* err;
    sqlite3* db;
    sqlite3_stmt* stmt;

    sqlite3_open(DB, &db);

    string sel = "SELECT username, password FROM utenti;";
    sqlite3_prepare_v2(db, sel.c_str(), -1, &stmt, 0);

    while (sqlite3_step(stmt) != SQLITE_DONE) {
        
        string utente = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        string passw = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (*(pARR + 0) == utente && *(pARR + 1) == passw) {
            sqlite3_reset(stmt);    //Per precauzione
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return true;  
        }
    }
    
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    
    return false;
}

bool autentTokenSingolo(string& buff2, int (&sessione)[300], string* pARR, matrici& mat) {
    leggiUniversale(buff2, &mat, 1, &mat.primari[1]);
    for (int i = 0; i < connessioni; ++i) {
        if (sessione[i] == stoi(*pARR) && stoi(*pARR) != 0)
            return true;
    }

    return false;
}

//La struttura static int è obbligatoria (libreria sqlite)
static int modElog(void* data, int numeroAttributi, char** valori, char** colonne) {  //viene chiamata ad ogni istanza del DB
    matrici *mat = (matrici *)data;

    for (int i = 1; i < numeroAttributi; i++) {
        if (valori[i] != mat->risultato[i]) {
            mat->tmpQuery += " " + (string)colonne[i] + " = \""+ mat->risultato[i] +"\", ";
        }
    }

    return 0;
}

bool modificaDB(string& str) {
    char* err;
    sqlite3* db;

    sqlite3_open(DB, &db);

    int rc = sqlite3_exec(db, str.c_str(), modElog, &mat, &err);
    sqlite3_close(db);
    if (rc != SQLITE_OK) {
        return false;
    }

    mat.tmpQuery = mat.tmpQuery.substr(0, mat.tmpQuery.length() - 2);

    string daEseg = "UPDATE " + mat.risultato[0] + " SET " + mat.tmpQuery + " WHERE id = \"" + mat.risultato[mat.id - 1] + "\";";

    mat.tmpQuery = "";

    if (eseguiUniversale(daEseg) == true) {
        return true;
    }
    else {
        return false;
    }
}

string queryEntitSingola(rispostaJSON& ris, string sel) {
    ris.risposta = "";

    char* err;
    sqlite3* db;

    sqlite3_open(DB, &db);

    int rc = sqlite3_exec(db, sel.c_str(), leggiDB, &ris, &err);
    sqlite3_close(db);
    if (rc != SQLITE_OK) {
        cerr << "Errore: " << err << endl;
    }

    size_t formatta = ris.risposta.find(']');
    ris.risposta = ris.risposta.substr(0, ris.risposta.length() - 1);
    ris.risposta = ris.header + ris.content + "[" +ris.risposta + "]";

    return ris.risposta;
}

bool logout(int (&sessione)[300]) {
    for (int i = 0; i < connessioni; ++i) {
        if (sessione[i] == stoi(mat.risultato[0])) {
            sessione[i] = 0;
            connessioni--;
        }
    }
    return true;
}


int main() {
    ris.risposta = "";

    SOCKET wsocket;
    SOCKET nuovoWSocket;
    WSADATA wsaData;
    struct sockaddr_in server;
    int server_len;
    int buffer_size = 30720;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Errore inizializzazione del server\n";
        return 1;
    }

    wsocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (wsocket == INVALID_SOCKET) {
        cerr << "Errore nella creazione del SOCKET\n";
        WSACleanup();
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;    //Per indirizzo locale INADDR_LOOPBACK
    server.sin_port = htons(8080);
    server_len = sizeof(server);
    if (bind(wsocket, (SOCKADDR*)&server, server_len) != 0) {
        cerr << "Errore durante operazione di binding\n";
        closesocket(wsocket);
        WSACleanup();
        return 1;
    }

    if (listen(wsocket, SOMAXCONN) != 0) {
        cerr << "Errore durante operazione di ascolto\n";
        closesocket(wsocket);
        WSACleanup();
        return 1;
    }

    cout << "Ascoltando: 127.0.0.1:8080\n";

    int bytes = 0;
    while (true) {
        nuovoWSocket = accept(wsocket, (SOCKADDR*)&server, &server_len);
        if (nuovoWSocket == INVALID_SOCKET) {
            cerr << "Non accettato\n";
            continue;
        }


        char buff[30720] = { 0 };
        bytes = recv(nuovoWSocket, buff, buffer_size, 0);
        if (bytes < 0) {
            cerr << "Errore nell'accettazione della richiesta\n";
        }
        string buff2 = buff;

        if (buff2.find("\"sec\":\"INVIO\"") != string::npos) {

            DefRis(nuovoWSocket, ris.header); 

            leggiUniversale(buff2, &mat, 1, &mat.primari[0]);

            switch(stoi(mat.risultato[0])) {
                case 0: {
                    leggiUniversale(buff2, &mat, 2, &mat.log[0]);
                    if (controlloCredenziali(&mat.risultato[0])) {
                        int acc = generaToken(connessioni, sessione);
                        string ok = ris.content + "{\"token\":"+to_string(acc)+"}";
                        DefRis(nuovoWSocket, ok);
                    }
                    else {
                        errore(nuovoWSocket, "token");
                    }
                    break;                    
                }
                case 1: {
                    if (autentTokenSingolo(buff2, sessione, &mat.risultato[0], mat) == true) {
                        leggiUniversale(buff2, &mat, 1, &mat.entSin[0]);
                        for (int i = 4; i < 8; ++i) {
                            if (mat.risultato[0] == mat.entit[i]) {
                                leggiUniversale(buff2, &mat, 2, &mat.entSin[0]);
                                string str = "INSERT INTO " + mat.risultato[0] + " VALUES (\""+ mat.risultato[1] + "\");";
                                if(eseguiUniversale(str) == true) {
                                    DefRis(nuovoWSocket, ris.OK);
                                    break;
                                }
                                else {
                                    errore(nuovoWSocket, "mssg");
                                    break;
                                }
                                break;
                            }
                        }

                        int nAttr = 0;
                        bool poli = false;
                        string str;
                        for (int i = 0; i < 4; ++i) {
                            if (mat.risultato[0] == mat.entit[i]) {
                                if (mat.risultato[0] == mat.entit[0]) {
                                    leggiUniversale(buff2, &mat, 7, &mat.prodotti[0]);
                                    nAttr = 7;
                                    poli = true;
                                    mat.guidaInser = "(id, codice, descriz, quantita, prezunit, aliq, categoria)";
                                    break;
                                }
                                else if (mat.risultato[0] == mat.entit[1]) {
                                    leggiUniversale(buff2, &mat, 5, &mat.utenti[0]);
                                    nAttr = 5;
                                    poli = true;
                                    mat.guidaInser = "(id, username, password, nome, cogn)";
                                    break;                                           
                                }
                                else if (mat.risultato[0] == mat.entit[2]) {
                                    leggiUniversale(buff2, &mat, 9, &mat.clienti[0]);
                                    nAttr = 9;
                                    poli = true;
                                    mat.guidaInser = "(id, nome, piva, paese, citt, indir, civix, tel, email)";
                                    break;                                            
                                }
                                else if (mat.risultato[0] == mat.entit[3]) {
                                    leggiUniversale(buff2, &mat, 8, &mat.vendite[0]);
                                    nAttr = 8;
                                    poli = true;
                                    mat.guidaInser = "(id, codice, quantven, nomeimp, data, ora, cliente, status)";
                                    break;                                            
                                }
                                else {
                                    errore(nuovoWSocket, "mssg");
                                    break;
                                }
                                
                            }
                        }
                        string str1;
                        if (poli) {
                            for (int i = 1; i < nAttr; i++) {
                                str1 += mat.prodotti[i] + ", ";
                                str += "\"" +mat.risultato[i] + "\", ";
                            }
                            str1 = str1.substr(0, str1.length() - 2);
                            str = str.substr(0, str.length() - 2);
                            str1 = "INSERT INTO " + mat.risultato[0] + " "+ mat.guidaInser +" VALUES (NULL, " + str + ");";

                            if (eseguiUniversale(str1) == true) {
                                DefRis(nuovoWSocket, ris.OK);
                            }
                            else{
                                errore(nuovoWSocket, "mssg");
                            }
                            break;
                        }
                    }
                    else {
                        errore(nuovoWSocket, "mssg");
                    }
                    break;
                }
                case 2: {
                    bool poli = false;
                    if (autentTokenSingolo(buff2, sessione, &mat.risultato[0], mat) == true) {
                        leggiUniversale(buff2, &mat, 1, &mat.entSin[0]);
                        for (int i = 4; i < 8; ++i) {
                            if (mat.risultato[0] == mat.entit[i]) {
                                string str = "UPDATE " + mat.risultato[0] + " SET " + mat.chiaviPrim[i - 4] + " = \"";
                                leggiUniversale(buff2, &mat, 1, &mat.entSin[1]);
                                str += mat.risultato[0] + "\" WHERE rowid = ";
                                leggiUniversale(buff2, &mat, 1, &mat.entSin[4]);
                                str += mat.risultato[0] + ";";
                                if (eseguiUniversale(str) == true) {
                                    DefRis(nuovoWSocket, ris.OK);
                                    break;  
                                }
                                else {
                                    errore(nuovoWSocket, "mssg");
                                    break;                                    
                                }
                            }
                        }
                        for (int i = 0; i < 4; ++i) {
                            if (mat.risultato[0] == mat.entit[i]) {
                                if (mat.risultato[0] == mat.entit[0]) {
                                    leggiUniversale(buff2, &mat, 8, &mat.prodotti[0]);
                                    poli = true;
                                    mat.id = sizeof(mat.prodotti) / sizeof(*mat.prodotti);
                                    break;
                                }
                                else if (mat.risultato[0] == mat.entit[1]) {
                                    leggiUniversale(buff2, &mat, 6, &mat.utenti[0]);
                                    poli = true;
                                    mat.id = sizeof(mat.utenti) / sizeof(*mat.utenti);
                                    break;                                           
                                }
                                else if (mat.risultato[0] == mat.entit[2]) {
                                    leggiUniversale(buff2, &mat, 10, &mat.clienti[0]);
                                    poli = true;
                                    mat.id = sizeof(mat.clienti) / sizeof(*mat.clienti);
                                    break;                                            
                                }
                                else if (mat.risultato[0] == mat.entit[3]) {
                                    leggiUniversale(buff2, &mat, 9, &mat.vendite[0]);
                                    poli = true;
                                    mat.id = sizeof(mat.vendite) / sizeof(*mat.vendite);
                                    break;                                            
                                }
                                else {
                                    errore(nuovoWSocket, "mssg");
                                    break;
                                }
                                
                            }
                        }
                        if (poli) {
                            string str = "SELECT * FROM " + mat.risultato[0] +" WHERE id = \""+ mat.risultato[mat.id - 1] +"\";";
                            if(modificaDB(str) == true) {
                                DefRis(nuovoWSocket, ris.OK);
                            }
                            else {
                                errore(nuovoWSocket, "mssg");
                            }
                        }
                    }
                    else {
                        errore(nuovoWSocket, "mssg");
                    }
                    break;
                }
                case 3: {
                    if (autentTokenSingolo(buff2, sessione, &mat.risultato[0], mat) == true) {
                        leggiUniversale(buff2, &mat, 1, &mat.entSin[0]);
                        for (int i = 0; i < 8; ++i) {
                            if (mat.entit[i] == mat.risultato[0]) {
                                leggiUniversale(buff2, &mat, 2, &mat.entSin[0]);    //PRAGMA foreign_keys=ON; \n
                                string elimNuple = "DELETE FROM " + mat.risultato[0] + " WHERE rowid = \""+ mat.risultato[1] +"\"";
                                if(eseguiUniversale(elimNuple) == true) {
                                    DefRis(nuovoWSocket, ris.OK);
                                }
                                else {
                                    errore(nuovoWSocket, "mssg");
                                }
                            }
                        }
                    }
                    else {
                        errore(nuovoWSocket, "mssg");
                    }
                    break;
                }
                case 4: {
                    if (autentTokenSingolo(buff2, sessione, &mat.risultato[0], mat) == true) {
                        leggiUniversale(buff2, &mat, 2, &mat.entSin[0]);
                        string sel = "SELECT " + mat.risultato[1] + " FROM "+ mat.risultato[0] +";";
                        queryEntitSingola(ris, sel);
                        DefRis(nuovoWSocket, ris.risposta);
                    }
                    else {
                        errore(nuovoWSocket, "mssg");
                    }
                    break;
                }
                case 5: {
                    leggiUniversale(buff2, &mat, 1, &mat.primari[1]);
                        if (logout(sessione) != true) {
                            errore(nuovoWSocket, "mssg");
                        }
                        else {
                            DefRis(nuovoWSocket, ris.OK);
                        }
                    break;  
                }
                default: {
                    errore(nuovoWSocket, "mssg");
                }
            }   
        }
        else{
            httpStatico(nuovoWSocket);
        }
        closesocket(nuovoWSocket);
    }

    closesocket(wsocket);
    WSACleanup();

    return 0;
}

