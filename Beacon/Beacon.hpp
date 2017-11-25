
/*

	BEACON - HEADER ONLY (.HPP)
	INTERPROCESS COMMUNICATIONS (IPC) FOR WINDOWS OPERATING SYSTEMS
	USING ADVANCED LOCAL PROCEDURE CALLS (ALPCS)

	written by LoukaMB,
	GitHub: https://github.com/LoukaMB

*/

#include <Windows.h>
#include <string>
#include "ntdll.h"
#pragma comment(lib, "ntdll")

#define MAX_LPC_DATA 0x120
#define LPC_COMMAND_REQUEST_NOREPLY  0x00000000
#define LPC_COMMAND_REQUEST_REPLY    0x00000001
#define LPC_COMMAND_STOP             0x00000002
#ifndef InitializeMessageHeader
#define InitializeMessageHeader(ph, l, t)                              \
{                                                                      \
    (ph)->u1.s1.TotalLength      = (USHORT)(l);                        \
    (ph)->u1.s1.DataLength       = (USHORT)(l - sizeof(PORT_MESSAGE)); \
    (ph)->u2.s2.Type             = (USHORT)(t);                        \
    (ph)->u2.s2.DataInfoOffset   = 0;                                  \
    (ph)->ClientId.UniqueProcess = NULL;                               \
    (ph)->ClientId.UniqueThread  = NULL;                               \
    (ph)->MessageId              = 0;                                  \
    (ph)->ClientViewSize         = 0;                                  \
}
#endif


namespace Beacon
{

	typedef struct _Message
	{
		PORT_MESSAGE Header;
		ULONG Command;
		PVOID DataPointer;
	} Message, *pMessage;

	class GCMessage
	{
	public:
		static Beacon::pMessage New()
		{
			return new Beacon::Message;
		}
	};

	class Server
	{
	public:
		LPCWSTR OpenPortName;
		HANDLE LpcPortHandle;
		HANDLE LpcProcess;
		HANDLE ServerHandle;

		BOOL Close()
		{
			if (ServerHandle)
				NtClose(ServerHandle);
			return FALSE;
		}

		BOOL Fetch(Beacon::pMessage Out)
		{
			if (!Out) return FALSE;
			if (NT_SUCCESS(NtListenPort(LpcPortHandle, &Out->Header)))
				if (NT_SUCCESS(NtAcceptConnectPort(&ServerHandle, NULL, &Out->Header, TRUE, NULL, NULL)))
					if (NT_SUCCESS(NtCompleteConnectPort(ServerHandle)))
						if (NT_SUCCESS(NtReplyWaitReceivePort(ServerHandle, NULL, NULL, &Out->Header)))
							return TRUE;
			return FALSE;
		}

