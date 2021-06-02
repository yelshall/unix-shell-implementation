#include <cstdio>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "shell.hh"

int yyparse(void);

// Prints prompt
void Shell::prompt() {
  printf("%s ", getenv("PROMPT"));
  fflush(stdout);
}

// Sets PID for background processes
void Shell::changePID(int newPid) {
  Shell::pid = newPid;
}

// Handler for CTRL-C
extern "C" void disp(int sig) {
  printf("\n");
  fflush(stdout);
  Shell::prompt();
}

// Handler for zombie processes
extern "C" void zombieKiller(int sig) {
  int error = waitpid(-1, NULL, WNOHANG);
  while (error > 0) {
    if (Shell::pid == error) {
      Command::setPID(error);
      printf("[%d] exited\n", Shell::pid);
      fflush(stdout);
    }
    error = waitpid(-1, NULL, WNOHANG);
  }
}

int main(int argc, char **argv) {
  // Set relative path for the shell
  Command::setRelativePath(argv[0]);

  // CTRL-C signal Handler
  struct sigaction sa;
  sa.sa_handler = disp;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if(sigaction(SIGINT, &sa, NULL)) {
    perror("sigaction");
    exit(2);
  }

  // Zombie processes signal Handler
  struct sigaction zombie;
  zombie.sa_handler = zombieKiller;
  sigemptyset(&zombie.sa_mask);
  zombie.sa_flags = SA_RESTART;

  if(sigaction(SIGCHLD, &zombie, NULL)) {
    perror("sigaction child");
    exit(2);
  }

  // Set prompt and error
  if(setenv("PROMPT", "myshell>", 1)) {
    perror("Set prompt\n");
  }

  if(setenv("ON_ERROR", "Error Occurred", 1)) {
    perror("Set error\n");
  }

  //Print Prompt
  if(isatty(0)) {
    Shell::prompt();
  }
  yyparse();
}

int Shell::pid;
Command Shell::_currentCommand;
