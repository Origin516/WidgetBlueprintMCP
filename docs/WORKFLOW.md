# Recommended Workflows

## 기본 원칙
1. **읽고 나서 수정** — node_id, 위젯 이름, 그래프 이름은 반드시 읽기 툴로 확인 후 사용
2. **compile → save** — 수정 후 항상 이 순서
3. **execute_batch 활용** — 여러 단계를 묶어 HTTP 요청 수를 줄임

---

## 시나리오 1: 이벤트 그래프에 노드 추가 후 연결

```
1. get_widget_blueprint       → 그래프 이름 확인
2. get_function_graph         → 기존 노드 id/핀 이름 확인
3. execute_batch:
   - add_node (Branch)        → id: "branch"
   - connect_pins             → source: 기존 노드, target: $branch.node_id
   - add_node (CallFunction)  → id: "call"
   - connect_pins             → source: $branch.node_id True, target: $call.node_id
   - compile_widget_blueprint
   - save_widget_blueprint
```

---

## 시나리오 2: 변수 추가 및 초기화

```
1. get_widget_blueprint       → 기존 변수 목록 확인
2. add_variable               → var_type: bool
3. set_variable_default       → "false"
4. modify_variable_flags      → instance_editable: true
5. compile_widget_blueprint
6. save_widget_blueprint
```

---

## 시나리오 3: 애니메이션 구조 파악

```
1. get_widget_blueprint       → animations[] 에서 이름 확인
2. get_animation              → possessables, tracks, keys 전체 확인
   (결과는 Saved/WidgetBlueprintMCP/에 JSON으로 저장됨)
```

---

## 에러 대응

| 에러 | 원인 | 해결 |
|------|------|------|
| exit code 7 | 서버 미실행 | 에디터 실행 + 플러그인 활성화 확인 |
| Blueprint not found | asset_path 오류 | list_widget_blueprints로 정확한 경로 확인 |
| Graph not found: 'X'. Available: ... | 그래프 이름 오류 | 에러 메시지의 Available 목록 사용 |
| Node not found | node_id 오류 | get_function_graph로 재확인 |
| Cannot connect: ... | 핀 타입 불일치 | 양쪽 핀 타입 확인 |
| Compile FAILED | BP 로직 오류 | errors[] 내용 확인 후 수정 |
