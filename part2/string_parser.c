

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GUN_SOURCE

int count_token (char* buf, const char* delim)
{
	//TODO：
	/*
	*	#1.	Check for NULL string
	*	#2.	iterate through string counting tokens
	*		Cases to watchout for
	*			a.	string start with delimeter
	*			b. 	string end with delimeter
	*			c.	account NULL for the last token
	*	#3. return the number of token (note not number of delimeter)
	*/
	char *tkn, *saveptr1, *str1, *placeholder;
	int counter = 1;

	strtok_r(buf, "\n", &saveptr1);

	for (placeholder = str1 = strdup(buf);; str1 = NULL) {
		tkn = strtok_r(str1, delim, &saveptr1);
		if (tkn == NULL)
		break;
		counter++;
	} 
	free(placeholder);
	return counter;
}





command_line str_filler(char* buf, const char* delim)
{
    command_line cmd = {0}; // Zero-initialize the structure
    char *tkn, *saveptr1, *str1, *placeholder;

    // Ensure buffer is not NULL
    if (buf == NULL) {
        return cmd;
    }

    // Count the number of tokens
    cmd.num_token = count_token(buf, delim) - 1; // Subtract 1 to match actual token count
    
    // Allocate memory for token array, with space for NULL termination
    cmd.command_list = (char **)calloc(cmd.num_token + 1, sizeof(char *));  
    if (cmd.command_list == NULL) {
        // Handle allocation failure
        return cmd;
    }
    
    // Tokenize and fill the command list
    int i = 0;
    placeholder = str1 = strdup(buf);
    
    while ((tkn = strtok_r(str1, delim, &saveptr1)) != NULL) {
        cmd.command_list[i] = strdup(tkn);
        str1 = NULL;
        i++;
        
        // Prevent buffer overflow
        if (i >= cmd.num_token) break;
    }

    free(placeholder);
    cmd.command_list[i] = NULL; // Null-terminate

    return cmd;
}



void free_command_line(command_line* command)
{
    if (command == NULL) return;
    
    if (command->command_list != NULL) {
        for (int i = 0; i < command->num_token; i++) {
            if (command->command_list[i] != NULL) {
                free(command->command_list[i]);
            }
        }
        free(command->command_list);
        command->command_list = NULL;
    }
    
    command->num_token = 0;
}

