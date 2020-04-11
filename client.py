#!/usr/bin/env python3

import threading
import socket
import select
import time
import sys, os, struct, string
import signal

REG_REQ = 0x00
REG_INFO = 0x01
REG_ACK = 0x02
INFO_ACK = 0x03
REG_NACK = 0x04
INFO_NACK = 0x05
REG_REJ = 0x06
ALIVE = 0x10
ALIVE_REJ = 0x11
SEND_DATA = 0x20
SET_DATA = 0x21
GET_DATA = 0x22
DATA_ACK = 0x23
DATA_NACK = 0x24
DATA_REJ = 0x25

#Client States

DISCONNECTED = 0xa0
NOT_REGISTERED = 0xa1
WAIT_ACK_REG = 0xa2
WAIT_INFO = 0xa3
WAIT_ACK_INFO = 0xa4
REGISTERED = 0xa5
SEND_ALIVE = 0xa6


t = 1
u = 2
n = 7
o = 3
p = 3
q = 3
s = 3
v = 2
r = 2
m = 3

START_AGAIN = -1
KEEP_SENDING = 0
RECEIVED = 1

debug = False

client_status = DISCONNECTED
sock = None
socktcp = None
socktcp_connections = None
already_been = False
tcp_server = "0000"
local_tcp = "0000"
server = ""
random_number_REG_ACK = None
server_id_REG_ACK = None
num_registers = 0
pid_commands = None
pid_wait_connections = None
elements = []

def main():
    global elements, client_id, local_tcp, server
    signal.signal(signal.SIGINT, exitprocess)
    signal.signal(signal.SIGUSR1, exitprocess)
    client_file = check_arguments()
    client_id, elements_semicolon, local_tcp, server, server_udp = read_client(client_file)
    elements = read_elements(elements_semicolon)

    # Register PDU-UDP
    register(client_id, server, server_udp, local_tcp, elements_semicolon)
    keep_comunication(client_id, server, server_udp)

def wait_connections(client_id):
    socktcp_connections.bind((server, int(local_tcp)))
    while True:
        if client_status == SEND_ALIVE:
            socktcp_connections.listen(1)
            connection, address = socktcp_connections.accept()
            data = connection.recv(127)
            check_connection_received(data, connection, address, client_id)

def set_elements(element, value):
    global elements

    for el in elements:
        if el[0] in element:
            el[1] = value

def check_connection_received(data, connection, address, client_id):
    global client_status
    packet_type, id, random_number, element, value, info = struct.unpack('1B 13s 9s 8s 16s 80s', data)

    print_pdu_tcp_debug("Rebut", data)

    if repr(info).split('\\')[0][2:] != client_id or id != server_id_REG_ACK:
        pdu_tcp = [DATA_REJ, client_id.encode(), random_number_REG_ACK, element, value,
                   "Els identificadors del servidor o del client són incorrectes".encode()]
        packet = create_pdu_tcp(pdu_tcp)
        connection.send(packet)
        print_msg("El client passa a l'estat NOT REGISTERED")
        client_status = NOT_REGISTERED
        return

    if packet_type == SET_DATA:
        if element_exists(element.decode()) and is_input(element.decode()):
            print_msg("Received SET DATA OK")
            pdu_tcp = [DATA_ACK, client_id.encode(), random_number_REG_ACK, element, value, client_id.encode()]
            packet = create_pdu_tcp(pdu_tcp)
            set_elements(repr(element).split('\\')[0][2:], repr(value).split('\\')[0][2:])
            connection.send(packet)
        else:
            print_msg("El element que s'ha enviat no existeix")
            pdu_tcp = [DATA_NACK, client_id.encode(), random_number_REG_ACK, element, value,
                       "El element que s'ha enviat no existeix".encode()]
            packet = create_pdu_tcp(pdu_tcp)
            connection.send(packet)

    elif packet_type == GET_DATA:
        if element_exists(element.decode()):
            print_msg("Received GET DATA OK")
            pdu_tcp = [DATA_ACK, client_id.encode(), random_number_REG_ACK, element, value, client_id.encode()]
            packet = create_pdu_tcp(pdu_tcp)
            connection.sendto(packet, address)

        else:
            print_msg("El element que s'ha enviat no existeix")
            pdu_tcp = [DATA_NACK, client_id.encode(), random_number_REG_ACK, element, value,
                       "El element que s'ha enviat no existeix".encode()]
            packet = create_pdu_tcp(pdu_tcp)
            connection.send(packet)

def is_input(element):
    element_split = element.split('-')
    return 'I' == element_split[2][0]

def element_exists(element_sent):
    for element in elements:
        if element[0] in element_sent:
            return True
    return False

