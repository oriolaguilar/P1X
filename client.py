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
sock_udp = None
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
first_time_main = True

def main():
    #S'obtenen totes les variables dels fitxers de config.
    global elements, client_id, local_tcp, server, first_time_main
    signal.signal(signal.SIGINT, exitprogram)
    signal.signal(signal.SIGUSR1, exitprogram)
    client_file = check_arguments()
    client_id, elements_semicolon, local_tcp, server, server_udp = read_client(client_file)
    elements = read_elements(elements_semicolon)

    if first_time_main:
        first_time_main = False
        stat(client_id)

    register(client_id, server, server_udp, local_tcp, elements_semicolon)
    keep_comunication(client_id, server, server_udp)

#Funcions de registre
def register(client_id, host, port, local_tcp, elements):
    """Funcio principal de registre dels clients"""
    global client_status, sock_udp, num_registers

    #Es crea el paquet REG REQ
    pdu_udp = [REG_REQ, client_id.encode(), "00000000".encode(), "".encode()]
    register_packet = create_pdu_udp(pdu_udp)
    client_status = NOT_REGISTERED

    #S'inicialitza el socket
    try:
        sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    except:
        print_msg("Error en la creació del socket UDP")
        exitprogram(0,0)

    address = (host, int(port))
    received_packet = False

    for count1 in range(num_registers, o):
        num_registers += 1
        print_msg("El client passa a l'estat NOT REGISTERED. Procés de registre num: "+str(count1+1))
        client_status = NOT_REGISTERED
        for count2 in range(p-1):
            time2wait = t
            sock_udp.sendto(register_packet, address)
            print_pdu_udp_debug("Enviat", register_packet)
            if client_status != WAIT_ACK_REG:
                client_status = WAIT_ACK_REG
                print_msg("El client ha passat a l'estat WAIT_ACK_REG")
            received_packet = check_response(client_id, local_tcp, elements, time2wait, host, 1)
            if received_packet == RECEIVED:
                #Si es rep RECEIVED, acabara la funcio de registre
                return
            if received_packet == START_AGAIN:
                #Si es rep START_AGAIN, començara un no procés de registre
                break

        if received_packet == START_AGAIN:
            time.sleep(u)
            continue

        time2wait = t+t
        for count3 in range(p-1, n):
            sock_udp.sendto(register_packet, address)
            print_pdu_udp_debug("Enviat", register_packet)
            if client_status != WAIT_ACK_REG:
                client_status = WAIT_ACK_REG
                print_msg("El client ha passat a l'estat WAIT_ACK_REG")
            received_packet = check_response(client_id, local_tcp, elements, time2wait, host, 1)
            if received_packet == RECEIVED:
                # Si es rep RECEIVED, acabara la funcio de registre
                return
            if time2wait < q*t:
                time2wait += t
            if received_packet == START_AGAIN:
                # Si es rep START_AGAIN, començara un no procés de registre
                break

        if received_packet == START_AGAIN:
            time.sleep(u)
            continue
        time.sleep(u)

    print_msg("No s'ha pogut registrar")
    exitprogram(0,0)

def check_response(client_id, local_tcp, elements, time, host, num_response):
    global sock_udp, client_status, server_id_REG_ACK, tcp_server

    sock = select.select([sock_udp], [], [], time)
    if sock[0]:
        response = sock[0][0].recv(84)
    else:
        if num_response == 1:
            return KEEP_SENDING
        else: #num_response == 2
            return START_AGAIN

    packet_type, server_id, random_number, data =  struct.unpack('1B 13s 9s 61s', response)

    #Depenent del paquet que rebrem, es faran diferentes accions
    if packet_type == REG_ACK:
        if client_status == WAIT_ACK_REG:
            server_id_REG_ACK = server_id
            print_pdu_udp_debug("Rebut", response)
            return process_REG_ACK(local_tcp, elements, client_id, random_number, data, host)

    elif packet_type == REG_NACK:
        print_pdu_udp_debug("Rebut", response)
        client_status = NOT_REGISTERED
        print_msg("El client ha passat a l'estat NOT_REGISTERED")
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
            return KEEP_SENDING

    #En cas de que no es rebi cap paquet dels esperats
    print_pdu_udp_debug("Rebut:", response)
    print_debug("S'ha rebut un paquet no esperat o amb les dades incorrectes")
    client_status = NOT_REGISTERED
    print_msg("El client ha passat al estat NOT_REGISTERED")
    return START_AGAIN