		BOOL Reply(Beacon::pMessage In, std::string ReplyMessage)
		{
			switch (In->Command)
			{
				case LPC_COMMAND_REQUEST_REPLY:
				{
					LPCSTR CStStr = ReplyMessage.c_str();
					ULONG StrLength = ReplyMessage.length();
					PVOID MemoryBloc;
					if (!(MemoryBloc = VirtualAllocEx(LpcProcess, NULL, StrLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
						return FALSE;
					if (!WriteProcessMemory(LpcProcess, MemoryBloc, CStStr, StrLength, NULL))
						return FALSE;
					In->Command = LPC_COMMAND_REQUEST_NOREPLY;
					In->DataPointer = MemoryBloc;
					if (!NT_SUCCESS(NtReplyPort(LpcPortHandle, &In->Header)))
						return FALSE;
					return TRUE;
				}

				case LPC_COMMAND_STOP:
					return Close();
			}
		}

		BOOL Reply(Beacon::pMessage In, PVOID ReplyStruct, size_t ReplySize)
		{
			switch (In->Command)
			{
			case LPC_COMMAND_REQUEST_REPLY:
			{
				PVOID MemoryBloc;
				if (!(MemoryBloc = VirtualAllocEx(LpcProcess, NULL, ReplySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
					return FALSE;
				if (!WriteProcessMemory(LpcProcess, MemoryBloc, ReplyStruct, ReplySize, NULL))
					return FALSE;
				In->Command = LPC_COMMAND_REQUEST_NOREPLY;
				In->DataPointer = MemoryBloc;
				if (!NT_SUCCESS(NtReplyPort(LpcPortHandle, &In->Header)))
					return FALSE;
				return TRUE;
			}

			case LPC_COMMAND_STOP:
				return Close();
			}
		}

		Server(LPCWSTR _PortName, HANDLE InProcess)
		{
			SECURITY_DESCRIPTOR SecDesc;
			OBJECT_ATTRIBUTES ObjAttr;
			UNICODE_STRING PortName;
			NTSTATUS Status;

			OpenPortName = _PortName;
			if (!InitializeSecurityDescriptor(&SecDesc, SECURITY_DESCRIPTOR_REVISION))
				throw GetLastError();
			if (!SetSecurityDescriptorDacl(&SecDesc, TRUE, NULL, FALSE))
				throw GetLastError();
			RtlInitUnicodeString(&PortName, OpenPortName);
			InitializeObjectAttributes(&ObjAttr, &PortName, NULL, NULL, &SecDesc);
			if (!NT_SUCCESS(NtCreatePort(&LpcPortHandle, &ObjAttr, NULL, sizeof(PORT_MESSAGE) + MAX_LPC_DATA, NULL)))
				throw FALSE;
		}
	};

	class Client
	{
	public:
		LPCWSTR OpenPortName;
		HANDLE LpcPortHandle;
		HANDLE LpcProcess;
		SECURITY_QUALITY_OF_SERVICE SecurityQos;

		BOOL Stop()
		{
			Beacon::pMessage In = Beacon::GCMessage::New();
			InitializeMessageHeader(&In->Header, sizeof(Message), NULL);
			In->Command = LPC_COMMAND_STOP;
			NtRequestPort(LpcPortHandle, &In->Header);
			NtClose(LpcPortHandle);
			delete In;
			return TRUE;
		}

		Beacon::pMessage SendExpectReply(std::string ReplyMessage)
		{
			Beacon::pMessage In = Beacon::GCMessage::New();
			Beacon::pMessage Out = Beacon::GCMessage::New();
			InitializeMessageHeader(&In->Header, sizeof(Message), NULL);
			InitializeMessageHeader(&Out->Header, sizeof(Message), NULL);
			LPCSTR CStStr = ReplyMessage.c_str();
			ULONG StrLength = ReplyMessage.length();
			PVOID MemoryBloc;
			if (!(MemoryBloc = VirtualAllocEx(LpcProcess, NULL, StrLength, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
				return FALSE;
			if (!WriteProcessMemory(LpcProcess, MemoryBloc, CStStr, StrLength, NULL))
				return FALSE;
			In->Command = LPC_COMMAND_REQUEST_REPLY;
			In->DataPointer = MemoryBloc;
			NtRequestWaitReplyPort(LpcPortHandle, &In->Header, &Out->Header);
			delete In;
			return Out;
		}

		Beacon::pMessage SendExpectReply(PVOID ReplyStruct, ULONG ReplySize)
		{
			Beacon::pMessage In = Beacon::GCMessage::New();
			Beacon::pMessage Out = Beacon::GCMessage::New();
			InitializeMessageHeader(&In->Header, sizeof(Message), NULL);
			InitializeMessageHeader(&Out->Header, sizeof(Message), NULL);
			PVOID MemoryBloc;
			if (!(MemoryBloc = VirtualAllocEx(LpcProcess, NULL, ReplySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
				return FALSE;
			if (!WriteProcessMemory(LpcProcess, MemoryBloc, ReplyStruct, ReplySize, NULL))
				return FALSE;
			In->Command = LPC_COMMAND_REQUEST_REPLY;
			In->DataPointer = MemoryBloc;
			NtRequestWaitReplyPort(LpcPortHandle, &In->Header, &Out->Header);
			delete In;
			return Out;
		}

		Client(LPCWSTR _PortName, HANDLE ToProcess)
		{
			UNICODE_STRING PortName;
			ULONG MessageSize = sizeof(Message);

			OpenPortName = _PortName;
			RtlInitUnicodeString(&PortName, _PortName);
			SecurityQos.Length = sizeof(SECURITY_QUALITY_OF_SERVICE);
			SecurityQos.ImpersonationLevel = SecurityImpersonation;
			SecurityQos.EffectiveOnly = FALSE;
			SecurityQos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
			if (!NtConnectPort(&LpcPortHandle, &PortName, &SecurityQos, NULL, NULL, &MessageSize, NULL, NULL))
				throw GetLastError();
		}
	};
}