def foo(client_id):
    while True:
        command = input()
        if client_status == SEND_ALIVE:
            command_tcp_server(command, client_id)
    print("EXIT FOO")
    exit(0)

def command_tcp_server(command, client_id):
    if command == "stat":
        stat(client_id)
    elif is_set_command_OK(command):
        command_decomposed = command.split()
        set(command_decomposed[1], command_decomposed[2])
    elif is_send_command_OK(command):
        command_decomposed = command.split()
        send(command_decomposed[1], client_id)
    elif command == "quit":
        os.kill(os.getpid(), signal.SIGUSR1)
        exit(0)
    else:
        print("No existeix aquesta comanda: "+command)

def stat(client_id):
    print("****************** DADES DISPOSITIU ******************")
    print("\t Identificador: "+client_id)
    print("\t Estat: "+hex_to_status(client_status))
    print("\t Param\t\t\tValor")
    print("\t -------\t\t------------")

    for i in range (len(elements)):
        print ("\t "+elements[i][0]+"\t\t"+elements[i][1])

    print("\n*******************************************************")
def set(id, value):
    global elements
    for i in range (len(elements)):
        if id == elements[i][0]:
            elements[i][1] = value
            return

def send(element, client_id):
    global socktcp
    value = "----" if value_of_element(element) == None else value_of_element(element)
    t = time.localtime()
    localtime_str = str(t.tm_year)+"-"+time_format(t.tm_mon)+"-"+time_format(t.tm_mday)+";"+time_format(t.tm_hour)+":"+time_format(t.tm_min)+":"+time_format(t.tm_sec)
    pdu_tcp = [SEND_DATA, client_id.encode(), random_number_REG_ACK, element.encode(), value.encode(), localtime_str.encode()]
    tcp_packet = create_pdu_tcp(pdu_tcp)
    address = ('localhost', int(tcp_server))

    socktcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    socktcp.connect(address)
    print_msg("Iniciada comunicació TCP amb el servidor (port: "+tcp_server)
    socktcp.send(tcp_packet)
    check_TCP_send_response(client_id, element, value, random_number_REG_ACK)

def check_TCP_send_response(client_id, element_sent, value_sent, random_sent):
   global client_status

   socktcp.settimeout(m)
   try:
        response = socktcp.recv(127)
   except:
       print("Les dades no han estat acceptades")
       print_msg("Finalitzada comunicacio TCP amb el servidor (port: " + tcp_server+")")
       return

   packet_type, id, random_number, element, value, info = struct.unpack('1B 13s 9s 8s 16s 80s', response)

   if packet_type == DATA_REJ or (repr(info).split('\\')[0][2:] != client_id or random_number != random_sent):
       print_msg("Finalitzada comunicacio TCP amb el servidor (port: "+tcp_server+")")
       socktcp.close()
       print_msg("El client ha passat a l'estat NOT REGISTERED")
       client_status = NOT_REGISTERED

   elif packet_type == DATA_NACK or element_sent not in repr(element).split('\\')[0][2:] or value_sent not in repr(value).split('\\')[0][2:]:
       print("Les dades no han estat acceptades (DATA NACK/element)")
       print_msg("Finalitzada comunicacio TCP amb el servidor (port: " + tcp_server + ")")
       socktcp.close()

   elif packet_type == DATA_ACK:
       print_msg("Finalitzada comunicacio TCP amb el servidor (port: " + tcp_server + ")")
       socktcp.close()
       

def value_of_element(element):
    for element_value in elements:
        if element_value[0] == element:
            return element_value[1]

def is_set_command_OK(command):
    str = command.split()
    if len(str) != 3:
        return False
    set_OK = str[0] == "set"
    element_OK = False
    for element in elements:
        if element[0] == str[1]:
            element_OK = True
    return set_OK and element_OK

def is_send_command_OK(command):
    str = command.split()
    if len(str) != 2:
        return False
    send_OK = str[0] == "send"
    return send_OK and len(str) == 2 #The server will tell us If the element doesn't exist


def exitprocess(signum, stack):
    os._exit(0)

def start_again(signum, stack):
    main()
    exit(0)

