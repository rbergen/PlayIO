#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PIPE_READ   0
#define PIPE_WRITE  1

// semantic version parts
#define VERSION_MAJOR   1
#define VERSION_MINOR   1
#define VERSION_PATCH   4

int hascmdparam(const char *str);
char *getcmdparam(char *str);
char *clipnewline(char *str);
void runscript(int childStdinFD, int childStdoutFD);

// not in #define but variable, because of use in write()
const char newline = '\n';

int main(int argc, const char* argv[]) {
    // no arguments given, so let's explain how we roll
    if (argc < 2) {
        printf("Usage:\n"); 
        printf("%s <program> [program options]\n", argv[0]);
        printf("%s -V\n", argv[0]);

        return 1;
    }

    if (!strcmp(argv[1],"-V")) {
        printf("PlayIO version %d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
        return 0;
    }

    // create pipe for child stdin
    int stdinPipe[2];
    if (pipe(stdinPipe) < 0) {
        perror("allocating pipe for child input redirect");
        return 1;
    }

    // create pipe for child stdout
    int stdoutPipe[2];
    if (pipe(stdoutPipe) < 0) {
        perror("allocating pipe for child output redirect");

        // clean up stdin pipe we already have
        close(stdinPipe[PIPE_READ]);
        close(stdinPipe[PIPE_WRITE]);

        return 1;
    }

    pid_t childPid = fork();
    if (childPid == 0) {
        // child continues here

        // redirect stdin
        if (dup2(stdinPipe[PIPE_READ], STDIN_FILENO) == -1) {
            perror("redirecting child stdin");
            return 1;
        }

        // redirect stdout...
        if (dup2(stdoutPipe[PIPE_WRITE], STDOUT_FILENO) == -1) {
            perror("redirecting child stdout");
            return 1;
        }

        // ...and stderr to the same pipe
        if (dup2(stdoutPipe[PIPE_WRITE], STDERR_FILENO) == -1) {
            perror("redirecting child stderr");
            return 1;
        }

        // all these are for use by parent only
        close(stdinPipe[PIPE_READ]);
        close(stdinPipe[PIPE_WRITE]);
        close(stdoutPipe[PIPE_READ]);
        close(stdoutPipe[PIPE_WRITE]);

        // command line arguments denote program to run, and its arguments
        char *childArgs[argc];

        // copy arguments for child program, because execvp demands char instead of const char
        for (int i = 1; i < argc; i++) {
            int argLen = strlen(argv[i]);

            childArgs[i - 1] = malloc(argLen + 1);
            strncpy(childArgs[i - 1], argv[i], argLen);
            childArgs[i - 1][argLen] = 0; 
        }

        childArgs[argc - 1] = NULL;

        // replace ourselves with the program we're supposed to run
        execvp(argv[1], childArgs);
        
        // we only reach this point if we couldn't load the child program
        return 127;
    }
    else if (childPid > 0) {
        // parent continues here
        signal(SIGPIPE, SIG_IGN);

        // close unused file descriptors, these are for child only
        close(stdinPipe[PIPE_READ]);
        close(stdoutPipe[PIPE_WRITE]);

        runscript(stdinPipe[PIPE_WRITE], stdoutPipe[PIPE_READ]);

        // close remaining pipes now that we're done
        close(stdinPipe[PIPE_WRITE]);
        close(stdoutPipe[PIPE_READ]);

        // exit with the most informative exit status we can give
        int status;
        return waitpid(childPid, &status, 0) != -1 
            ? (WIFEXITED(status) ? WEXITSTATUS(status) : 1)
            : 1;
    }
    else {
        // failed to create child
        perror("creating child process");

        // clean up pipes
        close(stdinPipe[PIPE_READ]);
        close(stdinPipe[PIPE_WRITE]);
        close(stdoutPipe[PIPE_READ]);
        close(stdoutPipe[PIPE_WRITE]);

        return 1;
    }

    return 0;
}

// read our I/O script from our own stdin, and execute it
void runscript(int childStdinFD, int childStdoutFD) {
    char *stdinLine = NULL;
    size_t stdinBufLen = 0;
    ssize_t stdinReadLen = -1;

    char *childLine = NULL;
    size_t childBufLen = 0;
    ssize_t childReadLen = -1;

    FILE *childStdout = fdopen(childStdoutFD, "r");

    // read lines from our own stdin (not the child's, yet)
    while((stdinReadLen = getline(&stdinLine, &stdinBufLen, stdin)) != -1) {
        // skip empty lines 
        if (stdinReadLen == 0)
            continue;

        // first char is our command
        switch(stdinLine[0]) {

            // output timestamp (μs since epoch)
            case 't': {
                struct timespec tms;

                if (timespec_get(&tms, TIME_UTC)) {
                    // we got our time in seconds and nanoseconds, so we need to convert
                    int64_t micros = tms.tv_sec * 1000000;
                    micros += tms.tv_nsec / 1000;

                    // round off microseconds mathematically correct
                    if (tms.tv_nsec % 1000 >= 500)
                        ++micros;

                    // output command parameter first, if there is one
                    const char *param = hascmdparam(stdinLine) ? getcmdparam(stdinLine) : "";

                    printf("%s%"PRId64"\n", param, micros);
                }
            }
            break;

            // read line(s) from child stdout, optionally until a specific string is found
            case 'f': {
                const char *param = hascmdparam(stdinLine) ? getcmdparam(stdinLine) : NULL;

                while((childReadLen = getline(&childLine, &childBufLen, childStdout)) != -1) {
                    // try to find the search string in the child output, if we have one
                    if (param == NULL || strstr(childLine, param) != NULL)
                        break;
                }
            }
            break;

            // output string or last line read from child stdout
            case 'o': {
                if (hascmdparam(stdinLine)) {
                    printf("%s\n", getcmdparam(stdinLine));
                    break;
                }
                
                // bail out if no child output is available
                if (childReadLen == -1)
                    break;

                printf("%s\n", clipnewline(childLine));
            }
            break;

            // pause for a specified number of seconds; fractional numbers are supported
            case 'p': {
                if (!hascmdparam(stdinLine))
                    break;

                // we need to pause in seconds + nanoseconds
                double duration = atof(getcmdparam(stdinLine));
                struct timespec tms = {
                    .tv_sec = (time_t)duration,
                    .tv_nsec = (long)((duration - (long)duration) * 1000000000)
                };

                nanosleep(&tms, NULL);
            }
            break;

            // write (optional) string to child stdin
            case 'w': {
                if (hascmdparam(stdinLine)) {
                    const char *param = getcmdparam(stdinLine);
                    write(childStdinFD, param, strlen(param));
                }

                write(childStdinFD, &newline, 1);
            }
            break;

            // we don't know this command character so we ignore it
            default:
            break;
        } // end of switch
    } // end of while

    // release buffers that were allocated by getline()
    free(stdinLine);
    free(childLine);
}

// check if a script command has a parameter: "c" = no, "c " = no, "c p..." = yes
// note that the second character is ignored whether it is a space or not!
inline int hascmdparam(const char *str) {
    size_t strLen = strlen(str);
    return strLen > 3 || (strLen == 3 && str[2] != newline);
}

// get pointer to script command parameter without trailing newline 
// it doesn't check if there is a parameter to point to!
inline char *getcmdparam(char *str) {
    return clipnewline(&str[2]);
}

// cut last newline off a string if there is one 
// this modifies the string passed as a parameter!
inline char *clipnewline(char *str) {
    size_t strLen = strlen(str);

    if (strLen == 0)
        return str;

    if (str[strLen - 1] == newline)
        str[strLen - 1] = 0;

    return str;
}
