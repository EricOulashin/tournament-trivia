//#include <fstream.h>
//#include <stdio.h>
#include <fstream>
#include <cstdio>
#include <algorithm> // For std::reverse()
#include <filesystem>
#include "../intrnode/gamesrv.h"
#include "trivia.h"
using std::ifstream;
using std::ios;


RegInfo::RegInfo()
{
   szHolder = NULL;
   nLength = 0;
   szRegdTo[0] = '\0';
   bInit = false;
   load();
}

// Loads the registration file.  Returns true the first time the reg file is loaded, 
// or false otherwise.
bool RegInfo::load()
{
   if ( bInit )
      return false;

   szHolder = nullptr;

   class BadFile { };

   bool retVal = false;

   try
      {
      nLength = (short)getFileLength((char*)"ttreg.dat");
      if ( nLength < 5 || nLength > 2000 )
         return false;

      szHolder = new unsigned char[nLength];   
      ifstream myFile;
      myFile.open("ttreg.dat", ios::binary | ios::in );
      //myFile.read(szHolder, nLength);
      myFile.read((char*)szHolder, nLength); // TODO: Should we do this, or make szHolder just a char*?
      myFile.close();
         
      unsigned char nLen1, nLen2, nHighVal, nChar2, nLastChar, nSpaceCount, nTot;
      long lPos = 0;

      nLen1 = decode(szHolder[lPos++], 0);
      nSpaceCount = decode(szHolder[lPos++], 1);
      nChar2 = decode(szHolder[lPos++], 2);
      nLastChar = decode(szHolder[lPos++], 3);
      nHighVal = decode(szHolder[lPos++], 4);
      nLen2 = decode(szHolder[lPos++], 5);
      nTot = decode(szHolder[lPos++], 6);
   
      if ( nLen1 != nLen2 )
         throw BadFile();
   
      short n = 0;
      while ( lPos < nLength && n < 79 )
         {
         szRegdTo[n++] = decode(szHolder[lPos], lPos);
         lPos++;
         long lToSkip = (unsigned char)(~szHolder[lPos++]) / 50;
         short nSum = 7;
         for ( short k = 0; k < lToSkip; k++ )
            {
            nSum += szHolder[lPos++]/6;
            }

         if ( szHolder[lPos++] != nSum )
            throw BadFile();
         }

      szRegdTo[n] = '\0';
	  // Note: strrev() is deprecated; use std:reverse() instead
      //strrev(szRegdTo);
	  std::reverse(szRegdTo, szRegdTo+strlen(szRegdTo));

      short nMyHigh = 0;
      for ( short n = 0; n < strlen(szRegdTo); n++ )
         {
         if ( szRegdTo[n] == ' ' )
            nSpaceCount--;
         if ( szRegdTo[n] > nMyHigh )
            nMyHigh = szRegdTo[n];
         nTot -= szRegdTo[n]/2;
         }

      if ( strlen(szRegdTo) != nLen1 )
         throw BadFile();
      if ( nHighVal != nMyHigh )
         throw BadFile();
      if ( nSpaceCount != 0 )
         throw BadFile();
      if ( szRegdTo[2] != nChar2 )
         throw BadFile();
      if ( szRegdTo[strlen(szRegdTo)-1] != nLastChar )
         throw BadFile();
      if ( nTot != 0 )
         throw BadFile();
      
      bInit = true;
      retVal = true;
      }

   catch ( const BadFile& bf )
      {
      szRegdTo[0] = '\0';
      nLength = 0;
      //delete szHolder;
	   delete[] szHolder;
      szHolder = NULL;
      }
   catch (const std::filesystem::filesystem_error& exc)
   {
      //delete szHolder;
	  delete[] szHolder;
     szHolder = NULL;
   }

   return retVal;
}



unsigned char RegInfo::decode(unsigned char nVal, long lPos)
{
   if ( lPos % 2 == 0 )
      return ~(unsigned char)nVal - (lPos%8)*3;
   else
      return nVal - ~((unsigned char)(lPos+23)) - ((lPos*3+7) % ((lPos % 17) + 2));
}

char* RegInfo::getRegName()
{
   return szRegdTo;
}


short RegInfo::getMaxCore()
{
   short nNotReg = 395;

   if ( nNotReg % 100 != 95 || nNotReg / 299 > 1 )
      return 10;

   if ( strlen(szRegdTo) > 2 && szHolder != NULL )
      return 32000;
   else
      return nNotReg;
}

short RegInfo::getMaxExtra()
{
   short nNotReg = 155;

   if ( nNotReg % 100 != 55 || nNotReg / 99 > 1 )
      return 10;

   if ( strlen(szRegdTo) > 2 && szHolder != NULL )
      return 32000;
   else
      return nNotReg;
}

void RegInfo::displayRegInfo(Player* pl)
{
   char szText[100];
   // display reg info, in black
   sprintf(szText, "File Length: %d\r\nFirst ten bytes: ", nLength);
   pl->print(szText, BLACK);
   for ( short n = 0; n < 10 && n < nLength; n++ )
      {
      sprintf(szText, "%u ", szHolder[n]);
      pl->print(szText, BLACK, 0);
      }
   pl->newline();
}
