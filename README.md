# JNU_DFXISP_DFX — 2장. Scene Checker IP 설계 및 DFX 전환 구성

AMD ZCU104(`xczu7ev-ffvc1156-2-e`) 기반 **DFXISP** 튜토리얼의 2장 저장소입니다.  
2장 A에서는 PL 정적 요소(Static Logic)에 배치되는 장면 조도 판정기(`checker_scan` HLS + 히스테리시스·DFX 트리거 RTL)를 Vitis Unified IDE 2024.1과 Vivado 2024.1에서 검증하고 사용자 IP(`checker_ip`)로 패키징합니다.

## 구성

```text
JNU_DFXISP_DFX/
├── docs/
│   ├── ch2A_checker_design.pdf       # 2장 A 튜토리얼 문서 (Scene Checker IP)
│   └── ch2B_dfx_integration.pdf      # 2장 B 튜토리얼 문서 (DFX 통합·구현·검증)
├── checker/
│   ├── checker.hpp, checker.cpp      # Scene Checker HLS C++ 소스 (checker_scan)
│   ├── checker_tutorial_tb.cpp       # HLS C-Simulation 튜토리얼 테스트벤치
│   ├── checker_hysteresis.v          # Schmitt 히스테리시스 FSM (PL 정적 요소 RTL)
│   ├── dfxc_trigger_adapter.v        # PG374 one-hot 트리거 어댑터 RTL
│   ├── pr_latency_probe.v            # DFX 전환 지연 계측 RTL
│   ├── checker_ip.v                  # checker 통합 래퍼 RTL
│   └── tb/                           # checker RTL 테스트벤치 3종
├── dfx_shell/
│   ├── icap_dfx_bridge.v             # ICAPE3 프리미티브 브리지 RTL
│   ├── dfx_safety_guard.v            # DFX 안전 가드 RTL (permit/commit/drain/fault)
│   ├── axi_response_error_observer.v # AXI 응답 오류 감시기 RTL
│   ├── axi_response_monitor_wrapper.v# BD MONITOR 래퍼 RTL
│   └── tb/                           # 가드·응답 관측기 시뮬레이션 테스트벤치 2종
├── rm_wrapper/
│   └── rm_control.v                  # RM AXI4-Lite AP_CTRL + rm_ap_idle 제어 래퍼
├── ip_repo/
│   ├── NormalISP/                    # NormalISP HLS export IP (ap_ctrl_hs 핀 노출)
│   └── LowlightISP/                  # LowlightISP HLS export IP (ap_ctrl_hs 핀 노출)
├── hls_workspace/                    # (로컬 전용, 사용자가 생성 — .gitignore 제외)
└── vivado_workspace/                 # (로컬 전용, 사용자가 생성 — .gitignore 제외)
```

> `hls_workspace/`와 `vivado_workspace/`는 로컬 전용 작업 공간으로 Git 추적에서 제외됩니다. 저장소에는 기본 소스 파일과 PDF 문서만 포함됩니다.  
> 참고: 전체 통합 BD·구현·비트스트림 배치 재현 Tcl 스크립트는 `dfxisp_v2/vivado/`에 있습니다.

## 시작하기

1. 2장 A: [docs/ch2A_checker_design.pdf](docs/ch2A_checker_design.pdf), 2장 B: [docs/ch2B_dfx_integration.pdf](docs/ch2B_dfx_integration.pdf)를 열고 절차를 따라 진행합니다.
2. Vitis Unified IDE (HLS 컴포넌트 작업):

```bash
source /tools/Xilinx/Vitis/2024.1/settings64.sh && vitis -w ./hls_workspace
```

3. Vivado (RTL 시뮬레이션 및 `checker_ip` 패키징):

```bash
source /tools/Xilinx/Vivado/2024.1/settings64.sh && vivado
```

- **대상 파트:** `xczu7ev-ffvc1156-2-e` (ZCU104)
- **목표 클럭:** `4ns` (250 MHz, HLS)
- **보드 필요 여부:** 2장 A는 불필요 (Ubuntu PC + Vitis/Vivado 2024.1로 완료)
