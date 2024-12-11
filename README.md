# Psush - A Custom Linux Shell

Psush is a custom Linux shell. This shell replicates core functionalities of standard shells while adding custom features to enhance user interaction. Written in C, Psush focuses on efficiency, modularity, and error-free execution.

---

## Features

### Built-in Commands
- **bye**: Exit the shell.
- **cd**: Change directory (`cd <dir>` or `cd` to switch to the home directory).
- **cwd**: Display the current working directory.
- **history**: Show the last 15 commands entered.
- **echo**: Echo the arguments passed, without variable expansion.

### External Commands
- Executes any external Linux command (e.g., `ls`, `cat`, `grep`) with full support for command-line options and arguments.

### Additional Functionalities
- **Pipelines**: Support for piped commands (e.g., `ls | wc`).
- **Input/Output Redirection**:
  - Redirect input (`wc < file.txt`).
  - Redirect output (`ls > output.txt`).
- **Custom Prompt**: Dynamically displays the current working directory, user name, and system name.
- **Signal Handling**: Graceful handling of `Ctrl+C` (SIGINT) without terminating the shell.
- **Memory Management**: No memory leaks, validated using `valgrind`.
- **No Orphans/Zombies**: Proper process handling to avoid orphaned or zombie processes.

---

## How to Build

A `Makefile` is provided for streamlined building and cleaning.

### Build the Shell
```bash
make
```

### Clean Up Compiled Files
```bash
make clean
```

---

## How to Run

Run PSUsh from the terminal:
```bash
./psush
```

---

## Requirements
- **Development Environment**: Linux
- **Compiler**: GCC with the following flags:
  - `-g -Wall -Wshadow -Wunreachable-code -Wredundant-decls`
  - `-Wmissing-declarations -Wold-style-definition`
  - `-Wmissing-prototypes -Wdeclaration-after-statement`
  - `-Wno-return-local-addr -Wuninitialized -Wextra -Wunused`
- **Language**: C

---

## Design Highlights
- **Command Parsing**: Uses linked lists to handle pipelines and arguments.
- **Dynamic Prompt**: Displays user and system-specific details.
- **Error Handling**: Custom messages for failed commands and memory errors.
- **Code Structure**: Modular design with reusable components in `cmd_parse.c` and `cmd_parse.h`.
  
