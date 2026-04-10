#include "WBMCPSseServer.h"
#include "WBMCPHttpServer.h"
#include "WidgetBlueprintMCPModule.h"

#include "Sockets.h"
#include "SocketSubsystem.h"

#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Guid.h"

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool SocketWriteAll(FSocket* Socket, const FString& Text)
{
	FTCHARToUTF8 Converter(*Text);
	const uint8* Data = reinterpret_cast<const uint8*>(Converter.Get());
	int32 Total = Converter.Length();
	int32 Sent  = 0;
	while (Sent < Total)
	{
		int32 BytesSent = 0;
		if (!Socket->Send(Data + Sent, Total - Sent, BytesSent) || BytesSent <= 0)
			return false;
		Sent += BytesSent;
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FSession
// ─────────────────────────────────────────────────────────────────────────────

bool FWBMCPSseServer::FSession::SendEvent(const FString& EventType, const FString& Data)
{
	FScopeLock Lock(&WriteLock);
	if (!bActive || !Socket) return false;
	FString Msg = FString::Printf(TEXT("event: %s\ndata: %s\n\n"), *EventType, *Data);
	return SocketWriteAll(Socket, Msg);
}

bool FWBMCPSseServer::FSession::SendPing()
{
	FScopeLock Lock(&WriteLock);
	if (!bActive || !Socket) return false;
	return SocketWriteAll(Socket, TEXT(":\n\n"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Accept loop
// ─────────────────────────────────────────────────────────────────────────────

uint32 FWBMCPSseServer::FAcceptRunnable::Run()
{
	while (Owner.bRunning)
	{
		// Wait up to 0.5s for an incoming connection so we can check bRunning frequently
		if (!Owner.ListenSocket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(0.5)))
			continue;

		if (!Owner.bRunning) break;

		FSocket* Client = Owner.ListenSocket->Accept(TEXT("WBMCP_SSE_Client"));
		if (!Client) continue;

		// Handle on a background thread pool thread
		FWBMCPSseServer* OwnerPtr = &Owner;
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [OwnerPtr, Client]()
		{
			OwnerPtr->HandleConnection(Client);
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Client);
		});
	}
	return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// FWBMCPSseServer — lifecycle
// ─────────────────────────────────────────────────────────────────────────────

FWBMCPSseServer::~FWBMCPSseServer()
{
	Stop();
}

void FWBMCPSseServer::Start(uint32 InPort)
{
	ListenPort = InPort;

	ISocketSubsystem* SS = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	ListenSocket = SS->CreateSocket(NAME_Stream, TEXT("WBMCP_SSE_Listen"), false);
	if (!ListenSocket)
	{
		UE_LOG(LogWidgetBlueprintMCP, Error, TEXT("SSE: failed to create listen socket"));
		return;
	}

	ListenSocket->SetReuseAddr(true);
	ListenSocket->SetNonBlocking(false);

	TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr();
	bool bIsValid = false;
	Addr->SetIp(TEXT("0.0.0.0"), bIsValid);
	Addr->SetPort(static_cast<int32>(ListenPort));

	if (!ListenSocket->Bind(*Addr))
	{
		UE_LOG(LogWidgetBlueprintMCP, Error, TEXT("SSE: failed to bind port %d"), ListenPort);
		SS->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		return;
	}

	if (!ListenSocket->Listen(8))
	{
		UE_LOG(LogWidgetBlueprintMCP, Error, TEXT("SSE: listen() failed on port %d"), ListenPort);
		SS->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
		return;
	}

	bRunning = true;
	AcceptRunnable = new FAcceptRunnable(*this);
	AcceptThread   = FRunnableThread::Create(AcceptRunnable, TEXT("WBMCP_SSE_Accept"), 0, TPri_Normal);

	UE_LOG(LogWidgetBlueprintMCP, Log, TEXT("MCP SSE server listening on http://localhost:%d/sse"), ListenPort);
}

void FWBMCPSseServer::Stop()
{
	if (!bRunning) return;
	bRunning = false;

	// Invalidate and close all open SSE sessions so their heartbeat loops exit
	CloseAllSessions();

	// Close listen socket — unblocks Wait() in the accept thread
	if (ListenSocket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}

	// Wait for accept thread
	if (AcceptThread)
	{
		AcceptThread->WaitForCompletion();
		delete AcceptThread;
		AcceptThread = nullptr;
	}
	delete AcceptRunnable;
	AcceptRunnable = nullptr;

	// Give background connection tasks time to finish.
	// Heartbeat loops sleep up to 0.5s per iteration, so 1.2s is enough for clean exit.
	FPlatformProcess::Sleep(1.2f);
}

// ─────────────────────────────────────────────────────────────────────────────
// Session registry
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FWBMCPSseServer::FSession> FWBMCPSseServer::FindSession(const FString& Id)
{
	FScopeLock Lock(&SessionsLock);
	TSharedPtr<FSession>* Found = Sessions.Find(Id);
	return Found ? *Found : nullptr;
}

void FWBMCPSseServer::RegisterSession(const FString& Id, TSharedPtr<FSession> Session)
{
	FScopeLock Lock(&SessionsLock);
	Sessions.Add(Id, Session);
}

void FWBMCPSseServer::UnregisterSession(const FString& Id)
{
	FScopeLock Lock(&SessionsLock);
	Sessions.Remove(Id);
}

void FWBMCPSseServer::CloseAllSessions()
{
	FScopeLock Lock(&SessionsLock);
	for (auto& Pair : Sessions)
	{
		FScopeLock SessLock(&Pair.Value->WriteLock);
		Pair.Value->bActive = false;
		if (Pair.Value->Socket)
			Pair.Value->Socket->Close();
	}
	Sessions.Empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// HTTP helpers
// ─────────────────────────────────────────────────────────────────────────────

bool FWBMCPSseServer::ReadHttpRequest(FSocket* Socket, FString& OutMethod, FString& OutPath, FString& OutBody)
{
	TArray<uint8> Raw;
	uint8 Chunk[4096];
	int32 BytesRead = 0;
	int32 HeaderEnd = -1;

	const double Deadline = FPlatformTime::Seconds() + 10.0;
	while (FPlatformTime::Seconds() < Deadline)
	{
		if (!Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(200)))
			continue;

		Socket->Recv(Chunk, sizeof(Chunk), BytesRead);
		if (BytesRead <= 0) break;
		Raw.Append(Chunk, BytesRead);

		// Scan for end-of-headers marker \r\n\r\n
		for (int32 i = 0; i <= Raw.Num() - 4; ++i)
		{
			if (Raw[i] == '\r' && Raw[i+1] == '\n' && Raw[i+2] == '\r' && Raw[i+3] == '\n')
			{
				HeaderEnd = i + 4;
				break;
			}
		}
		if (HeaderEnd >= 0) break;
	}

	if (HeaderEnd < 0) return false;

	// Convert header region to FString
	TArray<uint8> HdrBytes(Raw.GetData(), HeaderEnd);
	HdrBytes.Add(0);
	FString Headers = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(HdrBytes.GetData())));

	TArray<FString> Lines;
	Headers.ParseIntoArrayLines(Lines, true);
	if (Lines.Num() == 0) return false;

	// Parse request line: METHOD PATH HTTP/x.x
	TArray<FString> Parts;
	Lines[0].ParseIntoArray(Parts, TEXT(" "), true);
	if (Parts.Num() < 2) return false;
	OutMethod = Parts[0];
	OutPath   = Parts[1];

	// Parse Content-Length
	int32 ContentLength = 0;
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		if (Lines[i].StartsWith(TEXT("Content-Length:"), ESearchCase::IgnoreCase))
		{
			ContentLength = FCString::Atoi(*Lines[i].Mid(15).TrimStartAndEnd());
			break;
		}
	}

	// Read body if there is one
	if (ContentLength > 0)
	{
		int32 BodyAlready = Raw.Num() - HeaderEnd;
		const double BodyDeadline = FPlatformTime::Seconds() + 10.0;
		while (BodyAlready < ContentLength && FPlatformTime::Seconds() < BodyDeadline)
		{
			if (!Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(200)))
				continue;
			Socket->Recv(Chunk, sizeof(Chunk), BytesRead);
			if (BytesRead <= 0) break;
			Raw.Append(Chunk, BytesRead);
			BodyAlready = Raw.Num() - HeaderEnd;
		}

		int32 BodyLen = FMath::Min(ContentLength, Raw.Num() - HeaderEnd);
		TArray<uint8> BodyBytes(Raw.GetData() + HeaderEnd, BodyLen);
		BodyBytes.Add(0);
		OutBody = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(BodyBytes.GetData())));
	}

	return true;
}

