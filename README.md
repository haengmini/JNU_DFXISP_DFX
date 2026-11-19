<!--
=============================================================================
File   : README.md
Date   : 2026-08-06 KST
Function: JNU_DFXISP_FPGA — 연구용 저장소 haengmini/dfxisp에서 추출한
          인수인계용 저장소로, FPGA 실험에 필요한 것만 담고 있다:
          HLS 소스, DFX 플로우 스크립트, 실제 RAW 샘플 데이터
          (PASCALRAW / Sony NOD), 그리고 SPEC. FPGA 실험 담당자는
          이 저장소만 clone하여 Stage 6(보드) 작업을 시작할 수 있다.
Origin : https://github.com/haengmini/dfxisp (2026-08-06 추출, 커밋
         ea6c2de 기준)
=============================================================================
-->
# JNU DFXISP — FPGA 실험 저장소

연구용 저장소
[haengmini/dfxisp](https://github.com/haengmini/dfxisp)에서 추출하되
**FPGA 실험에 필요한 것만** 남겼다: HLS 소스, Vivado DFX 플로우, 실제 RAW
샘플 데이터, 그리고 설계 사양(SPEC.md). ML 평가 도구, 중간 보고서, 전체
데이터셋은 원본(origin) 저장소에 있다.

## 타깃 / 도구

| 항목 | 값 |
|---|---|
| 보드 | ZCU104 (Zynq UltraScale+ `xczu7ev-ffvc1156-2-e`) |
| 도구 | Vivado / Vitis HLS **2024.1** |
| 목표 클럭 | 5.0 ns (200 MHz) |
| 현재 단계 | Stage 4–5 완료 (HLS 합성 + DFX 구현, 패브릭 전용); **Stage 6(보드)는 미착수** |

## 디렉터리 구성

```
SPEC.md                          # 정본 설계 사양 — §6–7 구조/RP 경계,
                                 #   §10 측정된 자원/타이밍
isppipeline/hls/
  src/, include/                 # HLS C++ 소스 (dfxisp_accel = static+RP, dfxisp_rm = RM들,
                                 #   default_isp = Vitis-Vision 정렬 표준 ISP 암,
                                 #   lowlight_isp = 제안 저조도 암 v2)
  tests/                         # csim 테스트벤치 + 골든 벡터 CSV (커밋됨)
  scripts/vitis_hls.tcl          # HLS csynth/cosim 플로우
  scripts/dfx/                   # Vivado DFX 플로우 (dfx_flow.tcl 등)
  results/pr_controller/         # 패브릭 트리거 체인: 슈미트 모드 중재기 (checker_hysteresis.v)
                                 #   + DFXC 어댑터 + 지연 프로브 + TB (xsim PASS)
                                 #   (커스텀 pr_controller: 보관됨, results/archive/)
  results/icap_sim/              # ICAP PR 지연 시뮬레이션 TB
data/                            # 실제 RAW 샘플 ("데이터셋" 절 참조)
```

`scripts/dfx/dfx_flow.tcl`은 저장소 루트를 스크립트 위치의 네 단계 위로
계산하므로, 디렉터리 구조를 origin 저장소와 동일하게 유지했다 — 모든
스크립트가 수정 없이 그대로 동작한다.

소스의 긴 설계 배경 주석은 **각 파일 옆의 동일 이름 `.md` 노트로
분리**했다 (`src/dfxisp_accel.md`, `src/default_isp.md`,
`src/lowlight_isp.md`, `include/dfxisp_accel.md`,
`results/pr_controller/checker_hysteresis.md`,
`results/pr_controller/dfxc_adapter.md`,
`results/icap_sim/icap_pr_latency_tb.md`; 보관된
`results/archive/pr_controller/pr_controller.md`). 코드에는 짧은 요약과
노트 위치만 남겼다.

**모드 전환은 패브릭 내부에서 이루어진다** (2026-08-06): HLS checker가
프레임별 슈미트 밴드 플래그(`hyst_flags`, ap_vld 와이어)를 내보내고,
`checker_hysteresis.v`가 모드 상태를 보유하며, `dfxc_trigger_adapter.v`가
AMD DFX Controller의 하드웨어 트리거를 구동한다 — PS는 관찰만 한다.
(지연 프로브를 포함한) 전체 체인은 `checker_to_dfxc_tb.v`에서
시뮬레이션된다 (xsim PASS). 프로토콜과 남은 Stage 6 배선은
`checker_hysteresis.md`와 `dfxc_adapter.md`를 참조.

## 데이터셋 — 입력은 실제 RAW (pseudo-RAW 아님)

파이프라인 입력은 카메라 ISP를 거치지 않은 **실제 RAW 센서 데이터**이다.
두 개의 공개 데이터셋을 사용한다:

| 데이터셋 | 카메라 | 조명 | 이 저장소의 샘플 |
|---|---|---|---|
| **PASCALRAW** (Omid-Zohoor 외, Stanford Digital Repository) | Nikon D3200, 12비트 | 100% 주광 | `data/pascalraw_test/`, 3프레임 |
| **Sony NOD** (RAW-NOD, Night Object Detection) | Sony RX100 VII, RGGB | 저녁/야간 | `data/sonynod_test/`, 3프레임 |

각 샘플 디렉터리의 구성:

- `raw_bin/*.bin` — 16비트 컨테이너의 RAW Bayer 프레임. 스케일은
  `shift8` (`round(linear*255)<<8`); Bayer 위상 / 블랙 / 화이트 레벨은
  `meta.json`과 `frames_meta.csv`에 있다 (PASCALRAW: 블랙 0 / 화이트 4095;
  SonyNOD: 블랙 800 / 화이트 16380 원본에서 변환됨 — 항상 meta를 확인할 것).
- `images/*.jpg` — 참조용 렌더링 (RAW 아님; 육안 확인 전용).
- `labels/*.txt` — YOLO 형식 검출 레이블 (보드 실험에는 불필요; 참조용).
- `meta.json` — 정본 변환 파라미터.

**전체 데이터셋** (PASCALRAW 4,259 / SonyNOD 321 프레임, 수백 GB)은
GitHub 용량 제한 때문에 여기에 올릴 수 없다. 필요한 경우:

1. 공식 배포처에서 다운로드한다 — PASCALRAW는 Stanford Digital
   Repository, RAW-NOD는 해당 공개 저장소.
2. RAW → `raw_bin` 변환 도구는 origin 저장소의
   `isppipeline/hls/tools/`에 있다 (각 `meta.json`에 기록된 동일한 변환
   파라미터를 사용할 것).
3. 또는 데스크톱 머신에 있는 변환 완료본을 외장 드라이브 / 클라우드
   저장소를 통해 요청한다.

## 빌드 / 실행

비트스트림, DCP, 생성된 RTL(`build/`)은 git에 없다. 재생성하려면:

```bash
cd isppipeline/hls
make csim rm-csim default-isp-csim lowlight-isp-csim   # C 시뮬레이션 — 골든 CSV와 비교, 즉시 실행됨
# csynth/cosim: vitis_hls -f scripts/vitis_hls.tcl
#   주의 — 중첩된 프로젝트 경로에서 add_files로 추가한 설계 소스가
#   누락되는 알려진 버그가 있다 (링크 오류: undefined symbol: dfxisp_accel).
#   hpp/cpp/tb를 평평한 임시 디렉터리(예: /tmp/hls_dfxisp/)에 복사하고
#   그곳에서 vitis_hls를 실행할 것.
# DFX:  vivado -mode batch -source scripts/dfx/dfx_flow.tcl
```

`make verify` 같은 골든 재생성 타깃은 origin 저장소의 `tools/` Python이
필요하여 이 저장소에서는 동작하지 않는다 (csim은 커밋된 골든 CSV로
완전히 커버된다).

**주의 (실제 사고): HLS가 생성한 RTL을 다른 곳으로 복사할 때 `cp *.v`만
하면 gamma-ROM `.dat` 파일이 조용히 누락되어 합성 결과가 달라진다** —
`.dat` 파일을 항상 `.v`와 함께 복사할 것. 또한 **파티션 빌드와 플랫
빌드의 자원 수치를 절대 섞지 말 것** (동일 넷리스트에서 ~24% 차이,
원인 미규명 — SPEC.md §10). 자원 비교에는 플랫 축을 사용하고, 파티션
빌드는 비트스트림 / `pr_verify`에 사용한다.

## 할 일 — Stage 6 (보드)

순서대로:

1. **선행 조건**: 패브릭 내부 트리거 체인이 이제 존재한다 —
   `checker_hysteresis.v`(슈미트 모드 중재기)가 HLS 코어의
   `hyst_flags`/`ap_vld` 와이어를 소비하며, **운영용 재구성 경로는 AMD
   DFX Controller IP(PG374, 2026-08-06 채택)** 이고
   `dfxc_trigger_adapter.v`(RM별 one-hot HW 트리거 + `ap_idle`
   셧다운-ack 심)가 이를 연결한다 (체인은 `checker_to_dfxc_tb.v`의
   PG374 계약 모델로 검증됨, xsim PASS). `dfxc_adapter.md`의 통합
   체크리스트를 따를 것: IP 생성/구성 (1 VS, 2 RM, DDR 비트스트림 테이블,
   ICAP), DFX Decoupler 추가, 생성된 포트 이름 확인, 재합성된
   `hyst_flags` 포트와 `rm_ap_idle` 배선, 스왑 후 `ap_start` 정책 결정.
   손으로 작성한 `pr_controller.v`는 **보관됨**
   (`results/archive/pr_controller/`, 폐기 노트 포함).
2. PS/DDR 통합 (Vivado Block Design).
3. 새 pblock의 클럭/리셋 핀 할당 + WNS 재검증.
4. (선택) 파티션 핀 수 감소 15 → 3 원인 조사 (SPEC.md §10에 미조사로
   기록됨).
5. (선택) cosim 완료 — WSL2+XSIM에서의 자동 post-check SIGSEGV는 문서화된
   기존 버그이며, RTL 시뮬레이션 자체는 10/10 트랜잭션을 통과했다.
   네이티브 Linux/Windows Vivado에서는 재현되지 않을 수 있다.
6. (선택, 보조 계측) 논문의 보드 수치용 지연 측정: DFX Controller IP에는
   내장 지연 카운터가 없다 — `pr_latency_probe.v`를 IP의 핸드셰이크
   경계(drain, 종단 간 스왑)에 붙이고 ILA나 상태 레지스터로 읽는다.
   스왑 체인은 이것 없이도 동작한다.

## 보고 원칙

측정하지 않은 수치를 사실처럼 쓰지 말 것 (SPEC.md §11.4). 결과가 기대와
어긋나면 삭제하지 말고 있는 그대로 기록한다.
