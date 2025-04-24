[system programming lecture]

## project 2 phase2

csapp.{c,h}

__________________________

        CS:APP3e functions

myshell.c

_________________________

# MyShell - Phase 2

Added pipeline (`|`) functionality:
- Parses multiple commands separated by `|`.
- Uses `pipe()` and `dup2()` to redirect I/O between commands.
- Executes commands sequentially in a pipeline.

