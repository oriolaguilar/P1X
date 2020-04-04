#define _XOPEN_SOURCE
#define  _GNU_SOURCE

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
    char            id[13];
    int             local_udp;
    int             local_tcp;
};

struct client_spec {
    char            id[13];
    int             status_client;
    char            random_number[9];
    int             ip_address;
};

struct pdu_udp {
    char            type;
    char            id[13];
    char            random_number[9];
    char            data[61];
};

struct client_pid {
    char           id_client[13];
    int            pid;
};

struct return_register {
    char                  random_number[9];
    char                  client_id[13]; 
};

void print_debug(char *text);
void print_pdu_udp_debug(char *text, struct pdu_udp pdu);
void print_msg(char *text);
bool get_debug(int argc, char *argv[]);

char* get_clientfile(int argc, char *argv[]);
void read_configuration(char *file);
char* get_bbdd_dev_file(int argc, char *argv[]);
void read_bbdd_dev(char *file);
int where_is_client(char *id);
char* hex_to_packet(char hex);
int get_client_position(char *id);


void initialize_sockets(int local_tcp, int local_udp);
void attend_clients();
struct return_register register_clients(struct pdu_udp receivedpacket, struct sockaddr_in cliaddr, socklen_t len);
char* recv_packet();
struct pdu_udp get_pdu(char msg[]);
void set_pdu(char sendmsg[], struct pdu_udp pdu);
bool correct_packet_REG_REQ(struct pdu_udp packet);
bool correct_packet_REG_INFO(struct pdu_udp packet, char* id, char* random);
void another_REG();
void open_new_host();

int get_pid(char* id);
void keep_comunication(struct return_register necessary_info, struct sockaddr_in cliaddr, int len);
void send_ALIVE_response();
bool correct_ALIVE(struct pdu_udp packet);

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
bool wait_for_ALIVE = true;
struct config configuration;
struct client_spec client_specs[MAXNUMBER_CLIENTS]; //Considering max number of clients = 7
struct client_pid client_pids[MAXNUMBER_CLIENTS];
int sock_udp, sock_udp_v2, sock_tcp;
int FD[MAXNUMBER_CLIENTS][2];

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
    
    struct sockaddr_in cliaddr;
    struct pdu_udp receivedpacket;
    socklen_t len = sizeof(cliaddr);
    char recvmsg[84] = {'\0'};
    int pid;
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        pipe(FD[i]);
    }
    while(true){
        recvfrom(sock_udp, &recvmsg, 84, 0, (struct sockaddr*)&cliaddr, &len);
        receivedpacket = get_pdu(recvmsg);
        if (receivedpacket.type == REG_REQ){
            pid = fork();
            if (pid == 0){
                int client_i = get_client_position(receivedpacket.id);
                dup2(FD[client_i][0], (client_i*10)+10);
                print_msg("Creat procés UDP per atendre al fill");
                struct return_register ret = register_clients(receivedpacket, cliaddr, len);
                keep_comunication(ret, cliaddr, len);
            }else {
                for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
                    if(strcmp(client_pids[i].id_client, receivedpacket.id) == 0){
                        client_pids[i].pid = pid;
                    }
                }
            }
        }else if (receivedpacket.type == ALIVE){
            //int pid_client = get_pid(receivedpacket.id);
            int client_i2 = get_client_position(receivedpacket.id);
            dup2(FD[client_i2][1], (client_i2*10)+10+1);
            write((client_i2*10)+10+1, &receivedpacket, sizeof(struct pdu_udp));
        }
    }
}

char* get_client_id(int pid){
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        if(client_pids[i].pid == pid){
            return client_pids[i].id_client;
        }
    }
    return NULL;
}
int get_pid(char *id){
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        if(strcmp(client_pids[i].id_client, id) == 0){
            return client_pids[i].pid;
        }
    }
    print_msg("Paquet amb ID incorrecte!");
    exit(-1);
}

int get_client_position(char *id){
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        if(strcmp(client_specs[i].id, id) == 0){
            return i;
        }
    }
    return -1;
}

struct return_register register_clients(struct pdu_udp receivedpacket, struct sockaddr_in cliaddr, socklen_t len){
    //clear the socket set 
    //fd_set readfds;  
    //FD_ZERO(&readfds);   
    //add master socket to set  
    //FD_SET(sock_udp, &readfds);   
    //int nready = select(1, &readfds, NULL, NULL, NULL);
    //if (FD_ISSET(sock_udp, &readfds)){
    /*struct sockaddr_in cliaddr;
    int len = sizeof(cliaddr);
    char recvmsg[84] = {'\0'};
    recvfrom(sock_udp, &recvmsg, 84, 0, (struct sockaddr*)&cliaddr, &len);
    print_debug("Creat procés UDP per atendre al fill");
    struct pdu_udp receivedpacket = get_pdu(recvmsg);
    */
    char* client_id = receivedpacket.id;
    print_pdu_udp_debug("Rebut", receivedpacket);

