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
#include "./eval/tinyexpr.h"

#define SEM_NAME "/semaphore"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define PAGESIZE 4096

int LEVEL_LIMIT = 7;

int original_process;

int pipefds[2];
int pipefds2[2];

int readMessage;
char readMessage2[20];

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
    double time;
    boolean state;
    struct node * next;
} node_t;

node_t * head = NULL;

const char* getState(boolean isBool) {
    if (isBool == F ) {
        return "false";
    } else {
        return "true";
    }
}

void printList() {
    printf("Print Function\n");
    node_t * current = head;

    while (current != NULL) {
        printf("%d\n", current->meeseeks);
        printf("%f\n", current->time);
        printf("%s\n", getState(current->state));
        current = current->next;
    }
}

void push(int meeseeks, double time, boolean state) {
    printf("Push Function\n");
    node_t * current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = (node_t *) malloc(sizeof(node_t));
    current->next->meeseeks = meeseeks;
    current->next->time = time;
    current->next->state = state;
    current->next->next = NULL;
}


void initSharedVariables(){
    //setup semaforos
    sem_init(&mutex, 0, 1);
    //setup variable compartida de las instancias
    shared_instances = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *shared_instances = 1;
    //setup variable compartida para saber si algún meeseeks pudo completar la tarea
    isFinished = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *isFinished = 0;
}

int initPipe(){
    if( pipe(pipefds) == -1){
        printf("Unable to create pipe \n");
        return 1;
    }

    if( pipe(pipefds2) == -1){
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
    write(pipefds[1],  &readMessage, sizeof(difficulty));
    sem_post(&mutex);
    return difficulty;
}

char* getRequest(){
    sem_wait(&mutex);
    read(pipefds2[0],  &readMessage2, sizeof(readMessage2));
    char* request = readMessage2;
    sem_post(&mutex);
    return request;
}

void informFinish(){
    sem_wait(&mutex);
    *isFinished = 1;
    sem_post(&mutex);
}

void resolveArithmetic(char* arithmetic){
    pid_t meeseek;
    int input_pipe[2];

    if (pipe (input_pipe)) {
        toc = clock();
        double time_spent = (double)(toc - tic) / CLOCKS_PER_SEC;
        push(1, time_spent, F);
        fprintf (stderr, "Pipe failed.\n");
    }

    meeseek = fork ();

    if (meeseek < 0) {
        toc = clock();
        double time_spent = (double)(toc - tic) / CLOCKS_PER_SEC;
        push(1, time_spent, F);
        fprintf(stderr, "fork Failed" );
    } else if (meeseek == 0) {
        close(input_pipe[1]);
        char concat_str[100];
        read(input_pipe[0], concat_str, 100);
        const char *expression = concat_str;
        int result ;
        printf("%f\n", te_interp(expression, &result));
        toc = clock();
        double time_spent = (double)(toc - tic) / CLOCKS_PER_SEC;
        push(1, time_spent, T);

        close(input_pipe[0]);
    } else {
        close(input_pipe[0]);
        write(input_pipe[1], arithmetic, strlen(arithmetic)+1);
        close(input_pipe[1]);
        wait(NULL);
    }
    
}

void execExternalProgram(char* request){
    FILE *fp;
    const char* command = request;
    fp = popen(command, "r");
    char buffer[BUFSIZ + 1];
    int chars_read;
    memset(buffer, '\0', sizeof(buffer));
    if(fp != NULL){
        chars_read = fread(buffer, sizeof(char), BUFSIZ, fp);
        toc = clock();
        if(chars_read > 0){
            double time_spent = (double)(toc - tic) / CLOCKS_PER_SEC;
            push(1, time_spent, T);
            printf("Output was:-\n%s\n",buffer);
        }
        double time_spent = (double)(toc - tic) / CLOCKS_PER_SEC;
        push(1, time_spent, T);
        pclose(fp);
    }
}

void createMrMeeseeks(int cantidad, int nivel, int type) {
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
        printf("Hi I'm Mr Meeseeks! Look at Meeeee. (pid:%d, ppid: %d, n: %d, i: %d) \n", getpid(), getppid(), nivel, instances);
        if(type == 1){
            int difficulty = getDifficulty();
            srand(time(NULL) ^ (getpid()<<16));
            int prob  = rand() % 100 + 1;
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
                createMrMeeseeks(cantMeeseeks, nivel, type);
            }
        }else if(type == 3){
            char* request = getRequest();
            execExternalProgram(request);
            printf("Byeeeeee\n");
            return;
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
        int type = 0;
        printf("Welcome to the box menu\n\n");
        printf("1.  Written request\n");
        printf("2.  Math request\n");
        printf("3.  External program request\n");
        printf("4.  Close Mr.Meeseeks box\n");
        scanf("%d",&choice);
        getchar();
        switch(choice){
            case 1:  printf("What's the request:");
                scanf("%[^\n]%*c", &request);
                printf("Degree of difficulty of the task:");
                scanf("%d", &difficulty);
                tic = clock();
                write(pipefds[1],  &difficulty, sizeof(difficulty));
                if(difficulty < 45){
                    cantMeeseeks = 3;
                }else if(difficulty < 85){
                    cantMeeseeks = 1;
                }
                break;
            case 2:  printf("What's the request:");
                scanf("%s",&request);
                printf ( "%s\n", &request);
                resolveArithmetic(&request);
                break;
            case 3:  printf("What's the request:");
                scanf("%[^\n]%*c", &request);
                tic = clock();
                write(pipefds2[1],  &request, (strlen(&request))+1);
                cantMeeseeks = 1;
                type = 3;
                break;
            case 4: printf("Closing Mr.Meeseeks box, bye\n");
                printList();
                break;
            default:
                    printf("Noooo can't do, please try to enter a valid option\n");
                    break;
        }
        createMrMeeseeks(cantMeeseeks,1, type);
        if(getpid() != box_id){
            break;
        }
    }while(choice != 4);
    if(getpid() == box_id){
        sem_destroy(&mutex);
    }
    
    return 0;
}
