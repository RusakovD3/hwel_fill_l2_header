#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>

#define SERVER_IP "192.168.0.21"
#define CLIENT_IP "192.168.0.18"
#define SERVER_PORT 8080
#define CLIENT_PORT 8090
#define BUFFER_SIZE 1024


#define DEST_MAC0 0x08
#define DEST_MAC1 0x00
#define DEST_MAC2 0x27
#define DEST_MAC3 0xfc
#define DEST_MAC4 0x6d
#define DEST_MAC5 0x8f

#define SRC_MAC0  0x08
#define SRC_MAC1  0x00
#define SRC_MAC2  0x27
#define SRC_MAC3  0x1e
#define SRC_MAC4  0x36
#define SRC_MAC5  0x4a


unsigned short checksum(void *b, int len) {    
    unsigned short *buf = b;
    unsigned int sum = 0;
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

int main() {
    int client_socket;
    struct sockaddr_ll server_addr;
    char buffer[BUFFER_SIZE];

    // Создание raw-сокета
    if ((client_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct ethhdr *eth = (struct ethhdr *) buffer;
    // Заполнение Ethernet заголовка
    eth->h_source[0] = SRC_MAC0;
    eth->h_source[1] = SRC_MAC1;
    eth->h_source[2] = SRC_MAC2;
    eth->h_source[3] = SRC_MAC3;
    eth->h_source[4] = SRC_MAC4;
    eth->h_source[5] = SRC_MAC5;

    eth->h_dest[0] = DEST_MAC0;
    eth->h_dest[1] = DEST_MAC1;
    eth->h_dest[2] = DEST_MAC2;
    eth->h_dest[3] = DEST_MAC3;
    eth->h_dest[4] = DEST_MAC4;
    eth->h_dest[5] = DEST_MAC5;

    eth->h_proto = htons(ETH_P_IP);

    // Заполнение адреса сервера
    // Заполнение структуры sockaddr_ll
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sll_ifindex = if_nametoindex("eth0");
    server_addr.sll_halen = ETH_ALEN;


    // Создание UDP-заголовка
    struct udphdr udp_header;
    udp_header.source = htons(CLIENT_PORT);
    udp_header.dest = htons(SERVER_PORT);
    udp_header.len = htons(sizeof(udp_header) + strlen("Hello"));
    udp_header.check = 0;

    // Заполнение сообщения
    char message[BUFFER_SIZE];
    strcpy(message, "Hello");

    // Создание IP-заголовка
    struct iphdr ip_header;
    ip_header.version = 4;
    ip_header.ihl = 5;
    ip_header.tos = 0;
    ip_header.tot_len = htons(sizeof(ip_header) + sizeof(udp_header) + strlen(message));
    ip_header.id = htons(54321);
    ip_header.frag_off = 0;
    ip_header.ttl = 255;
    ip_header.protocol = IPPROTO_UDP;
    ip_header.check = checksum((unsigned short *)&ip_header, sizeof(struct iphdr));;
    ip_header.saddr = inet_addr(CLIENT_IP);
    ip_header.daddr = inet_addr(SERVER_IP);

    // Формирование полного сообщения с UDP-заголовком
    char packet[BUFFER_SIZE];
    memset(packet, 0, BUFFER_SIZE);   
    memcpy(packet, &ip_header, sizeof(ip_header));
    memcpy(packet + sizeof(ip_header), &udp_header, sizeof(udp_header));
    memcpy(packet + sizeof(ip_header) + sizeof(udp_header), message, strlen(message));

    // Отправка пакета серверу
    if (sendto(client_socket, packet, sizeof(struct ethhdr) +  sizeof(ip_header) + sizeof(udp_header) + strlen(message), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Sendto failed");
        exit(EXIT_FAILURE);
    }

    printf("Sent: Hello\n");

    // Получение ответа от сервера
    socklen_t len = sizeof(server_addr);
    int received_len = recvfrom(client_socket, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len);

    if (received_len > 0) {
        printf("%d\n", received_len);
        printf("Received: %s\n", buffer + 28 + sizeof(struct ethhdr));
    }

    close(client_socket);
    return 0;
}
