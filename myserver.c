#define _XOPEN_SOURCE //Utilitzat per evitar warning per utilitzar la funció kill()
#define  _GNU_SOURCE //Utilitzat per evitar warning per utilitzar la funcio getline()

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
#include <pthread.h> 



struct config {
    char            id[13];
    int             local_udp;
    int             local_tcp;
};

struct client_spec {
    char            id[13];
    int             status_client;
    char            random_number[9];
    char            ip_address;
    char            elements[32];
};

struct pdu_tcp {
    char            type;
    char            id[13];
    char            random_number[9];
    char            element[8];
    char            value[16];
    char            info[80];
};

struct pdu_udp {
    char            type;
    char            id[13];
    char            random_number[9];
    char            data[61];
};

struct client_pid {
    char            id_client[13];
    int             pid;
};

/*Funcions de debug i missatges per pantalla*/
void print_debug(char *text);
void print_pdu_udp_debug(char *text, struct pdu_udp pdu);
void print_pdu_tcp_debug(char *text, struct pdu_tcp pdu);
void print_msg(char *text);
bool get_debug(int argc, char *argv[]);
void print_client_specs();
char* hex_to_packet(char hex);
char* hex_to_status(int hex);

/*Funcions de lectura dels arguments*/
char* get_clientfile(int argc, char *argv[]);
void read_configuration(char *file);
char* get_bbdd_dev_file(int argc, char *argv[]);
void read_bbdd_dev(char *file);

/*Funcions per controlar els clients*/
int get_client_position(char *id);
void *control_client_specs(void *vargp);
void update_client_specs(struct client_spec client_updated);
void *commands_tcp(void *vargp);

/*Funcions de registre*/
void initialize_sockets(int local_tcp, int local_udp);
void attend_clients();
bool authorized_client(char* id);
void register_clients(struct pdu_udp receivedpacket, struct sockaddr_in cliaddr, socklen_t len);
char* recv_packet();
struct pdu_udp get_pdu_udp(char msg[]);
struct pdu_tcp get_pdu_tcp(char msg[]);
void set_pdu_udp(char sendmsg[], struct pdu_udp pdu);
bool correct_packet_REG_REQ(struct pdu_udp packet);
bool correct_packet_REG_INFO(struct pdu_udp packet, char* id, char* random);
void another_REG();
int open_new_host();
int get_tcp_from_data(char data[]);
char* get_elements_from_data(char data[]);
void client_to_DISCONNECTED(char* client_id);

/*Funcions per mantenir la comunicacio*/
int get_pid(char* id);
void keep_comunication(struct sockaddr_in cliaddr, int len);

/*Conneccions TCP amb el servidor*/
bool tcp_packet_OK(struct pdu_tcp received_packet);
bool is_substring(char string[], char substring[]);
void set_pdu_tcp(char sendmsg[], struct pdu_tcp pdu);
void wrong_random_response(struct client_spec spec_tcp_client, struct pdu_tcp receivedpacket, int sock_tcp_fd);
void element_not_exist_response(struct client_spec spec_tcp_client, struct pdu_tcp receivedpacket, int sock_tcp_fd);
void correct_tcppacket_response(struct client_spec spec_tcp_client, struct pdu_tcp receivedpacket, int sock_tcp_fd);
void write_in_memory(struct client_spec spec_tcp_client, struct pdu_tcp packet, int sock_tcp_fd);
void *tcp_commands();
void *tcp_connections(void *vargp);
void send_tcp_commands();
void set_command(char* id, char* element, char* value);
void get_command(char* id, char* element);
void exit_program();
void kill_all_processes();
void process_tcp_response(struct pdu_tcp received_packet);
void process_tcp_packet(int sock_tcp_fd);
void write_in_memory_set_get(struct client_spec spec_tcp_client, struct pdu_tcp packet);

//Estat clients
#define DISCONNECTED  0xa0
#define NOT_REGISTERED 0xa1
#define WAIT_ACK_REG 0xa2
#define WAIT_INFO 0xa3
#define WAIT_ACK_INFO 0xa4
#define REGISTERED 0xa5
#define SEND_ALIVE 0xa6

//Tipus de paquets
#define REG_REQ 0x00
#define REG_INFO 0x01
#define REG_ACK 0x02
#define INFO_ACK 0x03
#define REG_NACK 0x04
#define INFO_NACK 0x05
#define REG_REJ 0x06
#define ALIVE 0x10
#define ALIVE_REJ 0x11
#define SEND_DATA 0x20
#define SET_DATA 0x21
#define GET_DATA 0x22
#define DATA_ACK 0x23
#define DATA_NACK 0x24
#define DATA_REJ 0x25

//Timeout's
#define m 3
#define s 2
#define x 3
#define w 3

//Es considera que hi ha un número maxim de clients
#define MAXNUMBER_CLIENTS 7

