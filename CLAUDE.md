# WidgetBlueprintMCP — Claude Usage Guide

UE4 에디터 플러그인. HTTP MCP 서버(localhost:8765)를 통해 Widget Blueprint를 읽고 수정한다.

## 연결 확인
```bash
curl -s --max-time 5 -X POST http://localhost:8765/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
```
exit code 7 또는 응답 없음 → 에디터가 꺼져 있거나 플러그인이 비활성화된 것.

## 기본 호출 패턴
```bash
curl -s --max-time 15 -X POST http://localhost:8765/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"<tool>","arguments":{...}}}'
```

## 권장 작업 순서

### 읽기
1. `list_widget_blueprints` — asset_path 목록 확인
2. `get_widget_blueprint` — 위젯 트리, 변수, 애니메이션 이름 목록
3. `list_functions` → `get_function_graph` — 노드/핀/연결 확인
4. `get_animation` — 애니메이션 트랙/키프레임 상세

### 수정 (단일)
1. 읽기로 현재 상태 파악
2. 수정 툴 호출 (remove → add → connect 순서 권장)
3. `compile_widget_blueprint` — 컴파일 성공 확인
4. `save_widget_blueprint` — 저장

### 수정 (배치) — 권장
```bash
# execute_batch: 여러 명령을 한 번에, $id.node_id로 이전 결과 참조 가능
{
  "name": "execute_batch",
  "arguments": {
    "commands": [
      {"name":"remove_node","arguments":{"asset_path":"...","graph_name":"EventGraph","node_id":"ABC"}},
      {"name":"add_node","id":"newNode","arguments":{"asset_path":"...","graph_name":"EventGraph","node_type":"Branch","x":0,"y":0}},
      {"name":"connect_pins","arguments":{"asset_path":"...","graph_name":"EventGraph","source_node_id":"$newNode.node_id","source_pin":"then","target_node_id":"XYZ","target_pin":"execute"}},
      {"name":"compile_widget_blueprint","arguments":{"asset_path":"..."}},
      {"name":"save_widget_blueprint","arguments":{"asset_path":"..."}}
    ]
  }
}
```

## asset_path 형식
`/Game/Widgets/Foo/BarWidgetBP.BarWidgetBP`
- 앞부분(패키지): `/Game/Widgets/Foo/BarWidgetBP`
- 뒷부분(에셋명): `.BarWidgetBP`
- `list_widget_blueprints` 결과는 패키지명만 반환 → 뒤에 `.이름` 붙이기

## node_id 규칙
- 32자 hex GUID
- `get_function_graph` 결과의 `nodes[].id` 필드에서 확인
- 절대 추측 금지 — 반드시 읽기 후 사용

## 스냅샷 파일
`get_widget_blueprint`, `get_function_graph`, `get_animation` 호출 시
`Game/Saved/WidgetBlueprintMCP/` 에 JSON 파일 자동 저장.
에디터 없이 내용을 확인하거나 diff할 때 활용.

## 제한사항
- 컴파일은 저장과 별개 (`compile_widget_blueprint` → `save_widget_blueprint` 순서)
- 저장 후 에디터에서 내용이 반영되려면 에디터 재로드 필요할 수 있음
- 타입 변경(`var_type` 수정)은 미지원 — 삭제 후 재생성
- node_type 지원 목록: CallFunction, VariableGet, VariableSet, Branch, Sequence, CustomEvent, Cast, DoOnce
