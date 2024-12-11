//cmd_parse.c
//Drake Wheeler

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <limits.h> //to define PATH_MAX, MAXHOSTNAMELEN

#include "cmd_parse.h"

#define PROMPT_LEN 5000
#define HIST_SIZE 15 //number of commands kepy in history

// I have this a global so that I don't have to pass it to every
// function where I might want to use it. Yes, I know global variables
// are frowned upon, but there are a couple useful uses for them.
// This is one.
unsigned short is_verbose = 0;
static char* history[HIST_SIZE] = {NULL}; //array to store command history
static int history_count = 0; //number of commands currently stored
pid_t current_child_pid = 0; //global variable to track child PID

int process_user_input_simple(void)
{
    char str[MAX_STR_LEN] = {'\0'};
    char* ret_val = NULL;
    char* raw_cmd = NULL;
    cmd_list_t* cmd_list = NULL;
    int cmd_count = 0;
    char prompt[PROMPT_LEN] = {'\0'};
	char current_directory[PATH_MAX] = {'\0'};
	char host_name[MAXHOSTNAMELEN] = {'\0'};

	signal(SIGINT, sigint_handler); //set up signal handler for sigint

    for ( ; ; ) 
	{
		//get current working directory
		if (getcwd(current_directory, sizeof(current_directory)) == NULL)
		{
			perror("getcwd");
			exit(EXIT_FAILURE);
		}
		//get host name
		if (gethostname(host_name, sizeof(host_name)) != 0)
		{
			perror("gethostname");
			exit(EXIT_FAILURE);
		}

		//if stdout is connected to a terminal
		if (isatty(fileno(stdout)))
		{
			//set up prompt
			snprintf(prompt, PROMPT_LEN, " %s %s\n%s@%s # ", PROMPT_STR, current_directory, getenv("LOGNAME"), host_name);
			fputs(prompt, stdout); //display to terminal
		}

		//reset str
        memset(str, 0, MAX_STR_LEN);
		//get user input
        ret_val = fgets(str, MAX_STR_LEN, stdin);

        if (ret_val == NULL) 
		{
            // end of input, a control-D was pressed.
            // Bust out of the input loop and go home.
            break;
        }

        // STOMP on the pesky trailing newline returned from fgets().
        if (str[strlen(str) - 1] == '\n') {
            // replace the newline with a NULL
            str[strlen(str) - 1] = '\0';
        }
        if (strlen(str) == 0) {
            // An empty command line.
            // Just jump back to the prompt and fgets().
            // Don't start telling me I'm going to get cooties by
            // using continue.
            continue;
        }

        if (strcmp(str, BYE_CMD) == 0) {
            // Pickup your toys and go home. I just hope there are not
            // any memory leaks. ;-)
            break;
        }

		//updates history array with new command
		update_history(str);

        // Basic commands are pipe delimited.
        // This is really for Stage 2.
        raw_cmd = strtok(str, PIPE_DELIM);

        cmd_list = (cmd_list_t *) calloc(1, sizeof(cmd_list_t));

        // This block should probably be put into its own function.
        cmd_count = 0;
		//loop while there are still commands to be put into the list
		//this while loop just sets up the data strucutre that holds the commands
        while (raw_cmd != NULL ) 
		{
            cmd_t* cmd = (cmd_t *) calloc(1, sizeof(cmd_t));

            cmd->raw_cmd = strdup(raw_cmd);
            cmd->list_location = cmd_count++;

            if (cmd_list->head == NULL) {
                // An empty list.
                cmd_list->tail = cmd_list->head = cmd;
            }
            else {
                // Make this the last in the list of cmds
                cmd_list->tail->next = cmd;
                cmd_list->tail = cmd;
            }
            cmd_list->count++;

            // Get the next raw command.
            raw_cmd = strtok(NULL, PIPE_DELIM);
        }
        // Now that I have a linked list of the pipe delimited commands,
        // go through each individual command.
        parse_commands(cmd_list);

        // This is a really good place to call a function to exec the
        // the commands just parsed from the user's command line.
        exec_commands(cmd_list);

        // We (that includes you) need to free up all the stuff we just
        // allocated from the heap. That linked list of linked lists looks
        // like it will be nasty to free up, but just follow the memory.
        free_list(cmd_list);
        cmd_list = NULL;
    }

	//free history array
	for (int i = 0; i < history_count; ++i)
	{
		free(history[i]);
	}

    return(EXIT_SUCCESS);
}



