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
const int PIPE = 2;
const int AMP = 3;

void inputProcessor(int, char *[]);

void execRelativo(bool, int, int, char *[]);
int execAbs(int, int, char *[]);
void execPath(int, int, char *[]);

void changeIO(char []);
void stdIO();

int stdout_cpy;
int stdin_cpy;
int io_filename;

bool flag_biennegro = false;


bool flag_oper[4] = {false, false, false, false};


int main() {

    int buffer = 100;
    char input[buffer];
    char *token;

    int argc = 0;
    char * argv[buffer];

    char hostname[20];
    char * user;

    //User y Hostname

    struct passwd *pass;
    pass = getpwuid(getuid());
    user = pass -> pw_name;

    char cwd[256];

    gethostname(hostname, 20);

    while(1){

        getcwd(cwd, 256);

        printf(CYAN"%s@%s:"MAGENTA"~%s~"CRESET"$ ", user, hostname, cwd);

        fgets(input, buffer, stdin);
        input[strlen(input)-1] = '\0';

        if(feof(stdin) || strcmp(input, "exit") == 0){
            return 0;
        }

        char filename[256];
        token = strtok(input, " ");

        while(token != NULL){

            //printf("token: %s\n", token);

            if(!strcmp(token, "<")){
                flag_oper[IN] = true;

                //paso el siguiente argumento como filename
                token = strtok(NULL, " ");
                strcpy(filename, token);

                changeIO(filename);

                break;
            }

            if(!strcmp(token, ">")){
                flag_oper[OUT] = true;

                //paso el siguiente argumento como filename
                token = strtok(NULL, " ");
                strcpy(filename, token);

                changeIO(filename);

                break;
            }

            if(!strcmp(token, "|")){
                flag_oper[PIPE] = true;
            }

            if(strcmp(token, "\n") != 0){
                argc++;
                argv[argc-1] = token;
            }

            token = strtok(NULL, " ");
        }

        inputProcessor(argc, argv);

        argc = 0;

        if(flag_oper[OUT] || flag_oper[IN]){
            stdIO();
        }

        for(int i = 0; i<4; i++){
            flag_oper[i] = false;
        }


    }

    return 0;
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

    close(io_filename);

    if(flag_oper[OUT]){
        dup2(stdout_cpy, STDOUT_FILENO);
        close(stdout_cpy);
    } else{
        dup2(stdin_cpy, STDIN_FILENO);
        close(stdin_cpy);
    }

}

void inputProcessor(int argc, char *argv[] ){

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
        int background = !strcmp("&", argv[argc-1]);

        if(background){
            argv[argc-1] = NULL;
        } else{
            argc++;
            argv[argc-1] = NULL;
        }

        if(!strncmp(".", argv[0], 1) || prev_dir){

            //relativo
            execRelativo(prev_dir, background,argc, argv);

        } else if(!strncmp("/", argv[0], 1)){

            //absoluto
            execAbs(background, argc, argv);

        } else{

            //PATH o relativo
            execPath(background, argc, argv);
        }

    }
}

void execRelativo(bool prev_dir, int bg, int argc, char *argv[]){

    char path[256];
    char prevPath[256];

    if(prev_dir){
        getcwd(prevPath, 256);
        chdir("..");
    }

    getcwd(path, 256);

    char *str_aux;
    str_aux = strstr(argv[0], "/");

    strcat(path, str_aux);

    argv[0] = path;

    execAbs(bg, argc, argv);

    if(prev_dir){
        chdir(prevPath);
    }

}

int execAbs(int bg, int argc, char *argv[]){

    pid_t pid;
    pid = fork();

    if(pid < 0){
        printf("ERROR\n");
        exit(3);
    }

    if(pid != 0){
        if(!bg){
            wait(0);
        }
    } else{
        if(execv(argv[0], argv) == -1){
            return 0;
        }

        exit(1);
    }

    return 1;
}

void execPath(int bg, int argc, char *argv[]){

    //trato de ejecutar como relativo
    if(execAbs(bg, argc, argv)){
        return;
    }

    //BUSCAR PATH
    char path_aux[256];
    char abs_path[256];
    char argv_prev[256];

    strcpy(path_aux, getenv("PATH"));

    char * token = strtok(path_aux, ":");

    while(token != NULL){

        strcpy(abs_path, token);
        strcat(abs_path, "/");
        strcat(abs_path, argv[0]);

        strcpy(argv_prev, argv[0]);
        argv[0] = abs_path;

        if(execAbs(bg, argc, argv)){
            return;
        }

        argv[0] = argv_prev;

        token = strtok(NULL, ":");
    }

    printf("%s: command not found!\n", argv[0]);

}