/*Variables globals*/
bool debug = false;
bool wait_for_ALIVE = true;
struct config configuration;
struct client_spec client_specs[MAXNUMBER_CLIENTS]; //Considering max number of clients = 7
struct client_spec client_spec;
struct client_pid client_pids[MAXNUMBER_CLIENTS];
int sock_udp, sock_udp_v2, sock_tcp, tcp_port_client;
int FD[MAXNUMBER_CLIENTS][2], FD_tcp[MAXNUMBER_CLIENTS][2];
int FD_specs[2];

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
    signal(SIGUSR1, exit_program);
    //Inicialitza el pipe que servirà per actualitzar les especificacions dels clients
    pipe(FD_specs);
    pthread_t thread_id, thread_id_commands, threadid_tcp;
    //Es creen els diferents threads
    pthread_create(&thread_id, NULL, control_client_specs, NULL);
    pthread_create(&thread_id_commands, NULL, commands_tcp, NULL);
    pthread_create(&threadid_tcp, NULL, tcp_connections, NULL); 
    struct sockaddr_in cliaddr;
    struct pdu_udp receivedpacket;
    socklen_t len = sizeof(cliaddr);
    char recvmsg[84] = {'\0'};
    int pid;
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        //S'inicialitzen els pipes que serviran per comunicar-se amb els processos fills
        pipe(FD[i]);
        pipe(FD_tcp[i]);
    }
    while(true){
        recvfrom(sock_udp, &recvmsg, 84, 0, (struct sockaddr*)&cliaddr, &len);
        receivedpacket = get_pdu_udp(recvmsg);
        if (receivedpacket.type == REG_REQ){
            pid = fork();
            //En cas de rebre REG_REQ es crearà un procés nou que gestionarà al client
            if (pid == 0){
                print_msg("Creat procés UDP per atendre al fill");
                register_clients(receivedpacket, cliaddr, len);
                pthread_t thread_id_tcp;
                pthread_create(&thread_id_tcp, NULL, tcp_commands, NULL);
                keep_comunication(cliaddr, len);
            }else {
                //S'emmagatzemara el seu pid per poderlos matar quan fem el quit
                for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
                    if(strcmp(client_pids[i].id_client, receivedpacket.id) == 0){
                        client_pids[i].pid = pid;
                    }
                }
            }
        }else if (receivedpacket.type == ALIVE){
            if (!authorized_client(receivedpacket.id)){
                //En cas de que el el client no existeixi
                char* not_authorized = malloc(sizeof(char)*50);
                sprintf(not_authorized, "%s: No es un dispositiu autoritzat", receivedpacket.id);
                print_msg(not_authorized);
                continue;
            }
            //En cas de rebre un ALIVE se li reenviarà pel pipe al client en qüestió
            if (client_specs[get_client_position(receivedpacket.id)].status_client == REGISTERED ||
                client_specs[get_client_position(receivedpacket.id)].status_client == SEND_ALIVE){
                    int client_i2 = get_client_position(receivedpacket.id);
                    write(FD[client_i2][1], &receivedpacket, sizeof(struct pdu_udp));
                }else{
                    print_debug("Rebut ALIVE en el estat incorrecte (paquet ignorat)");
                }  
        }
    }
}
/*Register functions*/
void register_clients(struct pdu_udp receivedpacket, struct sockaddr_in cliaddr, socklen_t len){

    char client_status[150];
    char* client_id = receivedpacket.id;
    strcpy((char*)client_spec.id, (const char*)receivedpacket.id);
    print_pdu_udp_debug("Rebut", receivedpacket);

    if (!correct_packet_REG_REQ(receivedpacket)){
        print_debug("Informació rebuda incorrecta!");
        struct pdu_udp pdu_rej;
        pdu_rej.type = REG_REJ;
        strcpy((char *)pdu_rej.id, (const char*)configuration.id);
        strcpy((char *)pdu_rej.random_number, (const char*)"00000000");
        strcpy((char *)pdu_rej.data, (const char*)"Paquet amb info incorrecta!");
        char rej_msg[84] = {'\0'};
        set_pdu_udp(rej_msg, pdu_rej);
        sendto(sock_udp_v2, rej_msg, sizeof(rej_msg), 0, (struct sockaddr*)&cliaddr, len);
        print_pdu_udp_debug("Enviat", pdu_rej);
        client_to_DISCONNECTED(client_spec.id);
    }
    //Es crea el nou port i el numero aleatori
    srand(time(0));
    int random_number = rand()%100000000;
    int port = open_new_host();

    //Es crea la struct/paquet que s'enviara com a missatge
    struct pdu_udp pdu;
    char random_number_str[9], port_str[6];
    sprintf(random_number_str, "%08d", random_number);
    strcpy((char*)client_spec.random_number, (const char*)random_number_str);
    sprintf(port_str, "%d", port);
    pdu.type = REG_ACK;
    strcpy((char *)pdu.id, (const char*)configuration.id);
    strcpy((char *)pdu.random_number, (const char*)random_number_str);
    strcpy((char *)pdu.data, (const char*)port_str);

    char sendmsg[84] = {'\0'};
    set_pdu_udp(sendmsg, pdu);
    
    int n = sendto(sock_udp_v2, sendmsg, sizeof(sendmsg), 0, (struct sockaddr*)&cliaddr, len);
    if (n < 0){
        printf("Error al enviar el paquet!!");
        client_to_DISCONNECTED(client_spec.id);
    }
    print_pdu_udp_debug("Enviat", pdu);
    memset(&client_status, 0, sizeof(client_status));
    sprintf(client_status, "El client %s ha passat a l'estat WAIT_INFO", client_spec.id);
    print_msg(client_status);
    client_spec.status_client = WAIT_INFO;
    write(FD_specs[1], &client_spec, sizeof(struct client_spec));
    //Es crea el temporitzador per rebre el paquet REG_INFO
    fd_set fd;
    FD_ZERO (&fd);
    FD_SET (sock_udp_v2, &fd);

    struct timeval timeout={s,0};
    if (select(FD_SETSIZE, &fd, NULL, NULL, &timeout) == 0){
        print_msg("No s'ha rebut cap paquet REG_INFO");
        client_to_DISCONNECTED(client_spec.id);
    }
    char recvmsg[84] = {'\0'};
    n = recvfrom(sock_udp_v2, &recvmsg, 84, 0, (struct sockaddr*)&cliaddr, &len);
    if (n < 0){
        print_msg("Error en rebre el paquet REG_INFO");
        client_to_DISCONNECTED(client_spec.id);
    }
    receivedpacket = get_pdu_udp(recvmsg);
    char *elements = malloc(sizeof(char)*32);
    //Obtenim els elements i el port TCP del missatge
    tcp_port_client = get_tcp_from_data(receivedpacket.data);
    elements = get_elements_from_data(receivedpacket.data);
    strcpy((char*)client_spec.elements, (const char*)elements);

    print_pdu_udp_debug("Rebut", receivedpacket);

    if (!correct_packet_REG_INFO(receivedpacket, client_id, random_number_str)){
        //Resposta en cas de que el paquet rebut sigui incorrecte
        print_msg("Informació paquet REG_INFO incorrecta.");
        struct pdu_udp pdu_nack;
        pdu_nack.type = INFO_NACK;
        strcpy((char *)pdu_nack.id, (const char*)configuration.id);
        strcpy((char *)pdu_nack.random_number, (const char*)random_number_str);
        strcpy((char *)pdu_nack.data, (const char*)"Paquet amb info incorrecta!");
        char nack_msg[84] = {'\0'};
        set_pdu_udp(nack_msg, pdu_nack);
        sendto(sock_udp_v2, nack_msg, sizeof(sendmsg), 0, (struct sockaddr*)&cliaddr, len);
        print_pdu_udp_debug("Enviat", pdu_nack);
        client_to_DISCONNECTED(client_spec.id);
    }
    memset(client_status, 0, sizeof(client_status));
    sprintf(client_status, "El client %s ha passat a l'estat REGISTERED", client_id);
    print_msg(client_status);
    client_spec.status_client = REGISTERED;
    write(FD_specs[1], &client_spec, sizeof(struct client_spec));

    struct pdu_udp pdu_ack;
    //Es crea la estructura que s'enviara
    pdu_ack.type = INFO_ACK;
    strcpy((char *)pdu_ack.id, (const char*)configuration.id);
    strcpy((char *)pdu_ack.random_number, (const char*)random_number_str);
    char tcp_config[5];
    sprintf(tcp_config, "%d", configuration.local_tcp);
    strcpy((char *)pdu_ack.data, (const char*)tcp_config);
    char ack_msg[84] = {'\0'};
    set_pdu_udp(ack_msg, pdu_ack);
    n = sendto(sock_udp_v2, ack_msg, sizeof(ack_msg), 0, (struct sockaddr*)&cliaddr, len);
    close(sock_udp_v2);
    if (n < 0){
        print_msg("Error en enviar INFO_ACK");
        client_to_DISCONNECTED(client_spec.id);
    }

    print_pdu_udp_debug("Enviat", pdu_ack);
}

