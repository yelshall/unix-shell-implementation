#ifndef command_hh
#define command_hh

#include "simpleCommand.hh"

// Command Data Structure

struct Command {
  std::vector<SimpleCommand *> _simpleCommands;
  std::string * _outFile;
  std::string * _inFile;
  std::string * _errFile;

  static std::string _lastArg;
  static std::string _relativePath;
  static std::vector<std::string> fullCommand;

  bool _background;
  bool _append;
  bool _appendErr;
  bool _ambiguousRedirect;

  static bool _src;
  static bool _subshell;

  static int _returnCode;
  static int _pidBackground;
  static int _index;

  Command();
  void insertSimpleCommand( SimpleCommand * simpleCommand );

  void clear();
  void print();
  void execute();

  // Helper functions
  std::vector<std::string *> returnArgs(std::vector<std::string *> args, int size);
  static void setPID(int newPid);
  static void setRelativePath(char *relativePath);
  void buildCommand(std::string insert);
  void insertHistory();

  static SimpleCommand *_currentSimpleCommand;
};

#endif
