#include <stdio.h>
#include <stdbool.h>
#include <memory.h>

#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define CRESET   "\x1b[0m"

const int IN = 0;
const int OUT = 1;
const int AMP = 2;
const int PIPE = 3;

const int BUFF = 256;

const char pipeBuffer[] = {"buffer.txt"};

void readBaash();
void inputProcessor(int, char *[]);

void execRelativo(bool, int, char *[]);
int execAbs( int, char *[]);
void execPath( int, char *[]);
bool checkFlags(char *);

void changeIO(char []);
void stdIO();

int stdout_cpy;
int stdin_cpy;
int io_filename;

int srv_client_pipe[2];
int client_srv_pipe[2];

char pipeargs[256];


bool flag_oper[4] = {false, false, false, false};


int main() {

    char hostname[BUFF];
    char * user;

    //User y Hostname

    struct passwd *pass;
    pass = getpwuid(getuid());
    user = pass -> pw_name;

    char cwd[BUFF];

    gethostname(hostname, BUFF);

    while(1){

        getcwd(cwd, BUFF);

        printf(CYAN"%s@%s:"MAGENTA"~%s~"CRESET"$ ", user, hostname, cwd);

        readBaash();

        //Si hubo redireccion, vuelvo a los std
        if(flag_oper[OUT] || flag_oper[IN]){
            stdIO();
        }

        //Limpio flags
        for(int i = 0; i<3; i++){
            flag_oper[i] = false;
        }


    }

    return 0;
}

void readBaash(){

    int argc = 0;
    char * argv[BUFF];
    char input[BUFF];

    char *token;

    fgets(input, BUFF, stdin);

    if(feof(stdin)){
        exit(0);
    }

    /*if(!flag_oper[PIPE]){
        fgets(input, BUFF, stdin);
        if(feof(stdin)){
            exit(0);
        }
    }
    else{
        printf("ENTRE ACA LEA GIL\n");

        strcpy(input, pipeargs);

        flag_oper[PIPE] = false;
        flag_oper[IN] = true;
        changeIO(pipeBuffer);
    }*/

    input[strlen(input)-1] = '\0';

    if(!strcmp(input, "exit")){
        exit(0);
    }


    token = strtok(input, " ");

    while(token != NULL){

        //printf("token: %s\n", token);

        //Chequeo por '<' '>' '|'
        if(checkFlags(token)){
            break;
        }

        if(strcmp(token, "\n") != 0){
            argc++;
            argv[argc-1] = token;
        }

        token = strtok(NULL, " ");
    }

    inputProcessor(argc, argv);
}

bool checkFlags(char * token){

    char filename[BUFF];

    if(!strcmp(token, "<")){
        flag_oper[IN] = true;

        //paso el siguiente argumento como filename
        token = strtok(NULL, " ");
        strcpy(filename, token);

        changeIO(filename);

        return true;
    }

    if(!strcmp(token, ">")){
        flag_oper[OUT] = true;

        //paso el siguiente argumento como filename
        token = strtok(NULL, " ");
        strcpy(filename, token);

        changeIO(filename);

        return true;
    }

    if(!strcmp(token, "|")){

        flag_oper[PIPE] = true;

        //flag_oper[OUT] = true;



        token = strtok(NULL, "\0");
        strcpy(pipeargs, token);


        //changeIO(pipeBuffer);
        return true;
    }

    if(!strcmp(token, "&")){
        flag_oper[AMP] = true;

        return true;
    }

    return false;
}

void changeIO(char filename[]){

    int fid;
    int flags = O_RDWR|O_CREAT|O_TRUNC;
    int perm = S_IWUSR|S_IRUSR;

    stdout_cpy = dup(STDOUT_FILENO);
    stdin_cpy = dup(STDIN_FILENO);

    fid = open(filename, flags, perm);
    io_filename = fid;

    if(fid < 0){
        perror("open");
        exit(7);
    }

    if(flag_oper[OUT]){
        close(STDOUT_FILENO);
    } else{
        close(STDIN_FILENO);
    }

    if(dup(fid) < 0){
        perror("dup");
        exit(7);
    }

    close(fid);
}

void stdIO(){

    printf("ESTOY ACA y OUT: %i IN: %i"  "\n", flag_oper[OUT], flag_oper[IN]);

    close(io_filename);

    if(flag_oper[OUT]){

        printf("PONGO STDOUT\n");
        dup2(stdout_cpy, STDOUT_FILENO);
        close(stdout_cpy);
    } else{

        printf("PONGO STDIN\n");
        dup2(stdin_cpy, STDIN_FILENO);
        close(stdin_cpy);
    }

}

void inputProcessor(int argc, char *argv[] ){

    if(argc == 0){
        //No hay argumentos
        return;
    }

    if(strcmp(argv[0], "cd") == 0){

        //manejo de cd

        if(argc > 1){
            chdir(argv[1]); //si tiene arg me voy a esa direccion
        } else{
            chdir(getenv("HOME")); // caso contrario me voy al home
        }

    }
    else{

        bool prev_dir = !strncmp("..", argv[0], 2);

        argc++;
        argv[argc-1] = NULL;

        if(!strncmp(".", argv[0], 1) || prev_dir){
            //relativo
            execRelativo(prev_dir,argc, argv);
        }
        else if(!strncmp("/", argv[0], 1)){
            //absoluto
            if(!execAbs( argc, argv)){
                printf("%s: No such file or directory\n", argv[0]);
            }
        }
        else{
            //PATH o relativo
            execPath( argc, argv);
        }
    }
}

void execRelativo(bool prev_dir, int argc, char *argv[]){

    char path[BUFF];
    char prevPath[BUFF];

    if(prev_dir){
        getcwd(prevPath, BUFF);
        chdir("..");
    }

    getcwd(path, BUFF);

    char *str_aux;
    str_aux = strstr(argv[0], "/");

    strcat(path, str_aux);

    argv[0] = path;

    execAbs(argc, argv);

    if(prev_dir){
        chdir(prevPath);
    }

}

int execAbs(int argc, char *argv[]){

    pid_t pid;
    pid = fork();

    if(flag_oper[PIPE]){

    }

    if(pid < 0){
        printf("ERROR\n");
        exit(3);
    }

    if(pid != 0){
        //padre
        //Si no hay ampersand espero
        if(!flag_oper[AMP]){
            wait(0);
        }

    }
    else{
        //hijo
        if(execv(argv[0], argv) == -1){
            return 0;
        }

        exit(1);
    }

    return 1;
}

void execPath(int argc, char *argv[]){

    //trato de ejecutar como relativo
    if(execAbs(argc, argv)){
        return;
    }

    //BUSCAR PATH
    char path_aux[BUFF];
    char abs_path[BUFF];
    char argv_prev[BUFF];

    strcpy(path_aux, getenv("PATH"));

    char * token = strtok(path_aux, ":");

    while(token != NULL){

        strcpy(abs_path, token);
        strcat(abs_path, "/");
        strcat(abs_path, argv[0]);

        strcpy(argv_prev, argv[0]);
        argv[0] = abs_path;

        if(execAbs(argc, argv)){
            return;
        }

        argv[0] = argv_prev;

        token = strtok(NULL, ":");
    }

    printf("%s: command not found!\n", argv[0]);

}