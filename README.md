# unix-shell-implementation
This is a fully function UNIX shell implementation written in C and C++. It parses commands through Lex and Yacc and executes those commands using execvp.

# How it works
* Parse command using my builtin grammar. This is done by identifying tokens in Lex and constructing it using the grammar in Yacc.
* Identifying if there are any builtin commands, subshells commands, environment variables, etc. within the command before executing.
* Execute the command and setting error code, as well as, clean up for zombie processes.

# Features
* Path completion
* Subshells
* I/O commands
* Environment Variables
* Line Editory
* Command History

# How to run
* You can run it by cloning the repository, running `make`, and typing `./shell`. However, I am still in the process of editing this repository, so it is preferrable to wait before downloading anything right now. In order to exit out of the shell, you just type `exit`.
