#include <common.h>
#include <memory.h>
#include <cpu.h>
#include <monitor.h>
#include <state.h>
#include <string.h>

static const char * CLI_HELP = 
"Usage: Simulator  --[batch|debug] image_path\n";

static void exit_wrong_usage()
{
    printf("%s", CLI_HELP);
    exit(1);
}

int main(int argc, char *argv[]){
    if (argc != 3)
    {
        exit_wrong_usage();
    }

    if (!strcmp(argv[1], "--batch"))
    {
        init_state(argv[2]);
        cpu_exec();
        exit_success();
    } else if (!strcmp(argv[1], "--debug"))
    {
        while(monitor());
    }
    else
    {
        exit_wrong_usage();
    }


    return 0;
}
