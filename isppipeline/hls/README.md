# DFXISP HLS C-sim 스캐폴드

저장소 내 경로: `isppipeline/hls/`. 정본 아키텍처 문서:
origin 저장소 루트의 `RESEARCH.md`.

> **이 추출 저장소(JNU_DFXISP_FPGA)에 대한 참고:** 아래의 `tools/*.py` 및
> `results/*.md` 참조는 origin 저장소 haengmini/dfxisp를 가리킨다.
> `tools/`는 여기에 포함되어 있지 않으므로 `make verify` / `make report`와
> 골든 재생성 타깃은 실행되지 않는다. `make csim` / `make rm-csim`은
> 커밋된 골든 CSV 기준으로 그대로 동작한다.

## 목표 (2026-07-01 재설정)

이 스캐폴드는 **공유 baseline ISP 코어 + 상호 배타적 톤 RM 슬롯** 구조의
첫 번째 결정론적 C 시뮬레이션 타깃이다. 톤 RM 슬롯은 공유 baseline
코어를 **감싼다**:

```text
NORMAL:
  RAW Bayer RGGB uint16
    -> checker (모드 결정)
    -> RM_NORMAL_TONE (gain 1.25x + gamma2.0)
    -> baseline ISP core (demosaic + BLC + WB + CCM, 12-bit, gain/gamma 없음)
    -> packed RGB888 uint32  (H x W)

LOW_LIGHT:
  RAW Bayer RGGB uint16
    -> checker (모드 결정)
    -> RM_LOW_LIGHT_TONE.front : 2x2 RAW binning (정밀도 손실 이전, RESEARCH §4.2)
    -> baseline ISP core (demosaic + BLC + WB + CCM, 12-bit, gain/gamma 없음)
    -> RM_LOW_LIGHT_TONE.back  : 저조도 gain 2.0x + gamma2.0 톤
    -> packed RGB888 uint32  (H/2 x W/2, Policy A 형상 변경)
```

C-sim이 증명하는 불변 조건 (RESEARCH.md §8.2):

- 프레임당 **정확히 하나의 톤 RM**만 선택됨 (상호 배타)
- gain/gamma는 **톤 RM에만** 존재하며, baseline 코어에는 중복되지 않음
- 출력 메타데이터가 모드, 선택된 RM, 출력 형상을 보고함

의도적으로 최소화했다: 작은 HLS top 하나, 표준 라이브러리만 쓰는 C-sim,
로컬 스모크 테스트에 Vitis 의존성 없음, HLS pragma는 Vitis HLS/Vitis
플로우를 위해 보존.

## 파일

- `include/dfxisp_accel.hpp` — HLS top 인터페이스, 모드/선택 RM enum, 4개의 스칼라 메타데이터 출력 포인터
- `src/dfxisp_accel.cpp` — checker + baseline core12 (demosaic/BLC/WB/CCM, 12-bit) + RM_NORMAL_TONE (gain 1.25x + gamma2.0) + RM_LOW_LIGHT_TONE (2x2 bin + gain 2.0x + gamma2.0)
- `tests/test_dfxisp_csim.cpp` — C-sim 스모크 테스트 + 골든 CSV 비트 비교 + 아키텍처 불변 조건 검사
- `tools/gen_golden_vectors.py` (origin 저장소) — 표준 라이브러리만 쓰는 결정론적 골든 생성기 (`src/dfxisp_accel.cpp`의 비트 정확 미러)
- `tools/gen_verification_report.py` (origin 저장소) — 표준 라이브러리만 쓰는 Markdown 검증/보고서 생성기
- `scripts/vitis_hls.tcl` — `dfxisp_accel`용 Vitis HLS 프로젝트 스캐폴드
- `Makefile` — g++ 로컬 C-sim, 골든 생성, verify/report, Vitis HLS dry-run 리포트
- `include/default_isp.hpp` · `src/default_isp.cpp` — **default_ISP** (2026-08-06 신규):
  AMD Vitis Vision L3 `isppipeline`의 단계 순서와 도메인(Bayer 도메인
  BLC/gain → demosaic → 적응형 AWB → 실제 CCM → gamma)을 따르는 표준 ISP 암.
  `RM_NORMAL_TONE`과 **공존**하며 기존 골든 계약은 건드리지 않는다.
  상세: `src/default_isp.md`
