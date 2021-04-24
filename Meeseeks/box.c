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
#include <string.h>
#include "./eval/eval.h"

#define SEM_NAME "/semaphore"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define PAGESIZE 4096

int LEVEL_LIMIT = 7;

int original_process;

int pipefds[2];
int returnStatus;
int readMessage;

u_int8_t *shared_instances;
u_int8_t *isFinished;

sem_t mutex;

clock_t tic;
clock_t toc;

void initSharedVariables(){
    //setup semaforos
    sem_init(&mutex, 0, 1);
    //setup variable compartida de las instancias
    shared_instances = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *shared_instances = 0;
    //setup variable compartida para saber si algún meeseeks pudo completar la tarea
    isFinished = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *isFinished = 0;
}

int initPipe(){
    returnStatus = pipe(pipefds);
    if(returnStatus == -1){
        printf("Unable to create pipe \n");
        return 1;
    }
}

int addInstance(){
    int instances;
    sem_wait(&mutex);
    instances = *shared_instances + 1;
    *shared_instances = instances;
    sem_post(&mutex);
    return instances;
}

int getDifficulty(){
    sem_wait(&mutex);
    read(pipefds[0],  &readMessage, sizeof(readMessage));
    int difficulty = readMessage;
    if(readMessage > 0){
        difficulty = readMessage + 1;
    }
    write(pipefds[1],  &difficulty, sizeof(difficulty));
    sem_post(&mutex);
    return difficulty;
}

void informFinish(){
    sem_wait(&mutex);
    *isFinished = 1;
    sem_post(&mutex);
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
    int instances;
    if(pId == 0) {  //Proceso hijo
        sleep(5);
        nivel = nivel + 1;
        instances = addInstance();
        int difficulty = getDifficulty();
        srand(time(NULL) ^ (getpid()<<16));
        int prob  = rand() % 100 + 1;
        printf("Hi I'm Mr Meeseeks! Look at Meeeee. (pid:%d, ppid: %d, n: %d, i: %d) \n", getpid(), getppid(), nivel, instances);
        if( prob * 1.5 < difficulty){
            informFinish();
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
                informFinish();
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
    //setup pipes
    initSharedVariables();
    if(initPipe() == 1) return 1;

    int box_id = getpid();
    int difficulty;
    char request;
    int choice;
    do{
        int cantMeeseeks = 0;
        int nivel = 1;
        printf("Welcome to the box menu\n\n");
        printf("1.  Written request\n");
        printf("2.  Math request\n");
        printf("3.  Close Mr.Meeseeks box\n");
        scanf("%d",&choice);
        switch(choice){
            case 1:  printf("What's the request:");
                scanf("%s", &request);
                printf("Degree of difficulty of the task:");
                scanf("%d", &difficulty);
                write(pipefds[1],  &difficulty, sizeof(difficulty));
                printf(" Hi I'm Mr Meeseeks! Look at Meeeee. (pid:%d, ppid: %d, n: %d, i: %d)\n", getpid(), getppid(), nivel, *shared_instances);
                if(difficulty < 45){
                    cantMeeseeks = 3;
                }else if(difficulty < 85){
                    cantMeeseeks = 1;
                }
                break;
            case 2:  printf("Aqui va lo de eval\n");
                break;
            case 3:  printf("Closing Mr.Meeseeks box, bye\n");
                break;
            default:
                    printf("Noooo can't do, please try to enter a valid option\n");
                    break;
        }
        createMrMeeseeks(cantMeeseeks,1);
        if(getpid() != box_id){
            break;
        }
    }while(choice != 3);
    if(getpid() == box_id){
        sem_destroy(&mutex);
    }
    return 0;
}