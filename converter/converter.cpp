//
/**
* @file converter.cpp
*
* @brief Time zone converter
*
* @author Evgenii Onopchenko
* @version 1.0
*/


#include "stdafx.h"
#include <windows.h>
#include <winhttp.h>
#include <ctime>
#include <string>

#pragma comment(lib,"winhttp.lib")

#define MAX_HTTP_REQUEST_COUNT 50	///< Max server request attempt count
#define BUFFER_SIZE 128				///< Transitional buffers size
#define BARS_IN_WINDOW 48			///< How many bars can contain console window

///< File names
const TCHAR Str_InputFile[] = { "input.csv\0" };
const TCHAR Str_OutputFile[] = { "output.csv\0" };

///< HTTP request parameters
const LPCWSTR Str_UserAgent = { L"Time Zone API/1.0\0" };
const LPCWSTR Str_TimeZoneServer = { L"api.timezonedb.com\0" };
const TCHAR Str_TimeZoneRequest[] = { "/v2/get-time-zone?key=LA7QLFGL6YHU&format=xml&by=position&lat=%s&lng=%s\0" };

///< XML tag tokens
const TCHAR Str_ZoneNameOpenTagToken[] = { "<zoneName>\0" };
const TCHAR Str_ZoneNameCloseTagToken[] = { "</zoneName>\0" };
const TCHAR Str_GmtOffsetOpenTagToken[] = { "<gmtOffset>\0" };
const TCHAR Str_GmtOffsetCloseTagToken[] = { "</gmtOffset>\0" };

/**
* @brief <b> Get file lines count </b>
* @param handle - File handle
* @return File lines count
*/
uint32_t GetFileLinesCount(HANDLE handle)
{
	DWORD count = 0;
	TCHAR chr = 0;
	DWORD lines = 0;

	do
	{
		if (chr == '\n')
		{
			lines++;
		}
		ReadFile(handle, &chr, 1, &count, NULL);
	} while (count != 0);

	SetFilePointer(handle, 0, 0, FILE_BEGIN);

	return lines;
}

/**
* @brief <b> Http connection resource disposal </b>
* @param hSES - HTTP session handle
* @param hCON - HTTP connection handle
* @param hREQ - HTTP request handle
*/
void HttpDispose(HINTERNET hSES, HINTERNET hCON, HINTERNET hREQ)
{
	if(hSES != NULL) WinHttpCloseHandle(hSES);
	if(hCON != NULL) WinHttpCloseHandle(hCON);
	if(hREQ != NULL) WinHttpCloseHandle(hREQ);
}