- `tests/test_default_isp_csim.cpp` + `tests/default_isp_golden_vectors.csv` — 해당 C-sim과
  커밋된 골든 (`make default-isp-csim`; 골든을 재생성하려면 origin
  저장소의 `tools/gen_default_isp_golden.py`가 필요)
- `include/lowlight_isp.hpp` · `src/lowlight_isp.cpp` — **lowlight_ISP** (2026-08-06 신규):
  제안하는 저조도 암 v2. default_ISP의 보정 백본을 공유하며
  **{2×2 binning, 2.0× 업스트림 gain, GAT/VST 톤, 에지 보존 denoise}**
  에서만 차이가 나므로 암 비교가 통제된다. 배포된 RM_LOW_LIGHT_TONE은
  건드리지 않는다. 상세: `src/lowlight_isp.md`
- `tests/test_lowlight_isp_csim.cpp` + `tests/lowlight_isp_golden_vectors.csv` — 해당 C-sim과
  커밋된 골든 (`make lowlight-isp-csim`)

> 실험 암(§7)과 ablation(§12 Task 5)은 `src/dfxisp_rm.cpp` /
> `tools/rm_model.py` (static / reg_only / dfx_bin / dfx_fp)에 별도로
> 있다. 스캐폴드의 이전 post-RGB8 gain/lift 경로는 해당 dfx 변형 집합으로
> 이전되었으며 ablation으로만 남아 있다.

## `tools/` 파일 상태 (정본 / 프록시 / 레거시 — 2026-07-08 Hermes 리뷰 + 같은 날 gamma 재정렬)

(`tools/` 디렉터리 자체는 origin 저장소에 있다; 어떤 Python 파일이 무엇에
대해 권위 있는지를 정의하므로 표는 여기에 유지한다.)

골든/C-sim/교차 검증 경로 자체는 견고하지만, 정본 골든 ↔ SW 평가 프록시
↔ 레거시/ver0 코드 사이의 경계가 문서화되어 있지 않아 혼동 위험이
있었다. 아래 표는 그 경계를 명시한다 — 새 코드는 자신이 어느 범주에
속하는지 밝혀야 한다.

> **같은 날 두 번째 업데이트 (2026-07-08):** Hermes 리뷰(이 표의 첫
> 버전)와 별개로, `isp_pipeline_ver1.py`/`newrm_pipeline.py`의 gamma
> 곡선이 정본과 다르다는 것이 발견되었다 (정본은 두 모드 모두 gamma-2.0
> 정수 sqrt LUT 공유) — 이들은 gamma 2.2/2.5/없음을 사용했다. 두 파일은
> `tools/archive/`로 옮겨졌고 `baseline_isp_pipeline.py` (normal) /
> `low_light_isp_pipeline.py` (low-light) / `checker.py` (dark-ratio
> checker, 분리됨)로 대체되었다. Hermes가 수정한 demosaic 래핑 버그
> (`np.roll` → clamp-to-edge)도 새 파일들에 이식되었다 — 두 수정은 서로
> 다른 실제 버그를 독립적으로 잡아냈고 rebase 시 병합되었다. 상세:
> `results/isp-pipeline-recalibration-2026-07-08.md`.

