#include <cctype>
#include <cstring>
#include <cstdlib>

int strcmpi (const char * str1, const char * str2 )
{
	/*
	if (str1 == nullptr)
		return -1;
	else if (str2 == nullptr)
		return 1;
	*/

	const size_t len1 = strlen(str1);
	const size_t len2 = strlen(str2);
	char *str1Upper;
	char *str2Upper;
	int compRes;
	size_t i;
	str1Upper = (char*)malloc(len1+1);
	for (i = 0; i < len1; ++i)
		str1Upper[i] = toupper(str1[i]);
	str1Upper[len1] = '\0';
	str2Upper = (char*)malloc(len2+1);
	for (i = 0; i < len2; ++i)
		str2Upper[i] = toupper(str2[i]);
	str2Upper[len2] = '\0';
	compRes = strcmp(str1Upper, str2Upper);
	free(str1Upper);
	free(str2Upper);
	return compRes;
}

char *strlwr(char *str)
{
	const size_t len = strlen(str);
	size_t i;
	for (i = 0; i < len; ++i)
		str[i] = tolower(str[i]);
	return str;
}