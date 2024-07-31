#include <bits/stdc++.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <signal.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_LEN 200
#define NUM_COLORS 6

using namespace std;

struct terminal {
    int id;
    string name;
    SOCKET socket;
    thread th;
};

vector<terminal> clients;
string def_col = "\033[0m";
string colors[] = { "\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m" };
int seed = 0;
mutex cout_mtx, clients_mtx;

string color(int code);
void set_name(int id, char name[]);
void shared_print(string str, bool endLine);
int broadcast_message(string message, int sender_id);
int broadcast_message(int num, int sender_id);
void end_connection(int id);
void handle_client(SOCKET client_socket, int id);

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed. Error: " << WSAGetLastError() << endl;
        exit(-1);
    }

    SOCKET server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        perror("socket: ");
        WSACleanup();
        exit(-1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(10000);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        perror("bind error: ");
        closesocket(server_socket);
        WSACleanup();
        exit(-1);
    }

    if (listen(server_socket, 8) == SOCKET_ERROR) {
        perror("listen error: ");
        closesocket(server_socket);
        WSACleanup();
        exit(-1);
    }

    struct sockaddr_in client;
    SOCKET client_socket;
    int len = sizeof(client);

    cout << colors[NUM_COLORS - 1] << "\n\t  ====== Welcome to the chat-room ======   " << endl << def_col;

    while (1) {
        if ((client_socket = accept(server_socket, (struct sockaddr*)&client, &len)) == INVALID_SOCKET) {
            perror("accept error: ");
            closesocket(server_socket);
            WSACleanup();
            exit(-1);
        }
        seed++;
        thread t(handle_client, client_socket, seed);
        lock_guard<mutex> guard(clients_mtx);
        clients.push_back({ seed, string("Anonymous"), client_socket, move(t) });
    }

    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].th.joinable())
            clients[i].th.join();
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}

string color(int code) {
    return colors[code % NUM_COLORS];
}

// Set name of client
void set_name(int id, char name[]) {
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].id == id) {
            clients[i].name = string(name);
        }
    }
}

// For synchronisation of cout statements
void shared_print(string str, bool endLine = true) {
    lock_guard<mutex> guard(cout_mtx);
    cout << str;
    if (endLine)
        cout << endl;
}

// Broadcast message to all clients except the sender
int broadcast_message(string message, int sender_id) {
    char temp[MAX_LEN];
    strcpy(temp, message.c_str());
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].id != sender_id) {
            send(clients[i].socket, temp, sizeof(temp), 0);
        }
    }
    return 0;
}

// Broadcast a number to all clients except the sender
int broadcast_message(int num, int sender_id) {
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].id != sender_id) {
            send(clients[i].socket, (char*)&num, sizeof(num), 0);
        }
    }
    return 0;
}

void end_connection(int id) {
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].id == id) {
            lock_guard<mutex> guard(clients_mtx);
            clients[i].th.detach();
            closesocket(clients[i].socket);
            clients.erase(clients.begin() + i);
            break;
        }
    }
}

void handle_client(SOCKET client_socket, int id) {
    char name[MAX_LEN], str[MAX_LEN];
    recv(client_socket, name, sizeof(name), 0);
    set_name(id, name);

    // Display welcome message
    string welcome_message = string(name) + string(" has joined");
    broadcast_message("#NULL", id);
    broadcast_message(id, id);
    broadcast_message(welcome_message, id);
    shared_print(color(id) + welcome_message + def_col);

    while (1) {
        int bytes_received = recv(client_socket, str, sizeof(str), 0);
        if (bytes_received <= 0)
            return;
        if (strcmp(str, "#exit") == 0) {
            // Display leaving message
            string message = string(name) + string(" has left");
            broadcast_message("#NULL", id);
            broadcast_message(id, id);
            broadcast_message(message, id);
            shared_print(color(id) + message + def_col);
            end_connection(id);
            return;
        }
        broadcast_message(string(name), id);
        broadcast_message(id, id);
        broadcast_message(string(str), id);
        shared_print(color(id) + name + " : " + def_col + str);
    }
}
