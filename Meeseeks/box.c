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

#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define PAGESIZE 4096

int LEVEL_LIMIT = 7;

int box_id;

int pipefds[2];     //Pipe que comunica la dificultad de la tarea entre los Meeseeks
int pipefds2[2];    //Pipe que communica la solicitud del usuario al Meeseek

int readMessage;    //Variable que recibe el mensaje del primer pipe(dificultad)
char readMessage2[20];  //Recibe el mensaje del segundo pipe(Solicitud)

//Variables globales compartidas
u_int8_t *shared_instances;
u_int8_t *taskCompleted;
u_int8_t *isFinished;
double *time_spent;

//semaforos
sem_t sem_shared_instances;
sem_t sem_isFinished;
sem_t sem_pipefds;
sem_t sem_pipefds2;

clock_t tic;
clock_t toc;

typedef enum { F, T } boolean;

struct node // Estructura con la info del reporte
{
    const char* request;
    int meeseeks;
    double time;
    boolean state;
    struct node * next;
};

struct node * head = NULL;

const char* getState(boolean isBool) { // Imprime el estado
    if (isBool == F ) {
        return "false";
    } else {
        return "true";
    }
}

void printList() { // Imprime la lista simple enlazada
    struct node * current = head;
    if(current==NULL) {
        printf("Head Esta nulo\n");
    }

    while (current != NULL) {
        printf("\n Report \n");
        printf("Request: %s\n", current -> request);
        printf("Amount of Meeseeks: %d\n", current -> meeseeks);
        printf("Time: %f\n", current->time);
        printf("State: %s\n", getState(current->state));
        current = current->next;
    }
}

void push(const char* request, int meeseeks,  double time, boolean state) { // Anade un nuevo valor a la lista de reportes|
    printf("Request: %s\n", request);
    printf("Amount of Meeseeks: %d\n", meeseeks);
    printf("Time: %f\n", time);
    printf("State: %s\n", getState(state));

    struct node *current = (struct node*) malloc(sizeof(struct node));
    current->request = request;
    current->meeseeks = meeseeks;
    current->time = time;
    current->state = state;
    current->next = head;

    head = current;

    if(head==NULL) {
        printf("Head Esta nulo \n");
    }
}


