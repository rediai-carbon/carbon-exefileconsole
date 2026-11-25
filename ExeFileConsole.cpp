// Copyright © 2026 CCP ehf.
// This is a launcher for a windows application.  It allows us to run it from a command shell
// without spawning a new process.
// It is actually name agnostic, so rename the first part of it to match the windows app 
// and it will work.

#include "stdafx.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <locale>

std::wstring GetModuleName()
{
	wchar_t buf[ MAX_PATH+1 ];
	DWORD len = GetModuleFileName( 0, buf, _countof( buf ) );
	return std::wstring( buf, len );
}


int _tmain( int argc, _TCHAR* argv[] )
{
	std::wstring cmd = GetCommandLine();
	std::wstring module = GetModuleName();
	std::wstring exeName;

	//strip off the console part of the name:
	std::wstring::size_type ix = module.find( L"Console" );
	if( ix != std::wstring::npos )
	{
		exeName = module.replace( ix, strlen("Console"), L"" );
	}
	
	//find basename minus the .exe
	std::wstring::size_type found = exeName.rfind( L'\\' );
	std::wstring baseName = exeName.substr( found+1, exeName.size()-(found+1)-4 );

	//special signal for ExeFile.exe to go into inherit mode for console
	SetEnvironmentVariable( L"EXEFILE_INHERIT", L"" );

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );
	si.lpDesktop = (wchar_t*)L"";
	si.lpTitle = (wchar_t*)baseName.c_str();

	//pass on whatever it is we are using as stdandard IO
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

	wchar_t *tmp = new wchar_t[ cmd.size()*2 ];
	wcscpy_s( tmp, cmd.size()*2, cmd.c_str() );
	BOOL ok = CreateProcess( exeName.c_str(), 
							 tmp,
							 0, 
							 0,
							 TRUE, //inherit handles (std IO)
							 0,
							 0, 
							 0,
							 &si, 
							 &pi );
	delete [] tmp;
	if( !ok )
	{
		return GetLastError();
	}

	CloseHandle( pi.hThread );
	DWORD waited = WaitForSingleObject( pi.hProcess, INFINITE );

	if( waited == WAIT_FAILED ) 
	{
		DWORD err = GetLastError();
		CloseHandle( pi.hProcess );
		return err;
	}

	DWORD exitCode;
	ok = GetExitCodeProcess(pi.hProcess, &exitCode);
	if( !ok ) 
	{
		DWORD err = GetLastError();
		CloseHandle( pi.hProcess );
		return err;
	}

	CloseHandle( pi.hProcess );
	return exitCode;
}

#elif defined(__linux__)
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

int main(int argc, char** argv)
{
	char ownPath[4096];
	ssize_t ownLength = readlink("/proc/self/exe", ownPath, sizeof(ownPath) - 1);
	if (ownLength <= 0)
	{
		std::fprintf(stderr, "ExeFileConsole: could not resolve launcher path\n");
		return errno ? errno : ENOENT;
	}
	ownPath[ownLength] = '\0';

	std::string ownExecutable(ownPath);
	std::string::size_type slash = ownExecutable.find_last_of('/');
	std::string directory = slash == std::string::npos ? std::string() : ownExecutable.substr(0, slash + 1);
	std::string baseName = slash == std::string::npos ? ownExecutable : ownExecutable.substr(slash + 1);
	std::string::size_type consoleAt = baseName.find("Console");
	if (consoleAt == std::string::npos)
	{
		std::fprintf(stderr, "ExeFileConsole: launcher name does not contain Console\n");
		return ENOENT;
	}
	baseName.replace(consoleAt, std::strlen("Console"), "");
	std::string executable = directory + baseName;

	if (setenv("EXEFILE_INHERIT", "", 1) != 0)
	{
		std::fprintf(stderr, "ExeFileConsole: could not set EXEFILE_INHERIT\n");
		return errno;
	}

	pid_t child = 0;
	int spawned = posix_spawn(&child, executable.c_str(), nullptr, nullptr, argv, environ);
	if (spawned != 0)
	{
		std::fprintf(stderr, "ExeFileConsole: exec failed: %s\n", std::strerror(spawned));
		return spawned;
	}

	int status = 0;
	pid_t waited = 0;
	do
	{
		waited = waitpid(child, &status, 0);
	} while (waited < 0 && errno == EINTR);
	if (waited < 0)
	{
		return errno;
	}
	if (WIFEXITED(status))
	{
		return WEXITSTATUS(status);
	}
	if (WIFSIGNALED(status))
	{
		return 128 + WTERMSIG(status);
	}
	return 1;
}

#else
#error ExeFileConsole has no launcher for this platform
#endif
