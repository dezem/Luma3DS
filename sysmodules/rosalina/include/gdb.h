/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2019 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#pragma once

#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/synchronization.h>
#include <3ds/result.h>
#include "pmdbgext.h"
#include "sock_util.h"
#include "memory.h"

#define MAX_DEBUG           3
#define MAX_DEBUG_THREAD    127
#define MAX_BREAKPOINT      256
// 512+24 is the ideal size as IDA will try to read exactly 0x100 bytes at a time. Add 4 to this, for $#<checksum>, see below.
// IDA seems to want additional bytes as well.
// 1024 is fine enough to put all regs in the 'T' stop reply packets
#define GDB_BUF_LEN 1024

#define GDB_HANDLER(name)           GDB_Handle##name
#define GDB_QUERY_HANDLER(name)     GDB_HANDLER(Query##name)
#define GDB_VERBOSE_HANDLER(name)   GDB_HANDLER(Verbose##name)

#define GDB_DECLARE_HANDLER(name)           int GDB_HANDLER(name)(GDBContext *ctx)
#define GDB_DECLARE_QUERY_HANDLER(name)     GDB_DECLARE_HANDLER(Query##name)
#define GDB_DECLARE_VERBOSE_HANDLER(name)   GDB_DECLARE_HANDLER(Verbose##name)

typedef struct Breakpoint
{
    u32 address;
    u32 savedInstruction;
    u8 instructionSize;
    bool persistent;
} Breakpoint;

typedef struct PackedGdbHioRequest
{
    char magic[4]; // "GDB\x00"
    u32 version;

    // Request
    char functionName[16+1];
    char paramFormat[8+1];

    u64 parameters[8];
    size_t stringLengths[8];

    // Return
    int retval;
    int gdbErrno;
    bool ctrlC;
} PackedGdbHioRequest;

enum
{
    GDB_FLAG_SELECTED = 1,
    GDB_FLAG_USED  = 2,
    GDB_FLAG_ALLOCATED_MASK = GDB_FLAG_SELECTED | GDB_FLAG_USED,
    GDB_FLAG_EXTENDED_REMOTE = 4,
    GDB_FLAG_NOACK = 8,
    GDB_FLAG_PROC_RESTART_MASK = GDB_FLAG_NOACK | GDB_FLAG_EXTENDED_REMOTE | GDB_FLAG_USED,
    GDB_FLAG_PROCESS_CONTINUING = 16,
    GDB_FLAG_TERMINATE_PROCESS = 32,
    GDB_FLAG_ATTACHED_AT_START = 64,
    GDB_FLAG_CREATED = 128,
};

typedef enum GDBState
{
    GDB_STATE_DISCONNECTED,
    GDB_STATE_CONNECTED,
    GDB_STATE_ATTACHED,
    GDB_STATE_DETACHING,
} GDBState;

typedef struct ThreadInfo
{
    u32 id;
    u32 tls;
} ThreadInfo;

struct GDBServer;

typedef struct GDBContext
{
    sock_ctx super;
    struct GDBServer *parent;

    RecursiveLock lock;
    u16 localPort;

    u32 flags;
    GDBState state;
    bool noAckSent;

    u32 pid;
    Handle debug;

    // vRun and R (restart) info:
    FS_ProgramInfo launchedProgramInfo;
    u32 launchedProgramLaunchFlags;

    ThreadInfo threadInfos[MAX_DEBUG_THREAD];
    u32 nbThreads;
    u32 currentThreadId, selectedThreadId, selectedThreadIdForContinuing;
    u32 totalNbCreatedThreads;

    Handle processAttachedEvent, continuedEvent;
    Handle eventToWaitFor;

    bool catchThreadEvents;
    bool processEnded, processExited;

    DebugEventInfo latestDebugEvent;
    DebugFlags continueFlags;
    u32 svcMask[8];

    u32 nbBreakpoints;
    Breakpoint breakpoints[MAX_BREAKPOINT];

    u32 nbWatchpoints;
    u32 watchpoints[2];

    u32 currentHioRequestTargetAddr;
    PackedGdbHioRequest currentHioRequest;

    bool enableExternalMemoryAccess;
    char *commandData, *commandEnd;
    int latestSentPacketSize;
    char buffer[GDB_BUF_LEN + 4];

    char threadListData[0x800];
    u32 threadListDataPos;

    char memoryOsInfoXmlData[0x800];
    char processesOsInfoXmlData[0x2000];
} GDBContext;

typedef int (*GDBCommandHandler)(GDBContext *ctx);

void GDB_InitializeContext(GDBContext *ctx);
void GDB_FinalizeContext(GDBContext *ctx);

Result GDB_AttachToProcess(GDBContext *ctx);
void GDB_DetachFromProcess(GDBContext *ctx);
Result GDB_CreateProcess(GDBContext *ctx, const FS_ProgramInfo *progInfo, u32 launchFlags);

GDB_DECLARE_HANDLER(Unsupported);
GDB_DECLARE_HANDLER(EnableExtendedMode);
