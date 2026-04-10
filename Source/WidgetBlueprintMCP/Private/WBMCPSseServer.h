#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

class FSocket;

/**
 * SSE (Server-Sent Events) transport for the MCP protocol.
 * Allows Claude Desktop to connect via GET /sse and POST /message?sessionId=xxx.
 *
 * Flow:
 *   1. Client connects GET http://localhost:<SsePort>/sse
 *   2. Server sends SSE headers + "event: endpoint" with the POST URL
 *   3. Client POSTs JSON-RPC to /message?sessionId=<id>
 *   4. Server dispatches to game thread, responds via SSE "event: message"
 */
class FWBMCPSseServer
{
public:
	~FWBMCPSseServer();

	void Start(uint32 InPort);
	void Stop();

private:
	// ── Active SSE session (one per GET /sse connection) ──────────────────
	struct FSession
	{
		FSocket*         Socket   = nullptr;
		FCriticalSection WriteLock;
		bool             bActive  = false;

		// Thread-safe write to the SSE socket. Returns false if send failed.
		bool SendEvent(const FString& EventType, const FString& Data);
		bool SendPing();
	};

	// ── Accept loop (runs on a dedicated thread) ──────────────────────────
	class FAcceptRunnable : public FRunnable
	{
	public:
		explicit FAcceptRunnable(FWBMCPSseServer& InOwner) : Owner(InOwner) {}
		virtual uint32 Run() override;
	private:
		FWBMCPSseServer& Owner;
	};

	// ── Per-connection handler (runs on UE thread pool via AsyncTask) ──────
	void HandleConnection(FSocket* Socket);
	void HandleSseGet(FSocket* Socket);
	void HandlePost(FSocket* Socket, const FString& Path, const FString& Body);
	void HandleOptions(FSocket* Socket);

	// ── Session registry ───────────────────────────────────────────────────
	TSharedPtr<FSession> FindSession(const FString& Id);
	void RegisterSession(const FString& Id, TSharedPtr<FSession> Session);
	void UnregisterSession(const FString& Id);
	void CloseAllSessions();

	// ── Static helpers ─────────────────────────────────────────────────────
	static bool ReadHttpRequest(FSocket* Socket, FString& OutMethod, FString& OutPath, FString& OutBody);
	static bool WriteRaw(FSocket* Socket, const FString& Text);
	static FString ExtractQueryParam(const FString& Path, const FString& Key);
	static FString NewSessionId();

	// ── State ──────────────────────────────────────────────────────────────
	FSocket*           ListenSocket   = nullptr;
	FAcceptRunnable*   AcceptRunnable = nullptr;
	FRunnableThread*   AcceptThread   = nullptr;

	FCriticalSection                    SessionsLock;
	TMap<FString, TSharedPtr<FSession>> Sessions;

	volatile bool bRunning = false;
	uint32        ListenPort = 8766;
};
