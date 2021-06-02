/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT 
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE 
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM  
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include <cstdio>
#include <cstdlib>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>

#include "y.tab.hh"

#include "command.hh"
#include "shell.hh"
//#include "tty-raw-mode.c"

extern char **environ;
extern "C" void tty_term_mode(void);
extern "C" void setNextCommand(char *command);

Command::Command() {
  // Initialize a new vector of Simple Commands
  _simpleCommands = std::vector<SimpleCommand *>();

  // Initialize a new vector of full Command parts
  fullCommand = std::vector<std::string>();

  // Input, Output, Error Files
  _outFile = NULL;
  _inFile = NULL;
  _errFile = NULL;

  // Background Command Variable
  _background = false;

  // Append output and error variables
  _append = false;
  _appendErr = false;

  // Ambiguous Redirection variable
  _ambiguousRedirect = false;

  // bool to check for source command
  _src = false;

  // bool to check for subshell
  _subshell = false;

  // Return Code of previous command
  _returnCode = 0;

  // PID of previous background command
  _pidBackground = 0;

  // Index used in building command for history table
  _index = 0;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    // Clear output, input, and error files
    if ( _outFile == _errFile ) {
      delete _outFile;
      _outFile = NULL;
      _errFile = NULL;
    } else {
      if ( _outFile ) {
          delete _outFile;
      }
      _outFile = NULL;

      if ( _errFile ) {
          delete _errFile;
      }
     _errFile = NULL;
    }

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;


    _background = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::execute() {
    // Variable for builtin commands
    bool builtin = false;

    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
      if (isatty(0)) {
        Shell::prompt();
      }
      return;
    }

    // Don't do anything if there is ambiguous redirection
    if ( _ambiguousRedirect ) {
      return;
    }

    // Print contents of Command data structure
    if (isatty(0)) {
      //print();
    }

    //Storing stdin, stdout, and stderr
    int defaultin = dup( 0 );
    int defaultout = dup( 1 );
    int defaulterr = dup( 2 );

    int fdin;
    int fdout;
    int fderr;

    //Opening _inFile if needed
    if ( _inFile ) {
      fdin = open((char *) _inFile->c_str(), O_CREAT|O_RDWR, 0663);
    } else {
      fdin = dup( defaultin );
      close(fdin);
    }

    //Opening _errFile if needed
    if( _errFile ) {
      if( _appendErr ) {
        fderr = open((char *) _errFile->c_str(), O_CREAT|O_RDWR|O_APPEND, 0663);
      } else {
        fderr = open((char *) _errFile->c_str(), O_CREAT|O_RDWR|O_TRUNC, 0663);
      }
    } else {
      fderr = dup( defaulterr );
    }
    dup2(fderr, 2);
    close(fderr);

    // Insert into the history table
    if(!_src && !_subshell) {
      insertHistory();
    } else if (strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "source")) {
      insertHistory();
    }

    int x = 0;
    int ret;
    int fdpipe[2];

    // Checks whether it is a builtin command or exit command.
    if(strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "exit") == 0) {
      if(isatty(0)) {
        printf("Goodbye\n");
      }

      // Closing all file descriptors before exiting
      dup2(defaultin, 0);
      dup2(defaultout, 1);
      dup2(defaulterr, 2);
      close(defaultin);
      close(defaultout);
      close(defaulterr);
      close(fdin);

      clear();

      // Restore original terminal mode
      // ReturnCode is set appropriately
      // LastArg is set appropriately
      tty_term_mode();

      exit(0);
    } else if(strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "setenv") == 0) {
      char *name = (char *) _simpleCommands[0]->_arguments[1]->c_str();
      char *value = (char *) _simpleCommands[0]->_arguments[2]->c_str();

      Command::_returnCode = 0;
      if(setenv(name, value, 1)) {
        Command::_returnCode = 2;
        perror("setenv\n");
      }

      Command::_lastArg = *(_simpleCommands[0]->_arguments[2]);
      builtin = true;
    } else if(strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "unsetenv") == 0) {
      Command::_returnCode = 0;
      if(unsetenv(_simpleCommands[0]->_arguments[1]->c_str())) {
        Command::_returnCode = 2;
        perror("unsetenv\n");
      }

      Command::_lastArg = *(_simpleCommands[0]->_arguments[1]);
      builtin = true;
    } else if(strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "cd") == 0) {
      Command::_returnCode = 0;
      if(_simpleCommands[0]->_arguments.size() == 1) {
        if(chdir(getenv("HOME"))) {
          perror("cd");
          Command::_returnCode = 2;
        }

        Command::_lastArg = *(_simpleCommands[0]->_arguments[0]);
      } else {
        Command::_returnCode = 0;
        if(chdir(_simpleCommands[0]->_arguments[1]->c_str())) {
          fprintf(stderr, "cd: can't cd to %s\n", _simpleCommands[0]->_arguments[1]->c_str());
          Command::_returnCode = 2;
        }
        Command::_lastArg = *(_simpleCommands[0]->_arguments[1]);
      }
      builtin = true;
    }

  // Iterate over simple commands if it isn't a builtin command
  if(!builtin) {
    for ( auto & simpleCommand : _simpleCommands ) {
      //Sets stdin
      dup2(fdin, 0);
      close(fdin);

      //Check if it is the last command and opens _outFile if needed
      if ( x == _simpleCommands.size() - 1) {
        if( _outFile ) {
          if( _append ) {
            fdout = open((char *) _outFile->c_str(), O_CREAT|O_RDWR|O_APPEND, 0663);
          } else {
            fdout = open((char *) _outFile->c_str(), O_CREAT|O_RDWR|O_TRUNC, 0663);
          }
        } else {
          fdout = dup( defaultout );
        }
      } else {
        //Creates a new pipe
        pipe(fdpipe);
        fdout = fdpipe[1];
        fdin = fdpipe[0];
      }

      //Sets stdout
      dup2(fdout, 1);
      close(fdout);

      //Gets argv from simpleCommand for execv
      std::vector<std::string *> arguments = returnArgs(simpleCommand->_arguments, simpleCommand->_arguments.size());

      const char *argv[simpleCommand->_arguments.size()+1];
      for(int x = 0; x < simpleCommand->_arguments.size(); x++) {
        argv[x] = arguments.at(x)->c_str();
      }

      argv[simpleCommand->_arguments.size()] = NULL;
      Command::_lastArg = *(simpleCommand->_arguments.at(simpleCommand->_arguments.size()-1));

      ret = fork();

      if (ret == 0) {
        // Printenv command
        if(strcmp(argv[0], "printenv") == 0) {
          int i = 0;

          while(environ[i]) {
            printf("%s\n", environ[i++]);
          }

          fflush(stdout);

          Command::_returnCode = 0;
          Command::_lastArg = "printenv";

          dup2(defaultin, 0);
          dup2(defaultout, 1);
          dup2(defaulterr, 2);

          close(defaultin);
          close(defaultout);
          close(defaulterr);

          exit(0);
        } else {
          execvp(argv[0], (char *const *)argv);

          perror("execvp");
          _exit(1);
        }
      } else if (ret < 0) {
        perror("fork");
        exit(2);
      }

      x++;
    }
  }


  // Resetting stdin, stdout, stderr
  // and closing unecessary file descriptors
  dup2(defaultin, 0);
  dup2(defaultout, 1);
  dup2(defaulterr, 2);

  close(defaultin);
  close(defaultout);
  close(defaulterr);

  int status;

  if(!builtin) {
  //Waitpid if background is not set
    if ( !_background ) {
      waitpid(ret, &status, 0);

      Command::_returnCode = WEXITSTATUS(status);

      if(isatty(0)) {
        if(Command::_returnCode != 0) {
          printf("%s\n", getenv("ON_ERROR"));
          fflush(stdout);
        }
        Shell::prompt();
      }
    } else {
      // Set the _pidBackground variable
      Shell::changePID(ret);
      _pidBackground = ret;
    }
  } else {
    // Do nothign if it is a builtin
    builtin = false;
    if(isatty(0)) {
      Shell::prompt();
    }
  }
  // Clear to prepare for next command
  clear();
}

