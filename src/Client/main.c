#include <stdio.h>
#include <string.h>

#include "create.h"

int main()
{

    printf(" If you want to exit the write 'exit'\n");
    while (1) {
        char command[100];
        printf("ENTER THE COMMAND (all caps)\n");
        fgets(command, sizeof(command), stdin);
        if (strcmp(command, "end\n") == 0) {
            break;
        }
        if (strcmp(command, "CREATE\n") == 0) {
            create();
        }

    }

    printf("Thank you, Client is closed, Happy diwali\n");
}