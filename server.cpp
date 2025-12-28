/*
 * Project: C++ Multi-Client Chat Server
 * Description: A multi-threaded TCP server using Winsock & SQLite.
 * Handles concurrent clients via select() I/O multiplexing.
 */

#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <vector>
#include <string>
#include <sstream>
#include <map> 
#include <algorithm> 
#include "sqlite3.h"

using namespace std;

// Link against the Windows Socket library
#pragma comment(lib, "ws2_32.lib")

// Global Database Connection & Session Map
sqlite3* db;
map<SOCKET, string> connectedUsers;

// Initialize Windows Socket API (WSA)
bool Initialize() {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

// Ensure Database exists and USERS table is ready
void CheckDatabase() {
    sqlite3_open("chat_users.db", &db);
    string sql = "CREATE TABLE IF NOT EXISTS USERS("
        "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
        "USERNAME TEXT NOT NULL UNIQUE, "
        "PASSWORD TEXT NOT NULL);";
    char* msgError;
    sqlite3_exec(db, sql.c_str(), NULL, 0, &msgError);
}

// Database Helper: Add new user
bool RegisterUser(string username, string password) {
    string sql = "INSERT INTO USERS (USERNAME, PASSWORD) VALUES ('" + username + "', '" + password + "');";
    char* msgError;
    int exit = sqlite3_exec(db, sql.c_str(), NULL, 0, &msgError);
    if (exit != SQLITE_OK) {
        sqlite3_free(msgError); // Likely a duplicate username
        return false;
    }
    return true;
}

// Database Helper: Verify credentials
bool LoginUser(string username, string password) {
    string sql = "SELECT * FROM USERS WHERE USERNAME='" + username + "' AND PASSWORD='" + password + "';";
    sqlite3_stmt* stmt;

    // Prepare and execute the query
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0) != SQLITE_OK) return false;

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) found = true;

    sqlite3_finalize(stmt);
    return found;
}

// Check if user is already logged in (Prevent double login)
bool IsUserOnline(string username) {
    for (auto const& [sock, name] : connectedUsers) {
        if (name == username) return true;
    }
    return false;
}

