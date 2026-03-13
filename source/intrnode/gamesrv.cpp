#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <conio.h>
#include <dir.h>
//#include <new.h>
#endif

#include <new>
#include <thread>
#include <filesystem>
#include <random>

#include <stdio.h>
#include "../intrnode/gamesrv.h"
#include "trivlog.h"
#include "../trivia/doorset.h"

void handleThread(void*);
void startRoundThread(void*);
void badNew_Gamesrv();


/////////////////////////////////////////////////////////////////////////////////////////
// GameServer methods

// GameServer constructor
GameServer::GameServer()
{
	// Set the function for bad new call
	std::set_new_handler(badNew_Gamesrv);

	// Open the input slot
	hInputSlot = createSlot(-1, &inputSlotName);

	if ( hInputSlot == INVALID_HANDLE_VALUE )
	{
		inputSlotName = "";
		exit(0);
	}

	// Initialize all nodes to nullptr.
	for ( int n = 0; n < MAX_NODE; n++ )
	{
		gNode[n] = nullptr;
	}

	#ifdef _WIN32
	// Initialize the game's CRITICAL_SECTION object.
	InitializeCriticalSection(&csCritical);
	#endif

	nCriticalCount = 0;
	nPlayersInGame = 0;
}


// Prints a message to all on-line nodes that are flagged as being in the game.
void GameServer::printAll(char* szText, short nColor, short nNewlines, short nType)
{
	for ( short n = 0; n < MAX_NODE; n++ )
	{
		if ( gNode[n] != nullptr && gNode[n]->bInGame )
			gNode[n]->print(szText, nColor, nNewlines, nType);
	}
}


// Same as printAll() but uses word wrap.
void GameServer::printAllWordWrap(char* szText, short nColor, short nNewlines, short nOffset, bool bCarryIndent)
{
	for ( short n = 0; n < MAX_NODE; n++ )
	{
		if ( gNode[n] != nullptr && gNode[n]->bInGame )
			gNode[n]->printWordWrap(szText, nColor, nNewlines, nOffset, bCarryIndent);
	}
}


// Repeatedly read input from the input slot.
// Returns when last player exits game.
void GameServer::run()
{
	DWORD nNextMessageSize, nMessagesLeft, nBytesRead;
	InputData idMessage;
	#ifdef _WIN32
	char szBuffer[245];
	#else
	char szBuffer[MQ_MAX_MSG_SIZE + 1];
	#endif

	// Call randomize() in the GameServer's operational thread (usually same thread as main())
	myRandomize();

	while ( true )
	{
		nMessagesLeft = 0;
		nNextMessageSize = MAILSLOT_NO_MESSAGE;

		// Check the status of the slot.
		#ifdef _WIN32
		GetMailslotInfo(hInputSlot, nullptr, &nNextMessageSize, &nMessagesLeft, nullptr);
		#else
		{
			struct mq_attr attr;
			if (mq_getattr(hInputSlot, &attr) == 0 && attr.mq_curmsgs > 0)
			{
				nNextMessageSize = attr.mq_msgsize;
				nMessagesLeft = attr.mq_curmsgs;
			}
		}
		#endif

		// If there's no input, sleep and then return to top of loop.
		if ( nNextMessageSize == MAILSLOT_NO_MESSAGE )
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		// If there's a message, read it and process it.
		if ( nMessagesLeft > 0 )
		{
			// Ignore invalid message sizes (see MS Knowledge Base Q192276)
			if ( nNextMessageSize > 500 )
				continue;

			#ifdef _WIN32
			ReadFile(hInputSlot, szBuffer, nNextMessageSize, &nBytesRead, nullptr);
			#else
			{
				ssize_t bytesRead = mq_receive(hInputSlot, szBuffer, MQ_MAX_MSG_SIZE, nullptr);
				if (bytesRead < 0)
					continue;
				nBytesRead = (DWORD)bytesRead;
				szBuffer[bytesRead] = '\0';
			}
			#endif
			idMessage = szBuffer;
			trivlog("trivsrv: recv type=%d from=%d msg='%s'\n", idMessage.nType, idMessage.nFrom, idMessage.szMessage);
			enterCritical();
			handleInput(idMessage);
			leaveCritical();

			// If last player just exited, shut down the server.
			if ( idMessage.nType == IP_FINISHED && nPlayersInGame == 0 )
			{
				#ifdef _WIN32
				CloseHandle(hInputSlot);
				DeleteCriticalSection(&csCritical);
				#else
				mq_close(hInputSlot);
				mq_unlink(inputSlotName.c_str());
				#endif
				return;
			}
		}
	}
}


