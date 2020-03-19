#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>


void print_debug(char *text);

int main(){

    print_debug("Hola que tal");

}

void print_debug(char *text){
    /* Muestra, mediante la línea de comandos, un texto determinado con la hora actual*/
    
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