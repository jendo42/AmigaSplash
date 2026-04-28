#include <workbench/startup.h>
#include <graphics/gfxbase.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/icon.h>
#include <proto/graphics.h>
#include <intuition/intuition.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "system.h"

extern int    __argc;
extern char **__argv;
extern uint32_t __commandlen;
extern char *__commandline;

extern struct WBStartup *_WBenchMsg;

// Disable command line parsing
// argv and argc are null now
// TODO: maybe reimplement the buggy argument parsing
void __nocommandline(){};

static buffer_t g_command;
static buffer_t g_workdir;
static buffer_t g_executable;
static buffer_t g_commandline;
static buffer_t g_tooltypes;
static BPTR g_con;
static struct DiskObject *g_dobj;

#define LOG_DEBUG(...)
#define LOG_WARN(...)

bool sys_init()
{
	struct Process *process = (struct Process *)FindTask(NULL);
	if (!_WBenchMsg && !process->pr_CLI) {
		return false;
	}

	// initialize string buffers
	buffer_init(&g_command, 1, 64);
	buffer_init(&g_workdir, 1, 64);
	buffer_init(&g_executable, 1, 64);
	buffer_init(&g_commandline, 1, 64);
	buffer_init(&g_tooltypes, sizeof(char *), 8);

	// load path of current directory
	sys_getpath(process->pr_CurrentDir, &g_workdir);
	buffer_append_char(&g_workdir, 0);

	// process arguments
	if (process->pr_CLI) {
		// Parse argv and enter main processing loop.
		struct CommandLineInterface * cli = (struct CommandLineInterface *)BADDR(process->pr_CLI);
		char *cmdname = (char *)BADDR(cli->cli_CommandName);

		// reconstruct full path to executable
		buffer_append_string(&g_executable, g_workdir.data, false);
		buffer_append(&g_executable, cmdname + 1, *cmdname);
		buffer_append_char(&g_executable, 0);

		// reconstruct full program command line
		buffer_append_char(&g_commandline, '\"');
		buffer_append(&g_commandline, cmdname + 1, *cmdname);
		buffer_append_char(&g_commandline, '\"');
		if (__commandlen) {
			buffer_append_char(&g_commandline, ' ');
			buffer_append(&g_commandline, __commandline, __commandlen);
		}
		char *back = (char *)buffer_back(&g_commandline);
		if (*back == '\n') {
			*back = 0;
		} else {
			buffer_append_char(&g_commandline, 0);
		}

		LOG_DEBUG("CLI Args: '%s'", g_commandline.data);
		if (SysBase->LibNode.lib_Version >= 36) {
			// load pr_Arguments
			LOG_DEBUG("pr_Arguments: %s", process->pr_Arguments);
		}
	} else if (_WBenchMsg) {
		// Parse wbstartup and enter main processing loop.
		struct WBStartup *startup = _WBenchMsg;
		LOG_DEBUG("WB Args: %d; sm_ToolWindow: '%s'", startup->sm_NumArgs, startup->sm_ToolWindow);
		for (int i = 0; i < startup->sm_NumArgs; i++) {
			struct WBArg *arg = startup->sm_ArgList + i;
			LOG_DEBUG("Arg[%u] -> %p; '%s'", i, arg->wa_Lock, arg->wa_Name);
		}

		// reconstruct full path to executable
		sys_getpath(startup->sm_ArgList[0].wa_Lock, &g_executable);
		buffer_append_string(&g_executable, startup->sm_ArgList[0].wa_Name, true);
	}

	// load toolset params
	g_dobj = GetDiskObject(sys_exepath());
	if (g_dobj) {
		for (char **it = (char **)g_dobj->do_ToolTypes; *it; it++) {
			char *tooltype = *it;
			char **back = (char **)buffer_emplace_back(&g_tooltypes);
			if (back) {
				*back = tooltype;
			}
			LOG_DEBUG("tooltype -> '%s'", tooltype);
		}
	}

	LOG_DEBUG("Executable: '%s'", g_executable.data);
	LOG_DEBUG("Workdir: '%s'", sys_workdirpath());
	LOG_DEBUG("Debug: '%s'", sys_matchtooltype("DEBUG"));
	return true;
}

void sys_cleanup()
{
	LOG_DEBUG("Cleanup");
	if (g_dobj) {
		FreeDiskObject(g_dobj);
		g_dobj = NULL;
	}

	buffer_cleanup(&g_command);
	buffer_cleanup(&g_workdir);
	buffer_cleanup(&g_executable);
	buffer_cleanup(&g_commandline);
	buffer_cleanup(&g_tooltypes);

	struct Process *proc = (struct Process *)FindTask(NULL);
	if (proc->pr_CIS == g_con) {
		proc->pr_CIS = 0;
	}
	if (proc->pr_COS == g_con) {
		proc->pr_COS = 0;
	}
	if (g_con) {
		Close(g_con);
		g_con = 0;
	}
}

bool sys_isaga()
{
    if (GfxBase->LibNode.lib_Version >= 39) {
        if (GfxBase->ChipRevBits0 & GFXF_AA_ALICE) {
            return true;
        }
    }

    return false;
}

uint32_t sys_getpath(BPTR lock, buffer_t *buffer)
{
	uint32_t result = 0;
	struct FileInfoBlock fib;
	if (!lock) {
		return 0;
	}

	BPTR currLock = DupLock(lock);

	buffer_t stack;
	buffer_init(&stack, sizeof(fib.fib_FileName), 8);

	while (currLock) {
		if (!Examine(currLock, &fib)) {
			result = IoErr();
			break;
		}

		char *name = (char *)buffer_emplace_back(&stack);
		if (!name) {
			result = ERROR_NO_FREE_STORE;
			break;
		}

		BPTR parentLock = ParentDir(currLock);

		// store the file name on stack
		// don't have to be null terminated here
		*name = strlen(fib.fib_FileName);
		strcpy(name + 1, fib.fib_FileName);

		// generate separator for directory
		if (fib.fib_DirEntryType > 0) {
			name[*name + 1] = parentLock ? '/' : ':';
			*name = *name + 1;
		}

		// next parent
		UnLock(currLock);
		currLock = parentLock;
	}

	// output to buffer
	buffer_clear(buffer);
	while (stack.count) {
		char *name = (char *)buffer_back(&stack);
		buffer_append(buffer, name + 1,  *name);
		buffer_pop_back(&stack);
	}

	buffer_cleanup(&stack);
	return result;
}

const char *sys_workdirpath()
{
	return g_workdir.count ? g_workdir.data : NULL;
}

const char *sys_exepath()
{
	return g_executable.count ? g_executable.data : NULL;
}
