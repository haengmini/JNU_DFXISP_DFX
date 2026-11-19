---
type: spec
title: "DFXISP 시스템 명세 (입력 데이터셋 → 출력)"
project: DFXISP
version: 1.3
created: 2026-07-02
updated: 2026-08-06 — Schmitt 히스테리시스를 패브릭(본 저장소)으로 이동:
  `dfxisp_accel`이 이제 프레임별 밴드 플래그(`hyst_flags`, ap_vld 와이어,
  11번째 인자; 밴드 = 62% 중심 기준 2%p: 진입 64% / 해제 60%)를 내보내고,
  새 정적 영역 모듈 `results/pr_controller/checker_hysteresis.v`가 모드
  상태를 소유하며 `pr_controller.trigger`를 직접 구동한다(request/ack,
  결정 경로에 PS 없음) — 구현되지 않은 채 남아 있던 2026-07-03 채택안을
  구현한 것이며, 이전의 "드라이버 측 정책" 표현(§3.1, §7)은 그 과도기
  상태를 기술한 것으로 대체(superseded)되었다. 엔드투엔드 트리거 체인은
  xsim에서 검증됨(`checker_to_pr_tb.v`). 골든 계약은 변경 없음(csim
  비트 정확 PASS).
  이전 갱신(2026-08-04): (a) 모드별 WB 분리 기각(§4, §11.11): 배포된 공유
  WB가 저조도에 맞게 튜닝되어 있지 않다는 진단은 측정으로 확인되었으나
  mAP가 반응하지 않으므로 분리하지 않음
  (`results/lowlight-wb-mode-split-2026-08-03.md`). (b) RP 경계 표현을
  실제 구현에 맞게 정정(§7, §11.12): 합성된 RP는 "tone만"이 아니라 모드별
  전체 파이프라인을 감싼다. 이전 갱신(2026-07-20): BLC/체커 파라미터를
  07-13–07-20 실제 RAW 캠페인에서 배포된 값으로 갱신(§3.1, §4). 정본
  근거: `results/blc-recalibration-deploy-2026-07-20.md`,
  `results/checker-c1-deploy-2026-07-20.md`,
  `results/checker-oracle-label-gate2-2026-07-20.md`.
target: Zynq UltraScale+ ZCU104 / XCZU7EV (xczu7ev-ffvc1156-2-e)
status: active — 소스 수준 공유 베이스라인 코어 + 상호 배타적 tone RM
  슬롯; 물리적 RP 경계는 모드별 전체 파이프라인(§7, §11.12)
refs: "README.md · RESEARCH.md · isppipeline/hls · results/experiment-report-2026-07-02.md"
---

# DFXISP 시스템 명세

