#!/usr/bin/env python3

from multiprocessing import Pool
import socket
import select
import time
import sys, os, struct, string

REG_REQ = 0x00
REG_INFO = 0x01
REG_ACK = 0x02
INFO_ACK = 0x03
REG_NACK = 0x04
INFO_NACK = 0x05
REG_REJ = 0x06
ALIVE = 0x10
ALIVE_REJ = 0x11

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

START_AGAIN = -1
KEEP_SENDING = 0
RECEIVED = 1

debug = False

client_status = DISCONNECTED
sock = None
random_number_REG_ACK = None
server_id_REG_ACK = None

def main():

    client_file = check_arguments()
    client_id, elements_semicolon, local_tcp, server, server_udp = read_client(client_file)
    elements = read_elements(elements_semicolon)

    # Register PDU-UDP
    register(client_id, server, server_udp, local_tcp, elements_semicolon)

    pid = os.fork()
    if pid == 0:
        keep_comunication(client_id, server, server_udp)
    else:
        foo()
def foo():
    while True:
        x = input()
        print("Hola em dic "+x)

def keep_comunication(client_id, host, port):
    global sock, random_number_REG_ACK

    alive_array = [ALIVE, client_id.encode(), random_number_REG_ACK, "".encode()]
    alive_packet = create_pdu_udp(alive_array)
    address = (host, int(port))
    sock.sendto(alive_packet, address)
    print_debug("Paquet ALIVE num 1 enviat")
    num_of_strikes = 0
    num_alive = 2
    time.sleep(2)
    while True:
        sock.sendto(alive_packet, address)
        print_debug("Paquet ALIVE num "+str(num_alive)+" enviat")
        response_time_init = time.time()
        if check_ALIVE_response(client_id, num_alive-1):
            num_of_strikes = 0
        else:
            num_of_strikes += 1
            print_debug("No he rebut paquet resposta ALIVE. "+str(num_of_strikes)+"/3")
            if num_of_strikes == s:
                client_status = NOT_REGISTERED
                print_debug("No ha rebut 3 respostes d'ALIVE seguides")
                print_debug("El estat del client ha passat a NOT_REGISTERED")
                print_debug("Torna a començar el procés de registre")
                #main()
                exit()
                # Tornar a registrar-se

        num_alive += 1
        response_time_end = time.time()
        if (response_time_end-response_time_init) < v:
            time.sleep(v-(response_time_end-response_time_init))

def check_ALIVE_response(client_id, num):
    global sock, client_status

    sock.settimeout(v)
    before = time.time()
    try:
        response = sock.recv(85)
    except:
        return False

    after = time.time()
    packet_type, server_id, random_number_received, data = struct.unpack('1B 13s 9s 61s', response)

    if packet_type == ALIVE:
        if client_status == REGISTERED or client_status == SEND_ALIVE:
            client_id_received = repr(data).split('\\')[0][2:]
            client_id_equals = client_id == client_id_received
            random_equals = random_number_REG_ACK == random_number_received
            server_id_equals = server_id_REG_ACK == server_id
            if not(client_id_equals and random_equals and server_id_equals):
                print_debug("Dades del paquet ALIVE incorrectes")
                print_debug("Torna a començar el procés de registre")
                #main()
                exit()
                #Tornar a començar
            if client_status != SEND_ALIVE:
                client_status = SEND_ALIVE
                print_debug("El client ha passat a l'estat SEND_ALIVE")
            print_debug("Rebuda resposta ALIVE num "+str(num))
            return True
    elif packet_type == ALIVE_REJ:
        print_debug("S'ha rebut el parquet ALIVE_REJ")
        reason = repr(data).split('\\')[0][2:]
        print_debug("El problema en l'enviament del paquet ALIVE s'ha donat per: "+reason)
        client_status = NOT_REGISTERED
        print_debug("El client ha passat a l'estat NOT REGISTERED")
        print_debug("Torna a començar el procés de registre")
        #main()
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
        print_debug("S'inicia el procés de registre num: "+str(count1+1))
        for count2 in range(p):
            time2wait = t
            sock.sendto(register_packet, address)
            print_debug("Paquet de registre REG_REQ enviat")
            client_status = WAIT_ACK_REG
            print_debug("El client ha passat a l'estat WAIT_ACK_REG")
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
            print_debug("Paquet de registre REG_REQ enviat")
            client_status = WAIT_ACK_REG
            print_debug("El client ha passat a l'estat WAIT_ACK_REG")
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


    print_debug("No s'ha pogut registrarse")
    exit(1)