| 파일 | 상태 | 용도 |
|---|---|---|
| `gen_golden_vectors.py` | **정본 골든** | `src/dfxisp_accel.cpp`의 비트 정확 미러 |
| `verify_binning_cross_check.py` | **검증 게이트** | binning-demosaic의 독립 퍼즈 교차 검증 (`make verify`의 일부); `low_light_isp_pipeline.py`의 `_bin_demosaic_rggb16`을 교차 검증 (2026-07-08 이전: `isp_pipeline_ver1.py`) |
| `baseline_isp_pipeline.py` | **SW 평가 프록시 (정본 일치)** | normal 암: BLC+WB+gain(1.25x)+gamma-2.0, gain/gamma까지 `dfxisp_accel.cpp`와 일치하도록 2026-07-08 재작성. `isp_pipeline_ver1.py`를 대체 |
| `low_light_isp_pipeline.py` | **SW 평가 프록시 (정본 일치)** | low-light 암: 2x2 bin-demosaic+BLC+WB+gain(2.0x)+gamma-2.0 (normal과 동일한 LUT 공유, 정본 일치). BLC_OFFSET 재보정 값은 미확정으로 명시됨 (파일 내 주석 참조) |
| `checker.py` | **SW 평가 프록시 (정본 일치)** | dark-ratio 기반 적응형 모드 선택기, 두 파이프라인 파일과 독립 (상호 import 없음) |
| `newrm_pipeline.py` / `isp_pipeline_ver1.py` | **보관됨 (2026-07-08)** | `tools/archive/`로 이동. gamma가 정본과 달랐음 (2.2/2.5/없음); 위 3개 파일로 대체됨 — 새 작업에서 참조하지 말 것; 과거 ablation 계보 용도로만 유지 |
| `scheduler_sim.py` / `scheduler_sweep.py` | **정책 시뮬레이션** | 히스테리시스/시간적/최소 체류(min-dwell) 스케줄러 트레이드오프 실험. 합성 휘도 시퀀스를 사용 — checker 구현 자체에 대한 검증이 아님 |
| `internal_edge_smoke.py` | **회귀 테스트** | 1x1–8x8 극소/홀수 그리드 스모크 + demosaic 경계 clamp 회귀 테스트 (`make py-verify`). `baseline_isp_pipeline.py`와 `checker.py`에 있는 독립 demosaic 사본을 검사 (2026-07-08 이전: `isp_pipeline_ver1.py`) |

## Ponytail 리뷰 기록 (2026-08-06)

**Pass 2 (모듈 범위: checker / default_ISP / lowlight_ISP), 4건 발견 — 모두
삭제:** default_ISP의 AWB green gain (기준 채널이라 항상 256 = 항등
연산), checker에서 세 번 반복되던 `dark*100`, `sigma_clip`의 인자
재포장, `checker_hysteresis`의 고정 16비트 dwell 카운터 (이제 파라미터로
크기 결정). 재합성: default_ISP **FF 8,803 → 8,794**, LUT/DSP/타이밍은
변동 없음 — 합성기가 이미 ×256>>8을 접어 놓았으므로 절감분은 파이프라인
레지스터였다. 4개 골든 게이트와 두 xsim 벤치 모두 동일한 수치로 재통과.

**Pass 1 (전체 diff 범위):**

3개 암 추가분에 `/ponytail-review` 과잉 설계 게이트를 적용했다 (11건
발견, −195줄 가능). **세 벌로 중복된 골든 CSV 파서만 삭제했다** — 헤더
기반 로더 `tests/golden_csv.hpp`로 대체 (−134줄). 나머지 두 건은
**사유를 기록하고 유지**한다:

- **`src/`의 중복된 헬퍼/상수** (`clamp_i`, `pack_rgb`, `bin_dim`,
  `CCM_Q8`, `GAMMA2_LUT`; 약 45줄): HLS는 각 암을 별도 translation
  unit으로 합성하므로 헤더로 끌어올려도 **실리콘에는 아무 변화가 없다** —
  세 개의 비트 정확 골든 계약을 건드리는 리팩터링에 얻는 이득이 0이다.
  사다리의 1단계("이것을 해야 하는가?")에서 기각.