bool FWBMCPSseServer::WriteRaw(FSocket* Socket, const FString& Text)
{
	return SocketWriteAll(Socket, Text);
}

FString FWBMCPSseServer::ExtractQueryParam(const FString& Path, const FString& Key)
{
	int32 QPos = INDEX_NONE;
	if (!Path.FindChar(TEXT('?'), QPos)) return FString();

	FString Query = Path.Mid(QPos + 1);
	TArray<FString> Pairs;
	Query.ParseIntoArray(Pairs, TEXT("&"), true);
	for (const FString& Pair : Pairs)
	{
		FString K, V;
		if (Pair.Split(TEXT("="), &K, &V) && K == Key)
			return V;
	}
	return FString();
}

FString FWBMCPSseServer::NewSessionId()
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
}

// ─────────────────────────────────────────────────────────────────────────────
// Connection dispatch
// ─────────────────────────────────────────────────────────────────────────────

void FWBMCPSseServer::HandleConnection(FSocket* Socket)
{
	FString Method, Path, Body;
	if (!ReadHttpRequest(Socket, Method, Path, Body)) return;

	if (Method == TEXT("OPTIONS"))
	{
		HandleOptions(Socket);
	}
	else if (Method == TEXT("GET") && Path.StartsWith(TEXT("/sse")))
	{
		HandleSseGet(Socket);
	}
	else if (Method == TEXT("POST") && Path.StartsWith(TEXT("/message")))
	{
		HandlePost(Socket, Path, Body);
	}
	else
	{
		WriteRaw(Socket, TEXT("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n"));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /sse — keep-alive SSE stream
// ─────────────────────────────────────────────────────────────────────────────

void FWBMCPSseServer::HandleSseGet(FSocket* Socket)
{
	const FString SessionId = NewSessionId();

	// Create session and register before sending headers (POST may arrive fast)
	TSharedPtr<FSession> Session = MakeShared<FSession>();
	Session->Socket  = Socket;
	Session->bActive = true;
	RegisterSession(SessionId, Session);

	// SSE response headers
	WriteRaw(Socket,
		TEXT("HTTP/1.1 200 OK\r\n")
		TEXT("Content-Type: text/event-stream\r\n")
		TEXT("Cache-Control: no-cache\r\n")
		TEXT("Connection: keep-alive\r\n")
		TEXT("Access-Control-Allow-Origin: *\r\n")
		TEXT("X-Accel-Buffering: no\r\n")
		TEXT("\r\n"));

	// Send endpoint event so the client knows where to POST
	FString EndpointUrl = FString::Printf(TEXT("http://localhost:%d/message?sessionId=%s"), ListenPort, *SessionId);
	Session->SendEvent(TEXT("endpoint"), EndpointUrl);

	UE_LOG(LogWidgetBlueprintMCP, Log, TEXT("SSE client connected, session %s"), *SessionId);

	// Heartbeat loop — exits when connection drops or server stops
	double LastPing = FPlatformTime::Seconds();
	while (bRunning)
	{
		FPlatformProcess::Sleep(0.5f);

		double Now = FPlatformTime::Seconds();
		if (Now - LastPing >= 15.0)
		{
			if (!Session->SendPing())
				break; // Connection dropped
			LastPing = Now;
		}
	}

	// Cleanup
	{
		FScopeLock Lock(&Session->WriteLock);
		Session->bActive = false;
	}
	UnregisterSession(SessionId);

	UE_LOG(LogWidgetBlueprintMCP, Log, TEXT("SSE client disconnected, session %s"), *SessionId);
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /message — process JSON-RPC, respond via SSE
// ─────────────────────────────────────────────────────────────────────────────

void FWBMCPSseServer::HandlePost(FSocket* Socket, const FString& Path, const FString& Body)
{
	const FString SessionId = ExtractQueryParam(Path, TEXT("sessionId"));
	TSharedPtr<FSession> Session = FindSession(SessionId);

	if (!Session.IsValid() || !Session->bActive)
	{
		WriteRaw(Socket,
			TEXT("HTTP/1.1 404 Not Found\r\n")
			TEXT("Content-Length: 15\r\n")
			TEXT("Access-Control-Allow-Origin: *\r\n")
			TEXT("\r\n")
			TEXT("Session not found"));
		return;
	}

	// Acknowledge immediately so the client doesn't wait
	WriteRaw(Socket,
		TEXT("HTTP/1.1 202 Accepted\r\n")
		TEXT("Content-Length: 0\r\n")
		TEXT("Access-Control-Allow-Origin: *\r\n")
		TEXT("\r\n"));

	// Process JSON-RPC on the game thread (Blueprint API requires game thread)
	FString ResponseJson;
	FEvent* Done = FPlatformProcess::GetSynchEventFromPool(false);

	AsyncTask(ENamedThreads::GameThread, [&ResponseJson, &Body, Done]()
	{
		ResponseJson = FWBMCPHttpServer::ProcessJsonRpcBody(Body);
		Done->Trigger();
	});

	Done->Wait();
	FPlatformProcess::ReturnSynchEventToPool(Done);

	// Push response via the SSE stream — skip empty notifications response
	if (!ResponseJson.IsEmpty() && ResponseJson != TEXT("{}"))
	{
		Session->SendEvent(TEXT("message"), ResponseJson);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// OPTIONS — CORS preflight
// ─────────────────────────────────────────────────────────────────────────────

void FWBMCPSseServer::HandleOptions(FSocket* Socket)
{
	WriteRaw(Socket,
		TEXT("HTTP/1.1 204 No Content\r\n")
		TEXT("Access-Control-Allow-Origin: *\r\n")
		TEXT("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n")
		TEXT("Access-Control-Allow-Headers: Content-Type, Accept\r\n")
		TEXT("Access-Control-Max-Age: 86400\r\n")
		TEXT("\r\n"));
}
