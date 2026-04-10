# Tool Reference

총 20개 툴. 모든 툴은 `asset_path` 인자에 `/Game/...AssetName.AssetName` 형식 사용.

## 읽기 툴

### `list_widget_blueprints`
프로젝트 내 Widget Blueprint 목록 조회.
```json
{"path_filter": "/Game/Widgets/"}
```
| 인자 | 필수 | 타입 | 설명 |
|------|------|------|------|
| path_filter | 선택 | string | 기본값: `/Game/` |

---

### `get_widget_blueprint`
위젯 트리, 변수, 애니메이션 이름 목록, 이벤트 그래프/함수 이름 목록 반환.
스냅샷: `Saved/WidgetBlueprintMCP/<asset>.json`
```json
{"asset_path": "/Game/Widgets/MyWidgetBP.MyWidgetBP"}
```

---

### `list_functions`
이벤트 그래프, 함수, 매크로 목록과 노드 수 반환.

---

### `get_function_graph`
그래프 전체 노드/핀/연결 직렬화. **node_id는 여기서 확인.**
스냅샷: `Saved/WidgetBlueprintMCP/<asset>__<graph>.json`
```json
{"asset_path": "...", "graph_name": "EventGraph"}
```
에러 시 사용 가능한 그래프 목록 포함.

---

### `get_animation`
특정 애니메이션의 possessable(바인딩된 위젯), 트랙, 키프레임 반환.
스냅샷: `Saved/WidgetBlueprintMCP/<asset>__anim__<name>.json`
```json
{"asset_path": "...", "animation_name": "ShowAnimation"}
```
에러 시 사용 가능한 애니메이션 목록 포함.

---

## 위젯 수정 툴

### `add_widget`
위젯 트리에 새 위젯 추가.
```json
{"asset_path":"...","widget_class":"UButton","widget_name":"MyBtn","parent_widget":"MyPanel"}
```
| 인자 | 필수 |
|------|------|
| asset_path, widget_class, widget_name | ✓ |
| parent_widget | 선택 (없으면 루트) |

---

### `remove_widget`
위젯 트리에서 위젯 제거.

---

### `modify_widget_property`
위젯 프로퍼티 값 수정.
```json
{"asset_path":"...","widget_name":"MyText","property_name":"ColorAndOpacity","value":"(R=1,G=0,B=0,A=1)"}
```

---

## 그래프 노드 툴

### `add_node`
함수 그래프에 노드 추가. **반환값에 node_id 포함.**

| node_type | params 예시 |
|-----------|-------------|
| `CallFunction` | `{"function_class":"UWidget","function_name":"SetVisibility"}` |
| `VariableGet` | `{"variable_name":"MyVar"}` |
| `VariableSet` | `{"variable_name":"MyVar"}` |
| `Branch` | (없음) |
| `Sequence` | (없음) |
| `CustomEvent` | `{"event_name":"OnMyEvent"}` |
| `Cast` | `{"target_class":"UMyWidget"}` |
| `DoOnce` | (없음) |

```json
{"asset_path":"...","graph_name":"EventGraph","node_type":"Branch","x":0,"y":0,"params":"{}"}
```
반환: `"OK node_id=XXXXXXXX..."`

---

### `connect_pins`
두 핀을 연결. 핀 이름은 `get_function_graph`에서 확인.
```json
{
  "asset_path":"...", "graph_name":"EventGraph",
  "source_node_id":"GUID1", "source_pin":"then",
  "target_node_id":"GUID2", "target_pin":"execute"
}
```

---

### `disconnect_pins`
두 핀의 연결 해제. 인자 형식은 `connect_pins`와 동일.

---

### `remove_node`
노드 제거. node_id는 `get_function_graph`에서 확인.

---

### `modify_node_pin`
노드 핀의 기본값 수정.
```json
{"asset_path":"...","graph_name":"EventGraph","node_id":"GUID","pin_name":"Value","value":"true"}
```

---

## 변수 툴

### `add_variable`
변수 추가. var_type: `string` | `bool` | `int` | `float` | `text` | `name`
```json
{"asset_path":"...","var_name":"MyFlag","var_type":"bool","category":"UI"}
```

---

### `set_variable_default`
변수 기본값 설정.
```json
{"asset_path":"...","var_name":"MyFlag","default_value":"true"}
```

---

### `modify_variable_flags`
instance_editable, expose_on_spawn 플래그 수정.
```json
{"asset_path":"...","var_name":"MyFlag","instance_editable":true,"expose_on_spawn":false}
```

---

### `remove_variable`
변수 삭제. 타입 변경이 필요하면 삭제 후 재생성.

---

## 저장/컴파일 툴

### `compile_widget_blueprint`
블루프린트 컴파일. 반환: `{success, errors[], warnings[], summary}`
```json
{"asset_path":"..."}
```
**수정 후 반드시 컴파일 → 저장 순서로 호출.**

---

### `save_widget_blueprint`
에셋을 디스크에 저장. 소스 컨트롤 체크아웃 없이 강제 저장.

---

## 배치 실행

### `execute_batch`
여러 명령을 순서대로 실행. `$id.node_id`로 이전 `add_node` 결과 참조 가능.
```json
{
  "commands": [
    {"name":"add_node","id":"n1","arguments":{...}},
    {"name":"connect_pins","arguments":{"source_node_id":"$n1.node_id",...}}
  ],
  "stop_on_error": true
}
```
반환: 각 명령의 `{name, id, success, result}` 배열.
- `add_node` 성공 시 `node_id` 필드 추가 포함.
- `execute_batch` 안에 `execute_batch` 중첩 불가.
