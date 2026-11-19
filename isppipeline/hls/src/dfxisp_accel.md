# dfxisp_accel.cpp — 설계 배경 및 이력 노트

`dfxisp_accel.cpp`의 소스 주석에서 분리해 낸 설계 배경 문서입니다. 여기에
있는 내용은 코드 동작을 바꾸지 않으며, 상수와 구조가 *왜* 지금과 같은지를
기록한 것입니다.
(출처: haengmini/dfxisp 커밋 `ea6c2de`의 소스 헤더 주석.)

## 1. 파일 개요

DFXISP 코어 C-sim — 공유 baseline ISP 코어 + 상호 배타적인 tone RM 슬롯.
정수 연산만 사용하며, Python golden 모델(origin 저장소의
`tools/gen_golden_vectors.py`)과 비트 단위로 일치(bit-exact)하는 미러입니다.

주요 업데이트 이력:
- **2026-07-20** — BLC 재보정 16/8 → 2/2, checker C1 배포
  (DARK_RATIO_PCT 80 → 62)
- **2026-07-03** — 저조도 전용 BLC 완화 (07-20 재보정으로 대체됨)
- **2026-07-02** — 적대적 리뷰(adversarial review) 수정 2건 (§3)

## 2. BLC / checker 상수의 근거

### BLC_OFFSET12 = BLC_OFFSET12_LOWLIGHT = 2<<4 (블랙 레벨 2)

2026-07-20 재보정 (승인 및 배포 완료). 실제 센서 RAW 스윕(SonyNOD 321장 +
PASCALRAW 321장, BLC ∈ {0,1,2,4,8,16}, 표준 gamma-2.0 파이프라인) 결과,
**모든 arm과 모든 split에서 mAP 피크가 BLC 1–2에 위치**했습니다. 기존 값인
16(normal) / 8(low-light)은 야간 데이터에서 mAP를 최대 5.7배까지 손해 보게
했습니다. 이제 두 모드 모두 측정된 피크 값 2를 공유하며, 이는 2026-07-03의
모드별 완화 방식을 대체합니다.

경위: 2026-07-03 근본 원인 ablation에서 기존의 "BLC/WB" 묶음을 BLC-only /
WB-only / WB-skip 변형으로 나누어 보았고, **구동 요인은 화이트 밸런스
게인이 아니라 블랙 레벨 오프셋**임을 확인했습니다. BLC만 완화(16 → 8)해도
ExDark mAP가 0.062에서 0.150으로 회복(normal arm 대비 +42%)된 반면, WB
완화/생략은 효과가 없었습니다(+7% / −0.5%). COCO는 중립~긍정(+5%)을
유지했으므로, 이 변경은 목표 조건 밖에서도 안전합니다. 이것이
`apply_blc_wb12()`에 `blc_offset` 파라미터가 있는 이유이며, WB/CCM은 두
모드에서 문자 그대로 동일한 코드 경로를 사용합니다.

### DARK_RATIO_PCT = 62 (checker C1)

2026-07-20 배포 (gate 4). **dark16 비율 > 0.62** 규칙이 2026-07-02의 C0 규칙
(dark50 > 0.80)을 대체합니다. C1은 모든 지표에서 C0보다 우수합니다: recall
0.936 대 0.918, false-trigger 0.089 대 0.125, Youden J 0.847 대 0.793 —
그런데도 **RTL 변경은 전혀 없습니다**. dark 픽셀 임계값이 런타임
`dark_pixel_threshold` AXI-lite 레지스터이기 때문입니다:

- **이제 드라이버는 256을 써야 합니다** (이 raw12 도메인에서 16<<4와 같으며,
  데이터셋 raw16 표현에서의 등가값은 16<<8 = 4096).
- 컴파일 시점에는 비율 상수(62)만 바뀝니다.

실제 센서 검증 (gate 3): SonyNOD recall + PASCALRAW false-trigger 비율이
C0 92.9% → C1 41.8%. C1 사양의 Schmitt 히스테리시스 밴드(Δ = 2 %p)는 원래
드라이버 측 정책으로 남겨 두었으나, 2026-08-06부터 fabric에 구현되었습니다
(`results/pr_controller/checker_hysteresis.v`, `hyst_flags` export로
공급 — §7). 여기의 단일 프레임 규칙은 순수한 임계값 비교로 유지됩니다.

