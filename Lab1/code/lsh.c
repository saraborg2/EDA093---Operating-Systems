/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file(s)
 * you will need to modify the CMakeLists.txt to compile
 * your additional file(s).
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Using assert statements in your code is a great way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>
#include <signal.h>
#include "parse.h"
#include <errno.h>
#include <fcntl.h>

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);
void signal_handler(int sig);
void execute_commands(Command *cmd_list, int output_fd);
void run_commands(Command *cmd_list);

#define READ 0
#define WRITE 1

void signal_handler(int sig){
    if (sig == SIGINT){
        printf("\nCaught Ctrl-C! Use exit to quit the shell.\n");
    } else if (sig == SIGCHLD){
        // Reap all child processes
        pid_t pid;
        while ((pid = waitpid(-1, NULL, WNOHANG)) > 0){
            printf("Process killed %d\n", pid);
        }
    }
}

void execute_cmd_rec(Command *cmd_list, int output_fd){

    Pgm *current_pgm = cmd_list->pgm;
    char **args = current_pgm->pgmlist;
    int pipe_fd[2];

    pid_t pid = fork();
    if (pid < 0){
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0){ // CHILD
        // Background execution, interrupts are ignored
        if (cmd_list->background){
            setpgid(0,0);
            signal(SIGINT, SIG_IGN);
        }

        //If there is an output fd from a previous pipe, set output to that pipe
        if (output_fd >= 0){
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        
        // One or more pgms left of the command 
        if (current_pgm->next != NULL){
            // Set up new pipe for next command
            if (pipe(pipe_fd) == -1){
                perror("pipe failed");
                exit(EXIT_FAILURE);
            }

            dup2(pipe_fd[READ], STDIN_FILENO);
            close(pipe_fd[READ]);
            // Call function again but with next pgm and the write end of this pipe
            cmd_list->pgm = current_pgm->next;
            execute_cmd_rec(cmd_list, pipe_fd[WRITE]);
            // close write end after it is used ^
            close(pipe_fd[WRITE]);
            
        } else {
            // Redirect output if command specifies it
            if (cmd_list->rstdout){
                int fd_out = open(cmd_list->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd_out < 0) {
                    perror("Failed output redirect");
                    exit(EXIT_FAILURE);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }
            // Redirect input if command specifies it
            if (cmd_list->rstdin){
              int fd_in = open(cmd_list->rstdin, O_RDONLY, 0);
              if (fd_in < 0) {
                  perror("Failed input redirect");
                  exit(EXIT_FAILURE);
              }
              dup2(fd_in, STDIN_FILENO);
              close(fd_in);
            }
        }

        // Execute the command
        if (execvp(args[0], args) == -1){
            perror("Exexution failed");
            exit(EXIT_FAILURE);
        }
    } else { // PARENT
        // Wait for child process if it's not running in the background
        if (!cmd_list->background){
            waitpid(pid, NULL, 0);
        }  else {
            printf("Background process started [%d]\n", pid)
        }
    }
}


void run_commands(Command *cmd_list) {
    char **args = cmd_list->pgm->pgmlist;

    if (strcmp(args[0], "cd") == 0) {
        char *path = args[1];
        if (path == NULL){
            path = getenv("HOME");
        }
        if (chdir(path) == -1) {
            perror("cd");
        }
        return;
    } else if (strcmp(args[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }
    // Execute all commands
    execute_cmd_rec(cmd_list, -1);
}

int main(void)
{

  signal(SIGINT, signal_handler);
  signal(SIGCHLD, signal_handler);

  for (;;)
  {
    char *line;
    line = readline("> ");

    // Check for Ctrl+D 
    if (line == NULL){
      printf("\nExiting shell\n");
      break;
    }

    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    // If stripped line not blank
    if (*line)
    {
      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        run_commands(&cmd);
      } else {
        printf("Parse ERROR\n");
      }
    }
    // Clear memory
    free(line);
  }

  
  return 0;
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 *
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}


/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}