void client_to_DISCONNECTED(char* client_id){
    char client_status[70] = {'\0'};
    sprintf(client_status, "El client %s ha passat a l'estat DISCONNECTED", client_id);
    print_msg(client_status);
    client_spec.status_client = DISCONNECTED;
    write(FD_specs[1], &client_spec, sizeof(struct client_spec));
    exit(-1);
}

int open_new_host(){
    
    sock_udp_v2 = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_udp_v2 < 0){
        printf("Error a la creació dels nous sockets!\n");
        exit(-1);
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    //Fent el bind al port 0, utilitzara un port lliure qualsevol
    if (bind(sock_udp_v2, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Error al vincular el socket amb el port udp!\n");
        exit(-1);
    }
    //Obtenim el port amb enter i el retornem
    struct sockaddr_in rand_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    getsockname(sock_udp_v2, (struct sockaddr *)&rand_addr, &len);

    return ntohs(rand_addr.sin_port);
}

bool correct_packet_REG_REQ(struct pdu_udp packet){
    //Es comprova que el paquet REG_REQ es correcte
    bool random_number_OK = strcmp(packet.random_number, "00000000\0") == 0;
    bool data_OK = strcmp(packet.data, "") == 0;
    bool id_OK = get_client_position(packet.id) != -1;
    return random_number_OK && data_OK && id_OK;
}
bool correct_packet_REG_INFO(struct pdu_udp packet, char* id, char* random){
    //Es comprova que el paquet REG_INFO es correcte
    bool id_OK = strcmp(packet.id, id) == 0;
    bool random_number_OK = strcmp(packet.random_number, random) == 0;

    return random_number_OK && id_OK;
}

int get_tcp_from_data(char data[]){
    //Obtenim el port tcp del paquet rebut
    char tcp[7];
    for(int i = 0; i < strlen(data); i++){
        if(data[i] == ','){
            tcp[i] = '\0';
            return atoi((char*)tcp);
        }
        tcp[i] = data[i];
    }
    return -1;
}

char* get_elements_from_data(char data[]){
    //Obtenim els elements del paquet rebut
    char *elements = malloc(sizeof(char)*33);
    int coma = 0;
    bool coma_found = false;
    int i;
    for(i = 0; i < strlen(data); i++){
        if (data[i] == ','){
            coma_found = true;
        }
        if (coma_found){
            elements[i-(coma+1)] = data[i];
        }else {
            coma++;
        }
    }
    elements[i] = '\0';
    return elements;
}

/*Funcions mantenir la comunicació amb ALIVES*/
void keep_comunication(struct sockaddr_in cliaddr, int len){
    int client_i = get_client_position(client_spec.id);
    fd_set fd;
    FD_ZERO (&fd);
    FD_SET (FD[client_i][0], &fd);

    //Establim temporitzador pel primer ALIVE.
    struct timeval timeout={w,0};
    if (select(FD_SETSIZE, &fd, NULL, NULL, &timeout) == 0){
        print_msg("No s'ha rebut el primer ALIVE");
        client_to_DISCONNECTED(client_spec.id);
    }
    bool first_alive = false;
    while (true){
        if (first_alive){
            FD_ZERO (&fd);
            FD_SET (FD[client_i][0], &fd);
            //Establim temporitzador pels alive posteriors
            //Si en 9 segons no es rep res vol dir que no s'han rebut 3 alives consecutius
            struct timeval timeout={w*x,0};//
            if (select(FD_SETSIZE, &fd, NULL, NULL, &timeout) == 0){
                print_msg("No s'ha rebut 3 ALIVE consecutius");
                client_to_DISCONNECTED(client_spec.id);
            }
        }
        struct pdu_udp receivedpacket;
        read(FD[client_i][0], &receivedpacket, sizeof(struct pdu_udp));
        first_alive = true;
        print_pdu_udp_debug("Rebut", receivedpacket);
        //Comprovem si el paquet ALIVE es correcte
        if (strcmp((char *)receivedpacket.data, (const char*)"") == 0 
        && strcmp((char *)receivedpacket.random_number, (const char*)client_spec.random_number) == 0 ){
            //Es comprova si sa rebut en els estats correctes
            if(client_spec.status_client != SEND_ALIVE && client_spec.status_client != REGISTERED){
                client_to_DISCONNECTED(client_spec.id);
            }
            struct pdu_udp pdu_alive;
            pdu_alive.type = ALIVE;
            strcpy((char*)pdu_alive.id, (const char *)configuration.id);
            strcpy((char*)pdu_alive.random_number, client_spec.random_number);
            strcpy((char*)pdu_alive.data, client_spec.id);
            char msg_alive[84] = {'\0'};
            set_pdu_udp(msg_alive, pdu_alive);
            int n = sendto(sock_udp, msg_alive, sizeof(msg_alive), 0, (struct sockaddr*)&cliaddr, len);
            if (n < 0){
                print_msg("Error a l'hora d'enviar un ALIVE");
                client_to_DISCONNECTED(client_spec.id);
            }
            print_pdu_udp_debug("Enviat", pdu_alive);
            if (client_spec.status_client == REGISTERED){
                client_spec.status_client = SEND_ALIVE;
                char client_status_msg[70];
                sprintf(client_status_msg, "El client %s passa a l'estat SEND ALIVE", client_spec.id);
                print_msg(client_status_msg);
                write(FD_specs[1], &client_spec, sizeof(struct client_spec));
            }
        }else {
            //El paquet rebut es incorrecte i es fan les accions pertinents
            struct pdu_udp pdu_rej_alive;
            pdu_rej_alive.type = ALIVE_REJ;
            strcpy((char*)pdu_rej_alive.id, (const char *)configuration.id);
            strcpy((char*)pdu_rej_alive.random_number, client_spec.random_number);
            strcpy((char*)pdu_rej_alive.data, client_spec.id);
            char msg_alive[84] = {'\0'};
            set_pdu_udp(msg_alive, pdu_rej_alive);
            sendto(sock_udp, msg_alive, sizeof(msg_alive), 0, (struct sockaddr*)&cliaddr, len);
            print_pdu_udp_debug("Enviat", pdu_rej_alive);
            client_to_DISCONNECTED(client_spec.id);
        }
    }
}

/*Thread que controla el estat, random,... dels clients*/
void *control_client_specs(void *vargp){
    struct client_spec client_updated;
    while (true) {
        /*Cada cop que un fill canvia d'estat envia un missatge via pipe
        i aquest thread es l'encarregat de actualitzar-ho*/
        memset(&client_updated, 0, sizeof(struct client_spec));
        read(FD_specs[0], &client_updated, sizeof(struct client_spec));
        update_client_specs(client_updated);
    }
}

void update_client_specs(struct client_spec client_updated){
    //S'actualitza el client pertinent
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        if (strcmp((char*)client_specs[i].id, client_updated.id) == 0){
            client_specs[i].status_client = client_updated.status_client;
            strcpy((char*)client_specs[i].random_number, client_updated.random_number);
            strcpy((char*)client_specs[i].elements, client_updated.elements);
        }
    }
}
/*Funcions sobre les comandes TCP servidor a client*/
void *commands_tcp(void *vargp){
    while (true){
        char command[30];
        scanf("%s", command);
        if (strcmp((char*)command, (const char*)"list") == 0){
            print_client_specs();
        }else if (strcmp((char*)command, (const char*)"quit") == 0){
            kill_all_processes();
            print_msg("Es tanquen els sockets");
            exit_program();
        }else if (strcmp((char*)command, (const char*)"set") == 0){
            char client_id[13], element[8], value[18];
            scanf("%s %s %s", client_id, element, value);
            //Es llegeixen els demes atributs de set
            if (!authorized_client(client_id)){
                print_msg("El identificador de client escrit no està autoritzat a la BBDD");
            }else if(!is_substring(client_specs[get_client_position(client_id)].elements, element)){
                print_msg("L'element escrit no pertany al client indicat");
            }else if (element[strlen(element)-1] == 'O'){
                print_msg("L'element escrit és un sensor i no permet establir el seu valor");
            }else{
                set_command(client_id, element, value);
            }
        }else if (strcmp((char*)command, (const char*)"get") == 0){
            //Es llegeixen els demes atributs de get
            char client_id[13], element[8];
            scanf("%s %s", client_id, element);
            if (!authorized_client(client_id)){
                print_msg("El identificador de client escrit no està autoritzat a la BBDD");
            }else if(!is_substring(client_specs[get_client_position(client_id)].elements, element)){
                print_msg("L'element escrit no pertany al client indicat");
            }else{
                get_command(client_id, element);
            }
        }else{
            print_msg("Aquesta comanda no existeix");
        }
    }
}