/**
* @brief <b> Main Loop </b>
*/
int main()
{
	/// Open input file, save handle
	HANDLE hIStream = CreateFile(Str_InputFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIStream == INVALID_HANDLE_VALUE)
	{
		printf("File opening error %d (%s)\r\n", GetLastError(), Str_InputFile);
		system("PAUSE");
		return 0;
	}

	/// Get file lines count
	uint32_t lines = GetFileLinesCount(hIStream);

	/// Open output file, save handle
	HANDLE hOStream = CreateFile(Str_OutputFile, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOStream == INVALID_HANDLE_VALUE)
	{
		printf("File opening error %d (%s)\r\n", GetLastError(), Str_OutputFile);
		system("PAUSE");
		return 0;
	}

	/// Get input file size
	DWORD fileSize = GetFileSize(hIStream, NULL);

	/// Allocate input buffer and pointer
	BYTE inputBuffer[BUFFER_SIZE];

	printf("\r\n\r\n>>>   Time zone converter v1.0\r\n\r\nFile \"input.csv\" opened\r\n\r\n");
	printf("\r0%%\t[");
	for (uint_fast8_t i = 0; i < BARS_IN_WINDOW; i++)
	{
		printf(" ");
	}
	printf("]\t%u / %u", 0, lines);

	/// Execute until the end of the file
	uint_fast16_t lineNumber = 0;
	bool reading = true;
	while(reading)
	{
		memset(inputBuffer, 0, BUFFER_SIZE);
		BYTE* ptr = inputBuffer;

		/// Read until the end of current line
		while (((ptr - inputBuffer) < 2) || memcmp(ptr - 2, "\r\n", 2))
		{
			DWORD count = 0;
			ReadFile(hIStream, ptr, 1, &count, NULL);

			/// If the end of the file has been reached, interrupt the cycle
			if (count == 0)
			{
				break;
			}

			ptr += count;
		}

		/// Check if nothing has been read
		if ((ptr - inputBuffer) == 0)
		{
			reading = false;
			continue;
		}

		/// Set the pointer to the beginning
		ptr = inputBuffer;

		/// Temporary variable
		TCHAR tmp = 0;

		/// Parse UTC time, fill the structure
		tm time;
		tmp = ptr[4]; ptr[4] = '\0'; time.tm_year = std::stoi((TCHAR *)ptr) - 1900; ptr[4] = tmp; ptr += 5;
		tmp = ptr[2]; ptr[2] = '\0'; time.tm_mon = std::stoi((TCHAR *)ptr) - 1; ptr[2] = tmp; ptr += 3;
		tmp = ptr[2]; ptr[2] = '\0'; time.tm_mday = std::stoi((TCHAR *)ptr); ptr[2] = tmp; ptr += 3;
		tmp = ptr[2]; ptr[2] = '\0'; time.tm_hour = std::stoi((TCHAR *)ptr); ptr[2] = tmp; ptr += 3;
		tmp = ptr[2]; ptr[2] = '\0'; time.tm_min = std::stoi((TCHAR *)ptr); ptr[2] = tmp; ptr += 3;
		tmp = ptr[2]; ptr[2] = '\0'; time.tm_sec = std::stoi((TCHAR *)ptr); ptr[2] = tmp; ptr += 3;

		/// Parse latitude, longitude, fill request
		TCHAR* base = (TCHAR *)ptr;

		/// Terminate latitude
		char* charPtr = strstr((TCHAR *)ptr, ",");
		if (charPtr == NULL)
		{
			printf("Input file structure error\r\n");
			system("PAUSE");
			return 0;
		}
		BYTE latTerm = charPtr - (char *)base;
		ptr[latTerm] = '\0';

		/// Terminate longitude
		charPtr = strstr((TCHAR *)&ptr[latTerm + 1], "\r");
		if (charPtr == NULL)
		{
			printf("Input file structure error\r\n");
			system("PAUSE");
			return 0;
		}
		BYTE lngTerm = charPtr - (char *)base;
		ptr[lngTerm] = '\0';

		/// Build request
		size_t length = strlen(Str_TimeZoneRequest) + strlen((const char *)ptr) + strlen((const char *)&ptr[latTerm + 1]);
		TCHAR* tmpbuf = new TCHAR[length];
		sprintf_s(tmpbuf, length, Str_TimeZoneRequest, ptr, &ptr[latTerm + 1]);

		/// Convert request string
		wchar_t wtext[BUFFER_SIZE];
		mbstowcs(wtext, (const char *)tmpbuf, strlen(tmpbuf) + 1);
		LPWSTR request = wtext;

		/// Restore characters
		ptr[latTerm] = ',';
		ptr[lngTerm] = '\r';

		for (uint_fast8_t attempts = 0; attempts < MAX_HTTP_REQUEST_COUNT; attempts++)
		{
			/// Open HTTP session, save handle
			HINTERNET hHttpSession = WinHttpOpen(Str_UserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
			if (hHttpSession == NULL)
			{
				printf("HTTP session initialization error %d\r\n", GetLastError());
				system("PAUSE");
				return 0;
			}

			/// Connect to the timezone API server
			HINTERNET hConnection = WinHttpConnect(hHttpSession, Str_TimeZoneServer, INTERNET_DEFAULT_HTTP_PORT, 0);
			if (hConnection == NULL)
			{
				printf("Timezone API server is unavailable %d\r\n", GetLastError());
				system("PAUSE");
				return 0;
			}

			/// Create an HTTP Request handle
			HINTERNET hRequest = WinHttpOpenRequest(hConnection, L"GET", request, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_REFRESH);

			/// Repeat the request if an error occurred
			if (hRequest == NULL)
			{
				HttpDispose(hHttpSession, hConnection, NULL);
				continue;
			}

			/// Send request
			if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) == FALSE)
			{
				HttpDispose(hHttpSession, hConnection, hRequest);
				continue;
			}

			/// Receive response
			if (WinHttpReceiveResponse(hRequest, NULL) == FALSE)
			{
				HttpDispose(hHttpSession, hConnection, hRequest);
				continue;
			}

			DWORD count = 0;

			/// Check for available data
			if (WinHttpQueryDataAvailable(hRequest, &count) == NULL)
			{
				HttpDispose(hHttpSession, hConnection, hRequest);
				continue;
			}

			/// Allocate buffer for server response
			BYTE *responseBuffer = new BYTE[count + 1];
			memset(responseBuffer, 0, count + 1);

			/// Read response data
			DWORD downloaded = 0;
			if (WinHttpReadData(hRequest, (LPVOID)responseBuffer, count, &downloaded) == NULL)
			{
				HttpDispose(hHttpSession, hConnection, hRequest);
				continue;
			}

			/// Find time zone name open tag token
			const char* zoneNameOpenTagToken = strstr((const char *)responseBuffer, Str_ZoneNameOpenTagToken);
			if (zoneNameOpenTagToken == NULL)
			{
				HttpDispose(hHttpSession, hConnection, hRequest);
				continue;
			}
			
			/// Remember time zone name position
			const char* pTimeZoneName = zoneNameOpenTagToken + strlen(Str_ZoneNameOpenTagToken);

			/// Find time zone name close tag token
			char* zoneNameCloseTagToken = (char *)strstr((const char *)pTimeZoneName, Str_ZoneNameCloseTagToken);
			if (zoneNameCloseTagToken == NULL)
			{
				HttpDispose(hHttpSession, hConnection, hRequest);
				continue;
			}

			/// Terminate time zone name
			*zoneNameCloseTagToken = '\0';

			/// Find time zone name open tag token
			const char* gmtOpenTagToken = strstr((const char *)(zoneNameCloseTagToken + 1), Str_GmtOffsetOpenTagToken);
			if (gmtOpenTagToken == NULL)
			{
				HttpDispose(hHttpSession, hConnection, hRequest);
				continue;
			}

			/// Remember time zone name position
			const char* pGmtOffset = gmtOpenTagToken + strlen(Str_GmtOffsetOpenTagToken);

			/// Find time zone name close tag token
			char* gmtCloseTagToken = (char *)strstr((const char *)pGmtOffset, Str_GmtOffsetCloseTagToken);
			if (gmtCloseTagToken == NULL)
			{
				HttpDispose(hHttpSession, hConnection, hRequest);
				continue;
			}

			/// Terminate time zone name
			*gmtCloseTagToken = '\0';

			/// Calculate time adjustment
			time_t timeStamp = mktime(&time);
			time_t correctedTimeStamp = timeStamp + std::stoi(pGmtOffset);
			tm* correctedTime = localtime(&correctedTimeStamp);

			/// Form the time zone string
			length = strlen(pTimeZoneName) + 24;
			TCHAR* timeZoneString = new TCHAR[length];
			sprintf_s(timeZoneString, length, ",%s,%04u-%02u-%02uT%02u:%02u:%02u\r\n", pTimeZoneName, 
				correctedTime->tm_year + 1900,
				correctedTime->tm_mon + 1,
				correctedTime->tm_mday,
				correctedTime->tm_hour,
				correctedTime->tm_min,
				correctedTime->tm_sec);

			WriteFile(hOStream, inputBuffer, strlen((const TCHAR*)inputBuffer) - 2, &count, NULL);
			WriteFile(hOStream, timeZoneString, strlen(timeZoneString), &count, NULL);

			HttpDispose(hHttpSession, hConnection, hRequest);

			break;
		}

		uint_fast8_t percent = 100 * lineNumber / lines;
		printf("\r%u%%\t[", percent);

		uint_fast8_t bars = percent == 0 ? 0 : (BARS_IN_WINDOW * percent) / 100;

		for (uint_fast8_t i = 0; i < bars; i++)
		{
			printf("=");
		}

		for (uint_fast8_t i = bars; i < BARS_IN_WINDOW; i++)
		{
			printf(" ");
		}

		printf("]\t%u / %u", lineNumber, lines);

		lineNumber++;
	}

	printf("\r100%%\t[");
	for (uint_fast8_t i = 0; i < BARS_IN_WINDOW; i++)
	{
		printf("=");
	}
	printf("]\t%u / %u\r\n\r\nConversion completed\r\n\r\nFile \"output.csv\" saved\r\n\r\n", lines, lines);

	/// Close files, dispose resources
	CloseHandle(hIStream);
	CloseHandle(hOStream);

	/// Wait for anykey
	system("PAUSE");

    return 0;
}

