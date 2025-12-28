#include <iostream>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <string> 
#include <thread>
#include <windows.h> // Needed for Colors

using namespace std;

#pragma comment(lib, "ws2_32.lib")

// --- COLOR PALETTE ---
// 10 = Green (ME - Typing)
// 12 = Red (Errors)
// 14 = Yellow (Server Info)
// 13 = Pink (Private Messages - EXCLUSIVE)
// 8  = Gray (Status updates like "Message sent")

// --- USER COLORS (For Others - COOL COLORS ONLY) ---
// 9  = Blue 
// 11 = Cyan 
// 3  = Dark Aqua
// 1  = Dark Blue

void SetColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// FIX: Removed Pink(13) so Users never look like PMs.
int GetUserColor(string username) {
    int sum = 0;
    for (char c : username) {
        sum += c;
    }

    // We only pick from COOL colors for users (Blues/Greens)
    // This keeps them separate from the Warm colors (Red/Yellow/Pink) used for the System.
    int safeColors[] = { 9, 11, 3, 1 };

    return safeColors[sum % 4];
}

bool Initialize() {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

// THREAD 1: SENDING (Your Typing)
void SendMsg(SOCKET s) {
    string message;
    while (1) {
        // FORCE GREEN for your typing
        SetColor(10);

        getline(cin, message);

        // Reset to default white immediately after
        SetColor(7);

        if (message == "quit") {
            break;
        }
        send(s, message.c_str(), message.length(), 0);
    }
    closesocket(s);
    WSACleanup();
}

// THREAD 2: RECEIVING (Incoming Messages)
void ReceiveMsg(SOCKET s) {
    char buffer[4096];
    int recvlength;
    while (1) {
        recvlength = recv(s, buffer, sizeof(buffer), 0);
        if (recvlength <= 0) {
            SetColor(12); // Red
            cout << "Disconnected from server" << endl;
            SetColor(7);
            break;
        }
        string msg = string(buffer, recvlength);

        // --- SMART COLOR LOGIC ---

        // 1. ERRORS -> RED
        if (msg.find("Failed") != string::npos ||
            msg.find("taken") != string::npos ||
            msg.find("Please") != string::npos ||
            msg.find("not found") != string::npos ||
            msg.find("Error") != string::npos) {
            SetColor(12); // RED
        }
        // 2. PRIVATE MESSAGES -> PINK (Exclusive)
        else if (msg.find("[PM]") != string::npos) {
            SetColor(13); // PINK
        }
        // 3. SERVER INFO -> YELLOW
        else if (msg.find("Welcome") != string::npos ||
            msg.find("Login Successful") != string::npos ||
            msg.find("Registered") != string::npos ||
            msg.find("Online Users") != string::npos) {
            SetColor(14); // YELLOW
        }
        // 4. STATUS UPDATES -> GRAY (New!)
        // This fixes the clash with Priya/Cyan
        else if (msg.find("Message sent to") != string::npos) {
            SetColor(8); // GRAY
        }
        // 5. PUBLIC CHAT (Usernames) -> COOL COLORS
        else if (msg.find(":") != string::npos) {
            string username = msg.substr(0, msg.find(":"));
            SetColor(GetUserColor(username)); // Pick Blue/Cyan/Aqua
        }
        // 6. FALLBACK -> CYAN
        else {
            SetColor(11);
        }

        cout << msg << endl;
        SetColor(7); // Reset to white

        // FORCE GREEN AGAIN (In case you were typing when a message arrived)
        SetColor(10);
    }
    closesocket(s);
    WSACleanup();
}

int main() {
    if (!Initialize()) return 1;

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return 1;

    sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(12345);
    inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr));

    if (connect(s, reinterpret_cast<sockaddr*>(&serveraddr), sizeof(serveraddr)) == SOCKET_ERROR) {
        cout << "Could not connect to server!" << endl;
        return 1;
    }

    // Launch threads
    thread senderthread(SendMsg, s);
    thread receiver(ReceiveMsg, s);

    senderthread.join();
    receiver.join();

    return 0;
}