void kill_all_processes(){
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        char* id = client_pids[i].id_client;
        if (client_specs[get_client_position(id)].status_client != DISCONNECTED){
            kill(client_pids[i].pid, SIGUSR1);
        }
    }
}
void exit_program(){
    close(sock_tcp);
    close(sock_udp);
    exit(0);
}

void set_command(char* id, char* element, char* value){
    //Es crea el paquet a enviar i se li passa per pipe al proces fill en qüestio
    struct client_spec client_spec_set = client_specs[get_client_position(id)];
    struct pdu_tcp set;
    set.type = SET_DATA;
    strcpy((char *)set.id, (const char*)configuration.id);
    strcpy((char *)set.random_number, (const char*)client_spec_set.random_number);
    strcpy((char *)set.element, (const char*)element);
    strcpy((char *)set.value, (const char*)value);
    strcpy((char *)set.info, (const char*)id);

    write(FD_tcp[get_client_position(id)][1], &set, sizeof(struct pdu_tcp));
}

void get_command(char* id, char* element){
    //Es crea el paquet a enviar i se li passa per pipe al proces fill en qüestio
    struct client_spec client_spec_get = client_specs[get_client_position(id)];
    struct pdu_tcp get;
    get.type = GET_DATA;
    strcpy((char *)get.id, (const char*)configuration.id);
    strcpy((char *)get.random_number, (const char*)client_spec_get.random_number);
    strcpy((char *)get.element, (const char*)element);
    strcpy((char *)get.value, (const char*)"");
    strcpy((char *)get.info, (const char*)id);

    write(FD_tcp[get_client_position(id)][1], &get, sizeof(struct pdu_tcp));
}