// Processes input messages and redirects them to the proper node thread.
void GameServer::handleInput(InputData idMessage)
{
	// If message is from a totally out-of-range node number, ignore it.
	if ( idMessage.nFrom < 0 || idMessage.nFrom >= MAX_NODE )
		return;

	// If an enter-game message:
	if ( idMessage.nType == IP_ENTER_GAME )
	{
		if ( gNode[idMessage.nFrom] != nullptr )
		{
			gNode[idMessage.nFrom]->print((char*)"Error:  Node already in game.  Please wait 5 seconds and re-enter game.");
			gNode[idMessage.nFrom]->exitGame();
			return;
		}

		// Create the new node and then return.  Start round thread if needed.  Verify the player isn't a dupe.
		nPlayersInGame++;
		addNode(idMessage.nFrom, idMessage.szMessage);

		if ( nPlayersInGame == 1 )
		{
			#ifdef _WIN32
			_beginthread(startRoundThread, 4096, this);
			#else
			std::thread t(startRoundThread, static_cast<void*>(this));
			t.detach();
			#endif
		}

		for ( short n = 0; n < MAX_NODE; n++ )
		{
			if ( gNode[n] == nullptr || n == idMessage.nFrom )
				continue;
			if ( strcmpi(gNode[n]->szAlias, gNode[idMessage.nFrom]->szAlias) == 0 )
			{
				gNode[idMessage.nFrom]->print((char*)"You are on-line on multiple nodes!\r\nPlease wait 5 seconds and re-enter the game.");
				gNode[n]->print((char*)"You are on-line on multiple nodes!\r\nPlease wait 5 seconds and re-enter the game.");
				gNode[idMessage.nFrom]->exitGame();
				gNode[n]->exitGame();
			}
		}

		return;
	}

	// If input is from an invalid node, ignore it.
	if ( gNode[idMessage.nFrom] == nullptr )
		return;

	// If an exit-game message:
	if ( idMessage.nType == IP_FINISHED )
	{
		trivlog("trivsrv: IP_FINISHED from node %d, nPlayersInGame=%d\n", idMessage.nFrom, nPlayersInGame);

		// Kill the node.  If the node has a thread, wait until the
		// thread is terminated (by a IP_FORCE_EXIT) before proceeding.
		while ( gNode[idMessage.nFrom]->bHasThread )
		{
			leaveCritical();
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			enterCritical();
		}

		// Notify remaining players that this player has left.
		char szExitMsg[120];
		sprintf(szExitMsg, ">>> %s has left the game.", gNode[idMessage.nFrom]->szAlias);

		#ifdef _WIN32
		CloseHandle(gNode[idMessage.nFrom]->hOutputSlot);
		#else
		mq_close(gNode[idMessage.nFrom]->hOutputSlot);
		#endif
		delete gNode[idMessage.nFrom];
		gNode[idMessage.nFrom] = nullptr;
		nPlayersInGame--;

		trivlog("trivsrv: node %d removed, nPlayersInGame now %d\n", idMessage.nFrom, nPlayersInGame);

		if ( nPlayersInGame > 0 )
			printAll(szExitMsg, MAGENTA);

		return;
	}

	// Otherwise, message is IP_NORMAL or IP_FORCE_EXIT.  In either case, route the
	// InputData to the appropriate node, and let the node's thread take care of it.
	// If not using separate threads for each node, have an outside function handle
	// the message instead.
	if ( gNode[idMessage.nFrom]->bHasThread )
		gNode[idMessage.nFrom]->mqInput.enqueue(idMessage);
	else
		centralInput(idMessage);
}


