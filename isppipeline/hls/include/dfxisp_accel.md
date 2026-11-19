# dfxisp_accel.hpp — 인터페이스 설계 노트

`dfxisp_accel.hpp`의 소스 주석에서 분리해 낸 문서입니다. API를 사용하는 데
필요한 최소한의 내용(픽셀 포맷, 용량 규칙)은 헤더에 남아 있고, 이 파일에는
경로 다이어그램과 설계 근거를 담았습니다.
(출처: haengmini/dfxisp 커밋 `ea6c2de`.)

## 아키텍처 — 두 가지 처리 경로

공유 baseline ISP 코어 + 상호 배타적인 모드별 tone RM. tone RM 슬롯이 공유
baseline 코어를 *감쌉니다*:

```
NORMAL:
  raw -> demosaic (RGGB)
      -> baseline_isp_core (BLC + WB + CCM, no gain/gamma)
      -> RM_NORMAL_TONE (gain 1.25x + gamma 2.0)
      -> RGB32  (H x W)

LOW_LIGHT:
  raw -> RM_LOW_LIGHT_TONE.front (2x2 RAW binning-demosaic, fused: this IS
         the demosaic step for the binned grid, preserving per-channel
         identity — R = top-left, G = avg(top-right, bottom-left),
         B = bottom-right. NOT a 4-sample scalar average re-demosaiced,
         which would collapse chroma)
      -> baseline_isp_core (BLC + WB + CCM — same function as the NORMAL
         path, applied to the binned RGB instead of the full-res demosaic
         output)
      -> RM_LOW_LIGHT_TONE.back (gain 2.0x + gamma 2.0)
      -> RGB32  (H/2 x W/2, shape-changing Policy A)
```

C-sim golden gate로 증명된 불변 조건:
- 프레임당 정확히 하나의 tone RM (상호 배타적)
- gain/gamma는 tone RM에만 존재하며 baseline 코어에는 중복되지 않음
- 출력 메타데이터가 모드, 선택된 RM, 출력 shape을 보고함

## 입력 데이터에 대하여

입력은 **12비트 RAW Bayer RGGB**(uint16_t 컨테이너 안)입니다. 실험 입력은
실제 센서 RAW(PASCALRAW — Nikon D3200, Sony NOD — RX100 VII; 이 저장소의
`data/` 샘플과 `meta.json` 참조)를 변환한 것이고, csim 테스트벤치는 합성
벡터(`tests/golden_vectors.csv`)를 사용합니다. 초기 문서에서 볼 수 있는
"pseudo-RAW"라는 표현은 개발 초기 단계의 합성/파생 RAW 벡터를 가리킨 것이며,
현재 실험 환경은 실제 RAW를 입력으로 사용합니다.

## 메타데이터 인터페이스 근거 (2026-07-02 적대적 리뷰)

메타데이터 출력은 단일 `DfxIspResult*` 구조체 포인터(s_axilite)에서 네 개의
별도 스칼라 `int*` 출력 포인터로 변경되었습니다. s_axilite 위의 구조체
포인터는 검증되지 않은 HLS 패턴이며(s_axilite는 슬레이브 전용 제어
인터페이스이지 메모리에 쓰는 마스터가 아님), 구조체 필드가 개별 주소 지정이
가능한 레지스터로 합성된다는 것을 확인해 주는 산출물(인터페이스 리포트,
cosim)도 없었습니다. 별도의 스칼라 출력 포인터는 완료 후 상태/리드백
레지스터를 위한, 잘 정립된 Vitis HLS 관용 방식입니다. 어떤 포인터든 null일
수 있습니다(해당 필드의 메타데이터 쓰기는 생략됨).

같은 리뷰에서 헤더 드리프트도 바로잡았습니다: RM_NORMAL_TONE은 "identity
bypass"가 아니며(ver1에서 gain+gamma가 추가됨), RM_LOW_LIGHT_TONE의 tone은
gamma-4.0이 아니라 gamma-2.0입니다.

## `hyst_flags` 출력 (2026-08-06 추가)

11번째 인자 `int* hyst_flags`는 프레임별 Schmitt 밴드 비교 결과
(`DfxIspHystFlag` 비트: above-enter 64% / below-exit 60%; 중심 62% 주변
δ=2%p)를 fabric 와이어(`ap_vld` 인터페이스, s_axilite 아님)로 export하며,
이를 받는 `results/pr_controller/checker_hysteresis.v`가 모드 상태를 소유하고
PR 컨트롤러를 직접 트리거합니다. C-sim에서는 null일 수 있습니다(쓰기 생략).
자세한 내용: `src/dfxisp_accel.md` §7 및 `checker_hysteresis.md`.