void *tcp_commands(){
    while (true){
        //Cada iteració que doni es una comanda set/get que s'ha introduit
        send_tcp_commands();
    }
}

void send_tcp_commands(){
    struct pdu_tcp to_send;
    struct sockaddr_in servaddr;
    memset(&servaddr, '0', sizeof(servaddr));
    int sockfdtcp = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfdtcp == -1) { 
        printf("socket creation failed...\n"); 
        exit(-1); 
    }

    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(tcp_port_client);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    int len = sizeof(servaddr);
    
    //Espera a llegir la struct amb la informacio a enviar que se li passa pel pipe
    read(FD_tcp[get_client_position(client_spec.id)][0], &to_send, sizeof(struct pdu_tcp));

    if (connect(sockfdtcp, (const struct sockaddr *)&servaddr, len) == -1) { 
        printf("connection with the server failed...\n");
        exit(-1);
    }
    char msg[127] = {'\0'};
    set_pdu_tcp(msg, to_send);
    //Enviem pel socket tcp el missatge corresponent
    write(sockfdtcp, msg, 127);
    print_pdu_tcp_debug("Enviat", to_send);
    //Buidem el array per no tenir que crear-ne un de nou.
    memset(msg, 0, sizeof(msg)); 
    read(sockfdtcp, msg, 127);
    struct pdu_tcp received_packet = get_pdu_tcp(msg);
    print_pdu_tcp_debug("Rebut", received_packet);
    process_tcp_response(received_packet);
    
    //Es tanca el socket
    close(sockfdtcp);
}