def check_response(client_id, local_tcp, elements, time, host, num_response):
    global sock, client_status, server_id_REG_ACK

    sock.settimeout(time)
    try:
        response = sock.recv(85)
    except:
        print_debug("No s'ha rebut cap paquet en el temps esperat: "+str(time)+" segons")
        if num_response == 1:
            print_debug("Es continuarà enviant paquets REG_REQ, sense iniciar un nou registre")
            return KEEP_SENDING
        else: #num_response == 2
            return START_AGAIN

    packet_type, server_id, random_number, data =  struct.unpack('1B 13s 9s 61s', response)

    server_id_REG_ACK = server_id
    if packet_type == REG_ACK:
        if client_status == WAIT_ACK_REG:
            print_debug("Rebut el paquet REG_ACK")
            return process_REG_ACK(local_tcp, elements, client_id, random_number, data, host)

    elif packet_type == REG_NACK:
        print_debug("Rebut el paquet REG_NACK")
        client_status = NOT_REGISTERED
        print_debug("El client ha passat a l'estat NOT_REGISTERED")
        print_debug("Es continuarà enviant paquets REG_REQ sense iniciar un nou registre")
        return KEEP_SENDING

    elif packet_type == REG_REJ:
        print_debug("Rebut el paquet REG_REJ")
        client_status = NOT_REGISTERED
        print_debug("El client ha passat a l'estat NOT_REGISTERED")
        return START_AGAIN

    elif packet_type == INFO_ACK:
        if client_status == WAIT_ACK_INFO and INFO_ACK_data_is_OK(server_id.decode()):
            print_debug("Rebut el paquet INFO_ACK")
            client_status = REGISTERED
            print_debug("El client ha passat al estat REGISTERED")
            return RECEIVED
        print_debug("Rebut el paquet INFO_ACK")

    elif packet_type == INFO_NACK:
        if client_status == WAIT_ACK_INFO and INFO_ACK_data_is_OK(server_id.decode()):
            print_debug("Rebut el paquet INFO_NACK")
            reason = repr(data).split('\\')[0][2:]
            print_debug("No s'ha acceptat el paquet pel següent motiu: "+reason)
            client_status = NOT_REGISTERED
            print_debug("El client ha passat al estat NOT_REGISTERED")
            print_debug("Es continuarà enviant paquets REG_REQ sense iniciar un nou registre")
            return KEEP_SENDING
        print_debug("Rebut el paquet INFO_NACK")

    print_debug("S'ha rebut un paquet no esperat o amb les dades incorrectes")
    client_status = NOT_REGISTERED
    print_debug("El client ha passat al estat NOT_REGISTERED")
    return START_AGAIN


def  INFO_ACK_data_is_OK(server_id):
    #No se que es considera correcte o no
    server_id = server_id[:12]#treiem el últim caracter
    has_12_chars = len(server_id) == 12
    return has_12_chars and is_alphanumeric(server_id)

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
    print_debug("Enviat el paquet REG_INFO")
    client_status = WAIT_ACK_INFO
    print_debug("El client ha passat a l'estat WAIT_ACK_INFO")
    received_packet = check_response(client_id, local_tcp, elements, 2*t, host, 2)
    if not received_packet:
        client_status = NOT_REGISTERED
        print_debug("El client no ha rebut cap paquet en "+str(2*t)+" segons")
        print_debug("El client ha passat a l'estat NOT_REGISTERED")

    return received_packet


def get_ip_address(message):
    print(message)
    ip_address = ""
    for char in message:
        if not char.isdigit():
            return ip_address
        ip_address += char

def create_pdu_udp(pdu_udp):
    s = struct.Struct('1B 13s 9s 61s')
    return s.pack(*pdu_udp)

def read_elements(message):
    elements = []
    length = len(message)
    for i in range(0, length, 8):
        values = [message[i:i+3], message[i+4], message[i+6]]
        elements.append(values)
    return elements

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
        print(time_format(local_time.tm_hour)+':'+time_format(local_time.tm_min)+':'+time_format(local_time.tm_sec)+": MSG    => "+text)

def time_format(int_time):
    """"Sets time into the right format HH:MM:SS"""
    time = str(int_time)
    return '0'+time if int_time < 10 else time

if __name__ == "__main__":
    main()