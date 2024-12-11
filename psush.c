//psush.c
//Drake Wheeler

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>

#include "cmd_parse.h"

#ifndef FALSE
# define FALSE 0
#endif // FALSE
#ifndef TRUE
# define TRUE 1
#endif // TRUE
#define BUF_SIZE 100

//used to print helpful debug statements
#ifdef NOISY_DEBUG
# define NOISY_DEBUG_PRINT fprintf(stderr, "%s %s %d\n", __FILE__, __func__, __LINE__)
#else // NOISY_DEBUG
# define NOISY_DEBUG_PRINT
#endif // NOISY_DEBUG
	   

// I have this a global so that I don't have to pass it to every
// function where I might want to use it. Yes, I know global variables
// are frowned upon, but there are a couple of times where they can use
// their power for good. This is one.
extern unsigned short is_verbose;

int main( int argc, char *argv[] )
{
    int ret = 0;

    simple_argv(argc, argv);
    ret = process_user_input_simple();

    return ret;
}