void process_tcp_response(struct pdu_tcp received_packet){
    //En cas de que no es rebu cap resposta es rebrà un paquet amb les dades buides
    if (strcmp(received_packet.id, "") == 0 && strcmp(received_packet.random_number, "") == 0){
        print_msg("No s'ha rebut cap resposta");

    //En cas de que hi hagi algun problema d'identificacio
    }else if (strcmp(received_packet.id, client_spec.id) != 0 ||
        strcmp(received_packet.random_number, client_spec.random_number) != 0){

            client_to_DISCONNECTED(client_spec.id);
    }else if (received_packet.type == DATA_ACK){
        //Es comprova que el element existeixi
        if (is_substring(client_spec.elements, received_packet.element)){
            print_msg("2345678i");
            write_in_memory_set_get(client_spec, received_packet);
        }else{

            print_msg("El element que s'ha rebut no pertany al client");
        }
    }else if (received_packet.type == DATA_NACK){
        print_debug("Operació fallida");
    }else if (received_packet.type == DATA_REJ){
        client_to_DISCONNECTED(client_spec.id);
    }
}

/*Servidor concurrent que rep els paquets del client*/
void *tcp_connections(void *vargp){
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);

    while(true){
        int sock_tcp_fd = accept(sock_tcp, (struct sockaddr*)&cliaddr, &len);
        if (sock_tcp_fd < 0){
            print_msg("TCP accept failed");
            exit(0);
        }
        //Quan rebi una connexio crea un fill que se n'encarrega d'ella
        if(fork() == 0){
            process_tcp_packet(sock_tcp_fd);
        }
    }
}

void process_tcp_packet(int sock_tcp_fd){
    //Procés que se n'encarrega de la connexió rebuda
    struct pdu_tcp receivedpacket;
    char recvmsg[127] = {'\0'};
    
    fd_set fd;
    FD_ZERO (&fd);
    FD_SET (sock_tcp_fd, &fd);

    struct timeval timeout={m,0};
    if (select(FD_SETSIZE, &fd, NULL, NULL, &timeout) == 0){
        print_msg("No s'ha rebut el paquet TCP");
        //client_to_DISCONNECTED(client_spec.id);
    }
    read(sock_tcp_fd, &recvmsg, 127);
    receivedpacket = get_pdu_tcp(recvmsg);
    if (!authorized_client(receivedpacket.id)){
        char msg[70];
        sprintf(msg, "Id client %s no autoritzat", receivedpacket.id);
        print_msg(msg);
        exit(-1);
    }
    struct client_spec spec_tcp_client = client_specs[get_client_position(receivedpacket.id)];
    print_pdu_tcp_debug("Rebut", receivedpacket);
    if (tcp_packet_OK(receivedpacket)){
        write_in_memory(spec_tcp_client, receivedpacket, sock_tcp_fd);
        correct_tcppacket_response(spec_tcp_client, receivedpacket, sock_tcp_fd);
        exit(0);  
    }else if (spec_tcp_client.status_client != SEND_ALIVE){
        printf("%s\n", hex_to_status(spec_tcp_client.status_client));
        print_msg("Rebut paquet TCP en l'estat incorrecte"); 
        exit(-1);
          
    }else if (strcmp((const char*)receivedpacket.random_number, (const char*)spec_tcp_client.random_number) != 0){
        wrong_random_response(spec_tcp_client, receivedpacket, sock_tcp_fd);
        exit(-1);

    //comprovo si el element pertany al client
    }else if (!is_substring(spec_tcp_client.elements, receivedpacket.element)){
        print_msg("El element en el paquet no existeix");
        element_not_exist_response(spec_tcp_client, receivedpacket, sock_tcp_fd);
        exit(-1);
    }
}

void correct_tcppacket_response(struct client_spec spec_tcp_client, struct pdu_tcp receivedpacket, int sock_tcp_fd){
    //Resposta en cas de que s'hagi rebut tot correcte
    struct pdu_tcp response;
    response.type = DATA_ACK;
    strcpy((char *)response.id, (const char*)configuration.id);
    strcpy((char *)response.random_number, (const char*)spec_tcp_client.random_number);
    strcpy((char *)response.element, (const char*)receivedpacket.element);
    strcpy((char *)response.value, (const char*)receivedpacket.value);
    strcpy((char *)response.info, (const char*)spec_tcp_client.id);
    print_pdu_tcp_debug("Enviat ", response);
    char sendmsg[127] = {'\0'};
    set_pdu_tcp(sendmsg, response);
    write(sock_tcp_fd, sendmsg, 127);
}

void wrong_random_response(struct client_spec spec_tcp_client, struct pdu_tcp receivedpacket, int sock_tcp_fd){
    //Resposta en cas de que el random sigui incorrecte
    struct pdu_tcp response;
    response.type = DATA_REJ;
    strcpy((char *)response.id, (const char*)configuration.id);
    strcpy((char *)response.random_number, (const char*)spec_tcp_client.random_number);
    strcpy((char *)response.element, (const char*)receivedpacket.element);
    strcpy((char *)response.value, (const char*)receivedpacket.value);
    strcpy((char *)response.info, (const char*)"Random random incorrecte");
    print_pdu_tcp_debug("Enviat ", response);
    char sendmsg[127] = {'\0'};
    set_pdu_tcp(sendmsg, response);
    write(sock_tcp_fd, sendmsg, 127);
    spec_tcp_client.status_client = DISCONNECTED;
    write(FD_specs[1], &spec_tcp_client, sizeof(struct client_spec));
}