    if (!correct_packet_REG_REQ(receivedpacket)){
        print_debug("Informació rebuda incorrecta!");
        int i = where_is_client(client_id);
        client_specs[i].status_client = DISCONNECTED;
        print_debug("El client passa a l'estat DISCONNECTED");
        exit(-1);
    }
    srand(time(0));
    int port = (rand()%(9999 - 1024 + 1)) + 1024;
    int random_number = rand()%100000000;
    open_new_host(port);

    struct pdu_udp pdu;
    char random_number_str[9], port_str[5];
    sprintf(random_number_str, "%d", random_number);
    sprintf(port_str, "%d", port);
    pdu.type = REG_ACK;
    strcpy((char *)pdu.id, (const char*)configuration.id);
    strcpy((char *)pdu.random_number, (const char*)random_number_str);
    strcpy((char *)pdu.data, (const char*)port_str);

    char sendmsg[84] = {'\0'};
    set_pdu(sendmsg, pdu);
    
    int n = sendto(sock_udp_v2, sendmsg, sizeof(sendmsg), 0, (struct sockaddr*)&cliaddr, len);
    if (n < 0){
        printf("Error!!");
    }else{
        print_pdu_udp_debug("Enviat", pdu);
    }
    char client_status[100];
    sprintf(client_status, "El client %s ha passat a l'estat WAIT_INFO", client_id);
    print_msg(client_status);
    client_specs[where_is_client(client_id)].status_client = WAIT_INFO;

    //FER TIME OUT
    //struct timeval timeout={2,0};//s
    /*if (setsockopt(sock_udp_v2, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(struct timeval)) < 0){
        print_msg("No s'ha rebut cap paquet REG_INFO");
        print_msg("El client passa a DISCONNECTED");
        exit(-1);
    }*/
    char recvmsg[84] = {'\0'};
    recvfrom(sock_udp_v2, &recvmsg, 84, 0, (struct sockaddr*)&cliaddr, &len);
    receivedpacket = get_pdu(recvmsg);
    print_pdu_udp_debug("Rebut", receivedpacket);

    if (!correct_packet_REG_INFO(receivedpacket, client_id, random_number_str)){
        print_msg("Informació paquet REG_INFO incorrecta.");
        struct pdu_udp pdu_nack;
        pdu_nack.type = REG_NACK;
        strcpy((char *)pdu_nack.id, (const char*)configuration.id);
        strcpy((char *)pdu_nack.random_number, (const char*)random_number_str);
        strcpy((char *)pdu_nack.data, (const char*)"Paquet amb info incorrecta!");
        char nack_msg[84] = {'\0'};
        set_pdu(nack_msg, pdu_nack);
        n = sendto(sock_udp_v2, nack_msg, sizeof(sendmsg), 0, (struct sockaddr*)&cliaddr, len);
        if (n > 0){
            print_pdu_udp_debug("Enviat", pdu_nack);
        }
        exit(-1);
    }
    //client_status = ""; MIRAR SI client_status pot quedar mal escrit 
    sprintf(client_status, "El client %s ha passat a l'estat REGISTERED", client_id);
    print_msg(client_status);
    client_specs[where_is_client(client_id)].status_client = REGISTERED;

    struct pdu_udp pdu_ack;
    pdu_ack.type = INFO_ACK;
    strcpy((char *)pdu_ack.id, (const char*)configuration.id);
    strcpy((char *)pdu_ack.random_number, (const char*)random_number_str);
    char tcp_config[5];
    sprintf(tcp_config, "%d", configuration.local_tcp);
    strcpy((char *)pdu_ack.data, (const char*)tcp_config);
    char ack_msg[84] = {'\0'};
    set_pdu(ack_msg, pdu_ack);
    n = sendto(sock_udp_v2, ack_msg, sizeof(ack_msg), 0, (struct sockaddr*)&cliaddr, len);
    if (n < 0){
        print_msg("Error en enviar INFO_ACK");
        //client_status = ""; MIRAR SI client_status pot quedar mal escrit 
        sprintf(client_status, "El client %s ha passat a l'estat DISCONNECTED", client_id);
        print_msg(client_status);
        client_specs[where_is_client(client_id)].status_client = DISCONNECTED;
        exit(-1);
    }

