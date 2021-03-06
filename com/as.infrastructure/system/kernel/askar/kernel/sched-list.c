/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2018  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* ============================ [ INCLUDES  ] ====================================================== */
#include "kernel_internal.h"
#ifdef ENABLE_LIST_SCHED
#include "asdebug.h"
#include <string.h>
/* ============================ [ MACROS    ] ====================================================== */
#ifndef RDY_BITS
/* default 32 bit CPU */
#define RDY_BITS   32
#endif
#if (RDY_BITS == 32)
#define RDY_SHIFT 5
#define RDY_MASK  0x1F
#elif (RDY_BITS == 16)
#define RDY_SHIFT 4
#define RDY_MASK  0xF
#else
#error Not supported RDY_BITS, set it according to CPU bits.
#endif
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
static TAILQ_HEAD(ready_list, TaskVar) ReadyList[PRIORITY_NUM+1];
static int ReadyGroup;
static int ReadyMapTable[(PRIORITY_NUM+RDY_BITS)/RDY_BITS];
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
static inline void Sched_SetReadyBit(PriorityType priority)
{
	asAssert(priority <= PRIORITY_NUM);

	priority = PRIORITY_NUM-priority;

	ReadyGroup |= (1u<<(priority>>RDY_SHIFT));

	ReadyMapTable[priority>>RDY_SHIFT] |= (1u << (priority&RDY_MASK));
}

static inline void Sched_ClearReadyBit(PriorityType priority)
{
	asAssert(priority <= PRIORITY_NUM);

	priority = PRIORITY_NUM-priority;

	ReadyMapTable[priority>>RDY_SHIFT] &= ~(1u << (priority&RDY_MASK));

	if(0u == ReadyMapTable[priority>>RDY_SHIFT])
	{
		ReadyGroup &= ~(1u<<(priority>>RDY_SHIFT));
	}
}

static inline PriorityType Sched_GetReadyBit(void)
{
	int X,Y;
	PriorityType priority;

	Y = ffs(ReadyGroup);
	if(Y > 0)
	{
		asAssert(Y<=(sizeof(ReadyMapTable)/sizeof(int)));
		X = ffs(ReadyMapTable[Y-1]);
		asAssert((X>0) && (X<RDY_BITS));
		priority = ((Y-1)<<RDY_SHIFT) + (X-1);
		priority = PRIORITY_NUM-priority;
	}
	else
	{
		priority = 0;
	}

	return priority;
}
static void Sched_AddReadyInternal(TaskType TaskID, PriorityType priority)
{
	TaskVarType* pTaskVar = &TaskVarArray[TaskID];

	TAILQ_INSERT_TAIL(&(ReadyList[priority]), pTaskVar, entry);

	Sched_SetReadyBit(priority);

	if(priority > ReadyVar->priority)
	{
		ReadyVar = TAILQ_FIRST(&(ReadyList[priority]));
	}
	else if(ReadyVar == RunningVar)
	{
		priority = Sched_GetReadyBit();
		ReadyVar = TAILQ_FIRST(&(ReadyList[priority]));
	}
	else
	{
		/* no update of ReadyVar */
	}
}
/* ============================ [ FUNCTIONS ] ====================================================== */
void Sched_Init(void)
{
	PriorityType prio;

	ReadyGroup = 0;

	for(prio=0; prio < sizeof(ReadyMapTable); prio++)
	{
		ReadyMapTable[prio] = 0;
		TAILQ_INIT(&ReadyList[prio]);
	}

	for(prio=0; prio < (PRIORITY_NUM+1); prio++)
	{
		TAILQ_INIT(&(ReadyList[prio]));
	}
}

void Sched_AddReady(TaskType TaskID)
{
	Sched_AddReadyInternal(TaskID, TaskConstArray[TaskID].initPriority);
}

#if(OS_PTHREAD_NUM > 0)
void Sched_PosixAddReady(TaskType TaskID)
{
	Sched_AddReadyInternal(TaskID, TaskVarArray[TaskID].priority);
}
#endif

void Sched_Preempt(void)
{
	PriorityType priority;

	OSPostTaskHook();
	/* remove the ReadyVar from the queue */
	priority = ReadyVar->priority;
	TAILQ_REMOVE(&(ReadyList[priority]), ReadyVar, entry);
	if(TAILQ_EMPTY(&(ReadyList[priority])))
	{
		Sched_ClearReadyBit(priority);
	}

	/* put the RunningVar back to the head of queue */
	priority = RunningVar->priority;
	TAILQ_INSERT_HEAD(&(ReadyList[priority]), RunningVar, entry);
	Sched_SetReadyBit(priority);
}

void Sched_GetReady(void)
{
	PriorityType priority = Sched_GetReadyBit();

	ReadyVar = TAILQ_FIRST(&(ReadyList[priority]));

	if(NULL != ReadyVar)
	{
		TAILQ_REMOVE(&(ReadyList[priority]), ReadyVar, entry);
		if(TAILQ_EMPTY(&(ReadyList[priority])))
		{
			Sched_ClearReadyBit(priority);
		}
	}
}

bool Sched_Schedule(void)
{
	bool needSchedule = FALSE;

	PriorityType priority = Sched_GetReadyBit();

	ReadyVar = TAILQ_FIRST(&(ReadyList[priority]));

	if( (NULL != ReadyVar) && (ReadyVar->priority >  RunningVar->priority))
	{
		/* remove the ReadyVar from the queue */
		TAILQ_REMOVE(&(ReadyList[priority]), ReadyVar, entry);
		if(TAILQ_EMPTY(&(ReadyList[priority])))
		{
			Sched_ClearReadyBit(priority);
		}

		/* put the RunningVar back to the head of queue */
		priority = RunningVar->priority;
		TAILQ_INSERT_HEAD(&(ReadyList[priority]), RunningVar, entry);
		Sched_SetReadyBit(priority);

		needSchedule = TRUE;
	}
	return needSchedule;
}

#endif /* ENABLE_LIST_SCHED */
