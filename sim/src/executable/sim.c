#include <common.h>
#include <mem.h>
#include <cpu.h>
#include <monitor.h>
#include <state.h>
#include <string.h>
#include <arch_perf.h>
#include <cache.h>


static const char * CLI_HELP = 
"Usage: Simulator  --[batch|debug] [OPTIONS] image_path\n"
"OPTIONS: \n"
"--mem-trace: trace memory and write to memtrace.out\n"
"--perf [multicycle|pipeline|pipeline_pro]: set performance profiler\n"
"--cache: use cache backend\n";


static void exit_wrong_usage()
{
    printf("%s", CLI_HELP);
    exit(1);
}

int main(int argc, char *argv[]){
    int cache_backend = 0;
    if (argc != 3 && argc != 4 && argc != 5 && argc != 6 && argc != 7)
    {
        exit_wrong_usage();
    }

    init_state(argv[argc - 1]);


    for (int i = 2; i < argc - 1; i++)
    {
        if (!strcmp(argv[i], "--mem-trace"))
        {
            enable_mem_trace();
        }
        else if (!strcmp(argv[i], "--perf"))
        {
            if (i + 1 < argc - 1)
            {
                set_perf_profiler(argv[i + 1]);
                i++;
            }
            else
            {
                printf("Error: --perf option requires an argument.\n");
                exit_wrong_usage();
            }
        }
        else if (!strcmp(argv[i], "--cache"))
        {
            cache_backend = 1;
        }
    }
    init_cache(cache_backend);

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
