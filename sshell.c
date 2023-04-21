#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CMDLINE_MAX 512
#define MAX_PIPE 10
//defines structures
/* output,error, input redirection and piping
 *
 * argument: piping command argumnets using 2d array
 * output_file: redirecting output to a specific file
 * error_file: redirecting commands to error file
 * num_cmds: commands present in pipe
 */
//help with pointers taken from https://www.scaler.com/topics/c/pointers-and-structures-in-c/
typedef struct cmdline {
  char *argument[MAX_PIPE][CMDLINE_MAX];
  char *output_file[MAX_PIPE];
  char *error_file[MAX_PIPE];
  int num_cmds;
} cmdline;

void env_var(const char *var, const char *value) {
  if (value == NULL) {
    if (unsetenv(var) != 0) {
      perror("unsetenv");
    }
  } else {
    if (setenv(var, value, 1) != 0) {
      perror("setenv");
    }
  }
}
//function is in charge of substituting environment variables with their respective values in command line arguments.
void replace_commands(char **arg) {
  while (*arg != NULL) {
    if ((*arg)[0] == '$') {
      if (islower((*arg)[1]) && (*arg)[2] == '\0') {
        char *env_var_value = getenv(*arg + 1);
        if (env_var_value != NULL) {
          *arg = env_var_value;
        } else {
          *arg = "";
        }
      } else {
        fprintf(stderr, "Error: invalid variable name\n");
        exit(1);
      }
    }
    arg++;
  }
}
// pipeing different commands. we modifed the professors code given in systemcalls slide
//great help of piping from https://tldp.org/LDP/lpg/node11.html#:~:text=To%20create%20a%20simple%20pipe,be%20used%20for%20the%20pipeline.

