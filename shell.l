
/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/types.h>

#include "y.tab.hh"
#include "shell.hh"

extern "C" char * read_line();

int mygetc(FILE * f) {
  static char *p;
  char ch;

     if (!isatty(0)) {
  // stdin is not a tty. Call real getc
    return getc(f);
  }

  // stdin is a tty. Call our read_line.
  if (p==NULL || *p == 0) {
    char * s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

static  void yyunput (int c,char *buf_ptr  );
std::string envExpansion(std::string exp);

// Perform environment variable expansions if needed
std::string envExpansion(std::string exp) {
  std::string env = "";
  std::string fullArg = "";

  for(int i = 2; i < exp.size(); i++) {
    if(exp.at(i) == '}') {
      if(strcmp(env.c_str(), "$") == 0) {
        int pid = getpid();
        char env2[32];
        sprintf(env2, "%d", pid);
        fullArg += env2;
      } else if(strcmp(env.c_str(), "?") == 0) {
        char env2[32];
        sprintf(env2, "%d", Shell::_currentCommand._returnCode);
        fullArg += env2;
      } else if(strcmp(env.c_str(), "!") == 0) {
        char env2[32];
        sprintf(env2, "%d", Shell::_currentCommand._pidBackground);
        fullArg += env2;
      } else if(strcmp(env.c_str(), "_") == 0) {
        fullArg += Shell::_currentCommand._lastArg;
      } else if(strcmp(env.c_str(), "SHELL") == 0) {
        char actualPath[64];
        char *ptr;
        ptr = realpath(Shell::_currentCommand._relativePath.c_str(), actualPath);
        fullArg += ptr;
      } else {
        fullArg += getenv(env.c_str());
      }
      break;
    }
    env += exp.at(i);
  }
  return fullArg;
}

void myunputc(int c) {
  unput(c);
}



%}

%option noyywrap
%x src

%%

source {
  Shell::_currentCommand.buildCommand(yytext);
  BEGIN(src);
}

\n {
  Shell::_currentCommand._subshell = false;
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  Shell::_currentCommand.buildCommand(yytext);
  return GREAT;
}

">>" {
  Shell::_currentCommand.buildCommand(yytext);
  return GREATGREAT;
}

"<" {
  Shell::_currentCommand.buildCommand(yytext);
  return LESS;
}

">&" {
  Shell::_currentCommand.buildCommand(yytext);
  return GREATAMPERSAND;
}

"|" {
  Shell::_currentCommand.buildCommand(yytext);
  return PIPE;
}

"&" {
  Shell::_currentCommand.buildCommand(yytext);
  return AMPERSAND;
}

">>&" {
  Shell::_currentCommand.buildCommand(yytext);
  return GREATGREATAMPERSAND;
}

"2>" {
  Shell::_currentCommand.buildCommand(yytext);
  return TWOGREAT;
}

([^ \n\$]*\$\{[^ \n\}]+\}[^ \n\$]*)+ {
  Shell::_currentCommand.buildCommand(yytext);
  std::string cpp_str = yytext;

  //Perform Environment variable expansion
  std::string fullArg = "";
  for(int i = 0; i < cpp_str.size(); i++) {
    if(cpp_str.at(i) == '$') {
      for(int j = i; j < cpp_str.size(); j++) {
        if(cpp_str.at(j) == '}') {
          fullArg += envExpansion(cpp_str.substr(i, j-i+1));
          i = j;
          break;
        }
      }
    } else {
      fullArg += cpp_str.at(i);
    }
  }
  yylval.cpp_string = new std::string(fullArg);
  return WORD;
}

