#!/usr/bin/env python3

import socket
import select
import time
import sys

REG_REQ = 0x00
REG_INFO = 0x01
REG_ACK = 0x02
INFO_ACK = 0x03
REG_REJ = 0x06

DISCONNECTED = 0xa0
NOT_REGISTERED = 0xa1
WAIT_ACK_REG = 0xa2
WAIT_INFO = 0xa3
WAIT_ACK_INFO = 0xa4
REGISTERED = 0xa5
SEND_ALIVE = 0xa6


def main():
    # We test that arguments are OK
    if len(sys.argv) != 3:
        print("Us: "+sys.argv[0]+" -c <nom_arxiu>")
        return

    if sys.argv[1] != '-c':
        print("Us: "+sys.argv[0]+" -c <nom_arxiu>")
        return

    # Read client.cfg file
    client_id, elements_semicolon, local_tcp, server, server_udp = read_client(sys.argv[2])
    elements = read_elements(elements_semicolon)

    # Register PDU-UDP
    print(server, server_udp)
    register(client_id, server, server_udp)


def register(client_id, host, port):
    pdp_udp = [REG_REQ, client_id, "000000000", ""]

    client_state = NOT_REGISTERED
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((host, int(port)))
    sock.connect((host, int(port)))

    # S'envia el paquet
    client_state = WAIT_ACK_REG


def read_elements(message):
    elements = []
    length = len(message)
    end = 8
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





if __name__ == "__main__":
    main()
