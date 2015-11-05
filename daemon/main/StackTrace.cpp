/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef WIN32
#include <dbghelp.h>
#else
#include <unistd.h>
#include <sys/resource.h>
#include <signal.h>
#endif
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include "nzbget.h"
#include "Log.h"
#include "Options.h"
#include "StackTrace.h"

extern void ExitProc();

#ifdef WIN32

#ifdef DEBUG

void PrintBacktrace(PCONTEXT context)
{
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	char appDir[MAX_PATH + 1];
	GetModuleFileName(NULL, appDir, sizeof(appDir));
	char* end = strrchr(appDir, PATH_SEPARATOR);
	if (end) *end = '\0';

	SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);

	if (!SymInitialize(hProcess, appDir, TRUE))
	{
		warn("Could not obtain detailed exception information: SymInitialize failed");
		return;
	}

	const int MAX_NAMELEN = 1024;
	IMAGEHLP_SYMBOL64* sym = (IMAGEHLP_SYMBOL64 *) malloc(sizeof(IMAGEHLP_SYMBOL64) + MAX_NAMELEN);
	memset(sym, 0, sizeof(IMAGEHLP_SYMBOL64) + MAX_NAMELEN);
	sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
	sym->MaxNameLength = MAX_NAMELEN;

	IMAGEHLP_LINE64 ilLine;
	memset(&ilLine, 0, sizeof(ilLine));
	ilLine.SizeOfStruct = sizeof(ilLine);

	STACKFRAME64 sfStackFrame;
	memset(&sfStackFrame, 0, sizeof(sfStackFrame));
	DWORD imageType;
#ifdef _M_IX86
	imageType = IMAGE_FILE_MACHINE_I386;
	sfStackFrame.AddrPC.Offset = context->Eip;
	sfStackFrame.AddrPC.Mode = AddrModeFlat;
	sfStackFrame.AddrFrame.Offset = context->Ebp;
	sfStackFrame.AddrFrame.Mode = AddrModeFlat;
	sfStackFrame.AddrStack.Offset = context->Esp;
	sfStackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
	imageType = IMAGE_FILE_MACHINE_AMD64;
	sfStackFrame.AddrPC.Offset = context->Rip;
	sfStackFrame.AddrPC.Mode = AddrModeFlat;
	sfStackFrame.AddrFrame.Offset = context->Rsp;
	sfStackFrame.AddrFrame.Mode = AddrModeFlat;
	sfStackFrame.AddrStack.Offset = context->Rsp;
	sfStackFrame.AddrStack.Mode = AddrModeFlat;
#else
	warn("Could not obtain detailed exception information: platform not supported");
	return;
