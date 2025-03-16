#include <monitor.h>
#include <common.h>
#include <memory.h>
#include <cpu.h>
#include <state.h>
#include <dbg.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 64

enum CommandType
{
    Help,
    Continue,
    Quit,
    Step,
    Info,
    Examine,
    Invalid
};

struct Command
{
    enum CommandType type;
    union
    {
        struct {
            int step_n;
        } step_args;

        struct {
            size_t nbytes;
            int64_t addr;
        } examine_args;
    };
};

static struct Command parse(const char *command_buffer)
{
    struct Command cmd;
    static char buf1[BUFFER_SIZE], buf2[BUFFER_SIZE];
    sscanf(command_buffer, "%s", buf1);
    if (!strcmp(buf1, "help"))
    {
        cmd.type = Help;
    }
    else if (!strcmp(buf1, "c"))
    {
        cmd.type = Continue;
    }
    else if (!strcmp(buf1, "q"))
    {
        cmd.type = Quit;
    }
    else if (!strcmp(buf1, "si"))
    {
        cmd.type = Step;
        sscanf(command_buffer, "%*s %s", buf1);
        cmd.step_args.step_n = atoi(buf1);
    }
    else if (!strcmp(buf1, "info"))
    {
        cmd.type = Info;
    }
    else if (!strcmp(buf1, "x"))
    {
        cmd.type = Examine;
        sscanf(command_buffer, "%*s %s %s", buf1, buf2);
        cmd.examine_args.addr = atol(buf1);
        cmd.examine_args.nbytes = atol(buf2);
    }
    else
    {
        cmd.type = Invalid;
    }
    return cmd;
}

static void excute_help()
{
    static const char * HELP_MESSAGE = 
    "help: print this help message\n"
    "c: continue the stopped program\n"
    "q: exit the simulator\n"
    "si [N]: single step N times (default 1)\n"
    "info r: print register status\n"
    "x N ADDR: print N bytes at ADDR of the memory\n";

    printf("%s", HELP_MESSAGE);
}

int monitor()
{
    static int iter = 0;
    static char command_buffer[BUFFER_SIZE];
    if (++iter == 1)
    {
        excute_help();
    }

    printf("> ");
    fflush(stdout);

    fgets(command_buffer, BUFFER_SIZE, stdin);
    if (strchr(command_buffer, '\n') == NULL && !feof(stdin))
    {
        log_err("The command is too long for a buffer of %d bytes", BUFFER_SIZE);
        exit_failure();
    }

    struct Command cmd = parse(command_buffer);
    switch (cmd.type)
    {
        case Help:
        excute_help();
        break;
        case Continue:
        break;
        case Quit:
        break;
        case Step:
        break;
        case Info:
        break;
        case Examine:
        break;
        case Invalid:
        log_info("Invalid Command");
        excute_help();
        break;
    }

    return 1;
}