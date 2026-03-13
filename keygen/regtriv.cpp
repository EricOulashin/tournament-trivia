#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef LINUX
#include <fstream>
using namespace std;
#else
#include <fstream.h>
#endif

unsigned char encode(unsigned char, long);
unsigned char decode(unsigned char, long);
short getRegName(char*, unsigned char*, short);

#ifdef LINUX
// Linux replacement for strrev (not available in glibc)
char* strrev(char* str)
{
   if (!str) return str;
   int len = strlen(str);
   for (int i = 0; i < len / 2; i++)
      {
      char tmp = str[i];
      str[i] = str[len - 1 - i];
      str[len - 1 - i] = tmp;
      }
   return str;
}
#endif

#ifdef LINUX
int main()
#else
void main()
#endif
{
   char szName[80];
   unsigned char szHolder[500], nSpaceCount = 0, nHighestVal = 0, nTot = 0;
   long lPos = 0, lToSkip;

#ifdef LINUX
   srand(time(NULL));
#else
   randomize();
#endif
   printf("Enter Sysop name   >");
#ifdef LINUX
   fgets(szName, sizeof(szName), stdin);
   // Remove trailing newline from fgets
   size_t szNameLen = strlen(szName);
   if (szNameLen > 0 && szName[szNameLen - 1] == '\n')
      szName[szNameLen - 1] = '\0';
#else
   gets(szName);
#endif

   if ( strlen(szName) < 3 )
      exit(0);

   for ( short n = 0; n < strlen(szName); n++ )
      {
      if ( szName[n] == ' ' )
         nSpaceCount++;
      if ( szName[n] > nHighestVal )
         nHighestVal = szName[n];
      nTot += szName[n]/2;
      }

   szHolder[lPos++] = encode(strlen(szName), 0);
   szHolder[lPos++] = encode(nSpaceCount, 1);
   szHolder[lPos++] = encode(szName[2], 2);
   szHolder[lPos++] = encode(szName[strlen(szName)-1], 3);
   szHolder[lPos++] = encode(nHighestVal, 4);
   szHolder[lPos++] = encode(strlen(szName), 5);
   szHolder[lPos++] = encode(nTot, 6);

   strrev(szName);

   for ( short n = 0; n < strlen(szName); n++ )
      {
      szHolder[lPos] = encode(szName[n], lPos);
      lPos++;
#ifdef LINUX
      szHolder[lPos] = rand() % 204 + 50;
#else
      szHolder[lPos] = random(204) + 50;
#endif
      lToSkip = szHolder[lPos] / 50;
      szHolder[lPos] = ~szHolder[lPos];
      lPos++;

      unsigned char nSum = 7;
      for ( short k = 0; k < lToSkip; k++ )
         {
#ifdef LINUX
         szHolder[lPos] = rand() % 250;
#else
         szHolder[lPos] = random(250);
#endif
         nSum += szHolder[lPos]/6;
         lPos++;
         }
      szHolder[lPos++] = nSum;
      }

   fstream myFile;
#ifdef LINUX
   myFile.open("ttreg.dat", ios::binary | ios::out | ios::trunc );
#else
   myFile.open("e:\\ttreg.dat", ios::binary | ios::out | ios::trunc );
#endif
   myFile.write((char*)szHolder, lPos);
   myFile.close();

#ifdef LINUX
   myFile.open("regs.log", ios::out | ios::app);
#else
   myFile.open("e:\\doors\\trivia\\regs.log", ios::out | ios::app);
#endif
   myFile.write(strrev(szName), strlen(szName));
   char szText[120], szFullDate[12];
#ifdef LINUX
   {
      time_t t = time(NULL);
      struct tm* tm_info = localtime(&t);
      strftime(szFullDate, sizeof(szFullDate), "%m/%d/%y", tm_info);
   }
#else
   _strdate(szFullDate);
#endif
   sprintf(szText, "\n  Registered on %s\n", szFullDate);
   myFile.write(szText, strlen(szText));
   sprintf(szText, "  File size: %ld\n  First ten bytes: ", lPos);
   for ( short n = 0; n < 10; n++ )
      {
      sprintf(szFullDate, "%u ", szHolder[n]);
      strcat(szText, szFullDate);
      }
   strcat(szText, "\n\n");
   myFile.write(szText, strlen(szText));
   printf("\nFile created and logged.\n");

   char szCheckName[80];
   short nStatus = getRegName(szCheckName, szHolder, lPos);
   printf("  -> Reg status: %d; %s.\n\n", nStatus, szCheckName);

#ifdef LINUX
   return 0;
#endif
}


