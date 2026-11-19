# default_ISP — Vitis Vision 정렬 표준 ISP arm (2026-08-06)

`src/default_isp.cpp` / `include/default_isp.hpp` 설계 노트. 소스 주석에서
분리한 문서.

## 0. 왜 만들었나

기존 `RM_NORMAL_TONE`(`dfxisp_accel.cpp`)은 AMD Vitis Vision L3
`isppipeline` 예제와 **스테이지 순서와 스테이지 도메인이 다르다**(2026-08-06
실제 소스 대조로 확인). 따라서 논문에서 normal arm을 "표준 ISP 기준선"이라
부르려면 상수 차이가 아니라 **구조 차이**를 메워야 하며, 그래서 Vitis Vision
순서를 그대로 따르는 별도 arm을 만들었다. 2026-07-03에 제안되어 origin
저장소의 ROADMAP #7에 미착수로 기록되어 온 **Vitis-first 리팩터의 첫
산출물**이다.

**추가형(additive)이다.** `RM_NORMAL_TONE`, 그 golden 계약, 배포된
BLC/checker 결정은 손대지 않았다. default_ISP를 배포되는 normal 모드 RM으로
승격할지는 별개의 결정이다(origin `STRATEGY.md` 열린 질문 #4).

## 1. 파이프라인 대조 (Vitis Vision vs 두 arm)

| # | Vitis Vision `isppipeline` | **default_ISP (신규)** | RM_NORMAL_TONE (기존) |
|---|---|---|---|
| 1 | blackLevelCorrection — **Bayer** | ✅ 동일 (감산 + 레인지 복원) | demosaic가 먼저 |
| 2 | gaincontrol — **Bayer**, R/B 위치별 | ✅ 동일 (Q8 286/307) | — |
| 3 | demosaicing | ✅ RGGB bilinear | RGGB bilinear |
| 4 | AWB — **RGB, 프레임 적응형** | ✅ gray-world 적응형 (bypass 가능) | 고정 상수 WB (RGB) |
| 5 | colorcorrectionmatrix | ✅ **실제 3×3 Q8 행렬** | identity placeholder |
| 6 | quantization & dithering | △ `>>4`만 (디더링 생략) | `>>4` |
| 7 | gammacorrection (LUT) | ✅ 동일 LUT (γ2.0) | 동일 LUT |
| 8 | rgb2yuyv (출력 CSC) | ✗ 의도적으로 생략 → RGB888 | RGB888 |
| — | (게인) | 없음 — 노출 게인은 tone RM의 몫 | gain 1.25× (tone) |

## 2. Vitis와의 의도적 편차 3건 (근거 포함)

1. **출력 CSC(rgb2yuyv) 생략** — 소비자가 DPU/검출기라 RGB888을 그대로
   받는다(AXI 정렬용 32-bit 패킹 유지, SPEC §5.1). YUYV로 바꾸면 이득 없이
   평가 경로 전체가 바뀐다.
2. **디더링 생략** — Vitis의 quantization&dithering은 비트심도를 줄일 때
   밴딩을 완화하는 선택적 스테이지다. 검출 mAP에 미치는 영향이 확인된 바
   없어 `>>4` 절단만 남겼다(나중에 추가할 수 있음).
3. **AWB 통계원** — Vitis는 **직전 프레임 히스토그램**(1프레임 지연,
   double-buffer)으로 게인을 구한다. default_ISP는 **현재 프레임의 Bayer
   사이트 평균(gray-world)** 을 쓴다: 프레임 버퍼가 없고 지연도 없는 대신
   raw를 한 번 더 읽는다(§4). 통계량도 히스토그램 정규화가 아니라 gray-world라
   더 단순하다 — "Vitis와 같은 알고리즘"이 아니라 "같은 위치에서 같은
   역할을 하는 적응형 AWB 스테이지"로 읽을 것.

## 3. 상수

| 스테이지 | 상수 | 값 | 근거 |
|---|---|---|---|
| (1) BLC | `BLC_LEVEL12` | 32 (= 8-bit 2 << 4) | 2026-07-20 실제 RAW 재보정으로 배포된 값 |
| (1) BLC | `BLC_MUL_Q8` | 258 | `round(256 × 4095/(4095−32))` — 감산으로 잃은 레인지를 복원(Vitis 방식) |
| (2) gain | `GAIN_R_Q8` / `GAIN_B_Q8` | 286 / 307 | 기존 arm의 WB 값과 동일 — 두 arm은 게인을 **얼마나** 주느냐가 아니라 **어디에** 적용하느냐가 다르다 |
| (4) AWB | 클램프 | Q8 [64, 1024] | 0.25×–4× |
| (5) CCM | 3×3 Q8 | 각 행의 합 = 256 | 중성 회색 보존. **센서 캘리브레이션 전까지는 placeholder** — 색정확도 주장 금지 |
| (7) gamma | LUT | γ2.0 `isqrt(255v)` | `dfxisp_accel.cpp`와 byte-identical — 톤 축에서 arm 간 비교가 가능하도록 |

## 4. 구현상 참고 사항

- **BLC/gain은 버퍼 없이 즉시(on-the-fly) 적용한다**: demosaic 윈도우가
  픽셀을 읽을 때마다 보정이 실행된다. 보정이 pointwise라 결과는 동일하고
  프레임 버퍼가 필요 없지만, 같은 픽셀이 최대 9회 재보정된다(연산 중복을
  치르고 저장 공간을 아끼는 트레이드오프).
- **AWB는 raw를 두 번째로 읽는다.** checker의 프레임 전수 스캔과 합치면
  프레임당 gmem0 읽기 횟수가 늘어난다 — 보드 bring-up 시 대역폭 예산에
  반영할 것.
- **누산기 폭**: 1920×1080×4095 ≈ 8.5e9 → 64-bit 누산기를 쓴다.
- CCM의 음수 계수 때문에 누산값이 0 미만이 될 수 있어 **shift 전에 0으로
  floor**한다 — C++에서 구현정의(implementation-defined)인 음수 우측
  시프트를 피하고 Python golden과의 bit-exact를 보장하기 위함이다.

## 5. 검증

| 게이트 | 상태 |
|---|---|
| Python golden ↔ C++ bit-exact (`make default-isp-verify`) | ✅ 528 px, 10 케이스 (평탄 / 그라디언트 / 색캐스트 / 포화 / 홀수 크기 / 1×1) |
| BLC가 Bayer 도메인에서 demosaic에 선행 (pedestal 이하 → 순흑) | ✅ |
| AWB가 적응형으로 동작 (색캐스트에서 채널 불균형 감소) | ✅ |
| CCM이 중성 보존 (행 합 256 → 평탄 입력에서 채널 편차 ≤ 8) | ✅ |
| 포화 입력이 RGB8을 오버플로하지 않음 | ✅ |
| DFX 계약 (`rm_default_isp_top`, 6-인자, `default_isp(AWB_ON)`과 동일 출력) | ✅ |

이 인수인계(handover) 저장소에서 `make default-isp-csim`은 커밋된 golden CSV를
기준으로 실행된다. `make default-isp-golden`은 origin 저장소의 `tools/`
Python이 필요하다(`make verify`와 같은 상황).

## 6. 실측 자원/타이밍 (Vitis HLS 2024.1, xczu7ev, 5.0 ns)

`reports/csynth/rm_default_isp_top_csynth.rpt` — flat-tempdir 우회로 합성했다.

| 최상위 | BRAM_18K | DSP | FF | LUT | 추정 주기 |
|---|---:|---:|---:|---:|---:|
| **`rm_default_isp_top`** (default_ISP) | 4 | **28** | **8,794** | **12,659** | 3.650 ns |
| `rm_normal_tone_top` (RM_NORMAL_TONE) | 4 | 12 | 3,797 | 5,202 | 3.650 ns |

> **2026-08-06 ponytail 리뷰 후 갱신:** AWB의 녹색 게인은 구조상 항상 256
> (녹색이 기준 채널)이라 out-param, 곱셈, 클램프가 모두 항등이었다. 이를
> 제거하고 재합성한 결과 **FF 8,803 → 8,794(−9)** 이며 LUT, DSP, 타이밍은
> 변하지 않았다. 합성기가 이미 ×256>>8을 시프트로 접고 있었으므로 절감분은
> 곱셈기가 아니라 파이프라인 레지스터였다. 소스를 깔끔히 정리한다고 해서
> 반드시 실리콘 이득으로 이어지지는 않는다는 사례다.

**해석:** default_ISP는 기존 normal arm 대비 **LUT 2.43배, DSP 2.33배**다.
증가분은 구조 자체에서 나온다 — (a) AWB 통계 패스(전수 스캔 + 64-bit
누산기 + 나눗셈), (b) 실제 CCM(항등이 아니라 9-곱셈 행렬), (c) BLC 레인지
복원 곱셈. **타이밍은 동일**(3.650 ns = 273.97 MHz)하므로 표준 구조를
채택해도 Fmax는 희생되지 않는다.

> **주의:** 위 수치는 csynth 추정치다. SPEC.md §10.3의 arm 비교표는 post-route
> flat 축이므로 **섞어 쓰지 말 것**. default_ISP를 그 비교에 넣으려면
> 동일한 tie-off 래퍼로 post-route를 따로 돌려야 한다(미실행).

## 7. 남은 일

1. **mAP 미평가** — default_ISP의 검출 성능은 측정된 적이 없다. SW 프록시
   (`tools/baseline_isp_pipeline.py`, origin 저장소)는 아직 RM_NORMAL_TONE을
   그대로 따라 하므로, default_ISP를 평가하려면 대응하는 Python 프록시(또는
   `gen_default_isp_golden.py` 재사용)를 mAP 하네스에 연결해야 한다.
2. **post-route 실측** — §6의 주의 참조.
3. **채택 여부 결정** — default_ISP를 `RM_NORMAL`로 승격할지는
   `STRATEGY.md` 열린 질문 #4(RP 경계 서사)와 함께 결정한다. 승격하면
   golden 재생성과 checker 상수 재검토가 따라온다.
4. **CCM 캘리브레이션** — 현재 행렬은 placeholder다(§3).
