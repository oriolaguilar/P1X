#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>



struct config {
    unsigned char   id[13];
    int             local_udp;
    int             local_tcp;
};

struct client_spec {
    unsigned char   id[13];
    int             status_client;
    unsigned char   random_number[9];
    int             ip_address;
};

struct pdu_udp {
    unsigned char type;
    unsigned char id[13];
    unsigned char random_number[9];
    unsigned char data[61];
};

void print_debug(char *text);
void print_pdu_udp_debug(char *text, struct pdu_udp pdu);
bool get_debug(int argc, char *argv[]);

char* get_clientfile(int argc, char *argv[]);
void read_configuration(char *file);
char* get_bbdd_dev_file(int argc, char *argv[]);
void read_bbdd_dev(char *file);
int where_is_client(unsigned char id[]);
char* hex_to_packet(unsigned char hex);


void initialize_sockets(int local_tcp, int local_udp);
void attend_clients();
void register_clients();
char* recv_packet();
struct pdu_udp get_pdu(char msg[]);
char* set_pdu(struct pdu_udp pdu);
bool correct_packet(struct pdu_udp packet);
void another_REG();
void open_new_host();

#define DISCONNECTED  0xa0
#define NOT_REGISTERED 0xa1
#define WAIT_ACK_REG 0xa2
#define WAIT_INFO 0xa3
#define WAIT_ACK_INFO 0xa4
#define REGISTERED 0xa5
#define SEND_ALIVE 0xa6

#define REG_REQ 0x00
#define REG_INFO 0x01
#define REG_ACK 0x02
#define INFO_ACK 0x03
#define REG_NACK 0x04
#define INFO_NACK 0x05
#define REG_REJ 0x06
#define ALIVE 0x10
#define ALIVE_REJ 0x11

#define MAXNUMBER_CLIENTS 7

bool debug = false;
struct config configuration;
struct client_spec client_specs[MAXNUMBER_CLIENTS]; //Considering max number of clients = 7
int sock_udp, sock_udp_v2, sock_tcp;

int main(int argc, char *argv[]){

    debug = get_debug(argc, argv);
    read_configuration(get_clientfile(argc, argv));
    read_bbdd_dev(get_bbdd_dev_file(argc, argv));

    print_debug("Arxius de configuració llegits");

    initialize_sockets(configuration.local_tcp, configuration.local_udp);
    print_debug("Socket TCP i UDP creats");
    attend_clients();
}

void attend_clients(){
        register_clients();
}

void register_clients(){
    char recvmsg[85] = {'\0'};
    struct sockaddr_in cliaddr;
    int len = sizeof(cliaddr);
    recvfrom(sock_udp, &recvmsg, 85, 0, (struct sockaddr*)&cliaddr, &len);
    print_debug("Creat procés UDP per atendre al fill");
    struct pdu_udp receivedpacket = get_pdu(recvmsg);
    print_pdu_udp_debug("Rebut", receivedpacket);
    
    if (!correct_packet(receivedpacket)){
        print_debug("Informació rebuda incorrecta!");
        int i = where_is_client(receivedpacket.id);
        client_specs[i].status_client = DISCONNECTED;
        print_debug("El client passa a l'estat DISCONNECTED");
        exit(-1);
    }

    if (client_specs[where_is_client(receivedpacket.id)].status_client == DISCONNECTED){
        srand(time(0));
        int port = (rand()%(9999 - 1024 + 1)) + 1024;
        int random_number = rand()%100000000;
        open_new_host(port);
        print_debug("Creat nou port UDP");
        memset(&cliaddr, 0, sizeof(cliaddr));
        struct pdu_udp pdu;
        char random_number_str[9];
        char port_str[5];
        sprintf(random_number_str, "%d", random_number);
        sprintf(port_str, "%d", port);
        pdu.type = REG_ACK;
        strcpy((char *)pdu.id, (const char*)configuration.id);
        strcpy((char *)pdu.random_number, (const char*)random_number_str);
        strcpy((char *)pdu.data, (const char*)port_str);
        
        char sendmsg[85] = {'\0'};
        sendmsg[0] = pdu.type;
        strcpy((char*)&sendmsg[0 + 1], (char*)pdu.id);
        strcpy((char*)&sendmsg[0 + 1 + 13], (char*)pdu.random_number);
        strcpy((char*)&sendmsg[0 + 1 + 13 + 9], (char*)pdu.data);
        
        struct sockaddr_in targ;
        memset(&targ,0, sizeof(targ));
        targ.sin_family = AF_INET;
        targ.sin_addr.s_addr = INADDR_ANY;
        targ.sin_port = htons(configuration.local_udp);

        int a = sendto(sock_udp, sendmsg, sizeof(sendmsg), 0, (struct sockaddr*)&targ, len);
        if (a < 0){
            printf("Error a l'hora d'enviar el REG ACK!\n");
            exit(-1);
        }
        printf("%d\n", a);
        print_pdu_udp_debug("Enviat", pdu);
    }

}
void open_new_host(int random_number){
    
    sock_udp_v2 = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_udp_v2 < 0){
        printf("Error a la creació dels nous sockets!\n");
        exit(-1);
    }

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(random_number);
    
    if (bind(sock_udp_v2, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Error al vincular el socket amb el port udp!\n");
        exit(-1);
    }
}

