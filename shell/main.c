#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAXSIZEOFLINE 200
#define MAXNUMBEROFCOMMANDS 64
#define MAXNUMBEROFVARIABLES 200
#define MAXNUMBEROFPROCESSES

char lookuptable[MAXNUMBEROFVARIABLES][2][MAXSIZEOFLINE];
// first dimension : number of different variables
// second dimension : 0 for the name of the variable 1 for its value
// third dimension : pointer to the string of the name or variable
int numVariables = 0;
char currentDirectory[MAXSIZEOFLINE];
FILE *fptr;

char *read_input() {
    char *str = malloc(sizeof(char) * MAXSIZEOFLINE);
    printf("%s>> ", currentDirectory);
    fflush(stdout);
    fgets(str, MAXSIZEOFLINE, stdin);
    char *output = malloc(sizeof(char) * (strlen(str) + 1));
    strcpy(output, str);
    free(str);
    return output;
}

char **parse_input(char *input) {
    char **arrofStrings = malloc(sizeof(char *) * MAXNUMBEROFCOMMANDS);
    int counter = 0;
    char *temporaryArgStorage = malloc(sizeof(char) * MAXSIZEOFLINE);
    int i = 0;
    int j = 0;

    char c; // dereference to the starting character
    c = input[i];
    int quoted = 0;
    do {
        if (c == ' ' && !quoted || c == '\n') {
            if (j != 0) //read something
            {
                temporaryArgStorage[j] = '\0';
                arrofStrings[counter] = malloc(sizeof(char) * MAXSIZEOFLINE);
                strcpy(arrofStrings[counter++], temporaryArgStorage);
                j = 0;
            }
        } else if (c == '"') quoted = quoted ^ 1;
        else {
            temporaryArgStorage[j++] = c;
        }
        i++;
        c = input[i];
    } while (c != '\0');


    arrofStrings[counter] = NULL;
    free(temporaryArgStorage);
    //

    return arrofStrings;
}

char *evaluate_expression(char *arguements) {
    for (int j = 0; j < numVariables; j++) {
        char *dollarP = (char *) memchr(arguements, '$', strlen(arguements));
        if (dollarP == NULL) {
            //arguement doesn't contain any conversion
            return arguements;
        } else {
            //the string contains $ and its place is the following

            char *lhs = malloc(sizeof(char) * MAXSIZEOFLINE);
            strcpy(lhs, dollarP + 1);
            int u = 0;
            while (lhs[u] != ' ' && lhs[u] != '\n' && lhs[u] != '\0') {
                u++;
            }
            lhs[u] = '\0';
            if (strcmp(lookuptable[j][0], lhs) == 0) {
                free(lhs);
                long position = (long) dollarP - (long) arguements;
                int spaceposition = -1;
                long k = position;
                for (k = position; k < strlen(arguements) - 1; k++) {
                    arguements[k] = arguements[k + 1];
                    if (arguements[k] == ' ' || arguements[k] == '\n' || arguements[k] == '\0') spaceposition = k;
                }
                arguements[k] = '\0';
                //now from position to space position lies the string we want to replace
                char *temp = malloc(MAXSIZEOFLINE * sizeof(char));
                //put the values before like they were
                int x;
                for (x = 0; x < position; x++) { temp[x] = arguements[x]; }
                //overwrite the variable until the space
                //shift = length of the replacement- original variable

                int newshift = strlen(lookuptable[j][1]);
                int diffshift = strlen(lookuptable[j][0]);
                for (x = position; x < strlen(lookuptable[j][1]) + position; x++) {
                    temp[x] = lookuptable[j][1][x - position];
                }
                //shift the rest
                for (x = position; x < strlen(arguements) - diffshift; x++) {
                    temp[x + newshift] = arguements[x + diffshift];
                }
                temp[x + newshift] = '\0';
                strcpy(arguements, temp);
                free(temp);
            }


        }
    }
    return arguements;
}

void write_to_log_file(pid_t id) {


    fptr = fopen("/home/t-ony/CLionProjects/shell/log.txt", "a");
    if (fptr == NULL) {
        printf("Error!\n");
        exit(1);
    }
    fprintf(fptr, "Child terminated %d\n", id);
    fclose(fptr);

}

void on_child_exit(int sig,int id) {
    pid_t reaped;
    if(sig==1){
        waitpid(id,NULL,0);
        write_to_log_file(id);
    }

    while ((reaped = waitpid(-1, NULL, WNOHANG)) > 0) {
        write_to_log_file(reaped);
    }

}