def keep_comunication(client_id, host, port):
    global sock, random_number_REG_ACK, client_status

    alive_array = [ALIVE, client_id.encode(), random_number_REG_ACK, "".encode()]
    alive_packet = create_pdu_udp(alive_array)
    address = (host, int(port))
    sock.sendto(alive_packet, address)
    print_pdu_udp_debug("Enviat #1", alive_packet)
    num_of_strikes = 0
    num_alive = 2
    time.sleep(2)
    while True:
        sock.sendto(alive_packet, address)
        print_pdu_udp_debug("Enviat #"+str(num_alive), alive_packet)
        response_time_init = time.time()
        if check_ALIVE_response(client_id, num_alive-1):
            num_of_strikes = 0
        else:
            num_of_strikes += 1
            print_debug("No he rebut paquet resposta ALIVE. "+str(num_of_strikes)+"/3")
            if num_of_strikes == s:
                client_status = NOT_REGISTERED
                print_debug("No ha rebut 3 respostes d'ALIVE seguides")
                print_msg("El estat del client ha passat a NOT_REGISTERED")
                print_debug("Torna a començar el procés de registre")
                main()
                exit()

        num_alive += 1
        response_time_end = time.time()
        if (response_time_end-response_time_init) < v:
            time.sleep(v - (response_time_end - response_time_init))

        if client_status != SEND_ALIVE:
            main()
            exit()


def check_ALIVE_response(client_id, num):
    global sock, socktcp, socktcp_connections, client_status, already_been, thread1, thread2

    sock.settimeout(v)
    try:
        response = sock.recv(84)
    except:
        return False
    packet_type, server_id, random_number_received, data = struct.unpack('1B 13s 9s 61s', response)

    if packet_type == ALIVE:
        if client_status == REGISTERED or client_status == SEND_ALIVE:
            client_id_received = repr(data).split('\\')[0][2:]
            client_id_equals = client_id == client_id_received
            random_equals = random_number_REG_ACK == random_number_received
            server_id_equals = server_id_REG_ACK == server_id
            if not(client_id_equals and random_equals and server_id_equals):
                print_pdu_udp_debug("Rebut #" + str(num), response)
                print_debug("Dades del paquet ALIVE incorrectes")
                print_debug("Torna a començar el procés de registre")
                main()
                exit()

            if client_status != SEND_ALIVE:
                client_status = SEND_ALIVE
                socktcp_connections = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                print_msg("Obert port TCP " + local_tcp + " per la comunicació amb el servidor")

                print_msg("El client ha passat a l'estat SEND_ALIVE")
                if not already_been:
                    already_been = True
                    thread1 = threading.Thread(target=wait_connections, args=(client_id,))
                    thread1.start()
                    thread2 = threading.Thread(target=foo, args=(client_id,))
                    thread2.start()
            print_pdu_udp_debug("Rebut #"+str(num), response)
            return True
    elif packet_type == ALIVE_REJ:
        print_pdu_udp_debug("Rebut", response)
        reason = repr(data).split('\\')[0][2:]
        print_debug("El problema en l'enviament del paquet ALIVE s'ha donat per: "+reason)
        client_status = NOT_REGISTERED
        print_debug("El client ha passat a l'estat NOT REGISTERED")
        print_debug("Torna a començar el procés de registre")
        main()
        exit()
        #Tornar a registrar-se
    return False

def register(client_id, host, port, local_tcp, elements):
    global client_status, sock

    pdu_udp = [REG_REQ, client_id.encode(), "00000000".encode(), "".encode()]
    register_packet = create_pdu_udp(pdu_udp)
    client_status = NOT_REGISTERED

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    address = (host, int(port))

    received_packet = False

    for count1 in range(o):
        print_msg("S'inicia el procés de registre num: "+str(count1+1))
        for count2 in range(p):
            time2wait = t
            sock.sendto(register_packet, address)
            print_pdu_udp_debug("Enviat", register_packet)
            client_status = WAIT_ACK_REG
            print_msg("El client ha passat a l'estat WAIT_ACK_REG")
            received_packet = check_response(client_id, local_tcp, elements, time2wait, host, 1)
            if received_packet == RECEIVED:
                return
            if received_packet == START_AGAIN:
                break

        if received_packet == START_AGAIN:
            continue

        time2wait = t+t
        for count3 in range(p, n):
            sock.sendto(register_packet, address)
            print_pdu_udp_debug("Enviat", register_packet)
            client_status = WAIT_ACK_REG
            print_msg("El client ha passat a l'estat WAIT_ACK_REG")
            time2wait = u if count3 == n-1 else time2wait
            received_packet = check_response(client_id, local_tcp, elements, time2wait, host, 1)
            if received_packet == RECEIVED:
                return
            if time2wait < q*t:
                time2wait += t
            if received_packet == START_AGAIN:
                break

        if received_packet == START_AGAIN:
            continue


    print_msg("No s'ha pogut registrarse")
    exit(1)