// Virtual function for handling input on nodes that don't have their own
// threads.  Over-ride to add functionality if desired.
void GameServer::centralInput(InputData id)
{
}


// Virtual function for handling a round event.  Over-ride to add functionality
// if desired.
void GameServer::doorRound(time_t nRound)
{
}



// Used to enter a critical section.  Useful for tasks in which a thread is
// modifying global resources or game data, to prevent other threads from
// doing so.
void GameServer::enterCritical()
{
	nCriticalCount++;
	#ifdef _WIN32
	EnterCriticalSection(&csCritical);
	#else
	csCritical.lock();
	#endif
}


// Used to leave a critical section.
void GameServer::leaveCritical()
{
	#ifdef _WIN32
	LeaveCriticalSection(&csCritical);
	#else
	csCritical.unlock();
	#endif
	nCriticalCount--;
}



/////////////////////////////////////////////////////////////////////////////////////////
// GameNode methods


// GameNode constructor.  Opens the output slot and assigns default values.
GameNode::GameNode(short nNumber, char* szUserInfo)
{
   hOutputSlot = openSlot(nNumber);
   nIndex = nNumber;
   bInGame = false;
   bHasThread = false;

   // Read sysop status flag
   if ( atoi( strtok(szUserInfo, "&") ) == 1 )
      bSysop = true;
   else
      bSysop = false;

   // Read gender
   char* szTemp = strtok(nullptr, "&");
   cGender = szTemp[0];

   // Read platform
   nPlatform = atoi( strtok(nullptr, "&") );

   // Read alias
   strcpy( szAlias, strtok(nullptr, "&") );

   // Read real name
   strcpy( szRealName, strtok(nullptr, "") );
}


// Destructor (virtual -- override if needed)
GameNode::~GameNode()
{
}




// Prints a message to a given node.
void GameNode::print(char* szText, short nColor, short nNewlines, short nType)
{
   OutputData odMessage;
   char szBuffer[MQ_MAX_MSG_SIZE + 1];

   // Need to convert szText to an output message.
   odMessage.szMessage[0] = '\0';
   if ( szText != nullptr )
      {
      if ( strlen(szText) >= 200 - 2*nNewlines )
         szText[199 - 2*nNewlines] = '\0';
      strcpy(odMessage.szMessage, szText);
      }

   // Add the specified number of newlines (1 by default)
   for ( short n = 0; n < nNewlines; n++ )
      {
      strcat(odMessage.szMessage, "\r\n");
      }

   // Set the message color, type, and player stat info
   odMessage.nColor = nColor;
   odMessage.nType = nType;
   fillStats(&odMessage);

   // Send the message to the slot, after converting it to a string.
   sendToSlot(hOutputSlot, odMessage.toString(szBuffer));
}



// Prints a message to a given node; wraps words
void GameNode::printWordWrap(char* szText, short nColor, short nNewlines, short nOffset, bool bCarryIndent)
{
   char szBuffer[81], szCarryOffset[81];
   char* szPosition = szText;
   short nMarker;

   if ( bCarryIndent )
      {
      for ( short n = 0; n < nOffset; n++ )
         {
         szCarryOffset[n] = ' ';
         }
      szCarryOffset[nOffset] = '\0';
      }

   while ( strlen(szPosition) > (unsigned)(79 - nOffset) )
      {
      nMarker = 79 - nOffset;

      while ( szPosition[nMarker] != ' ' )
         {
         nMarker--;
         if ( nMarker == 0 )
            {
            print(szPosition, nColor, nNewlines);
            return;
            }
         }

      strncpy(szBuffer, szPosition, nMarker);
      szBuffer[nMarker] = '\0';
      print(szBuffer, nColor, 1);
      szPosition += nMarker+1;

      if ( bCarryIndent && strlen(szPosition) > 0 )
         print(szCarryOffset, LWHITE, 0);
      else
         nOffset = 0;
      }

   if (strlen(szPosition) > 0 )
      print(szPosition, nColor, nNewlines);
}


