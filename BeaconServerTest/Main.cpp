
#include "../Beacon/Beacon.hpp"
#include <TlHelp32.h>
#include <iostream>
#include <algorithm>

HANDLE GetProcByName(LPSTR procName)
{
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (Process32First(snapshot, &entry) == TRUE)
		while (Process32Next(snapshot, &entry) == TRUE)
			if (_stricmp(entry.szExeFile, procName) == 0)
				return OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
	return NULL;
}

int main()
{
	printf("BEACON SERVER TEST\n");
	HANDLE ClientProc;
	std::cout << "Waiting for client... ";
	while (!(ClientProc = GetProcByName("BeaconClientTest.exe")));
	std::cout << "OK!" << std::endl << "Opening server... ";
	Beacon::Server* nServer = new Beacon::Server(L"BeaconPort", ClientProc);
	std::cout << "OK!" << std::endl;
	Beacon::pMessage OutMessage = Beacon::GCMessage::New();
	while (nServer->Fetch(OutMessage))
	{
		std::cout << "STOPPING...";
		if (OutMessage->Command == LPC_COMMAND_STOP)
			return nServer->Reply(OutMessage, "STOP");
		const char* InMessage = (const char*)(OutMessage->DataPointer);
		std::string InString = InMessage;
		std::cout << "GOT > " << InString << std::endl;
		std::reverse(InString.begin(), InString.end());
		if (!nServer->Reply(OutMessage, InString))
			break;
	}
	return 0;
}