void element_not_exist_response(struct client_spec spec_tcp_client, struct pdu_tcp receivedpacket, int sock_tcp_fd){
    //Resposta en cas de que el element no existeixi o hi hagi problemes al escriureu
    struct pdu_tcp response;
    response.type = DATA_NACK;
    strcpy((char *)response.id, (const char*)configuration.id);
    strcpy((char *)response.random_number, (const char*)spec_tcp_client.random_number);
    strcpy((char *)response.element, (const char*)receivedpacket.element);
    strcpy((char *)response.value, (const char*)receivedpacket.value);
    strcpy((char *)response.info, (const char*)"Error en emmagatzemar la informació/element no existeix");
    print_pdu_tcp_debug("Enviat ", response);
    char sendmsg[127] = {'\0'};
    set_pdu_tcp(sendmsg, response);
    write(sock_tcp_fd, sendmsg, 127);
}

void write_in_memory(struct client_spec spec_tcp_client, struct pdu_tcp packet, int sock_tcp_fd){
    //S'escriu el missatge corresponent al .data corresponent
    char message[106];
    sprintf(message, "%s;%s;%s;%s\n", 
            packet.info, hex_to_packet(packet.type), packet.element, packet.value);
    char file_name[18];
    sprintf(file_name, "%s.data", packet.id);
    
    FILE *clientfile;
    //Es comprova si existeix el fitxer
    clientfile = fopen(file_name, "r");
    if (clientfile){
        clientfile = fopen(file_name, "a");
    } else {
        clientfile = fopen(file_name, "w");
    }

    if (fprintf(clientfile, "%s", message) < 0){
        element_not_exist_response(spec_tcp_client, packet, sock_tcp_fd);
        fclose(clientfile);
        exit(-1);
    }
    fclose(clientfile);
}

void write_in_memory_set_get(struct client_spec spec_tcp_client, struct pdu_tcp packet){
    //S'escriu el missatge corresponent al .data corresponent
    int year, month, day, hours, minutes, seconds;
    struct tm *local;
    time_t now;
    time(&now);
    local = localtime(&now);
    year = local->tm_year + 1900;
    month = local->tm_mon;
    day = local->tm_mday;
    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;

    char message[106];
    sprintf(message, "%d-%02d-%02d;%02d:%02d:%02d;%s;%s;%s\n", 
            year, month, day, hours, minutes, seconds, 
            hex_to_packet(packet.type), packet.element, packet.value);
    char file_name[18];
    sprintf(file_name, "%s.data", packet.id);
    
    FILE *clientfile;
    //Es comprova si existeix el fitxer
    clientfile = fopen(file_name, "r");
    if (clientfile){
        clientfile = fopen(file_name, "a");
    } else {
        clientfile = fopen(file_name, "w");
    }

    fprintf(clientfile, "%s", message);
    fclose(clientfile);
    
}

bool tcp_packet_OK(struct pdu_tcp packet){
    //Es comprova si el paquet TCP prebut es correcte
    struct client_spec spec_tcp_client = client_specs[get_client_position(packet.id)];
    
    bool id_OK = authorized_client(packet.id);
    bool random_OK = strcmp((char*) spec_tcp_client.random_number, (char*)packet.random_number) == 0;
    bool is_in_SEND_ALIVE = spec_tcp_client.status_client == SEND_ALIVE;
    bool element_exists = is_substring(spec_tcp_client.elements, packet.element);

    return id_OK && random_OK && is_in_SEND_ALIVE && element_exists;
}

bool is_substring(char string[], char substring[]){
    int j = 0;
    for(int i = 0; i < strlen(string); i++){
        if (string[i] == substring[j]){
            j++;
        }else {
            j = 0;
        }
        if ((j+1) == strlen(substring)){
            return true;
        }
    }
    return (j+1) == strlen(substring);
}