// Prints a newline
void GameNode::newline()
{
   print((char*)" \r\n", LWHITE, 0);
}


// Tells the node to pause for a keystroke
void GameNode::pausePrompt()
{
   print(nullptr, 7, 0, OP_FORCE_PAUSE);
}

// Tells the node to exit
void GameNode::exitGame()
{
   print(nullptr, 7, 0, OP_EXIT_NODE);
}

// Tells the node to clear its screen
void GameNode::clearScreen()
{
   print(nullptr, 7, 0, OP_CLEAR_SCREEN);
}

// Prints centered text on node's screen
void GameNode::center(char* szText, short nColor, short nNewlines)
{
   print(szText, nColor, nNewlines, OP_CENTER_TEXT);
}

// Displays an ANSI file for the node
void GameNode::displayScreen(char* szFileName)
{
   print(szFileName, 7, 0, OP_DISPLAY_ANSI);
}

// Displays an HLP file entry for the node
void GameNode::displayHlp(char* szFileName, char* szEntry, char* szError, bool bPartialMatch)
{
   char szText[180];

   short nTotalLength = 5 + strlen(szFileName) + strlen(szEntry);
   if ( szError == nullptr )
      nTotalLength += 4;
   else
      nTotalLength += strlen(szError);

   if ( nTotalLength > 175 )
      return;

   sprintf(szText, "%s&%s&", szFileName, szEntry);

   if ( szError == nullptr )
      strcat(szText, "none");
   else
      strcat(szText, szError);

   if ( bPartialMatch )
      strcat(szText, "&y");
   else
      strcat(szText, "&n");

   print(szText, 7, 0, OP_DISPLAY_HLP);
}


// Displays a message in a box
void GameNode::textBox(char* szBoxTitle, short nTextColor, short nBoxColor, bool bCenter)
{
   char szText[80], szHolder[50];
   short n;

   if ( strlen(szBoxTitle) < 1 || strlen(szBoxTitle) > 75 )
      return;

   if ( bCenter)
      {
      short nEmpty = 38 - (short)(strlen(szBoxTitle) / 2);
      for (n = 0; n < nEmpty; n++)
         szHolder[n] = ' ';
      szHolder[nEmpty] = '\0';
      print(szHolder, 7, 0);
      }

   for (n = 0; n < (short)(strlen(szBoxTitle) + 4); n++)
      szText[n] = '-';
   szText[0] = '+';
   szText[ strlen(szBoxTitle) + 3 ] = '+';
   szText[ strlen(szBoxTitle) + 4 ] = '\0';

   print(szText, nBoxColor);

   if ( bCenter )
      print(szHolder, 7, 0);
   print((char*)"| ", nBoxColor, 0);
   print(szBoxTitle, nTextColor, 0);
   print((char*)" |", nBoxColor);

   if ( bCenter )
      print(szHolder, 7, 0);
   szText[0] = '+';
   szText[ strlen(szBoxTitle) + 3 ] = '+';
   print(szText, nBoxColor, 2);
}


// Displays a menu option
void GameNode::menuOption(char cKey, char* szText, short nKeyColor, short nArrowColor, short nTextColor)
{
   char szHolder[5];
   sprintf(szHolder, " %c", cKey);
   print(szHolder, nKeyColor, 0);
   print((char*)"  ", nArrowColor, 0);
   print(szText, nTextColor);
}


// Displays underlined text
void GameNode::underline(char* szText, char* szUnderline, short nTextColor, short nUnderColor, bool bCenter)
{
   char* szHolder;

   szHolder = new char[ strlen(szText) + 5 ];
   szHolder[0] = '\0';

   // Make the underline long enough
   while ( strlen(szHolder) < strlen(szText) )
      {
      strcat(szHolder, szUnderline);
      }

   // Chop off extra/uneven characters from the end of the underline
   // (This only happens if szUnderline is more than 1 character long)
   while ( strlen(szHolder) > strlen(szText) )
      {
      szHolder[ strlen(szHolder)-1 ] = '\0';
      }

   if ( bCenter )
      {
      center(szText, nTextColor);
      center(szHolder, nUnderColor);
      }
   else
      {
      print(szText, nTextColor);
      print(szHolder, nUnderColor);
      }

   delete[] szHolder;
}