int main() {
    if (!Initialize()) return 1;
    CheckDatabase();

    // Setup TCP Listener on Port 12345
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(12345);
    inet_pton(AF_INET, "0.0.0.0", &serveraddr.sin_addr);

    if (bind(listenSocket, (sockaddr*)&serveraddr, sizeof(serveraddr)) == SOCKET_ERROR) return 1;
    listen(listenSocket, SOMAXCONN);

    cout << "Server started! Waiting for connections..." << endl;

    // --- NON-BLOCKING I/O SETUP ---
    // The 'master' set holds all active sockets (Listener + Clients)
    fd_set master;
    FD_ZERO(&master);
    FD_SET(listenSocket, &master);

    while (true) {
        fd_set reads = master; // Copy master set (select() modifies it)

        // Wait for ANY socket to be ready (Read/Connect)
        if (select(0, &reads, nullptr, nullptr, nullptr) < 0) break;

        for (u_int i = 0; i < reads.fd_count; i++) {
            SOCKET sock = reads.fd_array[i];

            // 1. New Connection Handling
            if (sock == listenSocket) {
                SOCKET client = accept(listenSocket, nullptr, nullptr);
                FD_SET(client, &master); // Add to watcher list

                string msg = "Welcome! Use: /register [name] [pass] OR /login [name] [pass]\r\n";
                send(client, msg.c_str(), msg.length(), 0);
                cout << "New connection: " << client << endl;
            }
            // 2. Incoming Message Handling
            else {
                char buf[4096];
                memset(buf, 0, 4096);
                int bytesIn = recv(sock, buf, 4096, 0);

                // Check for Disconnect
                if (bytesIn <= 0) {
                    closesocket(sock);
                    FD_CLR(sock, &master);

                    // Log the event if user was logged in
                    if (connectedUsers.find(sock) != connectedUsers.end()) {
                        string name = connectedUsers[sock];
                        cout << name << " disconnected." << endl;
                        connectedUsers.erase(sock);
                    }
                    else {
                        cout << "Client #" << sock << " disconnected (Guest)." << endl;
                    }
                }
                else {
                    string msg(buf);

                    // Pre-process for command parsing (Keep Original + Lowercase Copy)
                    string lowerMsg = msg;
                    transform(lowerMsg.begin(), lowerMsg.end(), lowerMsg.begin(), ::tolower);

                    size_t cmdPos = lowerMsg.find("/");
                    if (cmdPos != string::npos) {
                        msg = msg.substr(cmdPos);
                        lowerMsg = lowerMsg.substr(cmdPos);
                    }

                    // --- COMMAND HANDLERS ---

                    // [ /register ]
                    if (lowerMsg.find("/register") == 0) {
                        stringstream ss(msg);
                        string cmd, user, pass;
                        ss >> cmd >> user >> pass;
                        if (RegisterUser(user, pass)) {
                            string s = "Registered! Now please /login.\r\n";
                            send(sock, s.c_str(), s.length(), 0);
                        }
                        else {
                            string s = "Username taken.\r\n";
                            send(sock, s.c_str(), s.length(), 0);
                        }
                    }
                    // [ /login ]
                    else if (lowerMsg.find("/login") == 0) {
                        stringstream ss(msg);
                        string cmd, user, pass;
                        ss >> cmd >> user >> pass;

                        if (IsUserOnline(user)) {
                            string s = "Login Failed: User already online!\r\n";
                            send(sock, s.c_str(), s.length(), 0);
                        }
                        else if (LoginUser(user, pass)) {
                            connectedUsers[sock] = user; // Associate Socket -> User
                            string s = "Login Successful! Welcome, " + user + ".\r\n";
                            send(sock, s.c_str(), s.length(), 0);
                            cout << user << " logged in." << endl;
                        }
                        else {
                            string s = "Login Failed: Wrong user/pass.\r\n";
                            send(sock, s.c_str(), s.length(), 0);
                        }
                    }
                    // [ /who ]
                    else if (lowerMsg.find("/who") == 0) {
                        string s = "Online Users:\r\n";
                        for (auto const& [key, val] : connectedUsers) {
                            s += "- " + val + "\r\n";
                        }
                        send(sock, s.c_str(), s.length(), 0);
                    }
                    // [ /msg ] - Private Message
                    else if (lowerMsg.find("/msg") == 0 || lowerMsg.find("/message") == 0) {
                        stringstream ss(msg);
                        string cmd, targetName, content;
                        ss >> cmd >> targetName;
                        getline(ss, content);

                        bool sent = false;
                        // Find the target socket by username
                        for (auto const& [targetSock, username] : connectedUsers) {
                            if (username == targetName) {
                                string finalMsg = "[PM] " + connectedUsers[sock] + ":" + content + "\r\n";
                                send(targetSock, finalMsg.c_str(), finalMsg.length(), 0);
                                sent = true;
                                break;
                            }
                        }
                        if (sent) {
                            string s = "Message sent to " + targetName + ".\r\n";
                            send(sock, s.c_str(), s.length(), 0);
                        }
                        else {
                            string s = "User not found or offline.\r\n";
                            send(sock, s.c_str(), s.length(), 0);
                        }
                    }
                    // [ Broadcast ] - Normal Chat
                    else {
                        if (connectedUsers.find(sock) != connectedUsers.end()) {
                            string username = connectedUsers[sock];
                            string fullMsg = username + ": " + msg + "\r\n";

                            // Send to everyone except the sender and listener
                            for (u_int j = 0; j < master.fd_count; j++) {
                                SOCKET outSock = master.fd_array[j];
                                if (outSock != listenSocket && outSock != sock) {
                                    send(outSock, fullMsg.c_str(), fullMsg.length(), 0);
                                }
                            }
                        }
                        else {
                            string s = "Please /login first.\r\n";
                            send(sock, s.c_str(), s.length(), 0);
                        }
                    }
                }
            }
        }
    }
    sqlite3_close(db);
    WSACleanup();
    return 0;
}