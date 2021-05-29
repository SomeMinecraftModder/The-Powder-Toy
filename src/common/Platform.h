#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>
#include <vector>

namespace Platform
{
	char *ExecutableName();
	void DoRestart(bool saveTab, bool disableSignals);
	void OpenLink(std::string uri);
	void Millisleep(long int t);
	unsigned long GetTime();
	void LoadFileInResource(int name, int type, unsigned int& size, const char*& data);
	bool RegisterExtension();
	bool ShowOnScreenKeyboard(const char *str, bool autoCorrect = true);
	void GetOnScreenKeyboardInput(char * buff, int buffSize, bool autoCorrect = true);
	bool IsOnScreenKeyboardShown();
	void Vibrate(int milliseconds);
	int CheckLoadedPtsave();

	// Filesystem stuff (TODO: add more stuff)
	void MakeDirectory(std::string dir);
	std::vector<std::string> DirectorySearch(std::string directory, std::string search, std::vector<std::string> extensions);

#ifdef WIN
	std::string WinNarrow(const std::wstring& source);
	std::wstring WinWiden(const std::string& source);
#endif
}


#ifdef ANDROID
	#define IDENT_PLATFORM "ANDROID"
#elif WIN
#ifdef _64BIT
	#define IDENT_PLATFORM "WIN64"
#else
	#define IDENT_PLATFORM "WIN32"
#endif
#elif defined MACOSX
	#define IDENT_PLATFORM "MACOSX"
#elif defined LIN
#ifdef _64BIT
	#define IDENT_PLATFORM "LIN64"
#else
	#define IDENT_PLATFORM "LIN32"
#endif
#else
	#define IDENT_PLATFORM "UNKNOWN"
#endif

#ifdef BETA
	#define IDENT_RELTYPE "B"
#else
	#define IDENT_RELTYPE "R"
#endif

#if defined X86_SSE3
	#define IDENT_BUILD "SSE3"
#elif defined X86_SSE2
	#define IDENT_BUILD "SSE2"
#elif defined X86_SSE
	#define IDENT_BUILD "SSE"
#else
	#define IDENT_BUILD "NO"
#endif

#endif // PLATFORM_H
