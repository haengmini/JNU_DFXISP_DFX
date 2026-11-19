# AMD DFX Controller 채택 + dfxc_trigger_adapter.v — 결정 기록

**결정 (2026-08-06): 운영용 재구성 경로는 AMD DFX Controller IP(PG374)를 사용한다.**
직접 작성한 `pr_controller.v`는 처음에 지연 계측 장치로 유지했으나, 보조(선택)
카운터인 `pr_latency_probe.v`가 IP의 핸드셰이크 경계에서 측정 역할을 넘겨받자
(아래 §6) **같은 날 늦게 보관 처리**했다(`../archive/pr_controller/`).

## 이유

| | 커스텀 `pr_controller.v` | AMD DFX Controller IP |
|---|---|---|
| 비트스트림 소스 | 고정 BRAM 1개 | DDR/플래시에서 AXI 마스터 fetch, RM별 주소 테이블 |
| 트리거 | 와이어 1개 | HW 트리거 포트 + SW 트리거, 트리거→RM 매핑 |
| RP 지원 | 고정 1개 | 다중 Virtual Socket(VSM) |
| 셧다운/격리 | drain_ready 와이어 | 셧다운 핸드셰이크 + DFX Decoupler 생태계 |
| 오류 처리 | 없음 | 상태 레지스터, 오류 관리 |

커스텀 FSM을 운영 수준으로 키우려면 오른쪽 열 전부를 다시 구현해야 한다. IP는 이를
검증된 상태로 제공한다. 측정 서사(trigger→done 분해)는 보조 모듈
`pr_latency_probe.v`가 담당한다.

## 무엇이 어디에 연결되는가

```
checker_hysteresis ──pr_trigger/pr_busy──▶ dfxc_trigger_adapter ──▶ AMD DFX Controller
      (변경 없음)                               │                        │ m_axi → DDR (비트스트림)
                                                │                        │ ICAP → 컨피규레이션 엔진
                          rm_ap_idle ──────────▶│ shutdown-ack shim      │ decouple → DFX Decoupler(들)
```

`dfxc_trigger_adapter.v`는 `checker_hysteresis`를 변경하지 않고 다음을 제공한다.
- **RM별 one-hot HW 트리거**(`vs_hw_triggers`: [0]=RM_NORMAL,
  [1]=RM_LOW_LIGHT). IP가 수락할 때까지 유지되므로 ack가 올 때까지 요청을 유지하는
  규칙이 살아 있고, 교체 도중의 요청이 유실되지 않는다.
- IP에서 관측 가능한 활동(`rm_shutdown_req | rm_decouple`)으로 합성한 **busy ack**.
- **shutdown-ack shim**: `vs_rm_shutdown_ack = req & rm_ap_idle` — 커스텀
  컨트롤러의 `DRAIN_WAIT`가 구현하던 것과 동일한 drain 조건.

## IP 생성 프로브 (2026-08-06) — 포트 계약 확인됨

IP를 실제로 Vivado 2024.1(`dfx_controller` v1.0, xczu7ev)에서 생성했다(재현 가능한
스크립트: `scripts/dfx/gen_dfx_controller.tcl`). 설정은 NUM_HW_TRIGGERS=2,
TRIGGER0_TO_RM=RM_0, TRIGGER1_TO_RM=RM_1. 생성된 인스턴스화 템플릿(총 49포트)에서
확인한 내용:

