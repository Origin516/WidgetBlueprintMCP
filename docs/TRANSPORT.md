# Transport Notes

WidgetBlueprintMCP exposes an MCP-style HTTP interface for Widget Blueprint editing workflows.

한글 설명: 이 문서는 WidgetBlueprintMCP의 HTTP / SSE 기반 transport 방향을 설명합니다.

## Overview

The plugin currently exposes tool access over HTTP and includes optional SSE support for streaming-style workflows.

한글 설명: 기본적으로 HTTP를 통해 툴 호출을 처리하며, 필요에 따라 SSE 기반 스트리밍 워크플로우도 사용할 수 있습니다.

## HTTP interface

The HTTP endpoint is used for request/response style tool calls.

Typical operations include:
- `initialize`
- `tools/list`
- `tools/call`
- `notifications/initialized`

한글 설명: 일반적인 JSON-RPC 스타일 요청/응답은 HTTP endpoint를 통해 처리합니다.

## SSE support

Optional SSE support is available for clients that benefit from server-to-client event streaming.

한글 설명: SSE는 서버가 클라이언트로 이벤트를 지속적으로 전달하는 방식입니다.

### Practical uses
- Streaming compile logs
- Reporting batch progress
- Sending warnings or informational events
- Supporting more interactive workflows

한글 설명:
- 컴파일 로그 전달
- batch 진행 상황 전달
- 경고나 상태 메시지 전달
- 더 상호작용적인 워크플로우 지원

## Session flow

A client may establish an SSE session and then use the related message endpoint for follow-up requests.

한글 설명: 클라이언트는 SSE 세션을 만든 뒤, 관련 message endpoint를 통해 후속 요청을 처리할 수 있습니다.

## Current transport scope

The plugin is intended to be a practical MCP-style editor integration for UMG workflows.
Transport behavior may continue to evolve for broader compatibility across different MCP clients.

한글 설명: 이 플러그인은 실용적인 UMG 편집용 MCP 스타일 인터페이스를 목표로 하며, 더 넓은 MCP 클라이언트 호환성을 위해 transport는 계속 정리될 수 있습니다.

## Current positioning

This plugin should currently be considered:
- practical
- UMG-focused
- editor-oriented
- MCP-style
- evolving toward broader compatibility

한글 설명:
- 실용적인 툴이고
- UMG 특화이며
- 에디터 중심이고
- MCP 스타일 인터페이스를 제공하며
- 더 넓은 호환성을 향해 발전 중인 상태입니다

## Recommendations for clients

Clients should:
- Prefer small edits over large edits
- Compile frequently
- Re-read current state after errors
- Treat current transport behavior as implementation-driven

한글 설명:
- 큰 수정보다 작은 수정 단위를 선호하고
- 자주 컴파일하고
- 에러 후에는 현재 상태를 다시 읽고
- 현재 transport는 구현 중심으로 이해하는 것이 좋습니다

## Future direction

Possible future improvements may include:
- broader MCP transport alignment
- richer SSE event types
- improved session handling
- better structured streaming responses

한글 설명:
- 더 넓은 MCP transport 정합성 확보
- richer한 SSE 이벤트 추가
- 세션 처리 개선
- 더 구조화된 스트리밍 응답 개선
같은 방향으로 확장될 수 있습니다