def  INFO_ACK_data_is_OK(server_id, random_number):
    #Es comprova si el paquet INFO_ACK es correcte
    server_id_OK = server_id == server_id_REG_ACK
    random_number_OK = random_number == random_number_REG_ACK
    return server_id_OK and random_number_OK


def process_REG_ACK(local_tcp, elements, client_id, random_number, ip_address, host):
    #Accions que es realitzen despres de rebre REG_ACK
    global client_status, random_number_REG_ACK

    data = local_tcp + "," + elements
    pdu_udp = [REG_INFO, client_id.encode(), random_number, data.encode()]
    response_packet = create_pdu_udp(pdu_udp)
    #Aquesta funció serveix per passar de bytes-NO ASCII a string
    ip_address_ok = int(repr(ip_address).split('\\')[0][2:])
    random_number_REG_ACK = random_number
    address = (host, ip_address_ok)
    sock_udp.sendto(response_packet, address)
    print_pdu_udp_debug("Enviat", response_packet)
    client_status = WAIT_ACK_INFO
    print_msg("El client ha passat a l'estat WAIT_ACK_INFO")
    received_packet = check_response(client_id, local_tcp, elements, 2*t, host, 2)
    if not received_packet:
        client_status = NOT_REGISTERED
        print_msg("El client ha passat a l'estat NOT_REGISTERED")

    return received_packet

def get_ip_address(message):
    """S'obte la ip del missatge rebut"""
    print(message)
    ip_address = ""
    for char in message:
        if not char.isdigit():
            return ip_address
        ip_address += char

#Funcions per mantenir la conneccio
def keep_comunication(client_id, host, port):
    """Funcio que mante la comunicacio amb el servidor"""
    global sock_udp, random_number_REG_ACK, client_status, num_registers

    alive_array = [ALIVE, client_id.encode(), random_number_REG_ACK, "".encode()]
    alive_packet = create_pdu_udp(alive_array)
    address = (host, int(port))

    num_of_strikes = 0
    num_alive_send = 1
    num_alive_recv = 1
    num_con_alive_wo_recv = 0
    first_alive = False

    while True:
        sock_udp.sendto(alive_packet, address)
        print_pdu_udp_debug("Enviat", alive_packet)
        response_time_init = time.time()
        if check_ALIVE_response(client_id):
            num_alive_recv += 1
            num_of_strikes = 0
            num_con_alive_wo_recv = 0
            first_alive = True
        else:
            num_con_alive_wo_recv += 1
            if num_con_alive_wo_recv >= r:
                if client_status == REGISTERED:
                    print_debug("No ha rebut la primera resposta d'ALIVE")
                    print_msg("El estat del client ha passat a NOT_REGISTERED")
                    # torna a començar el registre
                    main()
                    exitprogram(0, 0)

                num_alive_recv += 1
                num_of_strikes += 1
                print_debug("No he rebut paquet resposta ALIVE. "+str(num_of_strikes)+"/3")

        response_time_end = time.time()
        if num_of_strikes >= s:
            client_status = NOT_REGISTERED
            print_debug("No ha rebut 3 respostes d'ALIVE seguides")
            print_msg("El estat del client ha passat a NOT_REGISTERED")
            print_debug("Torna a començar el procés de registre")
            num_registers = 0
            #torna a començar el registre
            main()
            exitprogram(0, 0)

        num_alive_send += 1

        #Es parara el 2 segons menys el temps que ha estat esperant
        if (response_time_end-response_time_init) < v:
            time.sleep(v - (response_time_end - response_time_init))

        #Si el estat de repent canvia tornaria a registar-se
        if client_status != SEND_ALIVE and first_alive:
            num_registers = 0
            main()
            exitprogram(0, 0)