bool authorized_client(char* id){
    //Es comprova si el client esta a la BBDD
    for (int i = 0; i < MAXNUMBER_CLIENTS; i++){
        if(strcmp((char*)id, (const char*)client_specs[i].id) == 0){
            return true;
        }
    }
    return false;
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

void set_pdu_udp(char sendmsg[], struct pdu_udp pdu){
    //S'escriu al char[] el contingut de la pdu
    sendmsg[0] = pdu.type;
    strcpy((char*)&sendmsg[0 + 1], (char*)pdu.id);
    strcpy((char*)&sendmsg[0 + 1 + 13], (char*)pdu.random_number);
    strcpy((char*)&sendmsg[0 + 1 + 13 + 9], (char*)pdu.data);
}
void set_pdu_tcp(char sendmsg[], struct pdu_tcp pdu){
    //S'escriu al char[] el contingut de la pdu
    sendmsg[0] = pdu.type;
    strcpy((char*)&sendmsg[0 + 1], (char*)pdu.id);
    strcpy((char*)&sendmsg[0 + 1 + 13], (char*)pdu.random_number);
    strcpy((char*)&sendmsg[0 + 1 + 13 + 9], (char*)pdu.element);
    strcpy((char*)&sendmsg[0 + 1 + 13 + 9 + 8], (char*)pdu.value);
    strcpy((char*)&sendmsg[0 + 1 + 13 + 9 + 8 + 16], (char*)pdu.info);
}

struct pdu_udp get_pdu_udp(char msg[]){
    //Es passa de char[] a pdu amb estructura
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

    return pdu;
}

struct pdu_tcp get_pdu_tcp(char msg[]){
    //Es passa de char[] a pdu amb estructura
    struct pdu_tcp pdu;
    unsigned char id[13], random[9], element[8], value[16], info[80];
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

    while(i < 31) {
        element[i - 23] = msg[i];
        i++;
    }
    strcpy((char*)pdu.element,(const char*) element);

    while(i < 47) {
        value[i - 31] = msg[i];
        i++;
    }
    strcpy((char*)pdu.value,(const char*) value);

    while(i < 127) {
        info[i - 47] = msg[i];
        i++;
    }
    strcpy((char*)pdu.info,(const char*) info);
    return pdu;
}

void initialize_sockets(int tcp_port, int udp_port){
    //S'inicialitzen els sockets TCP i UDP del fitxer de configuració
    
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
    //Es comprova si el usuari ha posar la opció -d
    for (int i = 1; i < argc; i++){
        if ((strcmp((char*)"-d", argv[i]) == 0)){
            return true;
        }
    }
    return false;
}
void print_msg(char *text){
    //S'imprimeix el debug tal i com als programes de prova
    int hours, minutes, seconds;
    struct tm *local;
    time_t now;
    time(&now);
    local = localtime(&now);
    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    printf("%02d:%02d:%02d: MSG => %s\n", hours, minutes, seconds, text);

}

void print_debug(char *text){
    //S'imprimeix el debug tal i com als programes de prova
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
    //S'imprimeix el debug tal i com als programes de prova
    if (debug){
        int hours, minutes, seconds;
        struct tm *local;
        time_t now;
        time(&now);
        local = localtime(&now);
        hours = local->tm_hour;
        minutes = local->tm_min;
        seconds = local->tm_sec;
        printf("%02d:%02d:%02d: DEBUG => %s: Mida: %ld, Tipus paquet: %s, Id: %s, rndm: %s, data: %s\n", 
                    hours, minutes, seconds, text, sizeof(pdu), hex_to_packet(pdu.type), pdu.id, pdu.random_number, pdu.data);
    }    
}
void print_pdu_tcp_debug(char *text, struct pdu_tcp pdu){
    //S'imprimeix el debug tal i com als programes de prova
    if (debug){
        int hours, minutes, seconds;
        struct tm *local;
        time_t now;
        time(&now);
        local = localtime(&now);
        hours = local->tm_hour;
        minutes = local->tm_min;
        seconds = local->tm_sec;
        printf("%02d:%02d:%02d: DEBUG => %s: Mida: %ld, Tipus paquet= %s, Id= %s, rndm= %s, elements= %s, value: %s, info: %s\n", 
                    hours, minutes, seconds, text, sizeof(pdu), hex_to_packet(pdu.type), pdu.id, pdu.random_number, pdu.element, pdu.value, pdu.info);
    }    
}

char* hex_to_packet(char hex){
    //Es transcriu el nom del tipus de paquet a string
    if (hex == REG_REQ)
        return "REG_REQ";
    if (hex == REG_INFO)
        return "REG_INFO";
    if (hex == REG_ACK)
        return "REG_ACK";
    if (hex == INFO_ACK)
        return "INFO_ACK";
    if (hex == REG_NACK)
        return "REG_NACK";
    if (hex == INFO_NACK)
        return "INFO_NACK";
    if (hex == REG_REJ)
        return "REG_REJ";
    if (hex == ALIVE)
        return "ALIVE";
    if (hex == ALIVE_REJ)
        return "ALIVE_REJ";
    if (hex == SEND_DATA)
        return "SEND_DATA";
    if (hex == SET_DATA)
        return "SET_DATA";
    if (hex == GET_DATA)
        return "GET_DATA";
    if (hex == DATA_ACK)
        return "DATA_ACK";
    if (hex == DATA_NACK)
        return "DATA_NACK";
    if (hex == DATA_REJ)
        return "DATA_REJ";
    return NULL;
}

char* hex_to_status(int hex){
    //Es transcriu el nom del tipus d'estat a string
    if (hex == DISCONNECTED)
        return "DISCONNECTED";
    if (hex == NOT_REGISTERED)
        return "NOT_REGISTERED";
    if (hex == WAIT_ACK_REG)
        return "WAIT_ACK_REG";
    if (hex == WAIT_INFO)
        return "WAIT_INFO";
    if (hex == WAIT_ACK_INFO)
        return "WAIT_ACK_INFO";
    if (hex == REGISTERED)
        return "REGISTERED";
    if (hex == SEND_ALIVE)
        return "SEND_ALIVE";
    
    return NULL;

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
    print_client_specs();
}

void print_client_specs(){
    printf(
    "-----Id.---- --RNDM-- ------ IP ----- -----ESTAT----- --ELEMENTS----------------------------------\n");
    for(int i = 0; i < MAXNUMBER_CLIENTS; i++){
        char* rndm = malloc(9);
        rndm = (client_specs[i].status_client == DISCONNECTED )? "    -    " : client_specs[i].random_number;
        char* ip = malloc(9);
        ip = (client_specs[i].status_client == DISCONNECTED )? "       -       " : "  127.0.0.1     ";
        char* elements = malloc(17);
        elements = (client_specs[i].status_client == DISCONNECTED )? "" : client_specs[i].elements;
        char* line = malloc(100);
        sprintf(line,"%s %s %s %s\t%s\n", 
        client_specs[i].id, rndm, ip, hex_to_status(client_specs[i].status_client), elements);
        printf("%s", line);
    }
    printf("\n");
}