> **이 추출 저장소(JNU_DFXISP_FPGA)에 대한 참고:** `results/*.md`,
> `tools/*`, `RESEARCH.md`, `STRATEGY.md`, `deliverables/` 형태의 참조는
> 원본 저장소
> [haengmini/dfxisp](https://github.com/haengmini/dfxisp)의 문서를
> 가리키며, 대부분은 여기에 포함되어 있지 않다.

> **아키텍처 참고 (2026-07-10):** 2026-07-10 리셋 v2에서 `RESEARCH.md`는
> RM 경계를 tone(gain/gamma)에서 ISP 데이터패스 전체로 넓혔다 — 정적
> 셸(체커 / DFX 컨트롤러 / AXI / 패커)만 정적으로 남고, BLC/AWB/디모자이크/
> CCM/gain/gamma는 모두 두 개의 전체 ISP 파이프라인 RM인
> `RM_NORMAL`/`RM_LOW_LIGHT`가 소유한다. **이 SPEC.md의 본문은 아직 v2로
> 이전되지 않은 현재(v1) 구현을 계속 기술한다** —
> `isppipeline/hls/src/dfxisp_accel.cpp`는 여전히 공유 베이스라인 코어 +
> tone RM 슬롯 구조이다. v1의 연산(파라미터 값, 인터페이스) 자체는
> 올바르고 유효하며, 이 문서는 v2 코드 이전과 함께 갱신될 것이다. 아래를
> 읽을 때 "베이스라인 ISP 코어"는 v1에만 있는 개념임을 유념할 것.

> 전체 체인의 데이터 형식, 연산, 인터페이스, 파라미터를 정의한다: 입력
> (RAW 데이터셋) → 체커 → tone RM 슬롯 → 베이스라인 ISP 코어 → RGB32
> 출력 → 검출기/mAP (v1, 현재 구현). 정본 아키텍처: `RESEARCH.md`;
> 구현: `isppipeline/hls/`. 모든 연산은 **정수(비트 정확)** 이다.

---

## 0. 범위와 두 도메인의 구분

DFXISP에는 두 개의 실행 도메인이 있다. **Bayer 패턴은 RGGB로
통일되었다**(2026-07-02); 남은 차이는 RAW 비트 표현뿐이다:

| 도메인 | 목적 | 입력 RAW | 디모자이크 | 정본 |
|---|---|---|---|---|
| **HW / C-sim** | 하드웨어 경로, 비트 정확 검증 | RAW **RGGB**, uint16 내 12비트 (`>>4`) | RGGB 3x3 | `src/dfxisp_accel.cpp` ↔ `tools/gen_golden_vectors.py` |
| **SW eval** | 데이터셋 규모 mAP/지표 | 데이터셋 RAW **RGGB16** (shift8, `>>8`) | RGGB nearest | `tools/newrm_pipeline.py` |

두 도메인은 이제 **동일한 Bayer 규약(RGGB)** 을 사용한다(2026-07-02 통일:
C-sim GRBG→RGGB). 남은 유일한 차이는 RAW 비트 표현(HW 12비트 대 SW
8비트 shift8)이며, 이를 통해 향후 데이터셋 raw를 C-sim/HW에 직접
스트리밍하여 엔드투엔드 비트 비교를 할 수 있게 된다. 절대적 비트 정확성은
HW 도메인 안에서(합성 픽스처에 대해) 보장되며, SW mAP는 **arm 간 상대
순위**로 판단한다.

---

## 1. 시스템 개요 (엔드투엔드)

```text
[입력 데이터셋]                  [DFXISP 파이프라인]                          [출력/평가]
 RAW Bayer          ──▶  ① Scene checker (모드 결정: dark_ratio + 히스테리시스)
 (raw_bin / fixture)          │
 + labels (COCO-80)           ├─ NORMAL  ─▶ ② baseline core12 (demosaic+BLC+WB+CCM, 12-bit)
                              │                 └─▶ ③ RM_NORMAL_TONE (gain 1.25× + gamma2.0)
                              │                       └─▶ RGB32 (H×W)
                              │
                              └─ LOW_LIGHT ─▶ ③ RM_LOW_LIGHT_TONE.front (2x2 RAW binning-demosaic,
                                                  융합: R=TL, G=avg(TR,BL), B=BR — 채널 보존)
                                                └─▶ ② baseline core12 (BLC+WB+CCM, 12-bit, 두 번째 demosaic 없음)
                                                     └─▶ ③ RM_LOW_LIGHT_TONE.back (gain 2.0× + gamma2.0)
                                                          └─▶ RGB32 (H/2 × W/2, Policy A)
                                                                       │
                              ④ metadata (스칼라 출력 포인터 4개) ──────┤
                                 out_w, out_h, selected_mode, selected_rm
                                                                       ▼
                                                        packed RGB888 0x00RRGGBB
                                                        ─▶ DPU / detector (YOLO·SSD) ─▶ mAP
```

**불변 조건:** 프레임당 정확히 하나의 tone RM(상호 배타적); gain/gamma는
tone RM에만 존재(베이스라인 코어에 중복 없음); 출력 메타데이터가 모드,
RM, 형상을 보고한다. (C-sim 게이트가 자동으로 검증한다.)

---

## 2. 입력 명세

### 2.1 데이터셋 구성

**정본 평가 쌍(실제 RAW, 목표 1의 교차 우위 입증용 — RESEARCH.md §10):**

| 데이터셋 | 조도 | 형식 | 역할 |
|---|---|---|---|
| **PASCAL RAW** | 밝음 | 실제 Bayer RAW | `normal` 모듈이 이겨야 하는 조건 |
| **LOD RAW** | 저조도 | **Sony `.ARW`** | `lowlight` 모듈이 이겨야 하는 조건 |

- 둘 다 실제 센서 RAW로 진짜 Poisson-Gaussian 노이즈를 지니므로,
  저조도 모듈의 비닝(SNR 복원) 정당성이 제대로 검증된다(pseudo-RAW에는
  복원할 노이즈가 없다).
- 어댑터: LOD는 Sony `.ARW`이므로 `tools/aodraw_adapter.py`의 rawpy 경로를
  그대로 적용할 수 있다(파일별 black/white 레벨과 Bayer 위상은 rawpy로
  읽는다). shift8 규약으로 정규화 → 아래 §2.2와 동일한 형식.
- **Arm 비교는 `normal`/`lowlight`/`adaptive`로 한정한다**(색 보정이 없는
  `none`은 배포 가능한 ISP 출력이 아니므로 제외 — RESEARCH.md §1.2).

**이력(대체된 프록시 — 초기 실험; 정성적 결론만 유효):**

| 데이터셋 | 조도 | 경로 | 이미지 수 | 유효 (raw==jpg) |
|---|---|---|---|---|
| COCO_val | 보통 | `data/coco_val/` | 575 | 347 |
| ExDark_val | 저조도 | `data/exdark_val/` | 491 | 260 |
| SonyNOD | 저조도 (실제 센서) | `data/sonynod_test/` | 321 | 321 |

각 데이터셋은 `raw_bin/`(pseudo-RAW 또는 shift8 실제 RAW), `images/`(jpg,
해상도 원본), `labels/`(YOLO txt)로 구성된다. `raw_bin` 크기가 jpg 해상도와
일치하지 않는 프레임은 건너뛴다(유효 프레임만 사용).

### 2.2 raw_bin 형식 (SW eval 입력)
- **레이아웃:** RGGB Bayer. `(0,0)=R (0,1)=G (1,0)=G (1,1)=B`.
- **타입:** 헤더 없는 little-endian `uint16` 배열, 길이 = `W*H`
  (행 우선).
- **스케일:** 값은 8비트를 위로 시프트한 것(`effective8bit = value >> 8`,
  `SHIFT=8`). 범위 0–65280.
- **해상도 (W,H):** 같은 stem의 `images/<stem>.jpg`의 SOF 마커에서
  파싱한다.

### 2.3 HW / C-sim 입력 형식 (정본 하드웨어 경로)
- **레이아웃:** RGGB Bayer (SW 데이터셋과 통일, 2026-07-02).
- **타입:** `uint16`에 저장된 12비트 값
  (`raw12_to_u8(v) = min(v,4095) >> 4`).
- **픽스처:** 합성 격자 프레임(`gen_golden_vectors.py`), 시나리오
  `NORMAL×3 → LOW_LIGHT×3 → NORMAL×1` + 임계값 경계 +
  밝기 복귀 + 홀수 치수.

### 2.4 라벨 형식
- YOLO txt: 한 줄 `class cx cy w h` (0–1로 정규화, 이미지 크기와
  무관).
- **클래스 id:** 두 데이터셋 모두 **COCO-80 id**
  `{0,1,2,3,5,8,15,16,39,41,56,60}`(12개 클래스)를 사용 → 검출기는
  COCO 사전학습이므로 **리매핑이 필요 없다**.

### 2.5 입력 해상도 분포 (실측)
| 데이터셋 | 너비 최소/중앙/최대 | 높이 최소/중앙/최대 | 대표 크기 |
|---|---|---|---|
| ExDark | 200 / 640 / 3200 | 178 / 499 / 3443 | 640×480, 640×427, 500×375 |
| COCO | 240 / 640 / 640 | 160 / 480 / 640 | 640×480, 640×427, 480×640 |

해상도는 가변(고정 아님)이다. RESEARCH의 1280×720@30fps는 HW 프레임 예산
목표이며, SW 데이터셋 입력과는 별개이다.

---

## 3. 파이프라인 단계 명세

### 3.1 ① Scene checker / 모드 결정 (정적 영역)

입력 모드 ∈ {NORMAL(0), LOW_LIGHT(1), AUTO(2)}.

```text
NORMAL     -> selected_mode = NORMAL
LOW_LIGHT  -> selected_mode = LOW_LIGHT
AUTO       -> dark_ratio = count(dark) / (W*H)
              dark = (raw < dark_pixel_threshold)     # RAW 도메인, HW와 SW에서 동일 (checker.py)
              # dark_pixel_threshold: raw16에서 16<<8 (SW 사전 계산); HW raw12
              # 레지스터 값은 관례상 256 (= 16<<4) — "dark16" 통계
              selected_mode = LOW_LIGHT  if  dark_ratio > 0.62  else NORMAL
              (정수 비교: dark_count*100 > 62*(W*H))
```

- **배포 이력:** 초기 0.40 (2026-07-01) → **C0** dark50>0.80
  (2026-07-02 ver2 재보정, Youden J 기준 최적화) → **C1**
  dark16>0.62 (**2026-07-20 정식 배포, 게이트 4**) — 모든 지표에서 C0를
  능가(recall 0.936 대 0.918, false-trigger 0.089 대 0.125,
  J 0.847 대 0.793)하며 HW 변경은 전혀 없다(`DARK_RATIO_PCT` 상수와
  런타임 `dark_pixel_threshold` 레지스터 값만 변경). 같은 배포에서 SW
  미러(`checker.py`)는 **raw 도메인 직접 비교**로 전환되어, 이전의
  luminance<50 근사(디모자이크 후 결정)를 제거하고 HW/보정 도메인과
  정확히 일치시켰다. 642개 실제 RAW 프레임(SonyNOD 321 + PASCALRAW 321)에서
  매니페스트에 사전 계산된 dark16/판정과 불일치가 0건이다. 상세:
  `results/checker-c1-deploy-2026-07-20.md`.
- **Oracle 라벨 재검증 (2026-07-20, 게이트 2, 최종 게이트):** 이중 arm
  렌더(normal/lowlight) + 프레임별 YOLOv8n 검출 델타로 "올바른 모드"를
  데이터셋 출처가 아니라 실제 검출 개선으로 재정의한다. C1의 잔여 naive
  라벨 불일치 154건 중 **89.6%는 라벨 아티팩트**이다(진짜 오류는 10.4%뿐).
  dark16은 "이 장면이 야간인가"는 잘 구분하지만(J 0.847) "저조도 처리가
  이 프레임의 검출에 도움이 되는가"는 잘 구분하지 못한다(oracle J 0.008) —
  그럼에도 C1의 임계값은 oracle 라벨 하에서도 **비용 중립 특성
  (C_miss ≈ C_FA)** 을 유지하므로, 결론은 **재튜닝 불필요**이다. 상세:
  `results/checker-oracle-label-gate2-2026-07-20.md`.
- **히스테리시스 (장면 수준) — 2026-08-06부터 패브릭에 구현:**
  단일 프레임 진입부는 무상태로 유지되지만, 이제 프레임마다 두 개의 Schmitt
  밴드 비교(`hyst_flags`: 진입 초과 64% / 해제 미만 60% — 62% 중심 기준
  δ=2%p, checker-principles 원칙 5)를 ap_vld 와이어로 내보내며, 정적 영역
  모듈 `results/pr_controller/checker_hysteresis.v`가 모드 플립플롭,
  최소 체류(`DWELL_FRAMES`, 기본 1), PR 컨트롤러로의 `pr_trigger`
  request/ack를 소유한다 — 결정 경로에 PS가 없다(PS는 AXI4-Lite로
  `selected_mode`를 관찰만 한다). 이는 2026-07-03 채택안을 구현한 것이며,
  이전의 "드라이버 측 정책" 표현은 구현되지 않은 과도기 상태를 기술한
  것이다. 이력: 스케줄러 수준 시뮬레이션(`tools/scheduler_sim.py`, 원본
  저장소)에서 좁은 밴드 + temporal_N=3 → 불일치 0.015, 스래싱 0을
  측정했다.

### 3.2 ② 베이스라인 ISP 코어 (공유 코드 경로, 모드별 BLC) — ver1

**gain/gamma 없음.** 보정은 **12비트 RAW 도메인에서** 수행되며, 최종 `>>4`는
tone에서 일어난다(ver1 코어의 핵심 아이디어: 정밀도 보존). WB/CCM은 모드
간에 정확히 같은 함수(`apply_blc_wb12`)를 공유하지만, **BLC 오프셋은
모드별**이다(2026-07-03, §11.9 근본 원인 결과에서 유래). 픽셀당:

```text
1. demosaic (RGGB) -> R,G,B 12-bit (0..4095)     # HW/C-sim: 여기서 >>4 없음 (12-bit 유지)
2. BLC   : v = clip(v - blc_offset, 0, 4095)     # normal: 256 (16<<4) / low-light: 128 (8<<4, 완화)
3. WB    : R = clip(R * 286 / 256, 0, 4095)      # Q8 채널 화이트 밸런스(색), 모드와 무관
           G = clip(G * 256 / 256, 0, 4095)
           B = clip(B * 307 / 256, 0, 4095)
4. CCM   : 항등                                  # 구조 유지, 색 변환 없음
return   : R,G,B 12-bit  (>>4 및 gamma는 tone RM에서 수행)
```

> SW eval 프록시(`isp_pipeline_ver1.py`)는 8비트 도메인(>>8) + 부동소수점
> γ LUT를 쓰는 근사이며, HW/C-sim이 정본이다(12비트, 정수 γ). §0 참조.

### 3.3 ③ Tone RM 슬롯 (상호 배타적, 재구성 가능) — ver1

Tone RM 슬롯은 베이스라인 코어를 (front/back으로) **감싼다**. tone = 노출
gain(12비트) → `>>4` → gamma.

```text
tone(v12, gnum, gden) = gamma2( clip(v12*gnum/gden, 0, 4095) >> 4 )
gamma2(v8) = floor(sqrt(255 * v8)) = isqrt(255*v8)     # γ=2.0, 정수 정확, 비트 정확
```

**RM_NORMAL_TONE (NORMAL):** gain **1.25×** (5/4) + gamma2.0. 형상 H×W.
(ver1: 항등 → gain+gamma)

**RM_LOW_LIGHT_TONE (LOW_LIGHT), Policy A:**
```text
front (RAW):  2x2 RAW binning-demosaic, 융합 — R=좌상, G=avg(우상,
              좌하), B=우하  -> (W/2, H/2) R,G,B 삼중항
core       :  apply_blc_wb12(front 출력, blc_offset=128)   # 위 §3.2, 두 번째 demosaic 없음,
                                                           # BLC만 완화 (2026-07-03)
back (tone):  gain **2.0×** (2/1) + gamma2.0
출력 형상:    H/2 × W/2   (bin_dim(d) = max(1, d/2))
```

**주의 (2026-07-02 수정):** 이전에는 4개 샘플을 하나의 스칼라
`(p00+p01+p10+p11)/4`로 평균낸 뒤 여전히 Bayer인 것처럼 다시 디모자이크했다
— 색 정보가 이미 파괴되어 거의 회색조 출력이 나왔다(적대적 리뷰에서
발견). 위의 채널 보존 방식으로 수정되었다(`tools/isp_pipeline_ver1.py`의
`_bin_demosaic_rggb16`과 비트 정확).

**주의 (2026-07-03 수정, BLC 완화):** 근본 원인 절제 실험(§11.6)에서
저조도 mAP 손실의 약 70%가 (WB가 아니라) BLC에서 나온다는 것이 드러났다
— 저조도 경로의 BLC 오프셋을 256 (16<<4)에서 **128 (8<<4)** 로 절반으로
줄였다. ExDark mAP 0.0586→0.1043 (+78%, 처음으로 normal 초과), COCO도
안전(0.2647→0.2857). HW 자원/타이밍은 **완전히 변동 없음**(상수만 변경).
상세: `results/blc-fix-resynthesis-2026-07-03.md`.

### 3.4 데이터플로 순서 결정 (ver1)

ver1 (2026-07-02): 보정(BLC/WB)은 **디모자이크 직후 12비트에서** 수행된다
(선형이므로 RAW 도메인과 등가이며, 최종 `>>4`까지 정밀도를 보존).
저조도 비닝은 **정밀도 손실 이전의 RAW에서** 일어나며, gain/gamma는 tone
RM(코어 이후)에 위치한다(RESEARCH §4.2). 중복 제거 불변 조건(gain/gamma는
tone RM에만 존재)이 유지된다.

---

## 4. 파라미터 표 (모든 상수, 정수)

| 단계 | 파라미터 | 값 | 비고 |
|---|---|---|---|
| checker | DARK_RATIO_PCT | **62** (C1, 2026-07-20 배포, 게이트 4 & 2 종료) — dark16 raw 도메인 직접 비교; 이전의 80 (C0, dark50, 디모자이크 후 Y<50 근사)은 폐기됨 | AUTO→LOW_LIGHT 임계값, `dark_pixel_threshold` 레지스터와 짝을 이룸 (HW raw12 규약 값 256 = 16<<4) |
| baseline core | BLC_OFFSET12 | **32 (= 2<<4)**, normal (2026-07-20 재보정; 이전 256 = 16<<4) | 12비트 블랙 레벨 |
| baseline core | BLC_OFFSET12_LOWLIGHT | **32 (= 2<<4)**, low-light (2026-07-20 재보정; 이전 128 = 8<<4) — normal과 같은 값(모드별 완화 불필요; 2가 두 arm 모두의 실측 최고점) | 12비트 블랙 레벨 |
| baseline core | AWB_R / G / B | 286 / 256 / 307 | Q8 (/256) 화이트 밸런스 — **모드 공유** (모드별 분리를 검토했으나 2026-08-03 기각, §11.11) |
| baseline core | CCM | 항등 (256) | 자리표시자 |
| normal tone | GAIN_NORMAL | 5/4 (1.25×) | 노출 gain (ver1에서 추가) |
| low-light tone | GAIN_LOWLIGHT | 2/1 (2.0×) | 노출 gain |
| tone (공유) | GAMMA | γ=2.0 | `floor(sqrt(255·v))` = isqrt, 정수 정확 |
| raw 변환 | RAW12_MAX / `>>4` (HW) | 4095 / 12→8 비트 | >>4는 tone에서 일어남 |
| raw 변환 | SHIFT (SW 프록시) | 8 | 16→8 비트 |
| 형상 | bin_dim | `max(1, d/2)` | Policy A |

> ver1(2026-07-02) 완전 적용: 12비트 RAW 도메인에서 보정, normal은
> gain+gamma를 갖고, low-light γ는 4.0→2.0(완화). SW 프록시
> (`isp_pipeline_ver1.py`)는 부동소수점 γ2.2/2.5, 8비트 근사이다(HW의
> 정수 γ2.0이 정본) — 이 파일 자체는 이후
> `baseline_isp_pipeline.py` / `low_light_isp_pipeline.py` / `checker.py`
> (정본, HW 상수를 직접 미러링)로 대체되었다(2026-07-08,
> `results/isp-pipeline-recalibration-2026-07-08.md`). BLC/체커 값은
> 2026-07-20 실제 RAW(SonyNOD+PASCALRAW) 캠페인에서 재보정 및 배포되었으며
> — 위 표가 현재의 정본이다.
>
> **전체 BLC 곡선 (SonyNOD 321, 정본 파이프라인, YOLOv8n — 왜 2인가):**
> `map_isp_sonynod_blcfix_yolov8n.csv`(07-08 재보정)가 전 범위를 스윕했다.
>
> | BLC | 0 | 1 | **2 (배포)** | 4 | 8 | 16 |
> |---|---:|---:|---:|---:|---:|---:|
> | lowlight mAP@[.5:.95] | 0.1965 | 0.2130 | **0.2140** | 0.1759 | 0.1030 | 0.0372 |
> | normal mAP@[.5:.95] | 0.1849 | 0.1932 | 0.1900 | 0.1625 | 0.0918 | 0.0344 |
>
> 곡선은 **1–2에서 정점을 이루고 양쪽으로 떨어진다** — 과도한 BLC(4–16)는
> 신호를 파괴하며, **BLC=0 또한 배포된 값보다 나쁘다**(lowlight −8.2%).
> **직관에 반하는 점(2026-08-04 정리):** BLC=2에서도 저조도 픽셀의
> 52–73%가 0으로 클립되지만
> (`results/lowlight-wb-mode-split-2026-08-03.md` §2), 그 클리핑을 없애면
> (BLC=0) 오히려 *더 나빠진다*. 이유는 **gamma 2.0이 sqrt라서 낮은 값을
> 강하게 증폭하기 때문**이다(8비트 1→16, 2→23). BLC=0이 보존하는 것은 대부분
> 센서의 블랙 페데스탈과 암전류 노이즈이며, gamma가 이를 눈에 보이는 회색으로
> 끌어올려 검출을 해친다. 다시 말해 **높은 클리핑 비율은 결함이 아니라
> 노이즈 제거로 작용하고 있다.**
>
> **모드별 색 보정 상수 둘 다 "분리 불필요"로 수렴했다(2026-08-04
> 확정).** BLC는 한때 모드별로 배포되었으나(07-03, 256→128) 실제 RAW
> 재보정에서 **2가 둘 다의 정점**임이 드러났고(07-20), WB는 실제 RAW에서
> 모드별로 재튜닝되었으나 **mAP 무반응**으로 기각되었다(08-03, §11.11).
> 따라서 normal/low-light의 실질적 차이는 **구조적인 것이지 색 보정 상수에
> 있지 않다**(2×2 비닝 여부, 노출 gain 1.25× 대 2.0×) — 베이스라인 코어의
> 색 보정 파라미터는 조도에 대해 견고하다.

---

## 5. 출력 명세

### 5.1 픽셀 형식
- **packed RGB888**, `uint32`, `0x00RRGGBB` =
  `[31:24]=0x00, [23:16]=R, [15:8]=G, [7:0]=B`.
- AXI DMA 정렬을 위해 24비트 대신 32비트 패킹(2의 거듭제곱 폭)을 쓴다.

### 5.2 형상 정책 (Policy A, 형상 변경)

| 모드 | 출력 형상 |
|---|---|
| NORMAL | H × W (입력과 동일) |
| LOW_LIGHT | ⌊H/2⌋ × ⌊W/2⌋ (최소 1) |

- `rgb_out` 버퍼 용량 ≥ `W*H` (저조도는 최대 그만큼만 사용).
- (Policy B = 고정 DPU ABI가 요구할 때에 한해 업샘플/패딩으로 H×W를
  복원; §11, 향후.)

### 5.3 출력 메타데이터

**별개의 스칼라 출력 포인터 4개**(2026-07-02 수정, §6.1 참조):
`out_width`(실제 출력 너비) · `out_height`(실제 출력 높이) ·
`selected_mode`(0=NORMAL, 1=LOW_LIGHT; AUTO는 해소됨) · `selected_rm`
(0=RM_NORMAL_TONE, 1=RM_LOW_LIGHT_TONE). HW에서 각각은 AXI-Lite 읽기
레지스터로 노출되며, DPU 프런트엔드는 출력 크기/모드를 알아야 한다.

> **이전 설계 (구조체 포인터, 적대적 리뷰로 폐기):** 단일
> `DfxIspResult*` 구조체를 `s_axilite`로 선언했으나, s_axilite는 슬레이브
> 전용 제어 인터페이스이며 구조체 필드 쓰기-되돌림이 실제로 합성되는지
> 검증한 산출물이 없었다(cosim도 끝내 완료되지 않았다). 개별 스칼라
> 포인터로 교체했다 — 완료 후 읽기-되돌림에 대한 Vitis HLS의 확립된
> 패턴이므로 신뢰할 수 있다.

---

## 6. 인터페이스 명세

### 6.1 HLS 최상위 함수

```c
extern "C" void dfxisp_accel(
    const uint16_t* raw_bayer,     // input RAW RGGB (W*H)
    uint32_t*       rgb_out,       // output RGB32 (capacity >= W*H)
    int             width,
    int             height,
    int             mode,          // DfxIspMode
    uint16_t        dark_pixel_threshold,  // AUTO checker RAW threshold
    int*            out_width,     // output metadata (individual scalar pointers)
    int*            out_height,
    int*            selected_mode,
    int*            selected_rm,
    int*            hyst_flags);   // Schmitt band flags (ap_vld wire, 2026-08-06)
```

AXI: `raw_bayer`/`rgb_out` = `m_axi` (gmem0/gmem1); 나머지 스칼라 인자,
메타데이터 출력 4개, `return` = `s_axilite`(제어).
`hyst_flags`는 예외이다: `checker_hysteresis.v`에 직접 연결되는 `ap_vld`
패브릭 와이어 쌍(`hyst_flags[31:0]` + `hyst_flags_ap_vld`, 프레임 완료당
펄스 1회)이며 s_axilite 레지스터가 아니다(§3.1; 비트 0 = 진입 초과 64%,
비트 1 = 해제 미만 60% — 중심 62%).

### 6.2 골든 벡터 CSV 형식 (검증 계약)

헤더: `case,in_w,in_h,mode,threshold,out_w,out_h,sel_mode,sel_rm,kind,idx,val`
- 케이스별 메타데이터 반복 + `kind=raw`(입력 RAW, val = 십진수) /
  `kind=rgb`(기대 출력, val = `0xRRGGBB`).
- 입력 픽셀 수(in_w×in_h)와 출력 픽셀 수(out_w×out_h)가 다를 수 있으므로
  두 행 종류는 분리되어 있다.

---

## 7. 하드웨어 / DFX 명세

| 항목 | 값 |
|---|---|
| 대상 디바이스 | ZCU104, `xczu7ev-ffvc1156-2-e` |
| 합성 도구 | Vitis HLS 2024.1 |
| 클럭 목표 | 5.0 ns (200 MHz) |
| static region | AXI/제어 래퍼, 체커/모드 FSM, Schmitt 모드 중재기(`checker_hysteresis.v`, 2026-08-06), 재구성 제어 = **AMD DFX Controller IP (PG374, 2026-08-06 채택**, `dfxc_trigger_adapter.v` 경유; 보조 지연 계측기 `pr_latency_probe.v`는 선택 사항; 커스텀 `pr_controller.v`는 보관됨), 출력/메타데이터 패커 (**베이스라인 ISP 코어는 정적이 아니다** — 아래 RP 경계 행 참조) |
| RM 슬롯 (재구성 가능) | RM_NORMAL_TONE / RM_LOW_LIGHT_TONE (상호 배타적, 동일한 포트 시그니처 = DFX 계약) |
| **RP 경계 (as-built, 중요)** | 합성된 RP(`rm_normal_tone_top`/`rm_low_light_tone_top`)는 **tone만이 아니라 모드별 전체 파이프라인 demosaic→BLC→WB→tone** 을 감싼다. `apply_blc_wb12()`는 **소스 수준에서만** 공유되며 실리콘에서는 RM마다 복제된다. 파티션 핀 3개. 근거: `results/design-limitations-2026-07-03.md` §4.3, `deliverables/verilog/rm_*_tone_top/`, `results/dfx-reimplementation-2026-08-01.md`. 더 세밀한 분할(베이스라인 코어를 진정한 정적 모듈로 만드는 것)은 **시도된 적 없다** |
| 전환 정책 | 프레임 단위가 아닌 장면 단위: `checker_hysteresis.v`의 패브릭 Schmitt δ=2%p + 최소 체류 → `pr_controller.trigger` request/ack; PS는 관찰만 한다(§3.1) |
| 재구성 지연 | 이론적 drain+ICAP+warm-up 분해: **최대 1.72 ms / 일반 6.87 ms** (명세 기반 도출, 보드 실측 아님). 상세 `results/pr-latency-breakdown-2026-07-02.md`. 드라이버/FSM 오버헤드는 TODO (보드) |

**실험 arm:** Arm1 (정적 베이스라인 + normal tone) / Arm2
(레지스터만으로 적응, DFX 없음) / Arm3 (DFX가 tone RM 슬롯을 교체).
절제(Ablation): post-RGB8 gain/lift, dfx_bin, dfx_fp (`dfxisp_rm.*`).

**default_ISP (2026-08-06 신규, 공존 arm):** AMD Vitis Vision L3
`isppipeline` 예제의 단계 순서와 도메인을 따르는 표준 ISP 구현
(`src/default_isp.cpp`; `rm_default_isp_top`은 동일한 6인자 DFX 계약을
공유하므로 같은 RP 슬롯에 들어맞는다). `RM_NORMAL_TONE`이 고정 WB와 항등
CCM으로 디모자이크 **이후** RGB 도메인에서 보정하는 반면, default_ISP는
Vitis를 따른다: **Bayer 도메인 BLC/gain → 디모자이크 → 적응형 AWB → 실제
CCM**. 실측 csynth: LUT 12,659 / DSP 28 / FF 8,794 (RM_NORMAL_TONE의
5,202/12/3,797 대비 LUT 2.43배, 타이밍은 3.650 ns로 동일). **아직 배포된
arm 아님** — mAP 미평가, post-route 미측정, `RM_NORMAL`로의 승격 여부는
미결정(원본 `STRATEGY.md` 미결 질문 #4). 상세: `src/default_isp.md`.

**lowlight_ISP (2026-08-06 신규, 공존 arm):** 제안하는 저조도 arm v2
(`src/lowlight_isp.cpp`, `rm_lowlight_isp_top`, 동일한 6인자 DFX 계약).
**default_ISP의 보정 골격(Bayer BLC, Bayer gain, CCM)을 공유**하며,
**{동일 색 2×2 비닝, 2.0× 노출 gain, GAT/Anscombe VST tone, VST 도메인
에지 보존 디노이즈}** 에서만 다르다 — arm 간 차이를 저조도 특화 부분만으로
좁혀 비교가 통제되도록 한다. GAT tone은 원점에서 선형이므로 gamma 2.0보다
읽기 노이즈 바닥을 절반만큼만 증폭하며(b = 0일 때 정확히 gamma로 퇴화),
분산 안정화 덕분에 디노이즈 임계값을 단일 상수로 둘 수 있다
(sigma_VST ≈ 2.1 LSB). 비닝은 **실제 동일 색 2×2 평균**(R/B 4샘플 +6 dB,
G 8샘플 +9 dB; 실측 +5.6–7.1 dB)이며 노이즈 정류 편향을 피하기 위해
**블랙 레벨 앞단**에 배치된다. 실측 csynth(**배포 구성 = 디노이즈 OFF**,
2026-08-06 절제 실험에 따름): 동일한 3.650 ns에서 LUT 8,115 / DSP 20 /
FF 5,483 / BRAM 11 — v1의 RM_LOW_LIGHT_TONE(4,204/9/3,243/8)의 1.93배이며
**normal arm(default_ISP, 12,659)보다 36% 작다**. 디노이즈 ON 빌드는
12,826으로 측정되어(normal arm보다 큼) 절제 실험에서 이득이 입증되지
않았으므로 배포 RM에서 제외된다. **배포된 arm 아님** — mAP 미평가,
post-route 미측정, 승격 미결정. 상세: `src/lowlight_isp.md`.

---

## 8. 검증 명세 (비트 정확 전파 체인)

| Lv | 대상 | 도구 | 상태 |
|---|---|---|---|
| L0 | Python 골든(기준) | `gen_golden_vectors.py` | ✅ |
| L1 | HLS C-sim (C++ == Python) | `make verify` | ✅ 646 px 비트 정확 |
| L1.5 | C-합성 (실제 Vitis HLS) | `DFXISP_HLS_FLOW=csynth` | ✅ 실측 (§10) |
| L2 | C/RTL co-sim (합성된 RTL == C TB) | `DFXISP_HLS_FLOW=cosim` | 🟡 RTL 실행은 통과(트랜잭션 7/7); 도구 하니스 SIGSEGV로 자동 비트 정확 비교는 끝내 완료되지 않음 (`results/stage4-hw-synthesis-2026-07-02.md` §6b) |
| L3 | RTL 래퍼 시뮬레이션 (AXI-Stream) | Vivado xsim | ⬜ |
| **L4** | **DFX 구현 + pr_verify (패브릭 전용, 비프로젝트 배치 플로)** | **Vivado 2024.1** | **✅ pr_verify PASS, 실제 부분 비트스트림 생성** (`results/stage5-dfx-implementation-2026-07-02.md`) |
| L5 | 보드 HIL (실제 PR, PS/DDR 통합, 실측 전력 및 PR 지연) | ZCU104 | ⬜ 유일하게 남은 단계 |

**아키텍처 게이트 (모두 PASS, `reports/latest.md`):** 베이스라인 코어
비트 정확 / RM_NORMAL_TONE / RM_LOW_LIGHT_TONE / 상호 배타적 RM
선택 / gain·gamma 중복 없음 / 형상 정책 (LOW_LIGHT H/2×W/2).

---

## 9. 평가 명세 (검출 정확도)

- **검출기:** YOLOv8n, YOLOv8s (ultralytics `val`, imgsz=640,
  mAP@[.5:.95] 및 @50), SSDLite-MobileNetV3 (torchvision, COCOeval)를
  교차 검증용으로 사용한다. 정확한 Vitis-AI `tf_ssdmobilenetv1`은 이
  환경에서 실행할 수 없으므로(가중치 없음, TF1.15) **보드 DPU 엔드투엔드
  단계**로 유보한다.
- **조건 표 A–G:** ExDark{A none, B normal, C lowlight} /
  COCO{D none, E normal, F lowlight} / G adaptive (체커의 프레임별
  선택).
- **주요 지표:** mAP@[.5:.95] (0–1 비율, ×100 = %). **arm 순위**(가드레일)로
  판단한다.

---

## 10. 성능 / 자원 (Stage 4 측정, 2026-07-02 — Vitis HLS 2024.1 C-합성)

**측정 완료 (C-합성 + 실제 Vivado DFX 구현, xczu7ev, 2024.1) —
2026-07-02 20:33 KST 적대적 리뷰 수정(색 보존 비닝-디모자이크 + 스칼라
메타데이터 포인터, 커밋 `a2d1b6d`)으로 재합성.** 통합 최상위(`dfxisp_accel`,
두 tone RM 모두 상주, 런타임 모드 선택, DFX 없음)는 **Arm2(레지스터
전용)** 이다. RM_NORMAL_TONE/RM_LOW_LIGHT_TONE은 **pr_verify PASS**를
받은 실제 Reconfigurable Partition으로 재구현되어 부분 비트스트림이
재생성되었으며, 이것이 **Arm3(DFX) 패브릭 전용 측정**이다(PS/DDR 미통합;
절대 전력과 PR 지연(ms)은 보드에서만 측정 가능). **Arm1(정적
베이스라인 전용)도 2026-08-04에 측정되었다** — 표와 §10.1 참조. 상세:
`results/stage4-hw-synthesis-2026-07-02.md`(csynth),
`results/stage5-dfx-implementation-2026-07-02.md`(DFX 구현).

| 지표 | **Arm1 (정적, 2026-08-04 실측)** | **Arm2 (레지스터 전용, 실측)** | **Arm3 (DFX, 실측 — BLC 수정 + pblock 수정 반영 최종, 2026-07-03)** |
|---|---|---|---|
| LUT / FF / BRAM / DSP | **5,202 / 3,797 / 4 / 12** | **8,264 / 5,536 / 9 / 24** (BLC 수정으로 변동 없음) | config1 (static+RM_NORMAL) 라우팅 완료: LUT 3,972 / BRAM 1.5 tile / DSP 12; config2 (static+RM_LOW_LIGHT) 라우팅 완료: LUT 2,927 / BRAM 3.5 tile / DSP 8 (`results/blc-fix-resynthesis-2026-07-03.md`) |
| Fmax @5.0ns | **273.97 MHz** (임계 경로 3.650 ns — Arm2와 동일) | **273.97 MHz** (임계 경로 3.650 ns, 수정 전후 동일) | 이전 pblock(X0Y0:X1Y0)에서 **200 MHz 제약 충족** (WNS config1 +0.619 ns / config2 +1.930 ns, 2026-07-03; `results/dfx-vivado-considerations-2026-07-03.md` §6) — **새 pblock(X1Y0:X2Y0)의 타이밍은 여전히 TODO** |
| pr_verify | — | — | **✅ PASS** (BLC 수정 + pblock 수정 후에도 static 완전 동일; 파티션 핀 **3**개 — 이전 floorplan의 15개와 다름, 원인 미조사) |
| 전체 비트스트림 크기 | — | — | **19,311,211 bytes ≈ 19.3 MB** (변동 없음) |
| 부분 비트스트림 크기 | — | — | **1,447,424 bytes ≈ 1.38 MB** (새 pblock; 이전 686,664 B의 **2.11×** — pblock 용량을 두 배로 한 데 대한 직접적 대가, `results/blc-fix-resynthesis-2026-07-03.md` §5) |
| 재구성 지연 (ms) | — | — | 새 비트스트림 기준 재계산: 최대 **3.618 ms** / 일반 **14.473 ms** (이전 1.72/6.87 ms의 2.11×); 드라이버/FSM 포함 실측은 TODO (보드) |
| normal 모드 전력 (W) | TODO | TODO | TODO (보드 측정 필요) |

### 10.1 Arm1 대 Arm2 — "적응성의 비용" (2026-08-04 신규)

**Arm1이 무엇인가:** Arm1(정적 베이스라인 + normal tone)은 알고리즘적으로
`run_normal()` 그 자체(디모자이크 → BLC/WB/CCM → gain 1.25× + gamma)이며,
AXI 래퍼는 이미 `rm_normal_tone_top`으로 존재했다. 따라서 **Arm1은 새
설계가 아니라 이미 합성된 최상위였고, 표의 `TODO`는 측정의 공백이 아니라
기록 정리의 공백이었다.** 2026-08-04에 이를 확정하기 위해 Arm1과 Arm2를
**동일 소스(BLC 2/2)로 같은 도구 세션에서** 재합성했다(Arm1은 07-03
수치와 정확히 일치 — 상수 변경은 csynth를 바꾸지 않는다는 §11.10의
관찰을 재확인).

> **부수 발견 (저장소 일관성):** 이 재합성 중, 커밋된
> `reports/csynth/dfxisp_accel_ver1_csynth.rpt`가 **LUT 11,217 / FF 7,008 /
> DSP 30** 으로 읽혀 이 표가 인용해 온 8,264 / 5,536 / 24와 불일치했다 —
> 2026-07-02 적대적 리뷰 수정(16:39) 이전의 리포트가 그대로 남아 있었고,
> 같은 날 20:33 재합성 결과(§11.5)는 이 표에만 반영되어 있었다.
> 2026-08-04 측정값으로 교체했다. **표의 수치가 정본이었고 리포트가 낡은
> 것이었다**(재합성이 표의 값을 정확히 재현했다).

| 지표 | Arm1 | Arm2 | Δ (적응성의 비용) |
|---|---:|---:|---:|
| LUT | 5,202 | 8,264 | **+3,062 (+58.9%)** |
| FF | 3,797 | 5,536 | +1,739 (+45.8%) |
| BRAM | 4 | 9 | +5 (+125%) |
| DSP | 12 | 24 | +12 (+100%) |
| Fmax | 273.97 MHz | 273.97 MHz | **0 (동일)** |

**+3,062 LUT의 분해** (Arm2의 인스턴스 분해와 일치, 합계 검증됨):

| 구성요소 | LUT | 비중 | DFX로 제거 가능? |
|---|---:|---:|---|
| `run_low_light` 데이터패스 | 2,110 | 69% | **예** (RM 교체 대상) |
| 체커 + 모드 mux + 추가 제어 레지스터 | 952 | 31% | **아니오** (항상 정적) |

**함의 (목표 2):** 적응성의 대가는 정적 ISP 대비 **+58.9% LUT**이며,
그중 **DFX로 회수 가능한 상한은 2,110 LUT = Arm2 전체 LUT의 25.5%** 이다.
나머지 31%(체커/mux)는 어떤 재구성 방식으로도 제거할 수 없다 — 이 수치가
**순 DFX 이득의 이론적 상한**을 규정한다. 타이밍은 세 arm 모두에서 동일하여
적응성이 Fmax를 희생하지 않음을 확인해 준다.

### 10.2 Arm1/Arm2 post-route 측정 (2026-08-04)

§10.1의 Δ는 csynth 추정치였다. Arm1과 Arm2를 **Vivado에서 실제로 배치 및
라우팅**하여 post-route 축으로 옮겼다(패브릭 전용 tie-off 래퍼, Arm3와
같은 기법: `ap_clk`/`ap_rst_n`만 칩 밖으로 나가고, 나머지 110개 포트는
tie-off + XOR로 관찰되는 출력이다).

| 지표 | Arm1 | Arm2 | Δ (적응성의 비용) |
|---|---:|---:|---:|
| CLB LUT | **3,363** | **4,768** | **+1,405 (+41.8%)** |
| CLB Register | 4,057 | 5,369 | +1,312 (+32.3%) |
| Block RAM Tile | 1.5 | 3.5 | +2 |
| DSP | 12 | 23 | +11 |
| WNS @5.0ns | +0.682 ns | +0.893 ns | 둘 다 제약 충족 |

**csynth 추정과의 비교:** 적응성의 비용은 csynth에서 +58.9% LUT였으나
post-route에서는 **+41.8%** 이다 — csynth가 오버헤드를 과대 추정한다.
방향은 변하지 않는다.

**견고성 검사 (플로에 둔감한 측정):** 동일한 설계를 세 가지 방식으로
구현했다 — (a) 컨텍스트 내 평면(flat) 합성, (b) Arm3와 같은 OOC 합성 + DCP
링크, (c) (b) + config1의 pblock(`CLOCKREGION_X1Y0:X2Y0`, CONTAIN_ROUTING,
EXCLUDE_PLACEMENT) — Arm1은 3,400 / 3,363 / 3,364 LUT, Arm2는 4,769 /
4,768 LUT로 **1% 이내**이다. 수치는 플로 선택이나 floorplan 제약에 따라
움직이지 않는다.

### 10.3 세 arm을 한 축에서 (2026-08-04) — 정본 DFX 절감 수치

**공개된 config1이 재현된다(확인됨).** 2026-08-04에 수정하지 않은
`scripts/dfx/dfx_flow.tcl`로 재실행한 결과 **LUT 2,630 / BRAM 1.5 /
DSP 12** 가 나왔으며, 08-01에 공개된 수치와 정확히 일치한다.

> **정정 기록:** 이 절의 첫 초안(같은 날의 앞선 커밋)은 "공개된 config1이
> 재현되지 않는다"고 주장했다 — **이는 틀렸다.** RTL을 `cp *.v`로 DFX 플로
> 경로에 옮기면서 **gamma-LUT ROM 초기화 `.dat` 파일이 누락**되었고,
> 합성이 그것 없이 실행되었다(따라서 BRAM 1.5→1, DSP 12→10). `.dat`
> 파일과 함께 다시 복사하자 즉시 일치했다. **재현성 문제는 없다.**
> 교훈: HLS `syn/verilog` 출력에는 `.v` 외에 ROM `.dat` 파일이 포함된다 —
> 통째로 옮겨라.

**자원 비교는 평면(flat) 축으로 통일한다.** 파티션 빌드
(`HD.RECONFIGURABLE`)는 동일한 넷리스트에서 평면보다 LUT가 약 24% 낮게
보고된다(RM_NORMAL: 평면 3,363 대 RP 2,544). §10.2 견고성 검사 (c)로
pblock은 원인에서 배제되었다. 원인은 파티션 설계가 구현/보고되는 방식의
차이로 보이며 **미해결**이지만, 실무 규칙은 명확하다 — **파티션 수치를
평면 수치에서 절대 빼지 말 것.** 따라서:

| arm / 모드 | 상주 로직 | CLB LUT | Arm2 대비 |
|---|---|---:|---:|
| Arm1 (정적, normal 전용) | RM_NORMAL | 3,363 | — |
| **Arm2 (레지스터 전용, 둘 다 상주)** | RM_NORMAL + RM_LOW_LIGHT + checker | **4,768** | 기준 |
| **Arm3 (DFX) — normal 모드** | RM_NORMAL | **3,363** | **−1,405 (−29.5%)** |
| **Arm3 (DFX) — low-light 모드** | RM_LOW_LIGHT | **2,344** | **−2,424 (−50.8%)** |

(모두 같은 tie-off 래퍼, 같은 OOC+DCP 플로, 같은 디바이스로 배치 및
라우팅; post-route 측정. RM_LOW_LIGHT: Reg 3,450 / BRAM 3.5 / DSP 8,
WNS +1.661 ns.)

**목표 2 결론:** always-on(Arm2) 대비 DFX는 normal 모드에서 **LUT 29.5%**,
low-light 모드에서 **50.8%** 를 절감한다. 저조도 절감이 더 큰 이유는 2×2
비닝으로 H/2×W/2만 처리되므로 데이터패스가 애초에 더 작기 때문이다.
§10.1의 csynth 기반 추정("DFX 회수 상한 = Arm2의 25.5%")은 **과소
추정이었다** — post-route 측정이 이를 초과한다.

> **파티션 빌드의 용도:** config1/config2(2,630/1,843 LUT)는 위 표와
> **다른 축**이며 자원 비교에 섞어서는 안 된다. 고유한 산출물, 즉
> **부분 비트스트림 크기, `pr_verify`, 파티션 핀, 재구성 지연**(§10 표)에
> 대해서만 인용한다.

Arm2 인스턴스 분해(통합 최상위 내부; 재합성 이후, 순 DFX 이득을 추정하기
위한 기준점):

| 인스턴스 | BRAM | DSP | FF | LUT |
|---|---|---|---|---|
| RM_NORMAL_TONE (`run_normal`) | 1 | 12 | 1,785 | 3,108 |
| RM_LOW_LIGHT_TONE (`run_low_light`) | 5 | 9 | 1,295 | 2,110 |
| AXI/제어 인프라 | 3 | 3 | 2,456 | 3,046 |

**사용률 (xczu7ev 대비):** BRAM 1%, DSP 1%, FF 1%, LUT 4% — 매우 여유롭다.

기대(H3): normal 모드에서 Arm3의 패브릭/전력 < Arm2(저조도 블록이 상주하지
않음). `run_low_light` 인스턴스(5 BRAM / 9 DSP / 1,295 FF / 2,110 LUT —
수정 이전보다 훨씬 작음)가 DFX가 제거할 수 있는 추정 상한이다 — Arm1/Arm3를
확인하려면 별도의 정적 베이스라인 전용 최상위 합성과 Vivado DFX
floorplan(PR / pr_verify / 부분 비트스트림)이 필요하다.

**독립 RM 최상위 측정 (Stage 5 준비, 재합성 이후):**
`RM_NORMAL_TONE`/`RM_LOW_LIGHT_TONE`을 각자의 AXI 인프라를 가진 최상위로
따로 합성했다(DFX 부분 비트스트림 크기에 대한 더 현실적인 추정):

| 최상위 | BRAM | DSP | FF | LUT | Fmax |
|---|---|---|---|---|---|
| `rm_normal_tone_top` | 4 | 12 | 3,797 | 5,202 | 273.97 MHz |
| `rm_low_light_tone_top` | 8 | 9 | 3,243 | 4,204 | 273.97 MHz |

`rm_normal_tone_top`은 수정으로 내부가 변하지 않았으므로 이전 측정과
정확히 일치한다(교차 검증). `rm_low_light_tone_top`은 두 번째 디모자이크
호출을 제거하여 줄었다: LUT −41.3% / FF −31.5% / DSP −40.0%.

> **참고 (초기 최적화 이력):** 첫 csynth에서 `gamma2()`는 런타임 정수
> sqrt(반복 나눗셈)를 사용하여 자원을 5배 이상 부풀렸다(총 FF 58,655 /
> LUT 52,053). 256 엔트리 ROM LUT로 교체하여 위의 수치를 얻었다(FF −88%,
> LUT −78%). 상세는 Stage 4 문서에 있다.

---

## 11. 제약, 가정, 알려진 문제

1. **SW eval은 프록시이다:** pseudo-RAW는 이미 ISP를 거친 JPEG에서
   역변환한 것이며, RGGB nearest, n=71–113, CPU이다. 절대값이 아니라 arm
   순위로 판단한다.
2. **Bayer 통일 (2026-07-02):** HW/C-sim과 SW 모두 RGGB이다. 남은 유일한
   차이는 RAW 비트 표현(HW 12비트 `>>4` 대 SW shift8 `>>8`)이다. (이전
   실험 보고서의 "GRBG 대 RGGB" 주의 사항은 통일 이전의 것이다.)
3. **Stage 1–3의 핵심 발견:** ver0(normal=항등,
   low-light=bin+gain+gamma-4.0)은 세 검출기와 두 데이터셋 모두에서
   **무처리(none)보다 낮은** 점수를 받았다 = mAP 가드레일 실패.
   **ver1 적용 (2026-07-02):** (a) RM_NORMAL_TONE = gain 1.25×+gamma
   **완료**, (b) 저조도 γ4.0→2.0 완화 **완료**, 12비트 RAW 도메인 보정
   **완료**. ver1은 저조도 arm을 약 20% 개선했으나 **여전히 none이
   이긴다**(SW 프록시 상한) → 방향 A 유지(mAP는 최소한의 처리를 요구하며,
   DFX/RM은 자원/전력으로 정당화된다). 남은 수정: (c) 체커 dark-level
   재보정, (d) Policy B / 디노이즈형 RM. 최종 판정은 보드 DPU + 실제 RAW의
   몫이다.
4. **HW 수치를 날조하지 말 것:** Vivado/보드 없이 §10 / L2–L5 수치를
   절대 지어내지 않는다(TODO는 TODO로 남긴다).
5. **적대적 리뷰 수정 (2026-07-02):**
   `/codex:adversarial-review --base 0e433f9`가 두 가지 결함을 발견하고
   수정했다: (a) 저조도 비닝이 4개 샘플을 스칼라로 평균낸 뒤 다시
   디모자이크하여 색을 파괴했다(골든 모델도 같은 버그를 미러링하여 비트
   정확 테스트로 잡을 수 없었고, 보고된 lowlight mAP 증거는 다른, 색을
   보존하는 알고리즘을 측정한 것이었다) — `_bin_demosaic_rggb16`과 비트
   정확한 융합 비닝-디모자이크로 수정(§3.3). (b) 메타데이터가 검증되지
   않은 구조체 포인터 s_axilite 패턴을 사용 — 스칼라 출력 포인터 4개로
   교체(§5.3/§6.1). **`make verify` 646 px 비트 정확 유지; 새 색 보존
   회귀 테스트 추가.** **2026-07-02 20:33 KST: 수정된 소스로 Vitis HLS
   csynth + Vivado DFX 재구현 완료(pr_verify PASS, 비트스트림 크기
   바이트 단위 동일). §10을 새 수치로 갱신.**
6. **저조도 RM이 mAP를 끌어올리지 못하는 이유 — 절제 실험 측정
   (2026-07-02):** 이전의 추측 "H/2 해상도 손실이 원인"(experiment-report
   §5.2)을 5단계 절제 실험(`tools/isp_pipeline_ablation.py`)으로
   검증했다: **원인은 조도에 따라 다르다.** ExDark(저조도)에서 해상도
   손실의 기여는 −1.4%에 불과하고 **BLC/WB(두 RM이 공유하는 베이스라인
   코어)가 손실의 약 70%**(−49.6 %p)를 차지한다 — RM 고유의 문제가 아니라
   공유 코어의 정적 WB gain이 저조도 색 통계를 왜곡하는 것이다. 반대로
   COCO(정상 조도)에서는 해상도 손실이 지배적이다(−11.7%; BLC/WB는
   −1.9%뿐). 상세: `results/lowlight-rm-map-rootcause-2026-07-02.md`.
7. **DFX 재구성 지연 — 이론적 단계별 분해 (2026-07-02):** drain(실측,
   74–171 사이클), ICAP 전송(686,532 B 페이로드 ÷ AMD UG570 ICAPE3 사양
   대역폭: 최대 1.72 ms / 일반 6.87 ms), warm-up(실측)으로 분해했다. ICAP
   전송이 전체의 99.9% 초과를 차지한다(drain/warm-up은 µs, ICAP은 ms).
   PR 컨트롤러가 합성되지 않아 드라이버/FSM 오버헤드는 계산할 수 없다 —
   TODO(보드)로 남는다. **더 정밀한 Vivado(XSIM) 시뮬레이션을 시도했으나
   (실제 부분 비트스트림을 ICAPE3 UNISIM 모델에 스트리밍), 격리된
   테스트벤치가 트리거→완료 신호를 끝내 만들어내지 못했다(SYNC는
   성공; 3가지 완료 방식 모두 실패 — 원인/한계 분석 포함, 있는 그대로
   기록)** — 페이로드 워드 수(171,633)는 파일에서 직접 검증하여
   반영했다. 상세: `results/pr-latency-breakdown-2026-07-02.md`,
   `results/pr-latency-vivado-sim-2026-07-02.md`.
8. **설계 한계 종합 + 실무 DFX 고려사항 (2026-07-03):** 다섯 계층(알고리즘 /
   SW eval / HW 합성 / DFX 구현 / 시뮬레이션)의 한계를 통합했다
   (`results/design-limitations-2026-07-03.md`). 같은 날 타이밍 제약
   재구현에서 실제 WNS를 얻었고(§10 Fmax 행), pblock이 사실상 하나의
   클럭 영역만 덮고 있음(둘 중 하나의 기여는 0.06%)을 발견했으며, 설계에
   `ICAPE3`/`STARTUPE3`가 전혀 없음을 확인했다 — 즉 PR 컨트롤러가 아직
   존재하지 않는다. Vivado DFX 문제 해결 전체 세트(I/O 핀 초과,
   SNAPPING_MODE, black-box+lock 방법론, DRC 우회책)를 체크리스트로
   정리했다. 상세: `results/dfx-vivado-considerations-2026-07-03.md`. 두
   문서는 실행 가능한 단계별 전략으로 재구성되었다:
   `results/improvement-strategy-2026-07-03.md`(유일한 진짜 차단 항목은
   PR 컨트롤러의 부재이며, 이를 해결하면 나머지는 병렬로 진행할 수 있다).
9. **Phase 0–2 즉시 실행 (같은 날 후속):** 6개 항목 모두 실행 — 독립
   골든 모델 교차 검증 게이트(`make verify`에 편입), pblock 클럭 영역
   편향 원인 규명(X0 열 Y0–Y3 = PS 매크로, SLICE 0개), 저조도 WB/BLC 분리
   절제 실험(**BLC 완화가 진짜 승자** — ExDark mAP 0.062→0.150, normal보다
   42% 높음, COCO에는 무해), 1차 PR 컨트롤러 FSM 설계 + 시뮬레이션(워드
   수 기반 완료 판정이 전날의 PRDONE 한계를 우회; 트리거→완료 1.716 ms
   실측 — 명세 기반 추정과 교차 검증), pblock 재-floorplan
   (X1Y0:X2Y0, 용량 2배: LUT 8,640→19,200). 상세:
   `results/phase0-2-execution-2026-07-03.md`.
10. **BLC 완화의 정본화 + 전체 재합성 (같은 날 후속):** 9번 항목의 절제
    실험 승자(BLC 완화)를 정본 `src/dfxisp_accel.cpp` /
    `gen_golden_vectors.py` / `isp_pipeline_ver1.py`에 적용했다
    (`apply_blc_wb12`에 `blc_offset` 파라미터 추가; 저조도만 128, normal은
    256 유지). 표준 샘플 재측정(n=71/80): ExDark lowlight 0.0586→**0.1043**
    (+78%, **처음으로 normal 초과**), COCO 0.2647→0.2857 (+8%, 무해). HLS
    csynth 세 대상 모두 자원 동일(순수 상수 변경). Vivado DFX를 9번 항목의
    pblock 수정과 함께 재구현: pr_verify는 PASS를 유지하지만 **부분
    비트스트림이 2.11배로 커졌다**(686,664 B→1,447,424 B) — pblock 용량을
    두 배로 한 데 대한 직접적 대가(재구성 지연도 2.11배: 최대 1.72 ms→3.62
    ms). 상세: `results/blc-fix-resynthesis-2026-07-03.md`.
11. **모드별 저조도 WB 분리 — 검토 후 기각 (2026-08-03, 실제 RAW 최종
    판정):** "gain/gamma를 tone RM으로 옮기고 normal/low-light를 독립
    모듈로 분리"하자는 제안의 하위 질문으로서 저조도 WB 재튜닝을
    측정했다. **진단은 확인되었다** — 배포된 공유 WB(286/256/307)는
    PASCAL(밝음)이 원하는 B gain의 0.92배(거의 정확)이지만 SonyNOD(저조도)가
    원하는 것의 **0.50배**에 불과하며, 두 조건은 B에서 **1.84배** 차이가
    난다(gray-world 측정, n=321×2). **그러나 검출은 반응하지 않는다** — WB
    제거부터 2배 과보정까지 전 범위에 걸친 mAP 변동폭은 **0.0020으로, BLC
    레버(0.0876)의 1/44** 이며, 채널 분리 실험이 효과를 반증했다(R만
    −0.05% / B만 −0.19% / 둘 다 +0.61% = 물리적 메커니즘이 없는 초가산적
    패턴 = mAP 지터). 채널별 전역 gain은 에지/형상을 움직이지 않는 대각
    선형 변환이며, WB는 BLC 클리핑 **이후**에 위치하므로 이미 0인 저조도
    픽셀 52–73%에는 작용할 수 없다. **결정: 분리하지 않음**(HW 상수 변경
    없음; 재합성이나 골든 재생성 불필요). WB는 이제 세 번(07-02 통합 /
    07-03 분리 / 08-03 실제 RAW) 시험되었고 모두 "레버가 아님"으로
    수렴했다 — 추가 실험 불필요. 부산물로 §1.4가 미측정으로 남겨 둔 포화율
    공백을 채웠다(최대 2.13%; 우려했던 11.25% 과포화는 현재 파라미터에서
    재현되지 않는다). 상세: `results/lowlight-wb-mode-split-2026-08-03.md`.
12. **RP 경계 표현 정정 (2026-08-04, 문서-구현 불일치 해소):** 이 문서의
    이전 개념도는 "베이스라인 코어는 정적이고 tone RM만 재구성 가능"으로
    읽혔으나, **실제로 합성된 RP는 모드별 전체 파이프라인
    (demosaic→BLC→WB→tone)을 감싼다** — `apply_blc_wb12()`는 소스
    수준에서만 공유되며 실리콘에서는 RM마다 복제된다. 사실 자체는
    `design-limitations-2026-07-03.md` §4.3에 기록되었으나 정본 명세에
    반영되지 않아 §7과 모순되었다 — 2026-08-04에 §7의 "RP 경계" 행에
    명시했다. **기능 버그가 아니라 문서 부채**이며, `pr_verify`는 계속
    PASS한다. 남은 선택지(베이스라인 코어를 진정으로 정적으로 만듦 = RP를
    tone만으로 축소)는 `STRATEGY.md` 미결 질문 #4로 미결 상태이다.

---

## 12. 용어집

| 용어 | 의미 |
|---|---|
| DFX / DPR | Dynamic Function eXchange / 부분 재구성 |
| RM | Reconfigurable Module (부분 비트스트림으로 교체되는 단위) |
| tone RM slot | **(v1 전용)** gain/gamma/비닝을 담는 상호 배타적 재구성 영역. v2(RESEARCH.md §0)에서는 RM 경계가 ISP 데이터패스 전체로 넓어지며 이 개념은 대체된다 |
| baseline ISP core | **(v1 전용)** 공유 demosaic+BLC+WB+CCM 단계(12비트, tone이 감쌈), gain/gamma 없음. v2에서는 이 공유 정적 단계가 사라지고 각 RM이 전체 파이프라인을 소유한다(RESEARCH.md §0/§3) |
| Policy A / B | 형상 변경(H/2×W/2) / 형상 보존(업샘플-패드) |
| guardrail | RM은 mAP ≥ 베이스라인(예: none/레지스터 전용)일 때만 채택된다 |
| arm | 실험 비교군 (정적 / 레지스터 전용 / DFX / 절제) |

문서 끝 — 무엇이든 변경될 때 `RESEARCH.md`, `src/dfxisp_accel.cpp`,
`tools/*`와 동기화를 유지할 것.
