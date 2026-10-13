#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static int g_wsa_init = 0;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <pthread.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void sockclose(int s);
int socksend(int s, void *dataa, int siz);

int sockinit_dest(short port, const char *ip, int retry_ms)
{
#ifdef _WIN32
    if (!g_wsa_init) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2), &wsa);
        g_wsa_init = 1;
    }
#endif
    int retries = 0;
    while(retries < 30){
        int sock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
        int optval = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&optval, sizeof(optval));
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&optval, sizeof(optval));
        int sndbuf = 262144;
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&sndbuf, sizeof(sndbuf));
#endif
        struct sockaddr_in servaddr;
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);
        servaddr.sin_addr.s_addr = inet_addr(ip);
        if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        {
            retries++;
            if (retries >= 30) {
                fprintf(stderr, "connect to %s:%d failed after %d retries, giving up.\n", ip, port, retries);
                fflush(stderr);
                sockclose(sock);
                return -1;
            }
#ifdef _WIN32
            fprintf(stderr, "connect to %s:%d failed (WSAError: %d), retry %d/%d...\n", ip, port, WSAGetLastError(), retries, 30);
#else
            perror("connect");
#endif
            fflush(stderr);
            sockclose(sock);
#ifdef _WIN32
            Sleep(retry_ms);
#else
            usleep(retry_ms*1000);
#endif
            continue;
        }
        printf("[TCP] Connected to %s:%d\n", ip, port);
        fflush(stdout);
        return sock;
    }
    return -1;
}

int sockinit(short port)
{
#ifdef _WIN32
    char pipename[64];
    snprintf(pipename, sizeof(pipename), "\\\\.\\pipe\\gpssim_%d", port);
    g_pipe = CreateNamedPipeA(
        pipename,
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        262144,
        262144,
        0,
        NULL
    );
    if (g_pipe == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "CreateNamedPipe failed: %lu\n", GetLastError());
        exit(1);
    }
    fprintf(stderr, "Waiting for pipe connection on %s ...\n", pipename);
    fflush(stderr);
    if (!ConnectNamedPipe(g_pipe, NULL))
    {
        if (GetLastError() != ERROR_PIPE_CONNECTED)
        {
            fprintf(stderr, "ConnectNamedPipe failed: %lu\n", GetLastError());
            exit(1);
        }
    }
    fprintf(stderr, "Pipe connected!\n");
    fflush(stderr);
    return 1;
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect");
        exit(1);
    }
    return sock;
#endif
}

void sockclose(int s){
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

int socksend(int s, void *dataa, int siz){
    char *ptr = (char *)dataa;
    int remaining = siz;
    while (remaining > 0) {
#ifdef _WIN32
        int sent = send(s, ptr, remaining, 0);
        if (sent == SOCKET_ERROR) return -1;
#else
        int sent = (int)send(s, ptr, remaining, 0);
        if (sent < 0) return -1;
#endif
        if (sent == 0) return -1;
        ptr += sent;
        remaining -= sent;
    }
    return 0;
}

long int timem(){
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER ui;
    GetSystemTimeAsFileTime(&ft);
    ui.LowPart = ft.dwLowDateTime;
    ui.HighPart = ft.dwHighDateTime;
    return (long int)((ui.QuadPart / 10000) - 11644473600000LL);
#else
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000+t.tv_usec/1000;
#endif
}

int udpinit(short port){
#ifdef _WIN32
    if (!g_wsa_init) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2), &wsa);
        g_wsa_init = 1;
    }
#endif
    int sock;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;
    for(int retry=0; retry<30; retry++){
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        int opt=1; setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
        if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr))>=0)
            return sock;
#ifdef _WIN32
        int err=WSAGetLastError();
        if(err!=WSAEADDRINUSE){ // only retry on addr-in-use
            fprintf(stderr, "bind failed: %d\n", err);
            closesocket(sock);
            return -1;
        }
        fprintf(stderr, "bind port %d busy, retrying...\n", port);
        fflush(stderr);
        closesocket(sock);
        Sleep(500);
#else
        perror("bind");
        closesocket(sock);
        return -1;
#endif
    }
    fprintf(stderr, "bind port %d failed after 30 retries\n", port);
    return -1;
}

int udprecv(int s, void *dataa, int siz){
    struct sockaddr from;
#ifdef _WIN32
    int fromlen = sizeof(from);
#else
    socklen_t fromlen = sizeof(from);
#endif
    return recvfrom(s, dataa, siz, 0, &from, &fromlen);
}

double llhr[3]={0,0,0};
int udp_received=0;

void threadrecv(void *p){
    short port=*(short*)p;
    int s=udpinit(port);
    if(s<0){
        fprintf(stderr, "threadrecv: failed to bind UDP port %d\n", port);
        return;
    }
    printf("listening on port %d\n",port);
    while(1){
        udprecv(s,llhr,3*sizeof(double));
        udp_received=1;
    }
}
