# Tools Reference

This document describes the main tools exposed by WidgetBlueprintMCP.

한글 설명: WidgetBlueprintMCP가 제공하는 주요 툴을 설명하는 문서입니다.

---

## `list_widget_blueprints`

Lists available Widget Blueprints in the project.

한글 설명: 프로젝트 내의 Widget Blueprint 목록을 조회합니다.

### Typical use
Use this first to discover candidate assets before editing.

한글 설명: 수정 전에 어떤 위젯 블루프린트를 다룰지 찾기 위한 첫 단계입니다.

---

## `get_widget_blueprint`

Reads a Widget Blueprint and returns its widget hierarchy, properties, graphs, variables, and related metadata.

한글 설명: 특정 Widget Blueprint의 구조, 위젯 트리, 그래프, 변수 등을 전체적으로 읽습니다.

### Typical use
Call this before modifying widgets, properties, or variables.

한글 설명: 수정 전에 현재 상태를 파악하기 위해 호출합니다.

---

## `list_functions`

Lists function graphs available in the target Widget Blueprint.

한글 설명: 대상 Widget Blueprint 안의 함수 그래프 목록을 조회합니다.

### Typical use
Use this to find the graph you want to inspect or edit.

한글 설명: 어떤 그래프를 읽거나 수정할지 찾을 때 사용합니다.

---

## `get_function_graph`

Returns graph-level information including nodes, pins, and links for a specific function graph.

한글 설명: 특정 함수 그래프의 노드, 핀, 연결 구조를 조회합니다.

### Typical use
Call this before adding nodes or rewiring graph logic.

한글 설명: 노드 추가나 연결 수정 전에 그래프 구조를 먼저 확인할 때 사용합니다.

---

## `modify_widget_property`

Modifies a property on a target widget.

한글 설명: 특정 위젯의 속성을 수정합니다.

### Typical use
Useful for changing text, visibility, enabled state, layout-related values, and similar widget properties.

한글 설명: 텍스트, 가시성, 활성화 여부, 레이아웃 관련 값 등을 바꿀 때 사용합니다.

---

## `add_widget`

Adds a widget to the widget tree.

한글 설명: 위젯 트리에 새 위젯을 추가합니다.

### Typical use
Useful when extending a UI layout from AI instructions.

한글 설명: 패널, 버튼, 텍스트 등 새로운 위젯을 레이아웃에 추가할 때 사용합니다.

---

## `remove_widget`

Removes a widget from the widget tree.

한글 설명: 위젯 트리에서 기존 위젯을 제거합니다.

### Typical use
Use when cleaning up layouts or removing obsolete UI elements.

한글 설명: 더 이상 필요 없는 위젯을 정리할 때 사용합니다.

---

## `add_node`

Adds a supported node to a graph.

한글 설명: 그래프에 지원되는 노드를 추가합니다.

### Typical use
Use this when extending or building Widget Blueprint graph logic.

한글 설명: 조건 분기, 함수 호출, 변수 접근 등 그래프 로직을 구성할 때 사용합니다.

### Notes
Node support is partial and may expand over time.

한글 설명: 모든 노드를 지원하는 것은 아니며, 지원 범위는 점차 확장될 예정입니다.

---

## `remove_node`

Removes a node from a graph.

한글 설명: 그래프에서 특정 노드를 삭제합니다.

---

## `connect_pins`

Creates a connection between two pins.

한글 설명: 두 핀을 연결합니다.

### Typical use
Usually used after adding or repositioning nodes in a graph.

한글 설명: 노드 추가 후 연결을 구성할 때 자주 사용합니다.

---

## `disconnect_pins`

Removes a connection between two pins.

한글 설명: 기존 핀 연결을 제거합니다.

---

## `modify_node_pin`

Modifies a node pin default value or related pin data.

한글 설명: 노드 핀의 기본값 또는 관련 데이터를 수정합니다.

---

## `add_variable`

Adds a Blueprint variable.

한글 설명: Widget Blueprint에 새 BP 변수를 추가합니다.

### Typical use
Useful when new widget logic requires stored state or references.

한글 설명: 상태값이나 참조값을 저장할 변수가 필요할 때 사용합니다.

---

## `set_variable_default`

Sets the default value of a Blueprint variable.

한글 설명: BP 변수의 기본값을 수정합니다.

---

## `modify_variable_flags`

Modifies variable flags such as editability or exposure.

한글 설명: Editable, Expose on Spawn 같은 변수 옵션을 수정합니다.

---

## `remove_variable`

Removes a Blueprint variable.

한글 설명: 기존 BP 변수를 삭제합니다.

---

## `compile_widget_blueprint`

Compiles the target Widget Blueprint and returns the compile result.

한글 설명: 수정 후 결과를 확인하기 위한 컴파일 툴입니다.

### Typical use
Run this after each meaningful change.

한글 설명: 의미 있는 수정이 들어간 뒤에는 자주 호출하는 것이 좋습니다.

---

## `save_widget_blueprint`

Saves the target Widget Blueprint.

한글 설명: 최종 변경 사항을 저장합니다.

### Typical use
Prefer saving after a successful compile whenever possible.

한글 설명: 가능하면 컴파일 성공 후 저장하는 것을 권장합니다.

---

## `execute_batch`

Executes multiple tool calls in order.

한글 설명: 여러 툴 호출을 순서대로 한 번에 실행합니다.

### Typical use
Useful for flows such as add node → connect pins → compile.

한글 설명: 노드 추가 → 핀 연결 → 컴파일 같은 연속 작업에 적합합니다.

### Notes
Smaller batches are usually easier to debug and retry.

한글 설명: 너무 큰 batch보다 작은 batch가 디버깅과 재시도에 유리합니다.