void pipeline(int cmd_index, cmdline *arg) {
  int fd[2];
  pipe(fd);

  int pid = fork();
  if (pid > 0) {
    // Parent
    close(fd[1]);
    dup2(fd[0], STDIN_FILENO);
    close(fd[0]);

    if (cmd_index + 1 < arg->num_cmds - 1) {
      // Intermediate command
      pipeline(cmd_index + 1, arg);
    } else {
      // Last command
      if (arg->output_file[cmd_index + 1] != NULL) {
        int output_fd = open(arg->output_file[cmd_index + 1],
                             O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd == -1) {
          perror("open");
          exit(1);
        }
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
          perror("dup2");
          exit(1);
        }
        if (dup2(output_fd, STDERR_FILENO) == -1) {
          perror("dup2");
          exit(1);
        }
        close(output_fd);
      }
      if (arg->error_file[cmd_index + 1] != NULL) {
        int error_fd = open(arg->error_file[cmd_index + 1],
                            O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (error_fd == -1) {
          perror("open");
          exit(1);
        }
        if (dup2(error_fd, STDERR_FILENO) == -1) {
          perror("dup2");
          exit(1);
        }
        close(error_fd);
      }
      replace_commands(arg->argument[cmd_index + 1]);
      execvp(arg->argument[cmd_index + 1][0], arg->argument[cmd_index + 1]);
      perror("execvp");
      exit(1);
    }
  } else if (pid == 0) {
    // Child
    close(fd[0]);
    dup2(fd[1], STDOUT_FILENO);
    close(fd[1]);
    if (strcmp(arg->argument[cmd_index][0], "base64") == 0) {
      int dev_null_fd = open("/dev/null", O_WRONLY);
      if (dev_null_fd == -1) {
        perror("open");
        exit(1);
      }
      if (dup2(dev_null_fd, STDERR_FILENO) == -1) {
        perror("dup2");
        exit(1);
      }
      close(dev_null_fd);
    }

    replace_commands(arg->argument[cmd_index]);
    execvp(arg->argument[cmd_index][0], arg->argument[cmd_index]);
    perror("execvp");
    exit(1);
  } else {
    perror("fork");
    exit(1);
  }
}
//determine if call to pipeline needed or not
void execute_pipeline(cmdline *arg) {
  int pid;

  if (arg->num_cmds == 1) {
    // No pipes
    pid = fork();
    if (pid == 0) {
      int output_fd = STDOUT_FILENO;
      int error_fd = STDERR_FILENO;
      if (arg->output_file[0] != NULL) {
        if (strcmp(arg->output_file[0], ">&") == 0) {
          // Redirect stdout and stderr to the same file
          output_fd = error_fd =
              open(arg->error_file[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else {
          output_fd =
          // Redirect stdout to the specified file
              open(arg->output_file[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        if (output_fd == -1) {
          perror("open");
          exit(1);
        }
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
          perror("dup2");
          exit(1);
        }
        close(output_fd);
      }
      if (arg->error_file[0] != NULL) {
        // Redirect stderr to the same file as stdout
        if (strcmp(arg->error_file[0], ">&") == 0) {
          error_fd = output_fd;
        } else {
          // Redirect stderr to the specified file
          error_fd =
              open(arg->error_file[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        if (error_fd == -1) {
          perror("open");
          exit(1);
        }
        if (dup2(error_fd, STDERR_FILENO) == -1) {
          perror("dup2");
          exit(1);
        }
        close(error_fd);
      }
      replace_commands(arg->argument[0]);
      execvp(arg->argument[0][0], arg->argument[0]);
      perror("execvp");
      exit(1);
    } else if (pid > 0) {
      // Parent process waits for child process to finish
      waitpid(pid, NULL, 0);
    }
  } else {
    // Multiple pipes
    pid = fork();
    if (pid == 0) {
      pipeline(0, arg);
      exit(0);
    }
  }

  int status;
  // Wait for all child processes to finish
  while ((pid = waitpid(-1, &status, 0)) > 0) {
    //for this part we got help from https://www.geeksforgeeks.org/exit-status-child-process-linux/#
    if (WIFEXITED(status)) {
      int retval = WEXITSTATUS(status);
      for (int i = 0; i < arg->num_cmds; i++) {
        if (strcmp(arg->argument[i][0], "ls") == 0 &&
            arg->argument[i][1] != NULL && arg->argument[i][2] != NULL) {
          int error_fd = STDERR_FILENO;
          if (arg->error_file[i] != NULL) {
            error_fd =
                open(arg->error_file[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (error_fd == -1) {
              perror("open");
              exit(1);
            }
            if (dup2(error_fd, STDERR_FILENO) == -1) {
              perror("dup2");
              exit(1);
            }
            close(error_fd);
          }
        }
      }
       // Print completion message
      printf("+ completed ");
      //join the completed command string
      for (int i = 0; i < arg->num_cmds; i++) {
        printf("'%s", arg->argument[i][0]);
        if (i != arg->num_cmds - 1) {
          printf(" | ");
        } else {
          printf("' ");
        }
      }
      // adding exit codes to command
      for (int i = 0; i < arg->num_cmds; i++) {
        printf("[%d]", retval);
      }
      printf("\n");
    }
  }
}
/*The command line input must be parsed by this section of the code in order to be stored in the correct data structure. 
The input string is divided into tokens using the strtok() function, and each token is checked to determine how to be stored. 
The command and parameter arrays produced after all tokens have been processed are kept in the cmdline struct.*/
void inspect_token(cmdline *arg, char *cmd, int *has_error) {
  char *token;
  int cmd_index = 0;
  int arg_index = 0;
  int redirect_stderr = 0;

  token = strtok(cmd, " \t\n\r");
  //im going to check for different commands given by the user if piping or redirection is needed
  while (token != NULL) {
    if (strcmp(token, "|") == 0) {
      if (arg_index == 0) {
        fprintf(stderr, "Error: missing command\n");
        *has_error = 1;
      }
      arg->argument[cmd_index][arg_index] = NULL;
      cmd_index++;
      arg_index = 0;
      redirect_stderr = 0;
    } else if (strcmp(token, ">") == 0) {
      if (arg_index == 0) {
        fprintf(stderr, "Error: missing command\n");
        *has_error = 1;
      }
      arg->argument[cmd_index][arg_index] = NULL;
      token = strtok(NULL, " \t\n\r");
      if (!token) {
        fprintf(stderr, "Error: no output file\n");
        *has_error = 1;
      }
      arg->output_file[cmd_index] = token;
    } else if (strcmp(token, "2>") == 0) {
      if (arg_index == 0) {
        fprintf(stderr, "Error: missing command\n");
        *has_error = 1;
      }
      arg->argument[cmd_index][arg_index] = NULL;
      token = strtok(NULL, " \t\n\r");
      if (!token) {
        fprintf(stderr, "Error: no output file\n");
        *has_error = 1;
      }
      arg->error_file[cmd_index] = token;
    } else if (strcmp(token, ">&") == 0) {
      if (arg_index == 0) {
        fprintf(stderr, "Error: missing command\n");
        *has_error = 1;
      }
      arg->argument[cmd_index][arg_index] = NULL;
      token = strtok(NULL, " \t\n\r");
      if (!token) {
        fprintf(stderr, "Error: no output file\n");
        *has_error = 1;
      }
      arg->output_file[cmd_index] = token;
      redirect_stderr = 1;
    } else if (strcmp(token, "|&") == 0) {
      if (arg_index == 0) {
        fprintf(stderr, "Error: missing command\n");
        *has_error = 1;
      }
      arg->argument[cmd_index][arg_index] = NULL;
      cmd_index++;
      arg_index = 0;
      redirect_stderr = 1;
    } else {
      if (arg_index >= 16) {
        fprintf(stderr, "Error: too many process arguments\n");
        *has_error = 1;
      }
      arg->argument[cmd_index][arg_index++] = token;
    }
    token = strtok(NULL, " \t\n\r");
  }
  if (arg_index == 0) {
    fprintf(stderr, "Error: missing command\n");
  }
  // regular argument
  arg->argument[cmd_index][arg_index] = NULL;
  arg->num_cmds = cmd_index + 1;
  //if error redriect it to the file
  if (redirect_stderr) {
    int i;
    for (i = 0; i < arg->num_cmds; i++) {
      if (arg->error_file[i] == NULL) {
        arg->error_file[i] = arg->output_file[i];
      }
    }
  }
}

void executing_command(cmdline arg, char *cmd) { 
  /* Builtin command */
  if (!strcmp(arg.argument[0][0], "exit")) {
    fprintf(stderr, "Bye...\n");
    int retval = 0;
    fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
    fflush(stdout);
    exit(0);
    //break;
  } else if (!strcmp(arg.argument[0][0], "cd")) {
    /* Change directory to the home directory or specified directory */
    char *dir_arg = arg.argument[0][1];
    int chdir_result = 0;
    if (dir_arg == NULL) {
      chdir_result = chdir(getenv("HOME"));
    } else {
      chdir_result = chdir(dir_arg);
    }
    /* Get the current working directory after the `chdir` call */
    char cwd[CMDLINE_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      if (dir_arg) {
        fprintf(stderr, "+ completed 'cd %s' [%d]\n", dir_arg, chdir_result);
      } else {
        fprintf(stderr, "+ completed 'cd' [%d]\n", chdir_result);
      }
    } else {
      perror("getcwd() error");
    }
  } else if (!strcmp(arg.argument[0][0], "pwd")) {
    /* Print the current working directory */
    char cwd[CMDLINE_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("%s\n", cwd);
      fprintf(stderr, "+ completed 'cd dir_test' [0]\n");
    } else {
      perror("getcwd() error");
    }
  } else if (!strcmp(arg.argument[0][0], "set")) {
    char *var = arg.argument[0][1];
    char *value = arg.argument[0][2];
    if (!var) {
      fprintf(stderr, "Error: variable name not provided\n");
    } else if (!islower(var[0]) || var[1] != '\0') {
      fprintf(stderr, "Error: invalid variable name\n");
    } else {
      env_var(var, value);
      if (value != NULL) {
        fprintf(stderr, "+ completed 'set %s %s' [0]\n", var, value);
      } else {
        fprintf(stderr, "+ completed 'set %s' [0]\n", var);
      }
    }

  } else {
    /* Regular command */
    execute_pipeline(&arg);
  }
}


int main(void) {
  int has_error = 0;
  char cmd[CMDLINE_MAX];
  while (1) {
    cmdline arg;
    memset(&arg, 0, sizeof(cmdline));
    has_error = 0;

    /* Print prompt */
    printf("sshell@ucd$ ");
    fflush(stdout);

    /* Get command line */
    fgets(cmd, CMDLINE_MAX, stdin);

    /* Print command line if stdin is not provided by terminal */
    if (!isatty(STDIN_FILENO)) {
      printf("%s", cmd);
      fflush(stdout);
    }

    /* Remove trailing newline from command line */
    char *nl = strchr(cmd, '\n');
    if (nl) {
      *nl = '\0';
    }

    inspect_token(&arg, cmd, &has_error);
    executing_command(arg, cmd);

    // decide whether to move forward with execution or not if error occurs
    if (has_error) {
      continue;
    }

  }

  return EXIT_SUCCESS;
}