## 3. 적대적 리뷰 수정 2건 (2026-07-02)

(1) **Chroma 붕괴 버그 (심각도 높음):** 기존 저조도 front end는 2×2 셀의
RGGB 샘플 4개를 모두 하나의 스칼라로 평균한 뒤, 그 격자를 여전히 Bayer
패턴인 것처럼 다시 demosaic했습니다. 즉 AWB/gain이 실행되기도 전에 chroma가
구조적으로 파괴되었습니다. 더 나쁜 점은 golden 모델도 같은 버그를 그대로
미러링했기 때문에, `make verify`의 bit-exact 테스트로는 이를 절대 잡을 수
없었다는 것입니다(HLS가 자신의 잘못된 golden과 완벽히 일치). 한편 보고된
SW mAP 근거는 *다른*, chroma를 보존하는 알고리즘(R = 좌상단, G =
avg(우상단, 좌하단), B = 우하단)을 사용했으므로, 이 파일이 실제로 계산하던
것에 대한 근거가 전혀 아니었습니다.
**수정:** `compute_binned_rgb_row()`가 2×2 binning+demosaic을 한 단계로
융합해 수행하여, 채널별 진짜 R/G/B를 직접 추출하고(SW 구현과 bit-exact)
그 결과를 공유 `apply_blc_wb12()` 코어에 전달합니다 — 두 번째(Bayer를
가정하는) demosaic 패스는 없습니다. 이로써 3행 슬라이딩 윈도우 헬퍼 계열
전체가 제거되었습니다(binning-demosaic은 자신의 셀에 속한 RAW 2행만
필요).

(2) **메타데이터 RTL 가시성 (심각도 중간):** `DfxIspResult*` 구조체
포인터가 `s_axilite`로 선언되어 있었습니다. 이는 가벼운 슬레이브 측 레지스터
접근을 위한 인터페이스 모드이지, 메모리에 쓰는 구조체 출력용이 아닙니다.
또한 네 필드가 개별 주소 지정이 가능한 레지스터로 합성된다는 것을 확인해
주는 산출물도 없었습니다. 이를 네 개의 별도 스칼라 `int*` 출력 포인터
(out_width / out_height / selected_mode / selected_rm)로 교체했습니다.
이는 완료 후 상태 레지스터를 위한 표준적이고 잘 지원되는 Vitis HLS 관용
방식입니다(rm_*_top 진입점과 동일한 패턴).

## 4. 이전(ver1) 이력

- RAW 도메인 우선 재배치: baseline 코어 = 12비트에서 demosaic → BLC → WB →
  CCM; RM_NORMAL_TONE은 identity 대신 gain+gamma를 갖게 됨;
  RM_LOW_LIGHT_TONE = gain 2.0x + gamma 후단.
- `gamma2()`: 런타임 Newton 방식 isqrt(csynth 리소스를 지배했음 —
  run_low_light에서 28.6k FF / 25.4k LUT)에서, Python으로 한 번 생성한
  256엔트리 ROM 테이블로 변경. 처음에는 `std::array`/constexpr를 시도했으나,
  Vitis HLS에 포함된 gcc-8.3.0 STL이 `-std=c++17`에서 `<array>`를 거부하여
  일반 C 배열을 사용했습니다.
- Bayer 패턴은 RGGB (SW 데이터셋 규약과 통일).

**순서 규칙:** tone RM 슬롯이 공유 baseline 코어를 감쌉니다. 저조도
binning-demosaic은 RAW에서(정밀도 손실 이전에) 실행되며, gain/gamma는 tone
단계에만 존재하고 baseline 코어 내부에는 절대 존재하지 않습니다(중복 없음 —
de-dup 규칙).

## 5. 독립 RM top 두 개 (rm_normal_tone_top / rm_low_light_tone_top)

