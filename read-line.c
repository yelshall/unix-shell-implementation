#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <regex.h>
#include <dirent.h>
#include <stdbool.h>

#define MAX_BUFFER_LINE 2048
#define MAX_HISTORY_NO 256
#define MAX_DIR 128

extern void tty_raw_mode(void);

// Buffer where line is stored
int line_length;
char line_buffer[MAX_BUFFER_LINE];

int cursor_index;

// Simple history array
// This history does not change. 
// Yours have to be updated.
int history_index = 0;
int history_no = 0;
char * history [MAX_HISTORY_NO];
int history_length = sizeof(history)/sizeof(char *);
int maxEntries = 20;
int nEntries = 0;

void read_line_print_usage()
{
  char * usage = "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
}

void insertArray(char ***array, char *insert) {
  if(nEntries == maxEntries) {
    maxEntries *= 2;
    *array = (char **) realloc(*array, maxEntries * sizeof(char *));
  }
  array[0][nEntries] = insert;
  nEntries++;
  return;
}

/*
 * Input a line with some basic editing.
 */
char * read_line() {

  // Set terminal in raw mode
  tty_raw_mode();

  line_length = 0;
  cursor_index = 0;

  // Read one line until enter is typed
  while (1) {
    // Read one character in raw mode
    char ch;
    read(0, &ch, 1);


    if(ch == 9) {
      // Get the last word to complete if there is multiple words
      char *s = strrchr(line_buffer, ' ');

      char *reg;
      // If there are is only one word or no words
      if(s == NULL) {
        reg = (char *)malloc(2*strlen(line_buffer) + 10);

        char *a = line_buffer;
        char *r = reg;

        *r = '^';
        r++;
        while(*a != '\0') {
          if(*a == '*') {
            *r = '.';
            r++;
            *r = '*';
            r++;
          } else if(*a == '?') {
            *r = '.';
            r++;
          } else if(*a == '.') {
            *r = '\\';
            r++;
            *r = '.';
            r++;
          } else {
            *r = *a;
            r++;
          }
          a++;
        }
        *r = '.';
        r++;
        *r = '*';
        r++;
        *r = '$';
        r++;
        *r = '\0';
      } else {
        // If there are multiple words
        s++;
        reg = (char *)malloc(2*strlen(line_buffer) + 10);
        char *a = s;

        if(strrchr(s, '/') != NULL) {
          a = strrchr(s, '/');
          a++;
        }

        char *r = reg;

        *r = '^';
        r++;
        while(*a != '\0') {
          if(*a == '*') {
            *r = '.';
            r++;
            *r = '*';
            r++;
          } else if(*a == '?') {
            *r = '.';
            r++;
          } else if(*a == '.') {
            *r = '\\';
            r++;
            *r = '.';
            r++;
          } else {
            *r = *a;
            r++;
          }
          a++;
        }
        *r = '.';
        r++;
        *r = '*';
        r++;
        *r = '$';
        r++;
        *r = '\0';
      }

      //Compile regex
      regex_t re;
      regmatch_t pmatch[1];

      if(regcomp(&re, reg, 0) != 0) {
        perror("regex compile");
      }

      free(reg);
      reg = NULL;

      char d[MAX_DIR];

      //Open directory
      if(s == NULL) {
        d[0] = '.';
        d[1] = '\0';
      } else if(strrchr(s, '/') != NULL) {
        char *temp = strrchr(s, '/');
        strncpy(d, s, temp - s);
        d[temp-s+1] = '\0';
      } else {
        d[0] = '.';
        d[1] = '\0';
      }
      DIR *dir = opendir(d);
      if(dir == NULL) {
        regfree(&re);
        closedir(dir);

        perror("opendir");
      }

      //Array to store matches
      char **matches = (char **) malloc(maxEntries * sizeof(char *));

      //Search current directory
      struct dirent *ent;
      while((ent = readdir(dir)) != NULL) {
        if(regexec(&re, ent->d_name, sizeof(pmatch)/sizeof(pmatch[0]), pmatch, 0) == 0) {
          insertArray(&matches, ent->d_name);
        }
      }

      if(nEntries == 0) {
        //If matches are zero do nothing
        continue;
      } else if(nEntries == 1) {
        //If there is one match then autocomplete it fully
        char *temp;
        if(s == NULL) {
          temp = &matches[0][line_length];
        } else {
          temp = &matches[0][strlen(s)];
        }

        for(int i = 0; i < strlen(temp); i++) {
          write(1,&temp[i],1);
          // add char to buffer
          line_buffer[line_length] = temp[i];
          line_length++;

          cursor_index++;
        }
      } else {
        //If there are multiple matches then autocomplete to the last common character
        char temp;
        int j;
        if(s == NULL) {
          j = strlen(line_buffer);
        } else {
          if(strrchr(s, '/') != NULL) {
            char *temp1 = strrchr(s, '/');
            j = strlen(s) - (temp1-s) - 1;
          } else {
            j = strlen(s);
          }
        }
        int x = j;
        int x1 = 0;
        int x2 = 0;
        bool b = true;
        while(b) {
          for(int i = 0; i < nEntries; i++) {
            if(i == 0) {
              temp = matches[i][j];
            }

            if(matches[i][j] == '\0' || matches[i][j] != temp) {
              x1 = i;
              x2 = j;
              b = false;
              break;
            }
          }
          if(b) {
            j++;
          }
        }

        if(x == j) {
          printf("\n");
          for(int i = 0; i < nEntries-1; i++) {
            printf("%s\t", matches[i]);
          }
          printf("%s\n", matches[nEntries-1]);
          printf("%s %s", getenv("PROMPT"), line_buffer);
          fflush(stdout);

          free(matches);
          matches = NULL;

          regfree(&re);
          closedir(dir);

          maxEntries = 20;
          nEntries = 0;
          continue;
        } else {
          for(int i = x; i < j; i++) {
            write(1,&matches[x1][i],1);
            // add char to buffer
            line_buffer[line_length]=matches[x1][i];
            line_length++;

            cursor_index++;
          }
        }
      }

      free(matches);
      matches = NULL;

      regfree(&re);
      closedir(dir);

      maxEntries = 20;
      nEntries = 0;
    } else if(ch == 4) {
      if (cursor_index == 0 && line_length == 1) {
        char bp = 8;
        line_length--;
        line_buffer[0] = '\0';
        ch = ' ';
        write(1,&ch,1);
        write(1, &bp, 1);
      } else if(cursor_index != line_length) {
        char temp = ' ';
        char bp = 8;

        char temp_buffer[MAX_BUFFER_LINE];
        for(int i = 0; i < cursor_index; i++) {
          temp_buffer[i] = line_buffer[i];
        }

        for(int i = cursor_index; i < line_length - 1; i++) {
          temp_buffer[i] = line_buffer[i+1];
        }

        temp_buffer[line_length] = '\0';
        strncpy(line_buffer, temp_buffer, line_length);

        line_length--;

        for(int i = cursor_index; i < line_length; i++) {
          write(1, &temp, 1);
        }
        for(int i = line_length; i > 0; i--) {
          ch = ' ';
          write(1,&ch,1);
          write(1, &bp, 1);
          write(1, &bp, 1);
        }
        for(int i = 0; i < line_length; i++) {
          write(1, &temp_buffer[i], 1);
        }
        for(int i = line_length-1; i>= cursor_index; i--) {
          write(1, &bp, 1);
        }
      } else {
        continue;
      }
    } else if(ch == 1) {
      while(cursor_index > 0) {
        cursor_index--;
        ch = 8;
        write(1,&ch,1);
      }
    } else if(ch == 5) {
      while(cursor_index < line_length) {
        cursor_index++;
        write(1,&line_buffer[cursor_index-1],1);
      }

    } else if(ch == 127 || ch == 8) {
      // <backspace> was typed. Remove previous character read.
      if(cursor_index - 1 >= 0) {
        if (cursor_index != line_length) {
          char temp = ' ';
          char bp = 8;

          char temp_buffer[MAX_BUFFER_LINE];
          for(int i = 0; i < cursor_index-1; i++) {
            temp_buffer[i] = line_buffer[i];
          }

          for(int i = cursor_index; i < line_length; i++) {
            temp_buffer[i-1] = line_buffer[i];
          }

          temp_buffer[line_length-1] = '\0';
          strncpy(line_buffer, temp_buffer, line_length);

          line_length--;
          cursor_index--;

          for(int i = cursor_index; i < line_length; i++) {
            write(1, &temp, 1);
          }
          for(int i = line_length; i >= 0; i--) {
            write(1, &bp, 1);
            ch = ' ';
            write(1,&ch,1);
            write(1, &bp, 1);
          }
          for(int i = 0; i < line_length; i++) {
            write(1, &temp_buffer[i], 1);
          }
          for(int i = line_length-1; i>= cursor_index; i--) {
            write(1, &bp, 1);
          }
        } else {
          // Go back one character
          ch = 8;
          write(1,&ch,1);

          // Write a space to erase the last character read
          ch = ' ';
          write(1,&ch,1);

          // Go back one character
          ch = 8;
          write(1,&ch,1);

          line_buffer[line_length-1] = '\0';


          line_length--;
          cursor_index--;
        }
      }
    } else if (ch>=32) {
      // It is a printable character

      // Do echo


      // If max number of character reached return
      if (line_length==MAX_BUFFER_LINE-2) break;

      if(cursor_index != line_length) {
        char temp = ' ';
        char bp = 8;
        char temp_buffer[MAX_BUFFER_LINE];
        for(int i = 0; i < cursor_index; i++) {
          temp_buffer[i] = line_buffer[i];
        }

        temp_buffer[cursor_index] = ch;

        for(int i = cursor_index+1; i < line_length+1; i++) {
          temp_buffer[i] = line_buffer[i-1];
        }

        temp_buffer[line_length+1] = '\0';
        strncpy(line_buffer, temp_buffer, line_length+1);

        line_length++;
        cursor_index++;

        for(int i = cursor_index; i < line_length+2; i++) {
          write(1, &temp, 1);
        }
        for(int i = line_length; i >= 0; i--) {
          write(1, &bp, 1);
          ch = ' ';
          write(1,&ch,1);
          write(1, &bp, 1);
        }
        for(int i = 0; i < line_length; i++) {
          write(1, &temp_buffer[i], 1);
        }
        for(int i = line_length-1; i>= cursor_index; i--) {
          write(1, &bp, 1);
        }
      } else {
        write(1,&ch,1);
        // add char to buffer
        line_buffer[line_length]=ch;
        line_length++;

        cursor_index++;
      }
    } else if (ch==10) {
      // <Enter> was typed. Return line
      history_index = history_no;

      // Print newline
      write(1,&ch,1);

      break;
    } else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0]=0;
      line_length = 0;
      cursor_index = 0;
      break;
    } else if (ch==27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.
      //
      char ch1;
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1==91 && ch2==65) {
        if(history_index != 0) {
          // Up arrow. Print next line in history.
          char temp = ' ';
          char bp = 8;
          for(int i = cursor_index; i < line_length; i++) {
            write(1, &temp, 1);
          }
          for(int i = line_length; i > 0; i--) {
            write(1, &bp, 1);
            ch = ' ';
            write(1,&ch,1);
            write(1, &bp, 1);
          }
          cursor_index = 0;
          line_length = 0;

          history_index--;

          // Copy line from history
          strcpy(line_buffer, history[history_index]);
          line_length = strlen(line_buffer);

          cursor_index = strlen(line_buffer);

          // echo line
          write(1, line_buffer, line_length);
        }
      } else if (ch1 == 91 && ch2 == 66) {
        //Down Key
        if(history_index != history_no) {
          if(history_index == history_no - 1) {
            char temp = ' ';
            char bp = 8;
            for(int i = cursor_index; i < line_length; i++) {
              write(1, &temp, 1);
            }
            for(int i = line_length; i > 0; i--) {
              write(1, &bp, 1);
              ch = ' ';
              write(1,&ch,1);
              write(1, &bp, 1);
            }
            cursor_index = 0;
            line_length = 0;
            history_index++;
          } else {
            // Up arrow. Print next line in history.
            char temp = ' ';
            char bp = 8;
            for(int i = cursor_index; i < line_length; i++) {
              write(1, &temp, 1);
            }
            for(int i = line_length; i > 0; i--) {
              write(1, &bp, 1);
              ch = ' ';
              write(1,&ch,1);
              write(1, &bp, 1);
            }
            cursor_index = 0;
            line_length = 0;

            history_index++;

            // Copy line from history
            strcpy(line_buffer, history[history_index]);
            line_length = strlen(line_buffer);

            cursor_index = strlen(line_buffer);

            // echo line
            write(1, line_buffer, line_length);
          }
        }
      } else if (ch1 == 91 && ch2 == 68) {
        //Left Key
        if(cursor_index - 1 >= 0) {
          cursor_index--;
          ch = 8;
          write(1,&ch,1);
        }
      } else if (ch1 == 91 && ch2 == 67) {
        //Right Key
        if(cursor_index + 1 <= line_length) {
          cursor_index++;
          write(1,&line_buffer[cursor_index-1],1);
        }
      } else if (ch1==91 && ch2 == 51) {
        char temp;
        read(0, &temp, 1);
        //Delete Key
        if (cursor_index == 0 && line_length == 1) {
          char bp = 8;
          line_length--;
          line_buffer[0] = '\0';
          ch = ' ';
          write(1,&ch,1);
          write(1, &bp, 1);
        } else if(cursor_index != line_length) {
          char temp = ' ';
          char bp = 8;

          char temp_buffer[MAX_BUFFER_LINE];
          for(int i = 0; i < cursor_index; i++) {
            temp_buffer[i] = line_buffer[i];
          }

          for(int i = cursor_index; i < line_length - 1; i++) {
            temp_buffer[i] = line_buffer[i+1];
          }

          temp_buffer[line_length] = '\0';
          strncpy(line_buffer, temp_buffer, line_length);

          line_length--;

          for(int i = cursor_index; i < line_length; i++) {
            write(1, &temp, 1);
          }
          for(int i = line_length; i > 0; i--) {
            ch = ' ';
            write(1,&ch,1);
            write(1, &bp, 1);
            write(1, &bp, 1);
          }
          for(int i = 0; i < line_length; i++) {
            write(1, &temp_buffer[i], 1);
          }
          for(int i = line_length-1; i>= cursor_index; i--) {
            write(1, &bp, 1);
          }
        } else {
          continue;
        }
      }
    }
  }

  // Add eol and null char at the end of string
  line_buffer[line_length] = 10;
  line_length++;
  line_buffer[line_length] = 0;

  return line_buffer;
}

void setNextCommand(char *command) {
  if(command[0] == '\0') {
    return;
  }
  if(history_no == MAX_HISTORY_NO) {
    for(int i = 0; i < MAX_HISTORY_NO; i++) {
      free(history[i]);
      history[i] = NULL;
    }
    history_no = 0;
    history_index = 0;
  }

  history[history_no] = command;

  history_no++;
  history_index = history_no;
}
