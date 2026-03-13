/*    INTRNODE.CPP
      Copyright 2003 Evan Elias

      ObjectDoor -- Code used in communication between Door Clients and Door Server.
      Original Windows implementation uses Mailslots for IPC.
      Linux implementation uses POSIX message queues for IPC, which provide similar
      message-oriented semantics.
*/

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include "intrnode.h"
#include "trivlog.h"
#include "../trivia/doorset.h"

#ifndef _WIN32
#include <sys/file.h>
#endif


// Creates a message queue for the given node, or for the gameserver if the
// node is -1.  Returns a handle for reading from the queue.
HANDLE createSlot(int nNode, std::string* pSlotName)
{
	char szText[80];
	if (nNode < 0)
		sprintf(szText, "/%s", DOOR_INP_SLOT);
	else
		sprintf(szText, "/%s%d", DOOR_OUT_SLOT, nNode);
	if (pSlotName != nullptr)
		*pSlotName = szText;

	#ifdef _WIN32
	/*
	HANDLE CreateMailslotA(
	  [in]           LPCSTR                lpName,
	  [in]           DWORD                 nMaxMessageSize,
	  [in]           DWORD                 lReadTimeout,
	  [in, optional] LPSECURITY_ATTRIBUTES lpSecurityAttributes
	);
	*/
	return CreateMailslot(szText, 240, 0, nullptr);
	#else
	// Use POSIX message queues as the Linux equivalent of Windows mailslots.
	// Both are message-oriented IPC mechanisms.
	struct mq_attr attr;
	attr.mq_flags = 0;
	attr.mq_maxmsg = MQ_MAX_MSGS;
	attr.mq_msgsize = MQ_MAX_MSG_SIZE;
	attr.mq_curmsgs = 0;

	if (nNode < 0)
	{
		// Server input queue.  Use a lock file to detect whether another
		// server is already running, mirroring the Windows behavior where
		// CreateMailslot fails if the mailslot already exists.
		static int lockFd = -1;
		lockFd = open("trivsrv.lock", O_CREAT | O_RDWR, 0666);
		if (lockFd >= 0 && flock(lockFd, LOCK_EX | LOCK_NB) != 0)
		{
			// Another server holds the lock — exit like Windows does.
			close(lockFd);
			lockFd = -1;
			return (mqd_t)-1;
		}

		// We hold the lock.  If the queue exists it is stale (from a
		// previous crash), so it is safe to unlink and recreate it.
		mqd_t mq = mq_open(szText, O_CREAT | O_EXCL | O_RDONLY | O_NONBLOCK, 0666, &attr);
		if (mq == (mqd_t)-1 && errno == EEXIST)
		{
			mq_unlink(szText);
			mq = mq_open(szText, O_CREAT | O_EXCL | O_RDONLY | O_NONBLOCK, 0666, &attr);
		}
		return mq;
	}

	// Per-node output queue.  If it already exists (stale from a crash),
	// unlink and recreate.
	mqd_t mq = mq_open(szText, O_CREAT | O_EXCL | O_RDONLY | O_NONBLOCK, 0666, &attr);
	if (mq == (mqd_t)-1 && errno == EEXIST)
	{
		mq_unlink(szText);
		mq = mq_open(szText, O_CREAT | O_EXCL | O_RDONLY | O_NONBLOCK, 0666, &attr);
	}
	return mq;
	#endif
}


// Opens a pre-existing message queue for writing.
HANDLE openSlot(int nNode)
{
	char szText[80];
	if ( nNode < 0 )
	  sprintf(szText, "/%s", DOOR_INP_SLOT);
	else
	  sprintf(szText, "/%s%d", DOOR_OUT_SLOT, nNode);

	#ifdef _WIN32
	return CreateFile(szText, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, (LPSECURITY_ATTRIBUTES) nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE) nullptr);
	#else
	return mq_open(szText, O_WRONLY | O_NONBLOCK);
	#endif
}


// Writes text to a given slot/queue.
bool sendToSlot(HANDLE hSlot, char* szInput)
{
	DWORD cbWritten;

	#ifdef _WIN32
	return WriteFile(hSlot, szInput,  strlen(szInput) + 1, &cbWritten, nullptr) == TRUE;
	#else
	// Non-blocking send with retry.  Windows mailslots don't block on write,
	// so we match that behavior.  If the queue is full (EAGAIN), wait briefly
	// for the reader to drain it and retry.
	for (int retries = 0; retries < 200; retries++)
	{
		if (mq_send(hSlot, szInput, strlen(szInput) + 1, 0) == 0)
			return true;
		if (errno != EAGAIN)
		{
			trivlog("trivsrv: mq_send error %d\n", errno);
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	trivlog("trivsrv: mq_send exhausted retries\n");
	return false;
	#endif
}


// Converts InputData to char*
char* InputData::toString(char* szBuffer)
{
   sprintf(szBuffer, "%d %d %s", nFrom, nType, szMessage);
   return szBuffer;
}

// Converts char* to InputData
InputData::InputData(char* szMsg)
{
   nFrom = atoi( strtok(szMsg, " ") );
   nType = atoi( strtok(nullptr, " ") );
   szMessage[0] = '\0';
   char* szTemp = strtok(nullptr, "");
   if ( szTemp != nullptr )
      strcpy( szMessage, szTemp );
}


InputData::InputData()
{
}

// Converts OutputData to char*
char* OutputData::toString(char* szBuffer)
{
   sprintf(szBuffer, "%d %d %d %d %d %d %d %s", nType, nColor, nHp, nSp, nMf, nEnemyPercent, nHpColor, szMessage);
   return szBuffer;
}

// Converts char* to OutputData
OutputData::OutputData(char* szMsg)
{
   nType = atoi( strtok(szMsg, " ") );
   nColor = atoi( strtok(nullptr, " ") );
   nHp = atoi( strtok(nullptr, " ") );
   nSp = atoi( strtok(nullptr, " ") );
   nMf = atoi( strtok(nullptr, " ") );
   nEnemyPercent = atoi( strtok(nullptr, " ") );
   nHpColor = atoi( strtok(nullptr, " ") );
   szMessage[0] = '\0';
   char* szTemp = strtok(nullptr, "");
   if ( szTemp != nullptr )
      strcpy( szMessage, szTemp );
}

OutputData::OutputData()
{
   szMessage[0] = '\0';
   nType = OP_NORMAL;
   nColor = 7;
   nHp = 0;
   nSp = -1;
   nMf = -1;
   nEnemyPercent = -1;
   nHpColor = 7;
}


QueueNode::QueueNode(InputData idMessage)
{
   id = idMessage;
   qnNext = nullptr;
}


MessageQueue::MessageQueue()
{
   qnTop = nullptr;
}

// Removes the Input message at the top of the input queue.
InputData MessageQueue::dequeue()
{
	if ( isEmpty() )
		return InputData();

	InputData idReturn = qnTop->id;
	QueueNode* qnNewTop = qnTop->qnNext;
	delete qnTop;
	qnTop = qnNewTop;
	return idReturn;
}


// Adds an input message to the end of an input queue.
void MessageQueue::enqueue(InputData idMessage)
{
	if ( isEmpty() )
	{
		qnTop = new QueueNode(idMessage);
		return;
	}

	QueueNode* qnCurrent = qnTop;

	while ( qnCurrent->qnNext != nullptr )
	{
		qnCurrent = qnCurrent->qnNext;
	}

	qnCurrent->qnNext = new QueueNode(idMessage);
}


bool MessageQueue::isEmpty()
{
	return qnTop == nullptr;
}