- **두 Python 골든 간 공유 상수/헬퍼** (약 30줄): C++은 중복인 채로
  Python 쪽만 공유하면 정본 소스가 **비대칭**이 되며, 그 혼란의 비용이
  절감분보다 크다.

## 로컬 C-sim 실행

```bash
cd isppipeline/hls
make csim              # 스모크 테스트
make default-isp-csim  # default_ISP (Vitis-Vision 정렬 암) 골든 비교
make lowlight-isp-csim # lowlight_ISP (제안 저조도 암 v2) 골든 비교
make verify    # 골든 재생성 + packed RGB888 비트 단위 비교 (origin 저장소 전용)
make report    # reports/latest.md 갱신 (아키텍처 게이트 표 포함; origin 저장소 전용)
```

`make verify`의 기대 출력:

```text
python3 tools/gen_golden_vectors.py --out tests/golden_vectors.csv
wrote tests/golden_vectors.csv (1498 rows including header; 1497 data rows; 9 cases)
./build/dfxisp_csim
DFXISP golden vector compare passed (566 pixels)
DFXISP C-sim smoke tests passed
```

## 골든 벡터 형식

CSV는 케이스별 메타데이터(모드, 임계값, 출력 형상, 선택된 RM)와 입력 RAW
행(`kind=raw`), 기대 출력 행(`kind=rgb`)을 함께 담는다. Policy A
(저조도 H/2×W/2)에서는 입력과 출력의 픽셀 수가 다르므로 두 행 종류를
분리했다. 커버리지: 밝음/어두움/혼합/임계값 경계/밝기 회복/홀수 치수.

## Vitis HLS 스캐폴드 실행

기본값: ZCU104 파트 `xczu7ev-ffvc1156-2-e`, 5.0 ns 클럭. 다른 환경에서는
재정의한다. `make hls-report`는 `vitis_hls` 설치 없이도
top/프로젝트/파트/클럭/소스/예상 출력 경로를 출력한다.

```bash
cd isppipeline/hls
make hls-report                              # dry run
make hls                                     # 기본값 DFXISP_HLS_FLOW=csim
DFXISP_HLS_PART=xczu7ev-ffvc1156-2-e \
DFXISP_HLS_CLOCK=5.0 \
DFXISP_HLS_FLOW=csynth make hls              # C-sim 후 합성
```

`vitis_hls`가 `PATH`에 없으면 `make hls`는 안내 메시지와 함께 종료한다.
비표준 위치라면 `VITIS_HLS=/path/to/vitis_hls`를 설정한다.

## HLS top 함수

```cpp
extern "C" void dfxisp_accel(
    const uint16_t* raw_bayer,
    uint32_t* rgb_out,             // 용량 >= width*height
    int width,
    int height,
    int mode,                      // NORMAL / LOW_LIGHT / AUTO
    uint16_t dark_pixel_threshold, // AUTO: 어두운 픽셀 비율 > 임계값 규칙이면 LOW_LIGHT
    int* out_width,                // 선택된 RM의 출력 너비
    int* out_height,               // 선택된 RM의 출력 높이
    int* selected_mode,            // 확정된 모드 (AUTO 해소 결과)
    int* selected_rm,              // 선택된 톤 RM
    int* hyst_flags);              // 슈미트 밴드 플래그 (ap_vld 패브릭 와이어, 2026-08-06)
```

메타데이터를 구조체 포인터 하나가 아닌 **네 개의 개별 스칼라 출력
포인터**로 둔 이유: 구조체 포인터를 `s_axilite`로 선언하는 것은
검증되지 않은(비표준) 패턴이며, 적대적 리뷰에서 지적되었다 (§ Hardware/DFX
structure 하단 참조). 개별 스칼라 포인터는 Vitis HLS에서 완료 후 읽기용
레지스터로 안정적으로 합성된다.

## 하드웨어 / DFX 구조