/////////////////////////////////////////////////////////////////////////////////////////
// GameThread methods


GameThread::GameThread(GameNode* aNode, GameServer* aServer)
{
   gn = aNode;
   gs = aServer;
}


// Default cleanup method for a thread.  Over-ride to have something different happen
// upon thread termination.
void GameThread::cleanup()
{
   gn->print(nullptr, LWHITE, 0, OP_COMMAND_PROMPT);
}


// Sets a new prompt for the user and then gets an entire string from the input
// queue for this thread's node.  If no string present, waits until one is.
char* GameThread::getStr(char* szBuffer, char* szPromptText, short nPromptCol, short nMaxLen)
{
   // Display the new prompt
   gn->print(szPromptText, nPromptCol, 0, OP_COMMAND_PROMPT);

   gs->leaveCritical();

   // While there's no input messages waiting, sleep.
   while ( gn->mqInput.isEmpty() )
      {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }

   gs->enterCritical();

   // Retreive the first waiting input message
   InputData idMessage = gn->mqInput.dequeue();

   // If it's an IP_FORCE_EXIT message, shut down this thread.
   if ( idMessage.nType == IP_FORCE_EXIT )
      throw ThreadException();

   // If the message is too long, shorten it.
   if ( strlen(idMessage.szMessage) > (unsigned)nMaxLen && nMaxLen < 190 )
      idMessage.szMessage[nMaxLen] = '\0';

   // Copy the message to the buffer and return it.
   strcpy(szBuffer, idMessage.szMessage);
   return szBuffer;
}


// Sets a new prompt for the user and then gets a single keypress (hotkey).
// May wait for input, depending on value of bWait.
char GameThread::getKey(char* szPromptText, short nPromptCol, bool bWait)
{
   // Display the new prompt
   gn->print(szPromptText, nPromptCol, 0, OP_HOTKEY_PROMPT);

   if ( bWait )
      gs->leaveCritical();

   // While there's no input, if bWait is true, pause until input is present.
   // But if there's no input and bWait is false, return 0.
   while ( gn->mqInput.isEmpty() )
      {
      if ( bWait )
         std::this_thread::sleep_for(std::chrono::milliseconds(50));
      else
         return 0;
      }

   if ( bWait )
      gs->enterCritical();

   // Retreive the first waiting input message
   InputData idMessage = gn->mqInput.dequeue();

   // If it's an IP_FORCE_EXIT message, shut down this thread.
   if ( idMessage.nType == IP_FORCE_EXIT )
      throw ThreadException();

   // Return the keystroke, which will be the first character of the input message.
   // Make it lowercase first.
   return tolower( idMessage.szMessage[0] );
}

// Tells the thread to pause for a keystroke
void GameThread::pause(bool bCenter)
{
   char szText[70];
   szText[0] = '\0';
   if ( bCenter )
      sprintf(szText, "%-27s", " ");
   strcat(szText, "[Hit any key to continue]");
   getKey(szText);
}


