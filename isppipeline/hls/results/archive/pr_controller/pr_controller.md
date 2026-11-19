# pr_controller.v — 설계 배경 노트

> **역할 변경 (2026-08-06):** AMD DFX Controller IP(PG374)가 운영용 재구성 경로로
> 채택되었다(`dfxc_adapter.md`). 이 모듈은 **지연 특성 분석 용도로만** 유지한다 —
> 자체 워드 카운트 기반 완료 판정 덕분에 단계별 trigger→done 측정이 가능하기 때문이다.

`pr_controller.v` 소스 주석에서 분리한 문서이다.
(출처: haengmini/dfxisp 커밋 `ea6c2de`.)

## 무엇인가

1차 DFX 부분 재구성(partial reconfiguration) 컨트롤러 FSM (Phase 2.1).

```
FSM: IDLE -> DRAIN_WAIT -> ICAP_ARM -> ICAP_STREAM -> DONE -> IDLE
```

## 완료 신호를 자체 워드 카운터에서 얻는 이유

config1/config2 사용률 리포트(2026-07-03)에는 ICAPE3/STARTUPE3 인스턴스가 **하나도**
없다. 즉 당시에는 PR 컨트롤러가 아직 없었고, 이것이 앞서 ICAPE3 단독으로 돌린
시뮬레이션이 신뢰할 만한 완료 신호를 끝내 만들어내지 못한 이유이기도 하다. ICAPE3의
PRDONE은 eos_startup에 의존하는데, 이는 이 설계에는 없는 실제 STARTUPE3 / 디바이스
수준 컨피규레이션 컨텍스트를 필요로 한다.

이 모듈은 그 문제를 아예 우회한다. ICAPE3의 PRDONE을 믿는 대신, **자체 워드
카운터가 이미 알려진 부분 비트스트림 워드 수(171,633 워드, 2026-07-02에 실제
`rm_lowlight_partial.bit`를 파싱해 검증)에 도달하면 완료**로 판정한다. 이 신호는
전적으로 이 설계가 소유하며 STARTUPE3 없이도 시뮬레이션에서 검증할 수 있다.

## 패브릭 전용 버전의 한계 (= Stage 6 선행 조건)

- 부분 비트스트림 소스가 **BRAM**이다(시뮬레이션에서는 `$readmemh`, 실사용은 BRAM
  초기화 가정). 아직 PS/DDR이 아니다. PS/DDR로의 업그레이드는 PS/DDR 통합 이후에 한다.
- `drain_ready`가 아직 실제 RM의 `ap_idle`에 연결되지 않았다.
- ICAPE3/STARTUPE3가 인스턴스화되어 있지 않다.

세 가지 모두 이 저장소 README의 "Stage 6 선행 조건" 항목이다.

## 트리거 소스 (2026-08-06 추가)

`trigger`는 더 이상 연결되지 않은 입력이 아니다. `checker_hysteresis.v`(같은
디렉터리)가 HLS 체커의 프레임별 슈미트 밴드 플래그로부터 이를 생성하고 `busy`가
ack할 때까지 유지한다 — `checker_hysteresis.md`와 종단 간 시뮬레이션
`checker_to_pr_tb.v`(xsim PASS)를 참고. 같은 문서에 소스 검토로 확인한 남은 통합
고려사항(새 pblock 비트스트림 대비 낡은 `NWORDS` 기본값, 워드당 2사이클 스트리밍,
ICAPE3 정격 100 MHz 대비 200 MHz TB 클럭)이 정리되어 있다.