void register_child_signal() {
    struct sigaction handler;
    handler.sa_handler = on_child_exit;
    handler.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &handler, NULL);
}

void execute_command(char **arguements) {
    //check foreground or background
    int foreground = 0;
    int i = 0;
    char *arg;
    arg = arguements[0];
    while (arg != NULL) {
        arg = arguements[i++];
    } // when arg is null go back one step for null, another to compensate the ++ before assigning null
    arg = arguements[i - 2];
    if (strcmp(arg, "&") == 0) {
        arguements[i - 2] = NULL; //to remove the &
    } else {
        //foreground
        foreground = 1;
    }

    int child_id = fork();
    if (child_id == 0) {
        execvp(arguements[0], arguements);
        printf("\nError, Didn't Identify the process to be launched. Try another command.\n");
        exit(1);
    } else {

        if (!foreground) {
            //background
            //don't wait
        } else {
            on_child_exit(1,child_id);
        }
    }

}

void echo(char **exp) {
    if (exp[1] != NULL)
        printf("%s \n", exp[1]);
    else
        printf("\n");

    fflush(stdout);
}

void cd(char **exp) {
    if (exp[1] == NULL || strcmp(exp[1], "~") == 0) {
        //change to home directory
        chdir(getenv("HOME"));
    } else {
        chdir(exp[1]);
    }
    getcwd(currentDirectory, sizeof(currentDirectory));
}

void export(char **exp) {
    char c = exp[1][0];
    int i = 0;
    int place = numVariables;
    char temp[MAXSIZEOFLINE];
    //read the part of the string before the = sign
    while (c != '=') {
        temp[i++] = c;
        c = exp[1][i];
        if (c == '\0') {
            //reached end before =
            printf("No assignment provided, please try again\n");
            return;
        }
    }
    temp[i] = '\0';
    int check = 0;
    //compare the string with the stored variables in the table
    while (check < numVariables) {
        if (strcmp(temp, lookuptable[check][0]) == 0) {
            place = check;
            numVariables--; //to compensate the ++ at the end of the function
        }
        check++;
    }
    //if it is new the name will be stored in a new place
    //if old the name will overwrite the old one
    strcpy(lookuptable[place][0], temp);
    c = exp[1][++i];

    //if there is no RHS
    if (c == '\0') {
        printf("No assignment provided, please try again\n");
        return;
    }
    int j = 0;
    while (c != '\0') {
        if (c == '"') c = exp[1][++i]; //to skip the quotes
        else {
            //store the value of the variable
            lookuptable[place][1][j++] = c;
            c = exp[1][++i];
        }
    }
    lookuptable[place][1][j] = '\0';
    numVariables++;
}

void execute_shell_bultin(char **exp) {
    if (strcmp(exp[0], "cd") == 0) {
        cd(exp);
    } else if (strcmp(exp[0], "export") == 0) {
        //case export
        export(exp);
    } else if (strcmp(exp[0], "echo") == 0) {
        //case echo
        echo(exp);
    }
}


void shell() {
    int exitFlag = 0;
    do {
        //evaluate expression and replace all the values
        char *inputevaluated = evaluate_expression(read_input());
        char **expression = parse_input(inputevaluated);
//        evaluate_expression(expression);
        int builtinFlag = 0;
        if (strcmp(expression[0], "cd") == 0 || strcmp(expression[0], "echo") == 0 ||
            strcmp(expression[0], "export") == 0)
            builtinFlag = 1;
        if (strcmp(expression[0], "exit") == 0) exitFlag = 1;
        else {
            switch (builtinFlag) {
                case 1:
                    execute_shell_bultin(expression); //self implemented builtin functions
                    break;
                case 0:
                    execute_command(expression);
                    break;
            }
        }

    } while (!exitFlag);

}


void setup_environment() {
    fptr = fopen("/home/t-ony/CLionProjects/shell/log.txt", "w");
    if (fptr == NULL) {
        printf("Error!\n");
        exit(1);
    }
    fprintf(fptr, "---Reaped Processes Log---\n");
    fclose(fptr);
    getcwd(currentDirectory, sizeof(currentDirectory));

}

int main(int argc, char *argv[]) {
    register_child_signal();
    setup_environment();
    shell();
    return 0;
}
