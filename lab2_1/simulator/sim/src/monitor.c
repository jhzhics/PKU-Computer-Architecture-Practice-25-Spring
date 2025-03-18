#include <monitor.h>
#include <common.h>
#include <mem.h>
#include <cpu.h>
#include <state.h>
#include <dbg.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 64
#define MAX_BREAK_POINTS 10

struct
{
    uint64_t break_points[MAX_BREAK_POINTS];
    size_t break_n;
} static prog_debug;

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
    Break,
    Delete,
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

        struct {
            uint64_t addr;
        } break_args;
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
    else if (!strcmp(buf1, "i"))
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
    else if (!strcmp(buf1, "b"))
    {
        cmd.type = Break;
        int cnt = sscanf(command_buffer, "%*s %s", buf1);
        if (cnt != 1)
        {
            cmd.type = Invalid;
        }
        else
        {
            cmd.break_args.addr = strtoul(buf1, NULL, 16);
        }
    }
    else if (!strcmp(buf1, "d"))
    {
        cmd.type = Delete;
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
    "i r: print register status\n"
    "b ADDR(Hex): set a breakpoint at ADDR\n"
    "d: delete all breakpoints"
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
    log_info("Excute %d steps successfully. PC:0x%016lx", cnt, cpu.pc);
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

static void exec_delete()
{
    prog_debug.break_n = 0;
    log_info("Break Points Deleted");
}

static void exec_break(uint64_t addr)
{
    if (prog_debug.break_n >= MAX_BREAK_POINTS)
    {
        log_warn("Have reached max breakpoints. Adding 0x%016lx breakpoint failed.", addr);
        return;
    }
    prog_debug.break_points[prog_debug.break_n++] = addr;
    log_warn("Adding 0x%016lx breakpoint succeeded.", addr);
}


static int hit_breakpoint(int64_t addr)
{
    for(int i = 0; i < prog_debug.break_n; i++)
    {
        if(prog_debug.break_points[i] == addr)
        {
            return 1;
        }
    }
    return 0;
}

static void exec_continue()
{
    while (running)
    {
        exec_once();
        if(hit_breakpoint(cpu.pc))
        {
            log_info("Hit breakpoints 0x%016lx.", cpu.pc);
            break;
        }
    }
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
        exec_continue();
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
        case Break:
        exec_break(cmd.break_args.addr);
        break;
        case Delete:
        exec_delete();
        break;
        case Invalid:
        log_info("Invalid Command");
        excute_help();
        break;
    }

    return running;
}