\$\([^\n\)]+\) {
  // Perform subshell operation
  Shell::_currentCommand.buildCommand(yytext);
  Shell::_currentCommand._subshell = true;
  std::string cpp_str = yytext;
  std::string temp_str = "";
  std::string final_str = "";
  std::vector<std::string> args = std::vector<std::string>();

  //Buffer for the output of subshell
  char *buffer = (char *)malloc(1024 * sizeof(char));

  //Get subshell command
  for(int i = 0; i < cpp_str.size(); i++) {
    if(cpp_str[i] == '$' || cpp_str[i] == '(') {
      continue;
    } else {
      if(cpp_str[i] == ' ' || cpp_str[i] == ')') {
        args.push_back(temp_str);
        temp_str = "";
      } else {
        temp_str += cpp_str[i];
      }
    }
  }

  for(int i = 0; i < cpp_str.size(); i++) {
    if(cpp_str[i] == '$' || cpp_str[i] == '(' || cpp_str[i] == ')') {
      continue;
    } else {
      final_str += cpp_str[i];
    }
  }

  int defaultin = dup(0);
  int defaultout = dup(1);
  int defaulterr = dup(2);

  int pin[2];
  int pout[2];

  pipe(pin);
  pipe(pout);

  dup2(pin[0], 0);
  dup2(pout[1], 1);

  //Write from parent to child
  write(pin[1], final_str.c_str(), strlen(final_str.c_str()));
  write(pin[1], "\n", strlen("\n"));
  write(pin[1], "exit\n", strlen("exit\n"));
  close(pin[1]);
  close(pin[0]);
  close(pout[1]);

  const char *argz[2];
  argz[0] = "/proc/self/exe";
  argz[1] = NULL;

  //Fork subshell process
  int ret = fork();

  if(ret == 0) {
    execvp(argz[0], (char *const *)argz);

    perror("execvp");
    _exit(1);
  } else if (ret < 0) {
    perror("fork");
    exit(2);
  }

  //Read from child to buffer
  read(pout[0], buffer, 1024);

  close(pout[0]);

  int length = 0;
  while(buffer[length] != '\0') {
    length++;
  }

  // Remove newline characters from buffer
  std::string outBuffer = "";
  for(int j = 0; j < length; j++) {
    if(buffer[j] == '\n') {
      outBuffer += " ";
    } else {
      outBuffer += buffer[j];
    }
  }

  //Put the buffer back into lex
  for(int i = length-1; i >= 0; i--) {
    myunputc(outBuffer.c_str()[i]);
  }

  //Restore file descriptors and free memory
  free(buffer);
  buffer = NULL;

  dup2(defaultin, 0);
  dup2(defaultout, 1);
  dup2(defaulterr, 2);

  close(defaultin);
  close(defaultout);
  close(defaulterr);
}

\"[^"\n]*\" {
  Shell::_currentCommand.buildCommand(yytext);
  std::string cpp_str = yytext;

  // Argument in quotes
  std::string final_str = "";

  for(int i = 0; i < cpp_str.size(); i++) {
    if(cpp_str[i] == '"') {
     continue;
    } else {
      final_str += cpp_str[i];
    }
  }

  yylval.cpp_string = new std::string(final_str);
  return WORD;

}

([^ \n\\]*\\[^ \t\n]+)+ {
  Shell::_currentCommand.buildCommand(yytext);
  std::string cpp_str = yytext;

  // Argument with escaped characters
  std::string final_str = "";

  for(int i = 0; i < cpp_str.size(); i++) {
    if(cpp_str[i] == '\\') {
      final_str += cpp_str[i+1];
      i++;
    } else {
      final_str += cpp_str[i];
    }
  }

  yylval.cpp_string = new std::string(final_str);
  return WORD;
}

[^ <>|\&\t\n]+ {
  Shell::_currentCommand.buildCommand(yytext);
  /* Assume that file names have only alpha chars */
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}

<src>[ \t]*    /* eat the whitespace */

<src>[^ \t\n]+ {
  Shell::_currentCommand.buildCommand(yytext);
  Shell::_currentCommand._src = true;

  // Open the source file
  yyin = fopen(yytext, "r");

  yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));
  BEGIN(INITIAL);
}

<<EOF>> {
  fclose(yyin);
  yypop_buffer_state();
  if ( !YY_CURRENT_BUFFER ) {
    yyterminate();
  }
}
