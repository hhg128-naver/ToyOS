#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"

#include "kernel.h"
#include "console.h"
#include "pmm.h"
#include "vfs.h"
#include "elf.h"
#include "task.h"

#define MAX_COMMAND_LEN 128
#define MAX_COMMANDS 16

class Command
{
public:
    virtual ~Command() {}
    virtual const char *getName() const = 0;
    virtual const char *getDescription() const = 0;
    virtual void execute(const char *args) = 0;
};

class Shell
{
private:
    Command *commands[MAX_COMMANDS];
    int commandCount;

public:
    Shell() : commandCount(0)
    {
        for (int i = 0; i < MAX_COMMANDS; i++)
        {
            commands[i] = nullptr;
        }
    }

    void registerCommand(Command *cmd)
    {
        if (commandCount < MAX_COMMANDS)
        {
            commands[commandCount++] = cmd;
        }
    }

    void printHelp()
    {
        kPrintf("Available commands:\n");
        for (int i = 0; i < commandCount; i++)
        {
            if (commands[i])
            {
                kPrintf("  %-6s - %s\n", commands[i]->getName(), commands[i]->getDescription());
            }
        }
    }

    void processInput(const char *input)
    {
        if (strlen(input) == 0)
            return;

        char cmdName[MAX_COMMAND_LEN];
        const char *args = nullptr;

        // Split input into command and arguments
        int i = 0;
        while (input[i] != ' ' && input[i] != '\0' && i < MAX_COMMAND_LEN - 1)
        {
            cmdName[i] = input[i];
            i++;
        }
        cmdName[i] = '\0';

        if (input[i] == ' ')
        {
            // Skip consecutive spaces
            int j = i;
            while (input[j] == ' ' && input[j] != '\0')
            {
                j++;
            }
            if (input[j] != '\0')
            {
                args = input + j;
            }
            else
            {
                args = "";
            }
        }
        else
        {
            args = "";
        }

        for (int j = 0; j < commandCount; j++)
        {
            if (commands[j] && strcmp(commands[j]->getName(), cmdName) == 0)
            {
                commands[j]->execute(args);
                return;
            }
        }

        kPrintf("Unknown command: '%s'\n", cmdName);
    }

    void run()
    {
        char input[MAX_COMMAND_LEN];

        kPrintf("\n--- ToyOS Interactive Shell (C++) ---\n");
        kPrintf("Type 'help' for a list of commands.\n\n");

        while (1)
        {
            kPrintf("ToyOS> ");

            if (fgets(input, MAX_COMMAND_LEN, stdin) == nullptr)
            {
                continue;
            }

            input[strcspn(input, "\n")] = 0;
            processInput(input);
        }
    }
};

// Global shell instance
Shell g_Shell;

// Command Implementations
class HelpCommand : public Command
{
public:
    const char *getName() const override { return "help"; }
    const char *getDescription() const override { return "Show this help message"; }
    void execute(const char *args) override
    {
        g_Shell.printHelp();
    }
};

class ClearCommand : public Command
{
public:
    const char *getName() const override { return "clear"; }
    const char *getDescription() const override { return "Clear the screen"; }
    void execute(const char *args) override
    {
        ClearScreen(boot_info_global, 0x00000033);
    }
};

class MemCommand : public Command
{
public:
    const char *getName() const override { return "mem"; }
    const char *getDescription() const override { return "Show physical memory status"; }
    void execute(const char *args) override
    {
        uint64_t total = PMM_GetTotalMemory();
        uint64_t free = PMM_GetFreeMemory();
        uint64_t used = total - free;

        kPrintf("Physical Memory Status:\n");
        kPrintf("  Total: %d MB\n", (int)(total / (1024 * 1024)));
        kPrintf("  Used : %d MB\n", (int)(used / (1024 * 1024)));
        kPrintf("  Free : %d MB\n", (int)(free / (1024 * 1024)));
    }
};

