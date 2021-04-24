#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include "./eval/tinyexpr.h"

#define SEM_NAME "/semaphore"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define PAGESIZE 4096

int LEVEL_LIMIT = 7;

int original_process;

int pipefds[2];
int returnStatus;
int readMessage;

u_int8_t *shared_instances;
u_int8_t *shared_levels;
u_int8_t *isFinished;

sem_t mutex;

clock_t tic;
clock_t toc;

typedef enum { F, T } boolean;

typedef struct node
{
    int meeseeks;
    int time;
    boolean state;
    struct node * next;
} node_t;

void print_list(node_t * head) {
    node_t * current = head;

    while (current != NULL) {
        printf("%d\n", current->meeseeks);
        printf("%d\n", current->time);
        printf("%s\n", getState(current->state));
        current = current->next;
    }
}

const char* getState(boolean isBool) {
    if (isBool == F ) {
        return "false";
    } else {
        return "true";
    }
}


void createMrMeeseeks(int cantidad, int nivel) {
    sem_wait(&mutex);
    if(*isFinished == 1){
        printf("Byeeeeee\n");
        return;
    }
    sem_post(&mutex);
    pid_t pId;
    printf("creando %d hijos \n", cantidad);
    for(int i = 0; i < cantidad; i++){
        pId = fork();
        if(pId < 0){
            fprintf(stderr, "Fork fallo"); 
            break;
        }else if(pId == 0) {
            break;
        }
    }
    int instancias;
    if(pId == 0) {  //Proceso hijo
        sleep(5);
        nivel = nivel + 1;
        sem_wait(&mutex);
        read(pipefds[0],  &readMessage, sizeof(readMessage));
        int difficulty = readMessage;
        if(readMessage > 0){
            difficulty = readMessage + 1;
        }
        write(pipefds[1],  &difficulty, sizeof(difficulty));

        instancias = *shared_instances + 1;
        *shared_instances = instancias;
        
        sem_post(&mutex);
        srand(time(NULL) ^ (getpid()<<16));
        int prob  = rand() % 100 + 1;
        printf("Hi I'm Mr Meeseeks! Look at Meeeee. (pid:%d, ppid: %d, n: %d, i: %d) \n", getpid(), getppid(), nivel, instancias);
        if( prob * 1.5 < difficulty){
            sem_wait(&mutex);
            *isFinished = 1;
            sem_post(&mutex);
            printf("Byeeeeee\n");
        }else{
            sleep(2);
            int cantMeeseeks = 0;
            if(difficulty < 45){
                cantMeeseeks = 3;
            }else if(difficulty < 85){
                cantMeeseeks = 1;
            }
            if(nivel >= LEVEL_LIMIT){
                printf("Byeeeeee\n");
                return;
            }   
            createMrMeeseeks(cantMeeseeks, nivel);
        }
    }
    else{ //Proceso Padre
        for(int i = 0; i < cantidad; i++){
            wait(NULL);
        } 
        printf("Byeeeeee\n");
    }
}


int main(){
    //setup semaforos
    sem_init(&mutex, 0, 1);


    //setup pipes
    returnStatus = pipe(pipefds);
    if(returnStatus == -1){
        printf("Unable to create pipe \n");
        return 1;
    }

    original_process = getpid();
    int difficulty;
    char solicitud[1024];
    printf("Que solicitud quiere pedir:");
    *solicitud = 0;
	gets (solicitud) ;
    printf("Grado de dificultad de la tarea:");
    scanf("%d", &difficulty);

    write(pipefds[1],  &difficulty, sizeof(difficulty));

    shared_instances = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *shared_instances = 1;

    isFinished = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *isFinished = 0;

    int cantMeeseeks = 0;
    int nivel = 1;
    tic = clock();
    printf(" Hi I'm Mr Meeseeks! Look at Meeeee. (pid:%d, ppid: %d, n: %d, i: %d)\n", getpid(), getppid(), nivel, *shared_instances);
    //int status = system("./box");

    int result ;
    printf("solicitud: %s\n", solicitud);
    printf("%f\n", te_interp("5*5", &result));
    //else
        //evaluator_perror ( ) ;
    //if(difficulty < 45){
        //cantMeeseeks = 3;
    //}else if(difficulty < 85){
        //cantMeeseeks = 1;
    //}
    //createMrMeeseeks(cantMeeseeks,1);
    //sem_destroy(&mutex);
    
    
    return 0;
}

// gcc box.c ./eval/eval.c -lpthread -lm -o box