// Static method used to launch a dedicated-node thread for temporarily handling a node's IO.
void GameThread::launch(GameThread* aThread)
{
	// Don't allow multiple threads for a single node.
	if ( !aThread->gn->bHasThread )
	{
		trivlog("trivsrv: launching thread for %s\n", aThread->gn->szAlias);
		#ifdef _WIN32
		_beginthread(handleThread, 4096, aThread);
		#else
		std::thread t(handleThread, static_cast<void*>(aThread));
		t.detach();
		#endif
	}
	else
	{
		trivlog("trivsrv: thread launch BLOCKED for %s (bHasThread=true)\n", aThread->gn->szAlias);
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// Functions

// handleThread is a friend function of GameThread(), hence it can access private
// GameThread members.
void handleThread(void* vArg)
{
   GameThread* gt = static_cast<GameThread*>(vArg);

   // Have to call randomize() once for each thread in BC5, apparently.
   myRandomize();

   gt->gs->enterCritical();
   gt->gn->bHasThread = true;

   // Run the thread, until it either exits naturally or gets interrupted (whether
   // intentionally by the server, or via a player hang-up)
   try
      {
      gt->run();
      }
   catch ( GameThread::ThreadException )
      {
      // No need to actually do anything here; we simply needed to break
      // out of run().
      }
   catch ( const std::exception& e )
      {
      trivlog("trivsrv: thread exception: %s\n", e.what());
      }
   catch ( ... )
      {
      trivlog("trivsrv: unknown thread exception\n");
      }

   gt->cleanup();
   gt->gn->bHasThread = false;
   gt->gs->leaveCritical();
   delete gt;
   #ifdef _WIN32
   _endthread();
   #endif
   // On Linux, the thread function simply returns, ending the thread naturally.
}


// Starting point for the round-ticker thread
void startRoundThread(void* gsVoidServer)
{
   GameServer* gsServer = static_cast<GameServer*>(gsVoidServer);

   // Have to call randomize() once for each thread in BC5, apparently.
   myRandomize();

   while ( gsServer->nPlayersInGame > 0 )
      {
      std::this_thread::sleep_for(std::chrono::milliseconds(ROUND_TIME));
      gsServer->enterCritical();
      try
         {
         gsServer->doorRound( time(nullptr) );
         }
      catch ( const std::exception& e )
         {
         trivlog("trivsrv: round thread exception: %s\n", e.what());
         }
      catch ( ... )
         {
         trivlog("trivsrv: round thread unknown exception\n");
         }
      gsServer->leaveCritical();
      }
   trivlog("trivsrv: round thread exiting (nPlayersInGame = %d)\n", gsServer->nPlayersInGame);
}


// Returns the length of a closed file, or 0 if invalid filename.
long getFileLength(char* filespec)
{
	long fileLength = 0;
	try
	{
		fileLength = static_cast<long>(std::filesystem::file_size(filespec));
	}
	catch (const std::exception& exc)
	{
	}
	catch (...)
	{
	}
	return fileLength;
}


// Deletes a file with the given name.  Wildcards are supported.
void myDeleteFile(char* szName)
{
	const std::string name(szName);
	std::filesystem::path dir = "."; // Current directory
	for (const auto& entry : std::filesystem::directory_iterator(dir))
	{
		if (entry.path().filename().string().find(name) != std::string::npos)
			std::filesystem::remove(entry);
	}
}


// Copies a file.  Wildcards are not supported.
void myCopyFile(char* szSource, char* szDestination, BOOL bFailIfThere)
{
	if (bFailIfThere == TRUE && std::filesystem::exists(szDestination))
		return;
	std::filesystem::copy_file(szSource, szDestination,
		std::filesystem::copy_options::overwrite_existing);
}


// Returns a random number between nMin and nMax.  Be sure to call myRandomize()
// before using *in a given thread*.
short dice(short nMin, short nMax)
{
	std::random_device rd; // Obtain a random seed from the OS
	std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()
	std::uniform_int_distribution<> distrib(nMin, nMax); // Define the range

	return (short)distrib(gen); // Generate and return the random number
}


// Seeds the random number generator, and discards first few rolls.
void myRandomize()
{
	// No-op on Linux: using std::random_device + mt19937 in dice() instead
}

// Does "intelligent search" ON first parameter, FOR second parameter
bool wordSearch(char* szString1, char* szString2)
{
   char szToSearch[162];
   char szToFind[162];

   if ( strlen(szString1) > 160 )
      szString1[160] = '\0';
   if ( strlen(szString2) > 160 )
      szString2[160] = '\0';

   sprintf(szToSearch, " %s", szString1);
   sprintf(szToFind, " %s", szString2);

   strlwr(szToSearch);
   strlwr(szToFind);

   if ( strstr(szToSearch, szToFind) != nullptr )
      return true;
   else
      return false;
}


// Function called when new fails
void badNew_Gamesrv()
{
	exit(0);
}