class LsCommand : public Command
{
public:
    const char *getName() const override { return "ls"; }
    const char *getDescription() const override { return "List files in root directory"; }
    void execute(const char *args) override
    {
        if (vfs_root == nullptr)
        {
            kPrintf("Error: VFS root not found.\n");
            return;
        }

        kPrintf("Index  Type   Size       Name\n");
        kPrintf("------------------------------------\n");

        int i = 0;
        while (1)
        {
            VFS_Node *node = VFS_ReadDir(vfs_root, i);
            if (node == nullptr)
                break;

            kPrintf("[%2d]   %s   %10d  %s\n",
                   i,
                   (node->flags & VFS_DIRECTORY) ? "<DIR>" : "FILE ",
                   node->size,
                   node->name);

            ::free(node);
            i++;
        }
    }
};

class CatCommand : public Command
{
public:
    const char *getName() const override { return "cat"; }
    const char *getDescription() const override { return "Print file contents"; }
    void execute(const char *args) override
    {
        if (args == nullptr || strlen(args) == 0)
        {
            kPrintf("Usage: cat <filename>\n");
            return;
        }

        if (vfs_root == nullptr)
            return;

        VFS_Node *target = nullptr;
        int i = 0;
        while (1)
        {
            VFS_Node *node = VFS_ReadDir(vfs_root, i++);
            if (node == nullptr)
                break;

            if (strcmp(node->name, args) == 0)
            {
                target = node;
                break;
            }
            ::free(node);
        }

        if (target == nullptr)
        {
            kPrintf("File not found: %s\n", args);
            return;
        }

        if (target->flags & VFS_DIRECTORY)
        {
            kPrintf("Error: '%s' is a directory.\n", args);
            ::free(target);
            return;
        }

        uint8_t *buffer = (uint8_t *)::malloc(target->size + 1);
        if (!buffer)
        {
            kPrintf("Error: Failed to allocate memory for file content.\n");
            ::free(target);
            return;
        }

        uint32_t read_bytes = VFS_Read(target, 0, target->size, buffer);
        buffer[read_bytes] = '\0';

        kPrintf("%s\n", (char *)buffer);

        ::free(buffer);
        ::free(target);
    }
};

class RebootCommand : public Command
{
public:
    const char *getName() const override { return "reboot"; }
    const char *getDescription() const override { return "Restart the system"; }
    void execute(const char *args) override
    {
        kPrintf("Rebooting is not implemented yet.\n");
    }
};

class RunCommand : public Command
{
public:
    const char *getName() const override { return "run"; }
    const char *getDescription() const override { return "Run a user binary (e.g. run hello.elf 123)"; }
    void execute(const char *args) override
    {
        if (args == nullptr || strlen(args) == 0)
        {
            kPrintf("Usage: run <filename> [arg]\n");
            return;
        }

        char filename[128];
        int arg_val = 0;
        int i = 0;
        while (args[i] != ' ' && args[i] != '\0' && i < 127)
        {
            filename[i] = args[i];
            i++;
        }
        filename[i] = '\0';

        if (args[i] == ' ')
        {
            arg_val = atoi(args + i + 1);
        }

        Task *t = LoadELFProcess(filename, arg_val);
        if (t)
        {
            kPrintf("Started process '%s' with PID %d\n", filename, (int)t->id);
            // 유저 앱이 종료될 때까지 쉘 대기
            WaitTask(t->id);
            kPrintf("Process '%s' (PID %d) finished.\n", filename, (int)t->id);
        }
        else
        {
            kPrintf("Failed to start process '%s'\n", filename);
        }
    }
};

// Global command instances
HelpCommand g_HelpCmd;
ClearCommand g_ClearCmd;
MemCommand g_MemCmd;
LsCommand g_LsCmd;
CatCommand g_CatCmd;
RebootCommand g_RebootCmd;
RunCommand g_RunCmd;

extern "C" void Shell_Main()
{
    static bool initialized = false;
    if (!initialized)
    {
        g_Shell.registerCommand(&g_HelpCmd);
        g_Shell.registerCommand(&g_ClearCmd);
        g_Shell.registerCommand(&g_MemCmd);
        g_Shell.registerCommand(&g_LsCmd);
        g_Shell.registerCommand(&g_CatCmd);
        g_Shell.registerCommand(&g_RebootCmd);
        g_Shell.registerCommand(&g_RunCmd);
        initialized = true;
    }

    g_Shell.run();
}