def check_response(client_id, local_tcp, elements, time, host, num_response):
    global sock, client_status, server_id_REG_ACK, tcp_server

    sock.settimeout(time)
    try:
        response = sock.recv(84)
    except:
        print_debug("No s'ha rebut cap paquet en el temps esperat: "+str(time)+" segons")
        if num_response == 1:
            print_debug("Es continuarà enviant paquets REG_REQ, sense iniciar un nou registre")
            return KEEP_SENDING
        else: #num_response == 2
            return START_AGAIN

    packet_type, server_id, random_number, data =  struct.unpack('1B 13s 9s 61s', response)

    if packet_type == REG_ACK:
        if client_status == WAIT_ACK_REG:
            server_id_REG_ACK = server_id
            print_pdu_udp_debug("Rebut", response)
            return process_REG_ACK(local_tcp, elements, client_id, random_number, data, host)

    elif packet_type == REG_NACK:
        print_pdu_udp_debug("Rebut", response)
        client_status = NOT_REGISTERED
        print_debug("El client ha passat a l'estat NOT_REGISTERED")
        print_debug("Es continuarà enviant paquets REG_REQ sense iniciar un nou registre")
        return KEEP_SENDING

    elif packet_type == REG_REJ:
        print_pdu_udp_debug("Rebut", response)
        client_status = NOT_REGISTERED
        print_msg("El client ha passat a l'estat NOT_REGISTERED")
        return START_AGAIN

    elif packet_type == INFO_ACK:
        print_pdu_udp_debug("Rebut", response)
        if client_status == WAIT_ACK_INFO and INFO_ACK_data_is_OK(server_id, random_number):
            client_status = REGISTERED
            tcp_server = repr(data).split('\\')[0][2:]
            print_msg("El client ha passat al estat REGISTERED")
            return RECEIVED

    elif packet_type == INFO_NACK:
        if client_status == WAIT_ACK_INFO and INFO_ACK_data_is_OK(server_id, random_number):
            print_pdu_udp_debug("Rebut:", response)
            reason = repr(data).split('\\')[0][2:]
            print_debug("No s'ha acceptat el paquet pel següent motiu: "+reason)
            client_status = NOT_REGISTERED
            print_msg("El client ha passat al estat NOT_REGISTERED")
            print_debug("Es continuarà enviant paquets REG_REQ sense iniciar un nou registre")
            return KEEP_SENDING

    print_pdu_udp_debug("Rebut:", response)
    print_debug("S'ha rebut un paquet no esperat o amb les dades incorrectes")
    client_status = NOT_REGISTERED
    print_msg("El client ha passat al estat NOT_REGISTERED")
    return START_AGAIN


def  INFO_ACK_data_is_OK(server_id, random_number):
    #No se que es considera correcte o no
    server_id_OK = server_id == server_id_REG_ACK
    random_number_OK = random_number == random_number_REG_ACK
    return server_id_OK and random_number_OK

def is_alphanumeric(s):
    numbers = False
    letters = False
    other_things = False
    for char in s:
        if char.isdigit():
            numbers = True
        elif char.isalpha():
            letters = True
        else:
            other_things = True

    return numbers and letters and not other_things



def process_REG_ACK(local_tcp, elements, client_id, random_number, ip_address, host):
    global client_status, random_number_REG_ACK

    data = local_tcp + "," + elements
    pdu_udp = [REG_INFO, client_id.encode(), random_number, data.encode()]
    response_packet = create_pdu_udp(pdu_udp)
    ip_address_ok = int(repr(ip_address).split('\\')[0][2:])
    random_number_REG_ACK = random_number
    address = (host, ip_address_ok)
    sock.sendto(response_packet, address)
    print_pdu_udp_debug("Enviat", response_packet)
    client_status = WAIT_ACK_INFO
    print_msg("El client ha passat a l'estat WAIT_ACK_INFO")
    received_packet = check_response(client_id, local_tcp, elements, 2*t, host, 2)
    if not received_packet:
        client_status = NOT_REGISTERED
        #print_debug("El client no ha rebut cap paquet en "+str(2*t)+" segons")
        print_msg("El client ha passat a l'estat NOT_REGISTERED")

    return received_packet


def get_ip_address(message):
    print(message)
    ip_address = ""
    for char in message:
        if not char.isdigit():
            return ip_address
        ip_address += char

def create_pdu_udp(pdu_udp):
    st = struct.Struct('1B 13s 9s 61s')
    return st.pack(*pdu_udp)

def create_pdu_tcp(pdu_tcp):
    st = struct.Struct('1B 13s 9s 8s 16s 80s')
    return st.pack(*pdu_tcp)

def read_elements(message):
    elements = message.split(';')
    elements_value = []
    for i in range(len(elements)):
        elements_value.append([elements[i], "NONE"])

    return elements_value

