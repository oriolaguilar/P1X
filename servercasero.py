#!/usr/bin/python3

import socket #import the socket module

s = socket.socket() #Create a socket object
port = 7777 # Reserve a port for your service
s.bind(('localhost', port)) #Bind to the port

s.listen(5) #Wait for the client connection
while True:
    c,addr = s.accept() #Establish a connection with the client
    print ("Got connection from", addr)
    c.send(b'Thank you for connecting!')
    c.close()