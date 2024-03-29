# PlayIO
![CI](https://github.com/rbergen/PlayIO/actions/workflows/CI.yml/badge.svg)
## Name
playio - control the interaction with a UN*X text-based application using a very simple scripting language

## Synopsis
**playio** _PROGRAM_ [_PROGRAM_OPTIONS_]<br/>
**playio** -V

## Description
**playio** executes the indicated _PROGRAM_, connecting to _PROGRAM_'s standard I/O streams (stdin, and stdout/stderr combined).<br/>
Any _PROGRAM_OPTIONS_ that are on the command line after _PROGRAM_ are passed to _PROGRAM_ unchanged.

**playio** reads a script from standard input that controls how _PROGRAM_'s standard input and output are handled. **playio** stops when end of file is reached on **playio**'s own standard input. The termination of _PROGRAM_ is then awaited before **playio** exits.

**playio** was originally written as a support tool for a specific solution in the [github.com/PlummersSoftwareLLC/Primes](https://github.com/PlummersSoftwareLLC/Primes) project, but may serve other purposes as well.

## Options
- **-V**<br/>
  Print the version number of **playio** to standard output.

## Scripting Language
### General Format
A **playio** script consists of one or more lines. Each line contains a one-letter command. All commands accept a textual parameter that consists of whatever else is on the line, including any trailing whitespace. In all cases but one, the parameter itself is optional and the command's behavior depends on whether it is present or not.

### Commands
- **f** [_TEXT_]<br/>
  Read one line of _PROGRAM_'s standard output or, if _TEXT_ has been specified, read lines from _PROGRAM_'s standard output until one is read that contains _TEXT_. If no output is available, this command waits until a line becomes available unless end of file occurs.

- **o** [_TEXT_]<br/>
  Output _TEXT_ to **playio** standard output. If no _TEXT_ is specified, output the last line read from _PROGRAM_'s standard output through the use of the **f** command. If no line was read when the **f** command was last executed (due to end of file), nothing is printed.

- **p** _SECONDS_<br/>
  Pause script processing for _SECONDS_ seconds. Fractional numbers are supported.

- **t** [_TEXT_]<br/>
  Output a timestamp to **playio** standard output. The timestamp is the number of microseconds since epoch. If _TEXT_ is specified, it is output immediately before the actual timestamp.

- **w** [_TEXT_]<br/>
  Write _TEXT_ to _PROGRAM_'s standard input, or an empty line if no _TEXT_ is specified.

## Exit Status
The exit status of **playio** is the exit status of _PROGRAM_ in the case of normal execution. If _PROGRAM_ could not be started, the exit status is 127. If any other error occurs, the exit status is 1.

## Error Conditions
Simply put, once the pipes between **playio** and _PROGRAM_ have been setup and _PROGRAM_ is running, **playio** simply ignores whatever it doesn't understand.<br/>
If _PROGRAM_ cannot be found/started, **playio** will still run the script until its end.<br/> 
**playio** can hang when it's waiting for a line of output that contains a specific text, and _PROGRAM_ is waiting for input.

## Building
After cloning this repo or downloading playio.c to your UN*X system with GCC and common libraries installed, issue the following command in the repo directory:
```
$ gcc playio.c -o playio
``` 

_- Rutger van Bergen - [github.com/rbergen](https://github.com/rbergen)_
