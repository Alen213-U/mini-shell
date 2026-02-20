\# Mini UNIX Shell (C)



\## Description

A simple UNIX-like shell implemented in C using system calls.



\## Features

\- Command execution using fork() and execvp()

\- Built-in commands: cd, go, pwd, exit

\- Input redirection (<)

\- Output redirection (>)

\- Append redirection (>>)

\- Pipe support (|)

\- Background execution (\&)

\- Signal handling (Ctrl+C safe, no zombies)



\## Concepts Covered

\- Process creation and management

\- System calls

\- Inter-process communication (pipes)

\- File descriptors and I/O redirection

\- Signal handling



\## Compile

gcc mini.c -o minishell



\## Run

./minishell