unsigned char encode(unsigned char nVal, long lPos)
{
   if ( lPos % 2 == 0 )
      return ~(nVal + (lPos%8)*3);
   else
      return nVal + ~((unsigned char)(lPos+23)) + ((lPos*3+7) % ((lPos % 17) + 2));
}

unsigned char decode(unsigned char nVal, long lPos)
{
   if ( lPos % 2 == 0 )
      return ~(unsigned char)nVal - (lPos%8)*3;
   else
      return nVal - ~((unsigned char)(lPos+23)) - ((lPos*3+7) % ((lPos % 17) + 2));
}

// prior to running, verify length is over 5 bytes
short getRegName(char* szNameBuffer, unsigned char* szHolder, short nLength)
{
   unsigned char nLen1, nLen2, nHighVal, nChar2, nLastChar, nSpaceCount, nTot;
   long lPos = 0;
   class BadFile
      {
      public:
         short nReason, nTwo, nThree;
         BadFile(short n1, short n2=0, short n3=0) {nReason = n1;nTwo=n2;nThree=n3;}
      };

   try
      {
      nLen1 = decode(szHolder[lPos++], 0);
      nSpaceCount = decode(szHolder[lPos++], 1);
      nChar2 = decode(szHolder[lPos++], 2);
      nLastChar = decode(szHolder[lPos++], 3);
      nHighVal = decode(szHolder[lPos++], 4);
      nLen2 = decode(szHolder[lPos++], 5);
      nTot = decode(szHolder[lPos++], 6);

      if ( nLen1 != nLen2 )
         throw BadFile(1, nLen1, nLen2);

      short n = 0;
      while ( lPos < nLength && n < 80 )
         {
         szNameBuffer[n++] = decode(szHolder[lPos], lPos);
         lPos++;
         long lToSkip = (unsigned char)(~szHolder[lPos++]) / 50;
         short nSum = 7;
         for ( short k = 0; k < lToSkip; k++ )
            {
            nSum += szHolder[lPos++]/6;
            }

         if ( szHolder[lPos++] != nSum )
            throw BadFile(7, szHolder[lPos++], nSum);
         }

      szNameBuffer[n] = '\0';
      strrev(szNameBuffer);

      short nMyHigh = 0;
      for ( short n = 0; n < strlen(szNameBuffer); n++ )
         {
         if ( szNameBuffer[n] == ' ' )
            nSpaceCount--;
         if ( szNameBuffer[n] > nMyHigh )
            nMyHigh = szNameBuffer[n];
         nTot -= szNameBuffer[n]/2;
         }

      if ( strlen(szNameBuffer) != nLen1 )
         throw BadFile(2, strlen(szNameBuffer), nLen1);
      if ( nHighVal != nMyHigh )
         throw BadFile(3, nHighVal, nMyHigh);
      if ( nSpaceCount != 0 )
         throw BadFile(4, nSpaceCount);
      if ( szNameBuffer[2] != nChar2 )
         throw BadFile(5, szNameBuffer[2], nChar2);
      if ( szNameBuffer[strlen(szNameBuffer)-1] != nLastChar )
         throw BadFile(6, szNameBuffer[strlen(szNameBuffer)-1], nLastChar);
      if ( nTot != 0 )
         throw BadFile(8, nTot);

      return 1;
      }

   catch ( BadFile bf )
      {
      szNameBuffer[0] = '\0';
      printf(" >> FAIL on %d because %d != %d << ", bf.nReason, bf.nTwo, bf.nThree);
      return 0;
      }
}