//signal handler for sigint, to kill child process or do nothing
void sigint_handler(__attribute__ ((unused)) int sig)
{
	if (current_child_pid != 0) //if there is a child process
	{
		kill(current_child_pid, SIGINT); //forward kill signal to child process
	}

	//if no child process ignore the signal

	return;
}


//to execute non built in commands, singular or multiple
void execute_external_command(cmd_t* cmd, cmd_list_t* cmd_list)
{
	int p_trail = -1; //set the file descriptor to the previous pipes read-end to -1 to idicate there's no previous pipe
	int P[2] = {-1, -1}; //file descriptors for pipe
	pid_t pid; //process ID

	//loop for each command in the list
	while(cmd)
	{

		//if not the last command
		if (cmd->next)
		{
			if (pipe(P) == -1) //create pipe
			{
				perror("pipe failed");
				exit(EXIT_FAILURE);
			}
		};
		
		pid = fork(); //fork a new process
		if (pid == -1)
		{
			perror("fork failed");
			exit(EXIT_FAILURE);
		}
					  
		if (pid == 0) //if child process
		{
			//reset SIGINT handling to default
			signal(SIGINT, SIG_DFL);

			//if this is the first command, and input needs to be redirected
			if (p_trail == -1 && cmd->input_src == REDIRECT_FILE)
			{
				int fd_in = open(cmd->input_file_name, O_RDONLY); //open input file

				if (fd_in < 0)
				{
					fprintf(stderr, "***** input redirection failed %d *****\n", errno);
					exit(7);
				}
				dup2(fd_in, STDIN_FILENO); //redirect standard input to file descriptor of opened file
				close(fd_in); //close the file descriptor after redirection
			}
			else if (p_trail != -1) //if not first command, reirect previous pipe's read-from end to stdin
            {
                dup2(p_trail, STDIN_FILENO); //redirect standard input to previous pipe's read-from end
                close(p_trail); //close the p_trail after using it to redirect stdin
            }

            //if last command, handle output redirection
            if (!cmd->next && cmd->output_dest == REDIRECT_FILE)
			{
				//open file to be written to 
				int fd_out = open(cmd->output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				if (fd_out < 0)
				{
					fprintf(stderr, "***** output redirection failed %d *****\n", errno);
					exit(7);
				}
				dup2(fd_out, STDOUT_FILENO); //redirect standard output to the file we opened
				close(fd_out); //close fd after using it to redirect stout
			}
            else if (cmd->next) //if not the last command, redirect stdout to current pipe's write end
            {
				dup2(P[1], STDOUT_FILENO); //redirect stdout to pipe's write-to end
				close(P[1]); //close the pipes write-to end descriptor
				close(P[0]); //close the pipes read-from end descriptor
			}

			//build arg for execvp()
			{
				int i = 1; //start filling arguments after the first element which is the command
				int argc = cmd->param_count + 2; //command + params + NULL
				param_t* param = cmd->param_list; //pointer to list of parameters
				char** argv = calloc(argc, sizeof(char*)); //allocate memory for arguments
				argv[0] = cmd->cmd; //set the command as the first argument

				//builds argv array with command arguments list for this command
				while(param)
				{
					argv[i++] = param->param;
					param = param->next;
				}
				argv[i] = NULL; //terminate argument array with NULL

				//execute the command
				//Pass in the command or "file name" and an array of the arguments with that command
				//This goes to the shell and executes that command program
				execvp(cmd->cmd, argv); //replaces current process with new process image corresponding to argv

				//if execcvp fails the following code will execute
				fprintf(stderr, "%s: command not found\n", cmd->cmd); //if execvp returns, it's an error
				{
					//free history array
					for (int j = 0; j < history_count; ++j)
					{
						free(history[j]);
					}
				}

				free_list(cmd_list);
				//free allocated memory for argv
				for (int j = 0; j < argc; ++j)
				{
					free(argv[j]);
				}
				free(argv);
				exit(EXIT_FAILURE); //only reaches here if execvp failed
			}
		}
		else if (pid > 0) //parent process
		{
			int status; //exit status of child process
			current_child_pid = pid; //store child PID for signal handling
			waitpid(pid, &status, 0); //wait for the child process to complete
			current_child_pid = 0; //reset after child terminates
								   

			//if not first command, close previous read end
			if (p_trail != -1) close(p_trail);

			if(cmd->next) //if there is anothe comman in the pipe
			{
				close(P[1]); //close the current pipe's write-to end in the parent
				p_trail = P[0]; //update p_trail to the current pipe's read-from end for the next command
			}
			else if (P[0] >= 0) //if this is the last command and the pipe was used, close the current pipe's read end
			{
				close(P[0]);
			}

			//if the child process was termined by a signal
			if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
			{
				printf("child killed\n");
			}
		}
		else
		{
			perror("fork failed");
		}

		cmd = cmd->next; //move to next command in the list
	}

	return;
}



//updates history array
void update_history(const char* new_command)
{
	if (history_count < HIST_SIZE) //history is not full
	{
		history[history_count] = strdup(new_command);
		history_count++;
	}
	else //histroy is full
	{
		free(history[0]); //free memory of the oldest command
		memmove(&history[0], &history[1], (HIST_SIZE - 1) * sizeof(char*));
		history[HIST_SIZE - 1] = strdup(new_command); //add new command at last index
	}

	return;
}


//displays upto the last HIST_SIZE commands
void display_history(void)
{
	for (int i = 0; i < history_count; ++i)
	{
		printf("%d %s\n", i + 1, history[i]); //print each command with it's count
	}

	return;
}




void simple_argv(int argc, char *argv[] )
{
    int opt;

    while ((opt = getopt(argc, argv, "hv")) != -1) {
        switch (opt) {
        case 'h':
            // help
            // Show something helpful
            fprintf(stdout, "You must be out of your Vulcan mind if you think\n"
                    "I'm going to put helpful things in here.\n\n");
            exit(EXIT_SUCCESS);
            break;
        case 'v':
            // verbose option to anything
            // I have this such that I can have -v on the command line multiple
            // time to increase the verbosity of the output.
            is_verbose++;
            if (is_verbose) {
                fprintf(stderr, "verbose: verbose option selected: %d\n"
                        , is_verbose);
            }
            break;
        case '?':
            fprintf(stderr, "*** Unknown option used, ignoring. ***\n");
            break;
        default:
            fprintf(stderr, "*** Oops, something strange happened <%c> ... ignoring ...***\n", opt);
            break;
        }
    }
}


void exec_commands(cmd_list_t* cmds ) 
{
    cmd_t* cmd = cmds->head;

    if (cmds->count == 1) 
	{
        if (!cmd->cmd) 
		{
            // if it is an empty command, bail.
            return;
        }

		//if command is "bye" exit program
		if (strcmp(cmd->cmd, BYE_CMD) == 0) exit(EXIT_SUCCESS);

		//chcek for built in commands

        if (strcmp(cmd->cmd, CD_CMD) == 0) //if command is "cd ..."
		{
            if (cmd->param_count == 0) 
			{
                // Just a "cd" on the command line without a target directory
                // need to cd to the HOME directory.

                // Is there an environment variable, somewhere, that contains
                // the HOME directory that could be used as an argument to
                // the chdir() fucntion?
				if(chdir(getenv("HOME")) != 0) //go to home directory
				{
					perror("cd failed");
				}
            }
            else 
			{
                // try and cd to the target directory. It would be good to check
                // for errors here.
                if (chdir(cmd->param_list->param) != 0) 
				{
                    // a sad chdir.  :-(
					if(is_verbose) perror("cd failed");
                }
            }
        }
        else if (strcmp(cmd->cmd, CWD_CMD) == 0) 
		{
            char str[MAXPATHLEN];

            // Fetch the Current Working Wirectory (CWD).
            // aka - get country western dancing
            getcwd(str, MAXPATHLEN); 
            printf(" " CWD_CMD ": %s\n", str);
        }
        else if (strcmp(cmd->cmd, ECHO_CMD) == 0) 
		{
			param_t* param = cmd->param_list; //set a pointer to the parameter list
											  
			while(param) //loop through the list
			{
				printf("%s ", param->param); //print each parameter with a space
				param = param->next; //go to next parameter in the list
			}
			printf("\n"); //end with a newline
        }
        else if (0 == strcmp(cmd->cmd, HISTORY_CMD)) 
		{
            // display the history here
			display_history();
        }
        else 
		{
			execute_external_command(cmd, cmds);
            // A single command to create and exec
            // If you really do things correctly, you don't need a special call
            // for a single command, as distinguished from multiple commands.
        }
    }
    else 
	{
		execute_external_command(cmd, cmds);
        // Other things???
        // More than one command on the command line. Who'da thunk it!
        // This really falls into Stage 2.
    }
}


void free_list(cmd_list_t* cmd_list)
{
	while(cmd_list->head) 
	{
		//hold onto current object
		cmd_t* temp_cmd_t = cmd_list->head;

		//set pointer to next object
		cmd_list->head = cmd_list->head->next;

		//free members in cmd_t strucut
		free_cmd(temp_cmd_t);


		//free current object
		free(temp_cmd_t);
	}

	cmd_list->head = NULL;
	cmd_list->tail = NULL;

	free(cmd_list);

	return;
}


void print_list(cmd_list_t *cmd_list)
{
    cmd_t *cmd = cmd_list->head;

    while (NULL != cmd) {
        print_cmd(cmd);
        cmd = cmd->next;
    }
}


//frees all members in cmd_t struct
void free_cmd(cmd_t *cmd)
{
	if (cmd == NULL) return;

	if (cmd->raw_cmd) free(cmd->raw_cmd);

	if (cmd->cmd) free(cmd->cmd);

	if (cmd->input_file_name) free(cmd->input_file_name);

	if (cmd->output_file_name) free(cmd->output_file_name);

	//loop through param list
	while(cmd->param_list)
	{
		//hold onto current param object
		param_t* temp_param = cmd->param_list;

		//free param member in object
		if (temp_param->param) free(temp_param->param);

		//set pointer to next param object
		cmd->param_list = cmd->param_list->next;

		//free actual parameter struct
		free(temp_param);
	}

	return;
}

// Oooooo, this is nice. Show the fully parsed command line in a nice
// easy to read and digest format.
void print_cmd(cmd_t *cmd)
{
    param_t *param = NULL;
    int pcount = 1;

    fprintf(stderr,"raw text: +%s+\n", cmd->raw_cmd);
    fprintf(stderr,"\tbase command: +%s+\n", cmd->cmd);
    fprintf(stderr,"\tparam count: %d\n", cmd->param_count);
    param = cmd->param_list;

    while (NULL != param) {
        fprintf(stderr,"\t\tparam %d: %s\n", pcount, param->param);
        param = param->next;
        pcount++;
    }

    fprintf(stderr,"\tinput source: %s\n"
            , (cmd->input_src == REDIRECT_FILE ? "redirect file" :
               (cmd->input_src == REDIRECT_PIPE ? "redirect pipe" : "redirect none")));
    fprintf(stderr,"\toutput dest:  %s\n"
            , (cmd->output_dest == REDIRECT_FILE ? "redirect file" :
               (cmd->output_dest == REDIRECT_PIPE ? "redirect pipe" : "redirect none")));
    fprintf(stderr,"\tinput file name:  %s\n"
            , (NULL == cmd->input_file_name ? "<na>" : cmd->input_file_name));
    fprintf(stderr,"\toutput file name: %s\n"
            , (NULL == cmd->output_file_name ? "<na>" : cmd->output_file_name));
    fprintf(stderr,"\tlocation in list of commands: %d\n", cmd->list_location);
    fprintf(stderr,"\n");
}

// Remember how I told you that use of alloca() is
// dangerous? You can trust me. I'm a professional.
// And, if you mention this in class, I'll deny it
// ever happened. What happens in stralloca stays in
// stralloca.
#define stralloca(_R,_S) {(_R) = alloca(strlen(_S) + 1); strcpy(_R,_S);}

void parse_commands(cmd_list_t *cmd_list)
{
    cmd_t *cmd = cmd_list->head;
    char *arg;
    char *raw;

    while (cmd) {
        // Because I'm going to be calling strtok() on the string, which does
        // alter the string, I want to make a copy of it. That's why I strdup()
        // it.
        // Given that command lines should not be tooooo long, this might
        // be a reasonable place to try out alloca(), to replace the strdup()
        // used below. It would reduce heap fragmentation.
        //raw = strdup(cmd->raw_cmd);

        // Following my comments and trying out alloca() in here. I feel the rush
        // of excitement from the pending doom of alloca(), from a macro even.
        // It's like double exciting.
        stralloca(raw, cmd->raw_cmd);

        arg = strtok(raw, SPACE_DELIM);
        if (NULL == arg) {
            // The way I've done this is like ya'know way UGLY.
            // Please, look away.
            // If the first command from the command line is empty,
            // ignore it and move to the next command.
            // No need free with alloca memory.
            //free(raw);
            cmd = cmd->next;
            // I guess I could put everything below in an else block.
            continue;
        }
        // I put something in here to strip out the single quotes if
        // they are the first/last characters in arg.
        if (arg[0] == '\'') {
            arg++;
        }
        if (arg[strlen(arg) - 1] == '\'') {
            arg[strlen(arg) - 1] = '\0';
        }
        cmd->cmd = strdup(arg);
        // Initialize these to the default values.
        cmd->input_src = REDIRECT_NONE;
        cmd->output_dest = REDIRECT_NONE;

        while ((arg = strtok(NULL, SPACE_DELIM)) != NULL) {
            if (strcmp(arg, REDIR_IN) == 0) {
                // redirect stdin

                //
                // If the input_src is something other than REDIRECT_NONE, then
                // this is an improper command.
                //

                // If this is anything other than the FIRST cmd in the list,
                // then this is an error.

                cmd->input_file_name = strdup(strtok(NULL, SPACE_DELIM));
                cmd->input_src = REDIRECT_FILE;
            }
            else if (strcmp(arg, REDIR_OUT) == 0) {
                // redirect stdout
                       
                //
                // If the output_dest is something other than REDIRECT_NONE, then
                // this is an improper command.
                //

                // If this is anything other than the LAST cmd in the list,
                // then this is an error.

                cmd->output_file_name = strdup(strtok(NULL, SPACE_DELIM));
                cmd->output_dest = REDIRECT_FILE;
            }
            else {
                // add next param
                param_t *param = (param_t *) calloc(1, sizeof(param_t));
                param_t *cparam = cmd->param_list;

                cmd->param_count++;
                // Put something in here to strip out the single quotes if
                // they are the first/last characters in arg.
                if (arg[0] == '\'') {
                    arg++;
                }
                if (arg[strlen(arg) - 1] == '\'') {
                    arg[strlen(arg) - 1] = '\0';
                }
                param->param = strdup(arg);
                if (NULL == cparam) {
                    cmd->param_list = param;
                }
                else {
                    // I should put a tail pointer on this.
                    while (cparam->next != NULL) {
                        cparam = cparam->next;
                    }
                    cparam->next = param;
                }
            }
        }
        // This could overwite some bogus file redirection.
        if (cmd->list_location > 0) {
            cmd->input_src = REDIRECT_PIPE;
        }
        if (cmd->list_location < (cmd_list->count - 1)) {
            cmd->output_dest = REDIRECT_PIPE;
        }

        // No need free with alloca memory.
        //free(raw);
        cmd = cmd->next;
    }

    if (is_verbose > 0) {
        print_list(cmd_list);
    }
}
