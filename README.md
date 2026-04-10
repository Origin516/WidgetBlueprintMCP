Licensed under the MIT License.

# WidgetBlueprintMCP

WidgetBlueprintMCP is a UMG-focused Unreal Editor plugin that exposes Widget Blueprint inspection and editing tools over an MCP-style HTTP interface.

한글 설명: 언리얼 에디터에서 Widget Blueprint를 외부 AI 도구가 읽고 수정하고, 컴파일 및 저장까지 할 수 있도록 만든 UMG 특화 플러그인입니다.

## What it does

- Inspect Widget Blueprint hierarchy
- Inspect Blueprint graphs and functions
- Modify widget properties
- Add, remove, and connect Blueprint nodes
- Inspect and edit Blueprint variables
- Compile and save Widget Blueprints
- Execute multiple edit operations in batch
- Optionally use SSE for streaming-style workflows

한글 설명:
- Widget Blueprint 구조 조회
- 그래프 / 함수 조회
- 위젯 속성 수정
- 블루프린트 노드 추가 / 삭제 / 연결
- BP 변수 조회 및 수정
- 컴파일 및 저장
- 여러 작업을 batch로 실행
- SSE 기반 스트리밍 스타일 워크플로우 일부 지원

## Scope

This plugin is focused on **Widget Blueprint / UMG editing workflows**.
It is not intended to be a full general-purpose Unreal Engine automation framework.

한글 설명: 이 플러그인은 범용 언리얼 자동화보다는 Widget Blueprint / UMG 수정 워크플로우에 집중합니다.

## Current capabilities

Currently supported workflows include:

- Listing Widget Blueprints
- Reading Widget Blueprint structure
- Reading function graphs
- Modifying widget properties
- Adding and removing widgets
- Adding and removing nodes
- Connecting and disconnecting pins
- Adding and removing Blueprint variables
- Editing variable defaults and flags
- Compiling Widget Blueprints
- Saving Widget Blueprints
- Executing multiple tool calls in batch

한글 설명: 현재는 위젯 구조, 그래프, 변수, 컴파일, 저장, batch 실행까지 지원하는 상태입니다.

## Current limitations

- Full animation editing is not supported yet
- General Blueprint editing outside Widget Blueprints is not the main goal
- Node creation support is partial
- Transport compatibility is still evolving

한글 설명:
- 애니메이션 수정은 아직 미지원입니다
- 일반 블루프린트 전체 지원이 주목표는 아닙니다
- 노드 생성 지원은 일부만 구현되어 있습니다
- transport 호환성은 계속 정리 중입니다

## Quick start

1. Copy the plugin into your Unreal project.
2. Enable the plugin in Unreal Editor.
3. Open your project in the editor.
4. Check the configured HTTP port.
5. Connect your AI tool or MCP client to the endpoint.
6. Start by listing Widget Blueprints before making edits.

한글 설명:
1. 플러그인을 프로젝트에 복사
2. 에디터에서 플러그인 활성화
3. 프로젝트 열기
4. 설정된 HTTP 포트 확인
5. AI 도구 또는 MCP 클라이언트 연결
6. 수정 전에는 먼저 Widget Blueprint 목록 조회부터 시작

## Recommended workflow

1. List Widget Blueprints
2. Read the target Widget Blueprint
3. Inspect function graphs if needed
4. Apply small changes
5. Compile
6. Save
7. Repeat if compile errors are returned

한글 설명: 한 번에 크게 수정하기보다는, 조회 → 소규모 수정 → 컴파일 → 저장 순서로 반복하는 방식이 가장 안전합니다.

## Documentation

- [Tools Reference](docs/TOOLS.md)
- [Usage Guide](docs/USAGE.md)
- [Transport Notes](docs/TRANSPORT.md)

## Notes

WidgetBlueprintMCP is designed as a practical UMG editing tool for Unreal Editor workflows.
It is intended to help with repetitive UI iteration and structured Widget Blueprint editing, especially in projects that rely heavily on UMG.

한글 설명: 반복적인 UI 수정이나 구조화된 Widget Blueprint 편집이 많은 프로젝트에서 특히 유용한 실용 툴을 목표로 합니다.