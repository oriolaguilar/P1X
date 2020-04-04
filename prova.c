#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>

struct dict{
    int *pipe;
};
typedef struct dict Dict, *PointerDict;

PointerDict dictionary;



void init();
void sighandler(int);
void aux();
int * obtenir();


int main(){
    init();
    pid_t father = getpid();
    signal(SIGCHLD, sighandler);
    


    if(pipe(obtenir())==-1){
        fprintf(stderr, "Pipe Failed" ); 
        return 1;
    }
    int * pipe_to_write = obtenir();

    pid_t pid_child = fork();
    if(pid_child==0){
        aux();
    }else{
        char *input_str="Hola"; 
        close(pipe_to_write[0]);
        int * pipe_to_write = obtenir();
        write(pipe_to_write[1], input_str, strlen(input_str)+1);
        sleep(2);
        char *input_str2="Adeu";
        write(pipe_to_write[1], input_str2, strlen(input_str2)+1);
        printf("Arribo \n");
        sleep(30);
    }

}

void sighandler(int signum) {
    pid_t n=wait(NULL);
    printf("Ha mort --> %d \n", n);
}

void init(){
    dictionary = malloc(1*sizeof(PointerDict));
    dictionary[0].pipe = malloc(2*sizeof(int));
}

void aux(){
    int i=0;
    while (i<2){
        char concat_str[5]; 
        int * pipe_to_read = obtenir();
        read(pipe_to_read[0], concat_str, 5);
        printf("%s \n", concat_str);
        i++;
    }
}

int * obtenir(){
    return dictionary[0].pipe;
}