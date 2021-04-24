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

#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define PAGESIZE 4096

int LEVEL_LIMIT = 3;

int original_process;

int pipefds[2];     //Pipe que comunica la dificultad de la tarea entre los Meeseeks
int pipefds2[2];    //Pipe que communica la solicitud del usuario al Meeseek

int readMessage;    //Variable que recibe el mensaje del primer pipe(dificultad)
char readMessage2[20];  //Recibe el mensaje del segundo pipe(Solicitud)

//Variables globales compartidas
u_int8_t *shared_instances;
u_int8_t *isFinished;

//semaforos
sem_t sem_shared_instances;
sem_t sem_isFinished;
sem_t sem_pipefds;
sem_t sem_pipefds2;

clock_t tic;
clock_t toc;

void initSharedVariables(){         //inicia las variables compartidas, utilizando un mmap, para asignarles un espacio de memoria a cada una
    //setup semaforos
    sem_init(&sem_shared_instances, 0, 1);
    sem_init(&sem_isFinished, 0, 1);
    sem_init(&sem_pipefds, 0, 1);
    sem_init(&sem_pipefds2, 0, 1);
    //setup variable compartida de las instancias
    shared_instances = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *shared_instances = 1;
    //setup variable compartida para saber si algún meeseeks pudo completar la tarea
    isFinished = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *isFinished = 0;
}

int initPipe(){                     //Inicializa los dos pipes que comunicaran a los Mr.Meeseeks               
    if( pipe(pipefds) == -1){
        printf("Unable to create pipe \n");
        return 1;
    }

    if( pipe(pipefds2) == -1){
        printf("Unable to create pipe \n");
        return 1;
    }
}

int addInstance(){                  //Agrega una instancia a las instancias totales del programa
    int instances;
    sem_wait(&sem_shared_instances);   //Espera a poder acceder a la variable compartida
    instances = *shared_instances + 1;
    *shared_instances = instances;  //aumenta la variable
    sem_post(&sem_shared_instances);   //Libera la variable, para que otro proceso la pueda accedar
    return instances;   
}

int getDifficulty(){                //Consigue la difficultad pasada por el primer pipe (pipefds)
    sem_wait(&sem_pipefds);
    read(pipefds[0],  &readMessage, sizeof(readMessage));   //Lee el mensaje del pipe
    int difficulty = readMessage;      
    if(readMessage > 0){
        difficulty = readMessage + 1;   //si la dificultad no es 0, la aumenta 1 por cada meeseeks que accede a la dificultad
    }
    write(pipefds[1],  &readMessage, sizeof(difficulty));   //escribe la nueva dificultad para que sea accedida por el proximo meeseeks
    sem_post(&sem_pipefds);
    return difficulty;
}

char* getRequest(){                 //Consigue la solicitud pasada por el segundo pipe
    sem_wait(&sem_pipefds2);
    read(pipefds2[0],  &readMessage2, sizeof(readMessage2));
    char* request = readMessage2;
    sem_post(&sem_pipefds2);
    return request;
}

void informFinish(){                //Si un meeseeks termina, utiliza la variable compartida como flag para informar a los otros meeseeks que la tarea ha sido finalizada
    sem_wait(&sem_isFinished);
    *isFinished = 1;
    sem_post(&sem_isFinished);
}

void informImposibleTask(){
    sem_wait(&sem_isFinished);
    if(*isFinished == 0){
        printf("Rick: Looks like we are destroying this dimension with all ..burp.. the meeseeks Morty\n");
        *isFinished = 1;
    }
    sem_post(&sem_isFinished);
}

void execExternalProgram(char* request){        //Ejecuta un comando que le entre como solicitud del usuario
    FILE *fp;
    const char* command = request;
    fp = popen(command, "r");   //se utiliza la función popen para ejecutar el comando, mediante un pipe
    char buffer[BUFSIZ + 1];
    int chars_read;
    memset(buffer, '\0', sizeof(buffer));
    if(fp != NULL){
        chars_read = fread(buffer, sizeof(char), BUFSIZ, fp);
        if(chars_read > 0){
            printf("Output was:-\n%s\n",buffer);    //Si no falla el comando, se informa del resultada al usuario
        }
        pclose(fp);
    }
}