void another_REG(){

}
char* recv_packet(){//////////////////////
    char packet[85] = {'\0'};
    recv(sock_udp, &packet, 85, 0);
    struct pdu_udp receivedpacket = get_pdu(packet);
    //if (receivedpacket.type == REG_REQ){
      //  print_pdu_udp_debug("Rebut", receivedpacket);
        //if(fork() > 0){
            //wait(NULL);
            //attend_clients();
          //  exit(0);
        //}
    //}
    char* r = &packet[0];
    return r;
}

bool correct_packet(struct pdu_udp packet){
    bool random_number_OK = strcmp(packet.random_number, "00000000\0") == 0;
    bool data_OK = strcmp(packet.data, "") == 0;
    bool id_OK = where_is_client(packet.id) != -1;
    return random_number_OK && data_OK && id_OK;
}
char* set_pdu(struct pdu_udp pdu){
    //char *sendmsg = malloc(sizeof(char)*85); 
    char sendmsg[85] = {'\0'};

    sendmsg[0] = pdu.type;
    strcpy((char*)&sendmsg[0 + 1], (char*)pdu.id);
    strcpy((char*)&sendmsg[0 + 1 + 13], (char*)pdu.random_number);
    strcpy((char*)&sendmsg[0 + 1 + 13 + 9], (char*)pdu.data);

    printf("%d %s %s %s\n", pdu.type, pdu.id, pdu.random_number, pdu.data);
    printf("FUNCIO -> %s Length -> %ld\n", sendmsg, sizeof(sendmsg));
    char* r = (char*)sendmsg;
    return r;
}
struct pdu_udp get_pdu(char msg[]){
    struct pdu_udp pdu;
    unsigned char type, id[13], random[9], data[61];
    memset(&pdu,0, sizeof(pdu));
    pdu.type = msg[0];

    int i = 1;
    while(i < 14){
        id[i - 1] = msg[i];
        i++;
    }
    strcpy((char*)pdu.id,(const char*) id);

    while(i < 23){
        random[i - 14] = msg[i];
        i++;
    }
    strcpy((char*)pdu.random_number,(const char*) random);

    while(i < 84) {
        data[i - 23] = msg[i];
        i++;
    }
    strcpy((char*)pdu.data,(const char*) data);

    //printf("PDU: %d %s %s %s\n", pdu.type, pdu.random_number, pdu.data, pdu.id);

    return pdu;
}

