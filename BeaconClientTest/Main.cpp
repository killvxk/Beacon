
#include "../Beacon/Beacon.hpp"
#include <TlHelp32.h>
#include <iostream>

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
	printf("BEACON CLIENT TEST");
	Sleep(1000);
	HANDLE ServerProc = GetProcByName("BeaconServerTest.exe");
	if (!ServerProc)
	{
		std::cout << "Please open server!" << std::endl;
		std::cin.ignore();
		return 1;
	}

	std::cout << "Connecting to server... ";
	Beacon::Client* nClient = new Beacon::Client(L"BeaconPort", ServerProc);
	std::cout << "OK!" << std::endl;
	Beacon::pMessage nMessage;

	while (true)
	{
		std::cout << "IN > ";
		std::string ChatMessage;
		std::getline(std::cin, ChatMessage);
		if (nMessage = nClient->SendExpectReply(ChatMessage))
			std::cout << "RESPONSE > " << (const char*)(nMessage->DataPointer) << std::endl;
		else
			break;
	}
}