`src/dfxisp_accel.cpp`는 로컬 C-sim을 위해 표준 라이브러리만 사용하면서,
의도된 static/RM 경계를 따라 분할되어 있다:

- `checker_select_mode()` — static 영역 장면 checker. `AUTO`에서는
  어두운 픽셀 비율로 NORMAL/LOW_LIGHT를 결정하며, (2026-08-06) 추가로
  프레임마다 두 개의 슈미트 밴드 비교(`hyst_flags`: enter 초과 64% /
  exit 미만 60%; 중심 62% 주변 δ=2%p)를 내보낸다. 장면 수준 히스테리시스
  상태 자체는 static 영역 RTL 모듈
  `results/pr_controller/checker_hysteresis.v`에 있으며, 이 모듈이 PR
  컨트롤러 트리거를 직접 구동한다 — 이 단일 프레임 C-sim 진입점은
  상태가 없는 채로 유지된다 (골든 계약 불변).
- `baseline_core12()` / `apply_blc_wb12()` — **공유 static** baseline 코어
  (ver1). BLC + WB (Q8 채널 gain) + CCM (항등)을 **12-bit로** 수행한다
  (마지막 >>4는 tone에서 일어남). **gain/gamma 없음.** normal 경로는
  `demosaic_rggb12()` (RGGB 3x3 Bayer demosaic) 결과를 입력하고,
  low-light 경로는 binning-demosaic 결과(아래)를 입력한다.
- `tone()` — 톤 RM 단계: 노출 gain (12-bit) → >>4 → gamma2.0.
  모드별 gain.
- `run_normal()` — RM_NORMAL_TONE = **gain 1.25× + gamma2.0**. baseline
  코어를 전체 해상도로 실행한다.
- `run_low_light()` / `compute_binned_rgb_row()` — **RM_LOW_LIGHT_TONE**
  (DFX 재구성 모듈 후보). 융합된 RAW 2x2 **binning-demosaic**
  (front; R=좌상, G=avg(우상, 좌하), B=우하 — 채널별 정체성 보존) →
  baseline 코어 → **gain 2.0× + gamma2.0** (back). Vivado DFX 구현에서
  이 톤 RM 슬롯은 RM 호환 블록으로 패키징되며, checker, baseline 코어,
  컨트롤러는 static 영역에 남는다.
- `gamma2()` — γ=2.0을 정수 sqrt `floor(sqrt(255·v))`로 정확히 구현한다
  (Python `isqrt`와 비트 정확). HW에서는 256 엔트리 LUT로 대체 가능.

### 2026-07-02 적대적 리뷰 수정 (중요)

`/codex:adversarial-review --base 0e433f9`가 두 가지 결함을 찾아 수정했다:

1. **색차 손실 버그 (높음):** 이전 저조도 front end는 2x2 RGGB 셀의 4개
   샘플을 **하나의 스칼라 평균**으로 합친 뒤, 그 값을 여전히 Bayer인 것처럼
   다시 demosaic했다 — demosaic 이전에 색 정보가 파괴되었다. 골든 모델
   (`gen_golden_vectors.py`)이 동일한 버그를 미러링했기 때문에 `make
   verify`의 비트 정확 테스트로는 잡을 수 없었고, 보고된 lowlight mAP
   증거(`isp_pipeline_ver1.py`)는 **다른, 채널을 보존하는 알고리즘**을
   측정한 것이어서 실제 HW 후보에 대한 증거가 아니었다. →
   `compute_binned_rgb_row()`에서 binning+demosaic을 한 단계로 융합하여
   수정했으며, `_bin_demosaic_rggb16` (SW ver1)과 비트 정확하다.
2. **RTL에서 검증되지 않은 메타데이터 (중간):** `s_axilite`로 선언된
   `DfxIspResult*` 구조체 포인터 — 합성된 RTL에서 읽을 수 있음이 어떤
   산출물로도 확인된 적 없다 (cosim도 post-check 단계에서 실패하여 미확인).
   → 4개의 개별 스칼라 포인터로 교체 (위 HLS top 함수 참조).