def check_ALIVE_response(client_id):
    """Processa les respostes als ALIVES"""
    global sock_udp, socktcp, socktcp_connections, client_status, already_been, thread1, thread2, num_registers

    sock = select.select([sock_udp], [], [], v)
    if sock[0]:
        response = sock[0][0].recv(84)
    else:
        return False

    packet_type, server_id, random_number_received, data = struct.unpack('1B 13s 9s 61s', response)

    if packet_type == ALIVE:
        if client_status == REGISTERED or client_status == SEND_ALIVE:
            client_id_received = repr(data).split('\\')[0][2:]
            client_id_equals = client_id == client_id_received
            random_equals = random_number_REG_ACK == random_number_received
            server_id_equals = server_id_REG_ACK == server_id
            if not(client_id_equals and random_equals and server_id_equals):
                print_pdu_udp_debug("Rebut", response)
                print_debug("Dades del paquet ALIVE incorrectes")
                print_debug("Torna a començar el procés de registre")
                if client_status == SEND_ALIVE:
                    num_registers = 0
                main()
                exitprogram(0,0)

            if client_status != SEND_ALIVE:
                client_status = SEND_ALIVE
                try:
                    socktcp_connections = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                except:
                    print_msg("Error en la creació del socket TCP")
                    exitprogram(0,0)
                print_msg("Obert port TCP " + local_tcp + " per la comunicació amb el servidor")

                print_msg("El client ha passat a l'estat SEND_ALIVE")
                #El primer cop que es s'arriba es crearan els threads
                if not already_been:
                    already_been = True
                    #Thread que espera conneccions del servidor
                    thread1 = threading.Thread(target=wait_connections, args=(client_id,))
                    thread1.start()
                    #Thread que escriura les comandes
                    thread2 = threading.Thread(target=send_commands, args=(client_id,))
                    thread2.start()
            print_pdu_udp_debug("Rebut", response)
            return True

    elif packet_type == ALIVE_REJ:
        print_pdu_udp_debug("Rebut", response)
        reason = repr(data).split('\\')[0][2:]
        print_debug("El problema en l'enviament del paquet ALIVE s'ha donat per: "+reason)
        client_status = NOT_REGISTERED
        print_debug("El client ha passat a l'estat NOT REGISTERED")
        print_debug("Torna a començar el procés de registre")
        if client_status == SEND_ALIVE:
            num_registers = 0
        main()
        exitprogram(0,0)
        #Tornar a registrar-se
    return False

#Funcions que processen el setdata i el getdata

def wait_connections(client_id):
    #S'enllaça el socket al port tcp
    try:
        socktcp_connections.bind((server, int(local_tcp)))
    except:
        print_msg("Error a l'hora de fer el bind")
        exitprogram(0,0)

    while True:
        #Nomes fara la condicio quan el servidor estigui a l'estat SEND_ALIVE
        if client_status == SEND_ALIVE:
            #S'espera que es connecti el servidor
            socktcp_connections.listen(1)
            connection, address = socktcp_connections.accept()
            data = connection.recv(127)
            check_connection_received(data, connection, address, client_id)

def check_connection_received(data, connection, address, client_id):
    """Es comprova la conneccio que envia el servidor"""
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
            print_pdu_tcp_debug("Enviat", packet)
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

def set_elements(element, value):
    #S'afefeix el valor al element
    global elements
    for el in elements:
        if el[0] in element:
            el[1] = value

def is_input(element):
    #Es comprova que el element acabi amb I
    element_split = element.split('-')
    return 'I' == element_split[2][0]

def element_exists(element_sent):
    #Es comprova si l'element existeix
    for element in elements:
        if element[0] in element_sent:
            return True
    return False

#Funcions que envien dades TCP al servidor
def send_commands(client_id):
    while True:
        command = input()
        if client_status == SEND_ALIVE:
            #Nomes es podran escriure comandes quan estiui amb SEND_ALIVE
            command_tcp_server(command, client_id)

