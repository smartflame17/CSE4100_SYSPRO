[system programming lecture]

## project 2 phase3

csapp.{c,h}

__________________________

        CS:APP3e functions

myshell.c

_________________________

# MyShell - Phase 3

Implemented job control and background processes (`&`):
- Manages jobs (foreground/background) using a `Job` struct and list.
- Added signal handlers (`SIGCHLD`, `SIGINT`, `SIGTSTP`) for job status changes.
- Implemented built-in commands: `jobs`, `fg`, `bg`, `kill`.