Stage 5 준비 (2026-07-02): 각 DFX Reconfigurable Module 후보가 **자체**
리소스/타이밍 수치를 가질 수 있도록, 개별적으로 합성 가능한 진입점입니다
(통합 `dfxisp_accel()` top 내부의 서브 인스턴스 breakdown만으로는 부족 —
SPEC.md §10). 같은 translation unit이므로 anonymous-namespace 헬퍼를 직접
호출하며, 동작은 bit-exact로 동일합니다. 합성 전용(csynth "synth" 플로우,
테스트벤치 없음)이며 `make verify`에서는 실행되지 않습니다.

**두 top의 포트 리스트는 정확히 일치해야 합니다**(인자 타입, 순서, 개수
동일) — DFX는 하나의 Reconfigurable Partition에 속한 RM 구현들 사이에서
동일한 포트 리스트를 요구합니다("identical downstream interface contract").
rm_normal_tone_top에서 out_width/out_height는 항상 width/height와 같습니다
(shape 보존).

## 6. cosim `depth=` 힌트에 얽힌 사연

`#pragma HLS INTERFACE m_axi ... depth=2048`의 `depth=`는 C/RTL cosim의
m_axi bus functional model을 위한 메모리 모델 크기 힌트일 뿐이며, 합성된 RTL
동작에는 영향을 주지 않습니다(실제 깊이는 런타임의 width*height).

- `depth=1920*1080` (전체 설계 envelope): ENTER_WRAPC에서 SIGSEGV (wrapc
  하니스의 스택 오버플로로 추정)
- `depth=1024`: 위 문제는 넘겼으나, 테스트 트랜잭션 7개가 모두 완료된 뒤
  ENTER_WRAPC_PC(사후 검사)에서 SIGSEGV — 한 세션에서 cosim BFM이 모든 호출에
  걸쳐 사용하는 *누적* 주소 범위(7회 호출 × 최대 256 px ≈ 1800)에 비해 너무
  작았던 것으로 추정
- `depth=2048`: 채택 — 현재 fixture 세트보다 여유를 둔 값

## 7. Schmitt 밴드 플래그 export (`hyst_flags`, 2026-08-06 추가)

이제 checker는 프레임마다 밴드 비교 플래그 두 개도 export합니다. 이로써
정적 영역 히스테리시스 블록(`results/pr_controller/checker_hysteresis.v`)이
Schmitt 모드 상태를 소유하고 PR 컨트롤러를 직접 트리거할 수 있습니다 —
2026-07-03 채택안(Schmitt delta = 2%p + min-dwell을 HW에서 처리, 판정 경로에
PS 없음)을 구현한 것입니다:

- `hyst_flags[0]` (`DFXISP_HYST_ABOVE_ENTER`) = dark 비율 >
  `HYST_ENTER_PCT` (64%, 진입; 밴드 = 중심 62% 주변 2%p — 단일 프레임
  판정은 `DARK_RATIO_PCT` = 62를 독립적으로 유지)
- `hyst_flags[1]` (`DFXISP_HYST_BELOW_EXIT`) = dark 비율 < `HYST_EXIT_PCT`
  (60%, 이탈); 둘 다 해제 = 밴드 내부
- 인터페이스: `#pragma HLS INTERFACE ap_vld` — fabric 와이어 쌍
  (`hyst_flags[31:0]` + `hyst_flags_ap_vld`)이며 s_axilite 레지스터가
  **아님**; 프레임 완료당 vld 펄스 1회, 잘못된 인자로 인한 조기 반환 시에는
  펄스 없음.
- 강제 NORMAL/LOW_LIGHT 모드에서는 override에 해당하는 플래그를 export합니다.
- 단일 프레임 판정(`selected_mode`)과 기존 모든 출력은 산술적으로 변하지
  않았으며, golden CSV는 bit-exact를 유지합니다(검증: 변경 후 726 px 비교
  PASS). 비용: 정수 비교 1회 + 32비트 출력 1개 추가; §10의 리소스 수치는
  재합성 시 미세하게 달라질 수 있습니다(가정하지 말고 재측정할 것).