def command_tcp_server(command, client_id):
    #Per cada commanda disponibles es criden a les funcions pertinents
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
    try:
        socktcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    except:
        print_msg("Error en la creació del socket TCP")
        exitprogram(0, 0)
    try:
        socktcp.connect(address)
    except:
        print_msg("Error en el connect")
        exitprogram(0, 0)

    print_msg("Iniciada comunicació TCP amb el servidor (port: "+tcp_server)
    socktcp.send(tcp_packet)
    check_TCP_send_response(client_id, element, value, random_number_REG_ACK)

def check_TCP_send_response(client_id, element_sent, value_sent, random_sent):
    #Es comprova la reposta del servidor despres del SEND_DATA
   global client_status

   #socktcp.settimeout(m)
   sock = select.select([socktcp], [], [], m)
   if sock[0]:
       response = sock[0][0].recv(127)
   else:
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
    #Obtenim el valor assignat a un aliment
    for element_value in elements:
        if element_value[0] == element:
            return element_value[1]

def is_set_command_OK(command):
    # Es comprova que la comanda estigui ben escrita
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
    #Es comprova que la comanda estigui ben escrita
    str = command.split()
    if len(str) != 2:
        return False
    send_OK = str[0] == "send"
    return send_OK and len(str) == 2 #The server will tell us If the element doesn't exist

def exitprogram(signum, stack):
    #Es tanquen tots els threads i els sockets en cas de que s'hagin inicialitzat
    if sock_udp:
        print_msg("Es tanquen les conexions del socket UDP")
        sock_udp.close()
    if socktcp_connections:
        print_msg("Es tanquen les conexions del socket TCP")
        socktcp_connections.close()
    os._exit(0)


def create_pdu_udp(pdu_udp):
    """#Empaqueta la tupla amb el pdu_udp corresponent"""
    st = struct.Struct('1B 13s 9s 61s')
    return st.pack(*pdu_udp)

def create_pdu_tcp(pdu_tcp):
    """Empaqueta la tupla amb el pdu_tcp corresponent"""
    st = struct.Struct('1B 13s 9s 8s 16s 80s')
    return st.pack(*pdu_tcp)

def read_elements(message):
    """Llegeix els elements de l'arxiu de configuracio"""
    elements = message.split(';')
    elements_value = []
    for i in range(len(elements)):
        elements_value.append([elements[i], "NONE"])

    return elements_value

def read_client(file):
    """Es llegeix la info de l'arxiu de configuracio"""
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
    """S'obtenen els arxius de configuració corresponents"""
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

    print ("Execució: ./client.py -d -c <arxiuclient.cfg> ")
    exit(1)

def print_debug(text):
    if debug:
        local_time = time.localtime()
        print(time_format(local_time.tm_hour)+':'+time_format(local_time.tm_min)+':'+time_format(local_time.tm_sec)+": DEBUG  => "+text)

def print_pdu_udp_debug(env_o_rec, response):
    if debug:
        packet_type, id, random_number, data =  struct.unpack('1B 13s 9s 61s', response)
        print_debug(env_o_rec+": bytes="+str(len(response))+", paquet="+hex_to_packet_str(packet_type)+", id="+repr(id).split('\\')[0][2:]+", rndm="+repr(random_number).split('\\')[0][2:]+", dades="+repr(data).split('\\')[0][2:])

def print_pdu_tcp_debug(env_o_rec, response):
    if debug:
        packet_type, id, random_number, element, value, info = struct.unpack('1B 13s 9s 8s 16s 80s', response)
        print_debug(env_o_rec+": bytes="+str(len(response))+", paquet="+hex_to_packet_str(packet_type)+", id="+
                    repr(id).split('\\')[0][2:]+", rndm="+repr(random_number).split('\\')[0][2:]+", element="+repr(element).split('\\')[0][2:]
                    +", valors ="+repr(value).split('\\')[0][2:]+", dades="+repr(info).split('\\')[0][2:])

def print_msg(text):
    local_time = time.localtime()
    print(time_format(local_time.tm_hour) + ':' + time_format(local_time.tm_min) + ':' + time_format(local_time.tm_sec) + ": MSG    => " + text)

def time_format(int_time):
    """"Sets time into the right format HH:MM:SS"""
    time = str(int_time)
    return '0'+time if int_time < 10 else time

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


if __name__ == "__main__":
    main()