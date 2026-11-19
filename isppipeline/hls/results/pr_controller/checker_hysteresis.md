# checker_hysteresis.v — 설계 노트

신규 모듈(2026-08-06, 이 저장소): 슈미트 모드 결정을 패브릭으로 옮겨 PS 개입 없이
checker → pr_controller 트리거 경로를 닫는다. 이는 2026-07-03에 채택되었으나 코드로
옮겨진 적이 없던 결정(원본 저장소 `checker-improvement-theory-2026-07-03.md` §7
제안 #1: "슈미트 + 최소 체류(min-dwell)를 HW 체커로 이전 — 즉시 채택")을
구현한 것이다. C1 배포 문서의 이후 표현인 "드라이버 측 정책"은 미구현 상태의 임시
단계를 서술한 것이었으며, DFX 쪽에서는 이 모듈로 대체된다.

## 역할 분담

```
dfxisp_accel (HLS 코어)                 checker_hysteresis.v (이 모듈, 정적 영역)
  프레임별 어두운 픽셀 스캔                모드 상태(FF 1개) + 슈미트 밴드
  밴드 비교 2개:                          최소 체류 카운터
    hyst_flags[0] = ratio > 64% (진입)    pr_trigger 요청/ack 핸드셰이크
    hyst_flags[1] = ratio < 60% (해제)
  hyst_flags + ap_vld 내보냄  ──────▶     프레임당 플래그 쌍 하나를 소비
```

- HLS 코어는 **무상태(stateless)** 로 유지된다. 단일 프레임 판정(`selected_mode`)과
  골든 CSV 계약은 변하지 않는다(csim은 여전히 비트 단위로 일치하며, 변경 후 검증:
  726 px 골든 비교 PASS).
- 밴드 경계는 `dfxisp_accel.cpp`의 컴파일 타임 상수이다(`HYST_ENTER_PCT` = 64,
  `HYST_EXIT_PCT` = 60 — checker-principles 원칙 5에 따라 중심 62% 둘레 델타 =
  2%p이며, 단일 프레임 판정은 `DARK_RATIO_PCT` = 62를 독립적으로 유지한다).
  어두운 픽셀 레벨 자체는 런타임 `dark_pixel_threshold` AXI-Lite 레지스터로
  유지된다(raw12에서 256 = 16<<4).
- 강제 NORMAL/LOW_LIGHT 모드는 오버라이드와 일치하는 플래그를 내보내므로,
  히스테리시스 블록은 강제 모드와 다투지 않고 그것을 추종한다.

## 트리거 프로토콜 (요청/ack)

- 모드 전환 시 `pr_trigger`가 high가 되고 **`pr_busy`가 관측될 때까지 high를
  유지**한다. 컨트롤러가 재구성 도중이면 펄스는 유실될 수 있기 때문이다.
  `pr_controller`는 `S_IDLE`에서만 `trigger`를 샘플링하며, busy에서 해제하면 DONE
  이후 컨트롤러가 IDLE로 돌아올 때 불필요한 재트리거가 없음이 보장된다.
- 요청이 대기 중이거나 `pr_busy`가 high인 동안에는 판정을 동결한다(어차피 그
  프레임들은 교체되는 쪽 RM이 처리했다).
- `DWELL_FRAMES`(기본값 1): 새로 진입한 모드를 다음 전환이 허용되기 전까지
  유지하는 프레임 수. 대기/busy 프레임은 세지 않는다. 1프레임이면 현재 PR 지연
  추정치(전형 14.5 ms < 33.3 ms 프레임 예산)를 충당한다. PR 지연이 한 프레임
  시간을 넘으면 늘려야 한다.

## 검증 (xsim, Vivado 2024.1 — 둘 다 PASS)

- `checker_hysteresis_tb.v` — 단위: 밴드 내부 프레임은 아무것도 바꾸지 않음(단일
  임계값 체커에는 없던 슈미트 특성), busy ack까지 트리거 유지, 전환 직후 프레임은
  체류(dwell)가 차단, 불필요한 재트리거 없음.
- `checker_to_pr_tb.v` — 실제 `pr_controller.v`와의 통합(NWORDS=16 오버라이드):
  어두운 프레임 → 트리거 → drain(RM ap_idle 모델, 8사이클) → ICAP 스트림 → done,
  이어서 반대 방향 전환. 재구성 2회, 루프에 PS 없음.

> **업데이트 (2026-08-06, 같은 날 늦게):** AMD DFX Controller IP(PG374)가 운영용
> 재구성 경로로 채택되었다 — `dfxc_trigger_adapter.v`가 동일한 요청/ack 의미론으로
> 이 블록을 IP에 연결하며(`checker_hysteresis` 자체는 변경 없음),
> `checker_to_dfxc_tb.v`가 PG374 계약 모델에 대해 이 체인을 검증한다(xsim PASS).
> `dfxc_adapter.md` 참고. 커스텀 `pr_controller.v`는 이후 **보관 처리**되었다
> (`../archive/pr_controller/`) — 지연 측정은 IP 핸드셰이크 신호를 보는
> `pr_latency_probe.v`가 담당한다. 아래 3–5번 항목은 커스텀 경로 계획의 이력으로
> 남겨 두며, 현행 체크리스트는 `dfxc_adapter.md`에 있다.

## 이에 더해 Stage 6에서 남은 일

1. 실제 HLS 코어의 `hyst_flags[1:0]` + `hyst_flags_ap_vld` RTL 포트를 이 블록에
   연결한다(`dfxisp_accel`을 재합성하면 포트가 생긴다 — `ap_vld` pragma는 이미
   소스에 있다).
2. `drain_ready` ← 활성 RM의 실제 `ap_idle`(TB는 모델을 사용).
3. BRAM 비트스트림 소스를 SD/DDR로 교체하고 ICAPE3/STARTUPE3를 인스턴스화한다
   (기존과 동일).
4. pr_controller 통합 시 알려진 고려사항(소스 검토 2026-08-06): 기본값
   `NWORDS=171,633`은 이전 floorplan 비트스트림(페이로드 686,532 B)에 맞춘 값이며,
   현재 pblock 부분 비트스트림은 1,447,424 B ≈ 361,856 워드이므로 다시 추출해
   갱신해야 한다. FETCH/WRITE FSM은 2사이클당 1워드를 스트리밍한다(ICAP 대역폭의
   절반 — 파이프라이닝 검토). 1.716 ms TB 측정값은 200 MHz에서 얻은 것인데
   ICAPE3 정격은 100 MHz(UG570)이므로, 정격 클럭에서는 이 FSM이 그 약 2배가
   되며 ICAP 클럭 도메인에는 CDC 결정이 필요하다.
5. 비트스트림 선택: RM이 둘이면 컨트롤러에 대상별 기준 주소 / 비트스트림 선택이
   필요하다 — 현재는 미리 적재된 단일 페이로드만 스트리밍한다.
