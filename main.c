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
#define CRESET   "\x1b[0m"

#define WRITE 1
#define READ 0

const int IN = 0;
const int OUT = 1;
const int AMP = 2;
const int PIPE = 3;

const int BUFF = 256;

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

char pipe_arguments[256];

void forkChild(int, char * []);

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
        for(int i = 0; i<4; i++){
            flag_oper[i] = false;
        }

        memset(pipe_arguments, 0, BUFF);
    }
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

        //No tomo al enter como argumento
        if(strcmp(token, "\n") != 0){
            argc++;
            argv[argc-1] = token;
        }

        token = strtok(NULL, " ");
    }

    forkChild(argc, argv);
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

        //guardo la cadena de argumentos restantes
        token = strtok(NULL, "/0");
        strcpy(pipe_arguments, token);

        return true;
    }

    if(!strcmp(token, "&")){
        flag_oper[AMP] = true;

        return true;
    }

    return false;
}

void forkChild(int argc, char * argv[]){

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

        pid_t child_pid;
        child_pid = fork();
        int status;

        if(child_pid < 0){
            printf("ERROR EN FORK HIJO\n");
            exit(3);
        }

        if(child_pid != 0){
            //padre

            //Si no hay ampersand espero al hijo
            if(!flag_oper[AMP]){
                waitpid(child_pid, &status, 0);
            }

        }
        else{
            //hijo

            if(flag_oper[PIPE]){
                //Manejo del pipe
                char * argv_pipe[BUFF];
                int    argc_pipe = 0;

                char * token;
                token = strtok(pipe_arguments, " ");

                //termino de recolectar argumentos
                while(token != NULL){

                    //No tomo al enter como argumento
                    if(strcmp(token, "\n") != 0){
                        argv_pipe[argc_pipe] = token;
                        argc_pipe++;
                    }

                    token = strtok(NULL, " ");
                }

                int p[2];
                pipe(p);

                pid_t grandch_pid;
                grandch_pid = fork(); //creamos un nieto

                if(grandch_pid < 0){
                    perror("ERROR EN FORK NIETO\n");
                    exit(1);
                }

                if(grandch_pid == 0){
                    //nieto
                    close(p[READ]);
                    dup2(p[WRITE],1);	//salida del nieto a entrada del hijo

                    inputProcessor(argc, argv);

                    exit(1);
                }
                else{
                    //hijo
                    close(p[WRITE]);
                    dup2(p[READ],0);

                    wait(0);	//Espera a su hijo

                    inputProcessor(argc_pipe, argv_pipe);
                    exit(1);
                }
            }
            else{

                inputProcessor(argc, argv);
                exit(1);
            }
        }
    }
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

        bool prev_dir = !strncmp("..", argv[0], 2);

        argc++;
        argv[argc-1] = NULL;

        if(!strncmp(".", argv[0], 1) || prev_dir){
            //relativo
            execRelativo(prev_dir, argc, argv);
        }
        else if(!strncmp("/", argv[0], 1)){
            //absoluto
            if(!execAbs(argc, argv)){
                printf("%s: No such file or directory\n", argv[0]);
            }
        }
        else{
            //PATH o relativo
            execPath(argc, argv);
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

    if(execv(argv[0], argv) == -1){
        return 0;
    }

    exit(1);

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