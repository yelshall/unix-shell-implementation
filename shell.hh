#ifndef shell_hh
#define shell_hh

#include "command.hh"

struct Shell {

  static void prompt();

  // Helper function
  static void changePID(int newPid);

  static Command _currentCommand;

  // PID of last background processes
  static int pid;
};

#endif