**2026-07-02 20:33 KST 업데이트:** 이 수정을 적용한 상태로
`results/stage4-hw-synthesis-2026-07-02.md` /
`results/stage5-dfx-implementation-2026-07-02.md`를 재합성 / 재구현하고
수치를 갱신했다 (pr_verify는 여전히 PASS; LUT/FF/DSP 감소 — 버그가 만든
불필요한 두 번째 demosaic 로직이 사라졌다). 상세는 두 문서와 `SPEC.md`
§10 참조.

C-sim에는 Vitis 전용 헤더가 필요 없다; HLS pragma만 존재하며 로컬 g++
빌드에서는 무시된다.

## 다음 하드웨어 단계

1. ~~`run_low_light()`의 static scratch binning 버퍼를 실제 스트리밍 라인
   버퍼로 교체.~~ **완료 (2026-07-02)** — `row_buf[3][MAX_BINNED_W]` 3행
   슬라이딩 버퍼로 교체, 비트 정확성 유지.
2. RM_LOW_LIGHT_TONE / RM_NORMAL_TONE을 독립 DFX RM 슬롯 패키징
   플로우로 승격 (§8.3 게이트).
3. DPU가 고정 H×W ABI를 요구하는 경우에만 Policy B (형상 보존
   업샘플/패드)를 추가 (§4.3).
4. Arm 2 (레지스터 전용) 대 Arm 3 (DFX)의 자원/전력/PR 지연 비교 (§7).
   **Arm2 측정 완료** (통합 top, C-합성) —
   `results/stage4-hw-synthesis-2026-07-02.md`. Arm1/Arm3와
   전력/PR 지연은 아직 TODO (Vivado DFX 플로어플랜 + 구현 필요).
5. **(시뮬레이션에서 완료, 2026-08-06)** 패브릭 내부 모드 전환:
   `checker_hysteresis.v`가 새 `hyst_flags` ap_vld 와이어를 소비하여
   `pr_controller.trigger`(request/ack)를 구동한다 — 종단 간 xsim PASS
   (`checker_to_pr_tb.v`). 남은 배선 (재합성 후 실제 `hyst_flags` RTL 포트,
   `drain_ready` ← RM `ap_idle`, ICAPE3)은 Stage 6이다 —
   `checker_hysteresis.md` 참조.

## C-합성 / co-sim 실행 노트 (Vitis HLS 2024.1)

이전 작업 기록의 두 가지 환경 문제가 재현된다:

- **소스 경로 버그:** 중첩된 `build/vitis_hls/...` 프로젝트 경로에서
  `add_files`로 추가한 설계 소스가 csim/csynth의 `HLS_SOURCES`에서
  누락되어 링크 실패가 발생한다. **우회:** hpp/cpp/tb를 평평한 임시
  디렉터리(예: `/tmp/hls_dfxisp/dfxisp_accel/`)에 복사하고 그 디렉터리에서
  실행한다.
- **종료 시 멈춤:** `close_project` 이후 프로세스가 종료되지 않는다
  (작업 자체는 이미 끝난 상태). `timeout -k <grace> <sec> vitis_hls -f
  run.tcl`로 감싸고 로그에서 완료 마커를 확인한다.
- **cosim은 `depth=` 필요:** `m_axi` 인터페이스는 co-simulation에
  `depth=`가 필요하다. 현재 C-sim 픽스처 최대치(16×16 = 256 px)에 맞춰
  설정했다; 전체 설계 범위(1920×1080)를 직접 쓰면 cosim의 auto-wrapc
  하네스가 SIGSEGV를 일으킨다 (내부 스택 할당 오버플로 의심). 측정된
  자원/타이밍에는 영향이 없다 (합성은 이 힌트를 무시함; cosim 전용).