    print_pdu_udp_debug("Enviat", pdu_ack);
    struct return_register ret;
    strcpy((char *)ret.random_number, (const char*)random_number_str);
    strcpy((char *)ret.client_id, (const char*)receivedpacket.id);
    return ret;
    //}
}
void send_ALIVE_response(){
    wait_for_ALIVE = false;
}

void keep_comunication(struct return_register necessary_info, struct sockaddr_in cliaddr, int len){
    char* client_status = "";
    while (true){
        int client_i = get_client_position(necessary_info.client_id);
        struct pdu_udp receivedpacket;
        read((client_i*10)+10, &receivedpacket, sizeof(struct pdu_udp));
        print_pdu_udp_debug("Rebut", receivedpacket);
        //while (wait_for_ALIVE){}
        if (strcmp((char *)receivedpacket.data, (const char*)"") == 0 
        && strcmp((char *)receivedpacket.random_number, (const char*)necessary_info.random_number) == 0 ){
            struct pdu_udp pdu_alive;
            pdu_alive.type = ALIVE;
            strcpy((char*)pdu_alive.id, (const char *)configuration.id);
            strcpy((char*)pdu_alive.random_number, necessary_info.random_number);
            strcpy((char*)pdu_alive.data, necessary_info.client_id);
            char msg_alive[84] = {'\0'};
            set_pdu(msg_alive, pdu_alive);
            sendto(sock_udp, msg_alive, sizeof(msg_alive), 0, (struct sockaddr*)&cliaddr, len);
            print_pdu_udp_debug("Enviat", pdu_alive);
        }else {
            struct pdu_udp pdu_rej_alive;
            pdu_rej_alive.type = ALIVE_REJ;
            strcpy((char*)pdu_rej_alive.id, (const char *)configuration.id);
            strcpy((char*)pdu_rej_alive.random_number, necessary_info.random_number);
            strcpy((char*)pdu_rej_alive.data, necessary_info.client_id);
            char msg_alive[84] = {'\0'};
            set_pdu(msg_alive, pdu_rej_alive);
            sendto(sock_udp, msg_alive, sizeof(msg_alive), 0, (struct sockaddr*)&cliaddr, len);
            print_pdu_udp_debug("Enviat", pdu_rej_alive);
            client_specs[where_is_client(necessary_info.client_id)].status_client = DISCONNECTED;
            sprintf(client_status, "El client %s ha passat a l'estat DISCONNECTED", necessary_info.client_id);
            print_msg(client_status);
            
            exit(-1);

        }
    }
}

void open_new_host(int random_number){
    
    sock_udp_v2 = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_udp_v2 < 0){
        printf("Error a la creació dels nous sockets!\n");
        exit(-1);
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
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
bool correct_packet_REG_REQ(struct pdu_udp packet){
    bool random_number_OK = strcmp(packet.random_number, "00000000\0") == 0;
    bool data_OK = strcmp(packet.data, "") == 0;
    bool id_OK = where_is_client(packet.id) != -1;
    return random_number_OK && data_OK && id_OK;
}
bool correct_packet_REG_INFO(struct pdu_udp packet, char* id, char* random){
    bool id_OK = strcmp(packet.id, id) == 0;
    bool random_number_OK = strcmp(packet.random_number, random) == 0;

    return random_number_OK && id_OK;
}
void set_pdu(char sendmsg[], struct pdu_udp pdu){
    //char *sendmsg = malloc(sizeof(char)*85); 
    sendmsg[0] = pdu.type;
    strcpy((char*)&sendmsg[0 + 1], (char*)pdu.id);
    strcpy((char*)&sendmsg[0 + 1 + 13], (char*)pdu.random_number);
    strcpy((char*)&sendmsg[0 + 1 + 13 + 9], (char*)pdu.data);
}

struct pdu_udp get_pdu(char msg[]){
    struct pdu_udp pdu;
    unsigned char id[13], random[9], data[61];
    memset(&pdu, 0, sizeof(pdu));
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
    memset(&addr, 0, sizeof(addr));
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
        if ((strcmp((char*)"-d", argv[i]) == 0)){
            return true;
        }
    }
    return false;
}
void print_msg(char *text){

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
char* hex_to_packet(char hex){

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
    return NULL;
}

int where_is_client(char* id){
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        if (memcmp(id, client_specs[i].id, 12) == 0){
            return i;
        }
    }
    return -1;
}
void read_configuration(char *file){
    FILE *config = fopen(file, "r");
    char* id, *local_tcp, *local_udp;
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
        strcpy(client_pids[i].id_client, line); 
        client_specs[i].status_client = DISCONNECTED;
        i++;
    }
}