void initSharedVariables(){         //inicia las variables compartidas, utilizando un mmap, para asignarles un espacio de memoria a cada una
    //setup semaforos
    sem_init(&sem_shared_instances, 0, 1);
    sem_init(&sem_isFinished, 0, 1);
    sem_init(&sem_pipefds, 0, 1);
    sem_init(&sem_pipefds2, 0, 1);
    //setup variable compartida de las instancias
    shared_instances = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *shared_instances = 0;
    //setup variable compartida para saber si algún meeseeks pudo completar la tarea
    isFinished = mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *isFinished = 0;

    taskCompleted =   mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *taskCompleted = 0;

    time_spent =   mmap(NULL, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *time_spent = 0;
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

int getInstances(){
    int instances;
    sem_wait(&sem_shared_instances);
    instances = *shared_instances;
    sem_post(&sem_shared_instances);
    return instances;
}

int getDifficulty(){                //Consigue la difficultad pasada por el primer pipe (pipefds)
    sem_wait(&sem_pipefds);
    read(pipefds[0],  &readMessage, sizeof(readMessage));   //Lee el mensaje del pipe
    int difficulty = readMessage;      
    if(readMessage > 0){
        difficulty = readMessage + 1;   //si la dificultad no es 0, la aumenta 1 por cada meeseeks que accede a la dificultad
    }
    write(pipefds[1],  &difficulty, sizeof(difficulty));   //escribe la nueva dificultad para que sea accedida por el proximo meeseeks
    sem_post(&sem_pipefds);
    return difficulty;
}

char* getRequest(){                 //Consigue la solicitud pasada por el segundo pipe
    sem_wait(&sem_pipefds2);
    read(pipefds2[0],  &readMessage2, sizeof(readMessage2));
    char* request = readMessage2;
    write(pipefds2[1],  &readMessage2, sizeof(readMessage2));
    sem_post(&sem_pipefds2);
    return request;
}

void informFinish(int completed, double time){                //Si un meeseeks termina, utiliza la variable compartida como flag para informar a los otros meeseeks que la tarea ha sido finalizada
    sem_wait(&sem_isFinished);
    *isFinished = 1;
    *taskCompleted = completed;
    *time_spent = time;
    sem_post(&sem_isFinished);
}

double generateReport(char* request, int instances){
    sem_wait(&sem_isFinished);
    double time = *time_spent;
    int task = *taskCompleted;
    if(task){
        push(request, instances, time, T);
    }else{
        push(request, instances, time, F);
    }
    sem_post(&sem_isFinished);
}


void resetSharedMemory(){
    sem_wait(&sem_shared_instances);
    *shared_instances = 0;
    sem_post(&sem_shared_instances);
    sem_wait(&sem_isFinished);
    *isFinished = 0;
    *taskCompleted = 0;
    *time_spent = 0;
    sem_post(&sem_isFinished); 
}

void resetDifficultyPipe(){
    sem_wait(&sem_pipefds);
    read(pipefds[0],  &readMessage, sizeof(readMessage));
    sem_post(&sem_pipefds);
}

void resetRequestPipe(){
    sem_wait(&sem_pipefds2);
    read(pipefds2[0],  &readMessage2, sizeof(readMessage2));
    sem_post(&sem_pipefds2);
}


void resolveArithmetic(char* arithmetic){ // Resuleve los problemas arimeticos
    pid_t meeseek;
    int input_pipe[2];
    int out_pipe[2];

    if (pipe (input_pipe)) {
        toc = clock();
        double time_spent = (double)(toc - tic) / CLOCKS_PER_SEC;
        push(arithmetic, 1, time_spent, F);
        fprintf (stderr, "Pipe failed.\n");
    }

    if (pipe (out_pipe)) {
        toc = clock();
        double time_spent = (double)(toc - tic) / CLOCKS_PER_SEC;
        push(arithmetic,1, time_spent, F);
        fprintf (stderr, "Pipe failed.\n");
    }

    meeseek = fork ();

    if (meeseek < 0) {
        toc = clock();
        double time_spent = (double)(toc - tic) / CLOCKS_PER_SEC;
        push(arithmetic,1, time_spent, F);
        fprintf(stderr, "fork Failed" );
    } else if (meeseek == 0) {
        close(input_pipe[1]);
        char concat_str[100];
        read(input_pipe[0], concat_str, 100);
        const char *expression = concat_str;
        int error ;
        float result = te_interp(expression, &error);
        printf("%f\n", result);

        close(input_pipe[0]);
        close(out_pipe[0]);
        write(out_pipe[1], &result, sizeof(result));
        close(out_pipe[1]);

    } else {
        double result;
        close(input_pipe[0]);
        write(input_pipe[1], arithmetic, strlen(arithmetic)+1);
        close(input_pipe[1]);
        wait(NULL);
        close(out_pipe[1]);
        read(out_pipe[0], &result, sizeof(result));

        toc = clock();
        double time_spent = (double)(toc - tic) / CLOCKS_PER_SEC;
        push(arithmetic,1, time_spent, F);

        close(out_pipe[0]);
    }
    
}

void informImposibleTask(double time){
    sem_wait(&sem_isFinished);
    if(*isFinished == 0){
        printf("Rick: Looks like we are destroying this dimension with all ..burp.. the meeseeks Morty\n");
        *isFinished = 1;
        *taskCompleted = 0;
        *time_spent = time;
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
        toc = clock();
        if(chars_read > 0){
            double time_sp = (double)(toc - tic) / CLOCKS_PER_SEC;
            informFinish(1,time_sp);
            //push(command, 1, time_spent, T);
            printf("Output was:-\n%s\n",buffer);    //Si no falla el comando, se informa del resultada al usuario
        }else{
            double time_sp = (double)(toc - tic) / CLOCKS_PER_SEC;
            informFinish(0,time_sp);
        }
        //push(command, 1, time_spent, F);
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
                double time_sp = (double)(toc - tic) / CLOCKS_PER_SEC;
                informFinish(1,time_sp);
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
                    double time_sp = (double)(toc - tic) / CLOCKS_PER_SEC;
                    informImposibleTask(time_sp);
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
        if(getpid() == box_id){
            instances = getInstances();
            char* request = getRequest();
            generateReport(request, instances);
            resetSharedMemory();
            resetRequestPipe();
            if(type == 1){
                resetDifficultyPipe();
            }
        }else{
            printf("Byeeeeee\n");
        }
       
    }
}


int main(){
    //setup pipes
    initSharedVariables();
    if(initPipe() == 1) return 1;

    box_id = getpid();
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
                write(pipefds2[1],  &request, (strlen(&request))+1);
                printf("Degree of difficulty of the task:");
                scanf("%d", &difficulty);
                tic = clock();
                write(pipefds[1],  &difficulty, sizeof(difficulty));        //escribe en el primer pipe la dificultad de la tarea
                if(difficulty < 45){
                    cantMeeseeks = 3;
                }else if(difficulty < 85){
                    cantMeeseeks = 1;
                }
                type = 1;
                break;
            case 2:  printf("What's the request:");
                scanf("%s",&request);
                printf ( "%s\n", &request);
                resolveArithmetic(&request);
                break;
            case 3:  printf("What's the request:");
                scanf("%[^\n]%*c", &request);
                tic = clock();
                write(pipefds2[1],  &request, (strlen(&request))+1);       //escribe en el segundo pipe el comando que desea ejecutar el usuario
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
        if(cantMeeseeks > 0){
            createMrMeeseeks(cantMeeseeks,0, type);
        }
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
