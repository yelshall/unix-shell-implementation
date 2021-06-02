/*
 * Example that shows how to read one line with simple editing
 * using raw terminal.
 *
 *
 */

 #include <unistd.h>
 #include <stdio.h>
 #include <string.h>

char * read_line();

char * prompt = "myshell>";

int main(int argc, char ** argv) 
{
  // Example to test read_line
  
  while (1) {

    char * s;

    // Print prompt
    write(1, prompt, strlen(prompt));

    // Read one line
    s = read_line();

    // Print line read
    printf("line read: %s\n", s);
  }
}