def read_client(file):
    cfg = open(file, 'r')
    line = cfg.readline()
    return_values = []
    while line:
        equal_found = start_reading = False
        message = ""
        for chars in line:
            if start_reading and chars != '\n':
                message += chars
            if chars == '=':
                equal_found = True
            if equal_found and chars == ' ':
                start_reading = True

        return_values.append(message)
        line = cfg.readline()

    cfg.close()
    return return_values

def check_arguments():
    global debug

    if len(sys.argv) == 1:
        return "client.cfg"
    elif len(sys.argv) == 2 and sys.argv[1] == '-d':
        debug = True
        return "client.cfg"
    elif len(sys.argv) == 3 and sys.argv[1] == '-c':
        return sys.argv[2]
    elif len(sys.argv) == 4 and sys.argv[1] == '-c' and sys.argv[3] == '-d':
        debug = True
        return sys.argv[2]
    elif len(sys.argv) == 4 and sys.argv[1] == '-d' and sys.argv[2] == '-c':
        debug = True
        return sys.argv[3]

    print("Diferents formes d'execucio: (L'ordre dels parametres es indiferent, inclús es poden ometre i llavors agafaran valors per defecte )")
    print ("Execució: ./client.py -d -c <arxiuclient.cfg> ")
    exit(1)

def print_debug(text):
    if debug:
        local_time = time.localtime()
        print(time_format(local_time.tm_hour)+':'+time_format(local_time.tm_min)+':'+time_format(local_time.tm_sec)+": DEBUG    => "+text)
def print_pdu_udp_debug(env_o_rec, response):
    if debug:
        packet_type, id, random_number, data =  struct.unpack('1B 13s 9s 61s', response)
        print_debug(env_o_rec+": bytes="+str(len(response))+", paquet="+hex_to_packet_str(packet_type)+", id="+repr(id).split('\\')[0][2:]+", rndm="+repr(random_number).split('\\')[0][2:]+", dades="+repr(data).split('\\')[0][2:])

def print_pdu_tcp_debug(env_o_rec, response):
    if True:#debug:
        packet_type, id, random_number, element, value, info = struct.unpack('1B 13s 9s 8s 16s 80s', response)
        print(env_o_rec+": bytes="+str(len(response))+", paquet="+hex_to_packet_str(packet_type)+", id="+
                    repr(id).split('\\')[0][2:]+", rndm="+repr(random_number).split('\\')[0][2:]+", element="+repr(random_number).split('\\')[0][2:]
                    +", valors ="+repr(value).split('\\')[0][2:]+", dades="+repr(info).split('\\')[0][2:])

def print_msg(text):
    local_time = time.localtime()
    print(time_format(local_time.tm_hour) + ':' + time_format(local_time.tm_min) + ':' + time_format(local_time.tm_sec) + ": MSG    => " + text)

def hex_to_packet_str(hex):
    if hex == REG_REQ:
        return "REG_REQ"
    if hex == REG_INFO:
        return "REG_INFO"
    if hex == REG_ACK:
        return "REG_ACK"
    if hex == INFO_ACK:
        return "INFO_ACK"
    if hex == REG_NACK:
        return "REG_NACK"
    if hex == INFO_NACK:
        return "INFO_NACK"
    if hex == REG_REJ:
        return "REG_REJ"
    if hex == ALIVE:
        return "ALIVE"
    if hex == ALIVE_REJ:
        return "ALIVE_REJ"
    if hex == SEND_DATA:
        return "SEND_DATA"
    if hex == SET_DATA:
        return "SET_DATA"
    if hex == GET_DATA:
        return "GET_DATA"
    if hex == DATA_ACK:
        return "DATA_ACK"
    if hex == DATA_NACK:
        return "DATA_NACK"
    if hex == DATA_REJ:
        return "DATA_REJ"

def hex_to_status(hex):
    if hex == DISCONNECTED:
        return "DISCONNECTED"
    if hex == NOT_REGISTERED:
        return "NOT_REGISTERED"
    if hex == WAIT_ACK_REG:
        return "WAIT_ACK_REG"
    if hex == WAIT_INFO:
        return "WAIT_INFO"
    if hex == WAIT_ACK_INFO:
        return "WAIT_ACK_INFO"
    if hex == REGISTERED:
        return "REGISTERED"
    if hex == SEND_ALIVE:
        return "SEND_ALIVE"


def time_format(int_time):
    """"Sets time into the right format HH:MM:SS"""
    time = str(int_time)
    return '0'+time if int_time < 10 else time

if __name__ == "__main__":
    main()