void createMrMeeseeks(int cantidad, int nivel, int type) {     //Función encargada de crear a los meeseeks
    sem_wait(&sem_isFinished);
    if(*isFinished == 1){       //Si la tarea fue finalizada por otro meeseek, todos los meeseeks se despiden y terminan
        printf("Byeeeeee\n");
        return;
    }
    sem_post(&sem_isFinished);
    pid_t pId;
    printf("creando %d hijos \n", cantidad);
    for(int i = 0; i < cantidad; i++){      //crea n procesos hijos para el proceso
        pId = fork();
        if(pId < 0){
            fprintf(stderr, "Fork fallo"); 
            break;
        }else if(pId == 0) {    //sino es el proceso padre, se sale del loop para no cree hijos no deseados
            break;
        }
    }
    int instances;
    if(pId == 0) {  //Proceso hijo
        sleep(1);
        nivel = nivel + 1;
        instances = addInstance();
        printf("Hi I'm Mr Meeseeks! Look at Meeeee. (pid:%d, ppid: %d, n: %d, i: %d) \n", getpid(), getppid(), nivel, instances);
        if(type == 1){  //Si la tarea es una solicitud textual
            int difficulty = getDifficulty();
            printf("Dificultad: %d\n", difficulty);
            srand(time(NULL) ^ (getpid()<<16));
            int prob  = rand() % 100 + 1;   //se hace un condición probabilistica, para determinar si el meeseeks logro hacer la tarea
            if( prob * 1.5 < difficulty){   //Si logro hacer la tarea se despide
                informFinish();
                printf("Byeeeeee\n");
            }else{                          //si no lo logro, crea más meeseeks dependiendo de la dificultad actual de la tarea
                sleep(2);           
                int cantMeeseeks = 0;
                if(difficulty < 45){
                    cantMeeseeks = 3;
                }else if(difficulty < 85){
                    cantMeeseeks = 1;
                }
                if(nivel >= LEVEL_LIMIT){   //si alcanza el limite de niveles, se terminan todos los meeseeks
                    informImposibleTask();
                    printf("Byeeeeee\n");
                    return;
                }   
                createMrMeeseeks(cantMeeseeks, nivel, type);
            }
        }else if(type == 3){    //si la solicitud es una ejecución de un programa externo
            char* request = getRequest();
            execExternalProgram(request);
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
                write(pipefds[1],  &difficulty, sizeof(difficulty));        //escribe en el primer pipe la dificultad de la tarea
                if(difficulty < 45){
                    cantMeeseeks = 3;
                }else if(difficulty < 85){
                    cantMeeseeks = 1;
                }
                type = 1;
                break;
            case 2:  printf("Aqui va lo de eval\n");
                break;
            case 3:  printf("What's the request:");
                scanf("%[^\n]%*c", &request);
                write(pipefds2[1],  &request, (strlen(&request))+1);       //escribe en el segundo pipe el comando que desea ejecutar el usuario
                cantMeeseeks = 1;
                type = 3;
                break;
            case 4: printf("Closing Mr.Meeseeks box, bye\n");
                break;
            default:
                    printf("Noooo can't do, please try to enter a valid option\n");
                    break;
        }
        createMrMeeseeks(cantMeeseeks,1, type);
        if(getpid() != box_id){ //si no es el proceso box, procede a terminar el proceso
            break;  
        }
    }while(choice != 4);
    if(getpid() == box_id){     //una vez terminado el proceso box, se cierra los pipes y el semaforo
        sem_destroy(&sem_shared_instances);
        sem_destroy(&sem_isFinished);
        sem_destroy(&sem_pipefds);
        sem_destroy(&sem_pipefds2);
        close(pipefds[1]);
        close(pipefds[0]);
        close(pipefds2[1]);
        close(pipefds2[0]);
    }
    return 0;
}
