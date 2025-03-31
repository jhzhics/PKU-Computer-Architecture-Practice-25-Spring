#include <common.h>
#include <mem.h>
#include <cpu.h>
#include <monitor.h>
#include <state.h>
#include <string.h>

static const char * CLI_HELP = 
"Usage: Simulator  --[batch|debug] [OPTIONS] image_path\n"
"OPTIONS: \n"
"--mem-trace: trace memory and write to memtrace.out";

static void exit_wrong_usage()
{
    printf("%s", CLI_HELP);
    exit(1);
}

int main(int argc, char *argv[]){
    if (argc != 3 && argc != 4)
    {
        exit_wrong_usage();
    }

    init_state(argv[argc - 1]);
    if (!strcmp(argv[2], "--mem-trace"))
    {
        enable_mem_trace();
    }


    if (!strcmp(argv[1], "--batch"))
    {
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