void initialize_sockets(int tcp_port, int udp_port){
    
    sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_tcp < 0 || sock_udp < 0){
        printf("Error a la creació d'un dels sockets!\n");
        exit(-1);
    }

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(udp_port);

    /*Bind UDP*/
    if (bind(sock_udp, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Error al vincular el socket amb el port udp!\n");
        exit(-1);
    }

    addr.sin_port = htons(tcp_port);
    /*Bind TCP*/
    if (bind(sock_tcp, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        printf("Error al vincular el socket amb el port tcp!\n");
        exit(-1);
    }

    if (listen(sock_tcp, MAXNUMBER_CLIENTS) < 0) {
        printf("Error al listen() del socket tcp!\n");
        exit(-1);
    }

}

bool get_debug(int argc, char *argv[]){
    for (int i = 1; i < argc; i++){
        if ((strcmp("-d", argv[i]) == 0)){
            return true;
        }
    }
    return false;
} 

void print_debug(char *text){
    
    if (debug){
        int hours, minutes, seconds;
        struct tm *local;
        time_t now;
        time(&now);
        local = localtime(&now);
        hours = local->tm_hour;
        minutes = local->tm_min;
        seconds = local->tm_sec;
        printf("%02d:%02d:%02d: DEBUG => %s\n", hours, minutes, seconds, text);
    }
}
void print_pdu_udp_debug(char *text, struct pdu_udp pdu){
    
    if (debug){
        int hours, minutes, seconds;
        struct tm *local;
        time_t now;
        time(&now);
        local = localtime(&now);
        hours = local->tm_hour;
        minutes = local->tm_min;
        seconds = local->tm_sec;
        printf("%02d:%02d:%02d: DEBUG => %s: Tipus paquet= %s, Id= %s, rndm= %s, data= %s\n", 
                    hours, minutes, seconds, text, hex_to_packet(pdu.type), pdu.id, pdu.random_number, pdu.data);
    }    
}
char* hex_to_packet(unsigned char hex){

    if (hex == 0x00)
        return "REG_REQ";
    if (hex == 0x01)
        return "REG_INFO";
    if (hex == 0x02)
        return "REG_ACK";
    if (hex == 0x03)
        return "INFO_ACK";
    if (hex == 0x04)
        return "REG_NACK";
    if (hex == 0x05)
        return "INFO_NACK";
    if (hex == 0x06)
        return "REG_REJ";
    if (hex == 0x10)
        return "ALIVE";
    if (hex == 0x11)
        return "ALIVE_REJ";
}

int where_is_client(unsigned char id[]){
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        if (memcmp(id, client_specs[i].id, 12) == 0){
            return i;
        }
    }
    return -1;
}
void read_configuration(char *file){
    FILE *config = fopen(file, "r");
    char trash;
    unsigned char* id, *local_tcp, *local_udp;
    id = malloc(12);
    local_tcp = malloc(4); 
    local_udp = malloc(4); 

    char *line;
    size_t len = 0;
    ssize_t read;
    int i = 0;
    while ((read = getline(&line, &len, config)) != -1) {
        line[read-1] = 0;
        if (i == 0){
            strncpy(id, &line[5], 12);
        }else if (i == 1){
            strncpy(local_udp, &line[11], 4);
        }else if (i == 2){
            strncpy(local_tcp, &line[11], 4);
        }
        i++;
    }
    strcpy(configuration.id, id);;
    configuration.local_udp = atoi(local_udp);
    configuration.local_tcp = atoi(local_tcp);
}

char* get_clientfile(int argc, char *argv[]) {
    
    for (int i = 1 ; i < argc ; i++){
        if (strcmp("-c", argv[i]) == 0){
            return argv[i + 1];
        }
    }
    return "server.cfg";
}

char* get_bbdd_dev_file(int argc, char *argv[]){
    for (int i = 1 ; i < argc ; i++){
        if (strcmp("-u", argv[i]) == 0){
            return argv[i + 1];
        }
    }
    return "bbdd_dev.dat";

}

void read_bbdd_dev(char *file){
    FILE *db = fopen(file, "r");
    char *line;
    size_t len = 0;
    ssize_t read;
    
    int i = 0;
    while ((read = getline(&line, &len, db)) != -1) {
        line[read-1] = 0;
        strcpy(client_specs[i].id, line);
        client_specs[i].status_client = DISCONNECTED;
        i++;
    }
}


