# EDA093 - Lab 1 Report

**Sara Borg, Didrik Håkansson, Han Zhou**  
*September 2024*

## 1. Specifications

The following sections list the specifications in the overall order that they were implemented. For each of the specifications, some elaboration is done on how they were implemented as well as why we implemented them the way we did. Furthermore, these sections also elaborate on problems we encountered while implementing them.

### 1.1 Basic Commands

The first specification to be implemented was the support for basic commands. The functionality was implemented in the function `execute_cmd_rec` and relied on the `execvp` command. `Execvp` automatically searches for the command executable in the directories specified by the `PATH` variable, eliminating the need for specifying paths to the executables in the program code. Another advantage of `execvp` is its ability to take an array of arguments, which allowed us to essentially pass the command array directly from the `Command` class. Also, it will only return if an error occurs.

The syntax for `execvp` is as follows:

`execvp(args[0], args);`


where `args` is a pointer to the `pgm_list` in the current command.

Implementing handling for simple commands was quite straightforward, and we didn’t run into any particular problems. Some time was spent testing implementations with `execv` and `execlp`, but as mentioned, `execvp` was chosen due to its convenient search functionality.

### 1.2 Ctrl-D Handling

Our Ctrl-D handler runs in the loop in the `main()` function. It reads the input from `readline()`, and when the user presses Ctrl+D to indicate that there is no more input, the read operation is complete. `readline()` returns `NULL`, indicating that the end of the input stream (EOF) has been reached. If this is the case, the main loop breaks, and the shell is exited. Implementing this was straightforward, as it’s only an if-statement.

### 1.3 Built-in Commands

The "built-in" commands that had to be implemented were `cd` and `exit`. To check for these commands, a simple if-statement was used over the `args` pointer in order to differentiate them from the commands that required running executables. The `cd` commands relied on the `chdir()` function with the following syntax:

`chdir(path);`


The function takes as an argument a string specifying the path to the desired directory. This path was taken directly from the second entry in the `pgm_list` (following the `cd` command). A special case is when the path is `NULL`, where the `cd` command should change to the home directory. With a simple if-statement, we detected these cases, and with the use of the `getenv()` function, the path argument could be set to home. If the command was detected to be `exit`, the shell simply calls the `exit()` function with `EXIT_SUCCESS`.

As with the basic commands, the built-in commands were also straightforward to implement. No significant problems were encountered during development.

### 1.4 Zombie Processes

We specified that any parent process uses `waitpid()` to specifically wait for the child that was forked in the same iteration. This ensures that any foreground process is always reaped.

For background processes, it was a bit more difficult. Whenever a background process finishes, a `SIGCHLD` signal is sent. Therefore, we set up a handler for this signal, which reaps all finished processes. We used `waitpid()` in a while loop to collect all finished background processes.

### 1.5 Ctrl-C Handling

We use a custom signal handler function `signal_handler()` to handle two kinds of signals: `SIGINT` (Ctrl-C) and `SIGCHLD` (signal produced when a child process terminates). The `signal(SIGINT, signal_handler)` binds the `SIGINT` signal to the custom `signal_handler()` in the main loop. When the user presses Ctrl-C, `signal_handler()` is called to display a message instead of terminating the shell. The `signal(SIGCHLD, signal_handler)` binds the `SIGCHLD` signal to `signal_handler()`, which is called when any child process terminates and is responsible for killing off child processes and preventing zombie processes.

When the user presses Ctrl-C, which sends the `SIGINT` signal to the running process, the signal handler captures it. The handler does not terminate the background process but prints a message to the user, prompting them to exit the shell using the `exit` command instead of Ctrl-C. If a foreground process is running, it will be terminated, but any background process will be left alone.

### 1.6 Background Execution

Whenever a background execution is specified in a command, the `Command` struct’s `background` variable is set to `true`. This variable is checked inside the forked child process using an if-statement. If true, we use `setpgid(0, 0)` to set the process to its own process group. This ensures that all signals sent to the shell are ignored by background processes (e.g., Ctrl-C doesn’t terminate them).

### 1.7 Piping

When implementing piping, we introduced a recursive execution structure of the linked list of commands:


`execute_cmd_rec(*Command cmd_list, int output_fd);`


With this structure, the execution of the commands starts from the back of the linked list, which is the first command, to the top of the list, which is the last command.

This function checks if there is a next command in the list, and if so, it creates a new pipe using `pipe()`. It recursively calls itself with the next command and the write-end of the new pipe. The read-end of the pipe is set as the standard input for the next command after duplicating it to `STDIN_FILENO`. After a recursive call is finished, the write-end of the pipe is closed.

The parent process in each recursion waits until the child finishes and then returns to the previous pipe. When the last command of the list is reached, regular execution is performed, and the result is written to the standard output.

This was the most complicated part to implement, as it required a deeper understanding of the command list and its ordering. Working with recursive structures was also challenging.

### 1.8 I/O Redirection

When a command uses I/O redirection (i.e., the `<` and `>` characters), the `Command` struct’s `rstdin` and `rstdout` are set to the specified locations. We checked these variables, and if they were not `NULL`, we redirected them.

For output redirection, we attempted to open the file using `open()` with write access. If successful, we duplicated the file descriptor to `STDOUT_FILENO` and then closed the original file descriptor. Input redirection followed the same process, but with read access and duplication to `STDIN_FILENO`.

When you’ve already implemented redirection through pipes, this was relatively straightforward to implement.

## 2. Lab Feedback

Initially, the material provided felt overwhelming, as there were many files to go through. Once familiarized with the structure, it felt well-organized and clear. The test cases were helpful, and the accompanying questions provided helpful guidance.

At the start, a lot of time was spent figuring out the provided `Command` and `Pgm` structs. The provided figure illustrating how the pointers were set up was helpful, but more guidance on how to use them would be appreciated.

The automated tests were very helpful and provided continuous feedback. However, we encountered a bug in the `test_CTRL_C_with_fg_and_bg`, which led us to spend a lot of unnecessary time trying to pass it.

The material provided clear instructions for compiling/building the code and running automated tests. We wrote simple bash scripts to replicate this process, but for less experienced users, it could be helpful to provide a bash script for building and compiling.