| 생성된 포트 | 방향/폭 | 어댑터 측 |
|---|---|---|
| `vsm_VS_0_hw_triggers` | in [1:0] | `vs_hw_triggers` — **트리거당 와이어 1개, 트리거 i는 TRIGGERi_TO_RM을 로드** → one-hot 설계와 일치 |
| `vsm_VS_0_rm_shutdown_req` | out 1 | `vs_rm_shutdown_req` |
| `vsm_VS_0_rm_shutdown_ack` | in 1 | `vs_rm_shutdown_ack` (shim = req & ap_idle) |
| `vsm_VS_0_rm_decouple` | out 1 | `vs_rm_decouple` (→ DFX Decoupler도 구동) |
| `vsm_VS_0_rm_reset` | out 1 | 어댑터에는 없음 — RM 리셋 트리에 연결(Stage 6) |
| `vsm_VS_0_event_error`, `sw_shutdown_req`, `sw_startup_req` | out 1 | 관측성 / SW 흐름 |
| `clk`, `reset` + `icap_clk`, `icap_reset` | in | **별도의 ICAP 클럭 도메인 — IP가 CDC를 내장(CDC_STAGES 파라미터)하므로 "ICAP 100 MHz CDC" 미결 항목이 해소됨: `icap_clk`에 100 MHz만 공급하면 된다** |
| `icap_o` [31:0] out / `icap_i` [31:0] in, `icap_csib`, `icap_rdwrb` | — | ICAPE3 관점에서 명명: IP `icap_o` → ICAPE3 `I`, IP `icap_i` ← ICAPE3 `O`; 추가로 `icap_avail`/`icap_prdone`/`icap_prerror` 입력 |
| `m_axi_mem_*` | AR/R만, 32비트 주소 | 읽기 전용 비트스트림 fetch 마스터(쓰기 채널 없음) |
| `s_axi_reg_*` | AXI4-Lite, 32비트 주소 | SW 트리거/상태 레지스터 접근 |

**프로브 이후에도 남은 미결 사항(솔직히):** RM별 설정(SHUTDOWN_REQUIRED=hw —
셧다운 핸드셰이크가 동작하려면 필요 — , RESET_REQUIRED, 비트스트림 ADDRESS/SIZE
테이블)은 배치 모드에서 점(.) 경로 Tcl API로 설정할 수 없었다(RM 수준 키는 오류,
VS 수준 키는 동작). Stage 6에서 IP 커스터마이즈 GUI / Block Design으로 설정하고
다시 검증한다. `CONFIG.ALL_PARAMS`를 직접 대입하는 방식은 2024.1 배치에서
`GUI_SELECT_TRIGGER_3 = -1` 전파 오류로 실패하므로 API를 사용한다.

`checker_to_dfxc_tb.v`는 여전히 생성된 넷리스트가 아니라 **동작 계약 모델**에 대해
검증한다. IP를 통합한 시뮬레이션은 Stage 6 단계로 남아 있으나, TB와 어댑터가
가정하는 포트 이름/폭은 이제 생성된 것과 같다.

## Stage 6 통합 체크리스트 (커스텀 컨트롤러 배선 계획을 대체)

1. DFX Controller IP를 생성/구성한다. 기본 생성은 스크립트화되어 있다
   (`scripts/dfx/gen_dfx_controller.tcl`, 포트 계약 확인됨 — 위 프로브 절 참고).
   GUI/BD로 구성할 것: RM별 SHUTDOWN_REQUIRED=hw, RESET 설정, DDR 비트스트림
   ADDRESS/SIZE 테이블.
2. RP 경계에 DFX Decoupler IP를 추가하고 IP의 decouple 출력으로 구동한다.
3. `hyst_flags`/`ap_vld`(재합성한 `dfxisp_accel`) → `checker_hysteresis` →
   어댑터 → IP HW 트리거를 연결한다. 활성 RM의 `rm_ap_idle`을 어댑터로,
   `vsm_VS_0_rm_reset`을 RM 리셋 트리로 연결한다. ICAP 클럭: `icap_clk`에
   100 MHz를 공급한다 — IP가 CDC를 내장(프로브 결과)하므로 외부 CDC 설계는
   필요 없다.
4. 교체 후 RM 재시작 정책(교체 후 `ap_start`를 누가 발행하는가: 정적 로직 vs
   PS) — 아직 미결 결정이다.
5. PS는 관측만 유지한다(AXI4-Lite: `selected_mode`, IP 상태 레지스터).
6. (보조, 선택 — 이것 없이도 교체 체인은 동작한다) 지연 측정: IP에는 **내장 지연
   카운터가 없지만**(상태/오류 레지스터뿐) 핸드셰이크 신호가 모든 단계를
   제한한다 — `pr_latency_probe.v`(같은 디렉터리)가 drain(셧다운 req→ack)과
   종단 간 교체(trigger→decouple 해제)를 카운트하며, `checker_to_dfxc_tb.v` 내부에서
   검증되었다(프로브: 계약 모델에서 drain=4, swap=38 사이클). 보드에서는 ILA로
   읽거나 상태 레지스터에 래치한다. 따라서 커스텀 `pr_controller.v`는 **보관
   처리**되었다(`../archive/pr_controller/`) — 계측 장치로조차 더 이상 필요하지
   않다.
