# Usage Guide

This guide describes the recommended way to use WidgetBlueprintMCP in practical editing workflows.

한글 설명: 실제 편집 흐름에서 WidgetBlueprintMCP를 어떻게 사용하는 것이 좋은지 정리한 문서입니다.

## General rule

Always inspect first, then modify, then compile, then save.

한글 설명: 기본 원칙은 조회 → 수정 → 컴파일 → 저장 순서입니다.

---

## Workflow 1: Safe property editing

1. Call `list_widget_blueprints`
2. Choose the target Widget Blueprint
3. Call `get_widget_blueprint`
4. Find the target widget and property
5. Call `modify_widget_property`
6. Call `compile_widget_blueprint`
7. Call `save_widget_blueprint`

한글 설명:
- 목록 확인
- 대상 위젯 선택
- 위젯 블루프린트 읽기
- 수정할 위젯과 속성 찾기
- 속성 수정
- 컴파일
- 저장

---

## Workflow 2: Graph editing

1. Call `list_widget_blueprints`
2. Call `list_functions`
3. Call `get_function_graph`
4. Add or modify nodes
5. Connect pins
6. Compile
7. Save

한글 설명:
- 위젯 블루프린트 선택
- 함수 목록 조회
- 그래프 구조 확인
- 노드 추가 또는 수정
- 핀 연결
- 컴파일
- 저장

### Recommendation
Avoid making too many graph changes at once unless you are using a carefully controlled batch.

한글 설명: 그래프 변경은 한 번에 너무 많이 하기보다 작은 단위로 나누는 것이 좋습니다.

---

## Workflow 3: Variable editing

1. Read the Widget Blueprint
2. Check existing variables
3. Add or modify a variable
4. Compile
5. Save

한글 설명:
- 위젯 블루프린트 읽기
- 기존 변수 확인
- 변수 추가 또는 수정
- 컴파일
- 저장

### Recommendation
Be conservative with variable removals and type-level changes.

한글 설명: 변수 삭제나 타입 변경은 영향 범위가 클 수 있으니 보수적으로 처리하는 것이 좋습니다.

---

## Workflow 4: Batch editing

1. Prepare a small ordered batch
2. Execute the batch
3. Inspect the result
4. Compile
5. Save

한글 설명:
- 작은 단위의 batch 준비
- batch 실행
- 결과 확인
- 컴파일
- 저장

### Recommendation
Prefer small batches that are easy to retry.

한글 설명: 재시도가 쉬운 작은 batch가 더 안전합니다.

---

## Error handling

If a tool fails:
- Re-read the Widget Blueprint
- Re-check function graph names
- Re-check widget names
- Re-check variable names
- Retry with a smaller change set

한글 설명:
- 에러가 나면 먼저 현재 상태를 다시 읽고
- 그래프 이름, 위젯 이름, 변수 이름을 다시 확인하고
- 더 작은 수정 단위로 재시도하는 것이 좋습니다

---

## Recommended AI behavior

AI clients should:
- Read before editing
- Apply small changes
- Compile frequently
- Save only after successful compile when possible
- Avoid assuming graph or widget names

한글 설명:
- 수정 전에 먼저 읽고
- 작은 단위로 수정하고
- 자주 컴파일하고
- 가능하면 컴파일 성공 후 저장하고
- 이름을 추측하지 않는 것이 좋습니다

---

## What this plugin is best at

WidgetBlueprintMCP is best suited for:
- Repetitive UMG edits
- Simple graph updates
- Variable management
- Fast iteration loops in UI-heavy projects

한글 설명:
- 반복적인 UMG 수정
- 비교적 단순한 그래프 수정
- 변수 관리
- UI 비중이 큰 프로젝트에서의 빠른 반복 작업에 잘 맞습니다

---

## Current limits

- Node support is partial
- Animation editing is not finalized
- General Blueprint editing is not the main goal
- Some behavior may depend on project-specific or engine-version-specific conditions

한글 설명:
- 노드 지원은 일부만 구현되어 있고
- 애니메이션 수정은 아직 정리되지 않았으며
- 일반 블루프린트 전체 지원이 목표는 아니고
- 일부 동작은 프로젝트나 엔진 버전에 따라 차이가 날 수 있습니다