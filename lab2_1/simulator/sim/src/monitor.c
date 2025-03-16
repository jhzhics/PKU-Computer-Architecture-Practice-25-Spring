#include <monitor.h>
#include <common.h>
#include <memory.h>
#include <cpu.h>
#include <state.h>
#include <dbg.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 64

int is_little_endian() {
    unsigned int x = 1;
    char *c = (char*)&x;
    return *c;
}

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
            uint64_t addr;
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
        int cnt = sscanf(command_buffer, "%*s %s", buf1);
        cmd.step_args.step_n = cnt != -1 ? atoi(buf1) : 1;
    }
    else if (!strcmp(buf1, "info"))
    {
        cmd.type = Info;
        int cnt = sscanf(command_buffer, "%*s %s", buf1);
        if(cnt == -1 || strcmp(buf1, "r"))
        {
            cmd.type = Invalid;
        }
    }
    else if (!strcmp(buf1, "x"))
    {
        cmd.type = Examine;
        int cnt = sscanf(command_buffer, "%*s %s %s", buf1, buf2);
        if (cnt != 2)
        {
            cmd.type = Invalid;
        }
        else
        {
            cmd.examine_args.nbytes = atol(buf1);
            cmd.examine_args.addr = strtoul(buf2, NULL, 16);
        }
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
    "x N ADDR(Hex): print 4N bytes at ADDR of the memory. ";

    printf("%s%s\n", HELP_MESSAGE, is_little_endian() ? "Show in little endian" : "Show in big endian");
}

static void exec_step(int n)
{
    int cnt = 0;
    for(int i = 0; i < n && running; i++)
    {
        cnt++;
        exec_once();
    }
    log_info("Excute %d steps successfully", cnt);
}

static void excute_info()
{
    printf("PC  : 0x%016lx\n", cpu.pc);
    for(int i = 0; i < REG_N; i += 2)
    {
        printf("x%-2d : 0x%016lx\tx%-2d : 0x%016lx\n", i, cpu.reg[i], i + 1, cpu.reg[i + 1]);
    }
}

static void excute_examine(uint64_t vaddr , int n)
{
    int is_invalid = 0;
    int last = 0; int first = 1;
    for (int i = 0; i < n; i++)
    {
        uint32_t *addr = guest_to_host(vaddr + 4 * i);
        if (addr + 1 > (uint32_t *)(mem + MEM_SIZE) || vaddr < MEM_BASE)
        {
            is_invalid = 1;
            continue;
        }
        if(last++ == 0)
        {
            if(first)
            {
                first = 0;
            }
            else
            {
                putc('\n', stdout);
            }
            printf("0x%016lx :", vaddr + 4 * i);
        }
        printf(" %08x", *addr);
        last %= 4;
    }
    if (!first) putc('\n', stdout);
    if(is_invalid)
    {
        log_info("Try to read inaccessible memory");
    }
    fflush(stdout);
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
        cpu_exec();
        break;
        case Quit:
        exit_success();
        break;
        case Step:
        exec_step(cmd.step_args.step_n);
        break;
        case Info:
        excute_info();
        break;
        case Examine:
        excute_examine(cmd.examine_args.addr, cmd.examine_args.nbytes);
        break;
        case Invalid:
        log_info("Invalid Command");
        excute_help();
        break;
    }

    return running;
}