// Returns vector of Arguments from simpleCommand
std::vector<std::string *> Command::returnArgs(std::vector<std::string *> args, int size) {
  std::vector<std::string *> argv = std::vector<std::string *>();
  std::string env = "";
  std::string fullArg = "";
  char *env1;

  for ( auto & arg : args ) {
    // Perform Tilde expansion
    if(arg->at(0) == '~') {
      if(arg->size() == 1) {
        passwd *pass = getpwnam(getenv("USER"));
        fullArg += pass->pw_dir;
      } else {
        std::string user = "";
        for(int i = 1; i < arg->size(); i++) {
          if (arg->at(i) == '/' || i == arg->size()-1) {
            fullArg += "/homes/";
            fullArg += user;
            fullArg += arg->substr(i, arg->size()-i+1);
            break;
          }
          user += arg->at(i);
        }
      }
    } else {
      for(int j = 0; j < arg->size(); j++) {
        fullArg += arg->at(j);
      }
    }
    *arg = fullArg;
    argv.push_back(arg);
    fullArg = "";
  }
  argv.push_back(NULL);;


  return argv;
}

// Builds the full command to be inserted
void Command::buildCommand(std::string insert) {
  if(!_src && !_subshell) {
    Command::fullCommand.push_back(insert);
  }
}

// Inserts the command into the history table
void Command::insertHistory() {
  std::string command = "";

  for(std::string s : Command::fullCommand) {
    command += s;
    command += " ";
  }

  setNextCommand(strdup(command.substr(0, command.size() - 1).c_str()));
  fullCommand.clear();
}

// Set PID of last background process run
void Command::setPID(int newPid) {
  Command::_pidBackground = newPid;
}

// Set relative path
void Command::setRelativePath(char *relativePath) {
  Command::_relativePath = relativePath;
}

SimpleCommand * Command::_currentSimpleCommand;
int Command::_pidBackground;
std::string Command::_lastArg;
std::string Command::_relativePath;
std::vector<std::string> Command::fullCommand;
int Command::_returnCode;
int Command::_index;
bool Command::_src;
bool Command::_subshell;