#endif

	for (int frameNum = 0; ; frameNum++)
	{
		if (frameNum > 1000)
		{
			warn("Endless stack, abort tracing");
			return;
		}

		if (!StackWalk64(imageType, hProcess, hThread, &sfStackFrame, context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
		{
			warn("Could not obtain detailed exception information: StackWalk64 failed");
			return;
		}

		DWORD64 dwAddr = sfStackFrame.AddrPC.Offset;
		char symName[1024];
		char srcFileName[1024];
		int lineNumber = 0;

		DWORD64 dwSymbolDisplacement;
		if (SymGetSymFromAddr64(hProcess, dwAddr, &dwSymbolDisplacement, sym))
		{
			UnDecorateSymbolName(sym->Name, symName, sizeof(symName), UNDNAME_COMPLETE);
			symName[sizeof(symName) - 1] = '\0';
		}
		else
		{
			strncpy(symName, "<symbol not available>", sizeof(symName));
		}

		DWORD dwLineDisplacement;
		if (SymGetLineFromAddr64(hProcess, dwAddr, &dwLineDisplacement, &ilLine))
		{
			lineNumber = ilLine.LineNumber;
			char* useFileName = ilLine.FileName;
			char* root = strstr(useFileName, "\\daemon\\");
			if (root)
			{
				useFileName = root;
			}
			strncpy(srcFileName, useFileName, sizeof(srcFileName));
			srcFileName[sizeof(srcFileName) - 1] = '\0';
		}
		else
		{
			strncpy(srcFileName, "<filename not available>", sizeof(symName));
		}

		info("%s (%i) : %s", srcFileName, lineNumber, symName);

		if (sfStackFrame.AddrReturn.Offset == 0)
		{
			break;
		}
	}
}
#endif

LONG __stdcall ExceptionFilter(EXCEPTION_POINTERS* exPtrs)
{
	error("Unhandled Exception: code: 0x%8.8X, flags: %d, address: 0x%8.8X",
		exPtrs->ExceptionRecord->ExceptionCode,
		exPtrs->ExceptionRecord->ExceptionFlags,
		exPtrs->ExceptionRecord->ExceptionAddress);

#ifdef DEBUG
	PrintBacktrace(exPtrs->ContextRecord);
#else
	 info("Detailed exception information can be printed by debug version of NZBGet (available from download page)");
#endif

	ExitProcess(-1);
	return EXCEPTION_CONTINUE_SEARCH;
}

void InstallErrorHandler()
{
	SetUnhandledExceptionFilter(ExceptionFilter);
}

#else

#ifdef DEBUG
typedef void(*sighandler)(int);
std::vector<sighandler> SignalProcList;
#endif

#ifdef HAVE_SYS_PRCTL_H
/**
* activates the creation of core-files
*/
void EnableDumpCore()
{
	rlimit rlim;
	rlim.rlim_cur= RLIM_INFINITY;
	rlim.rlim_max= RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);
	prctl(PR_SET_DUMPABLE, 1);
}
#endif

void PrintBacktrace()
{
#ifdef HAVE_BACKTRACE
	printf("Segmentation fault, tracing...\n");

	void *array[100];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace(array, 100);
	strings = backtrace_symbols(array, size);

	// first trace to screen
	printf("Obtained %zd stack frames\n", size);
	for (i = 0; i < size; i++)
	{
		printf("%s\n", strings[i]);
	}

	// then trace to log
	error("Segmentation fault, tracing...");
	error("Obtained %zd stack frames", size);
	for (i = 0; i < size; i++)
	{
		error("%s", strings[i]);
	}

	free(strings);
#else
	error("Segmentation fault");
#endif
}

/*
 * Signal handler
 */
void SignalProc(int signum)
{
	switch (signum)
	{
		case SIGINT:
			signal(SIGINT, SIG_DFL);   // Reset the signal handler
			ExitProc();
			break;

		case SIGTERM:
			signal(SIGTERM, SIG_DFL);   // Reset the signal handler
			ExitProc();
			break;

		case SIGCHLD:
			// ignoring
			break;

#ifdef DEBUG
		case SIGSEGV:
			signal(SIGSEGV, SIG_DFL);   // Reset the signal handler
			PrintBacktrace();
			break;
#endif
	}
}

void InstallErrorHandler()
{
#ifdef HAVE_SYS_PRCTL_H
	if (g_Options->GetDumpCore())
	{
		EnableDumpCore();
	}
#endif

	signal(SIGINT, SignalProc);
	signal(SIGTERM, SignalProc);
	signal(SIGPIPE, SIG_IGN);
#ifdef DEBUG
	signal(SIGSEGV, SignalProc);
#endif
#ifdef SIGCHLD_HANDLER
	// it could be necessary on some systems to activate a handler for SIGCHLD
	// however it make troubles on other systems and is deactivated by default
	signal(SIGCHLD, SignalProc);
#endif
}

#endif

#ifdef DEBUG
class SegFault
{
public:
	void DoSegFault()
	{
		char* N = NULL;
		strcpy(N, "");
	}
};

void TestSegFault()
{
	SegFault s;
	s.DoSegFault();
}
#endif
