# DFX용 제공 ISP IP

NormalISP와 LowlightISP는 dfxisp_v2의 3차 작업에서 Vitis HLS 2024.1로 내보낸 IP다. 대상은 ZCU104의 `xczu7ev-ffvc1156-2-e`, 4 ns, Vivado IP Flow다. 보드 top은 `ap_ctrl_hs`이며 인자 AXI-Lite 레지스터를 유지한다. 2.5절의 RTL 래퍼가 PS의 AP_CTRL을 제공한다.

| IP | V2와 일치한 component.xml MD5 | component.xml SHA-256 |
| --- | --- | --- |
| NormalISP | 821bc0aa55cd2d17c2f5879b7f8eb1ac | 26803c64656f9079994a25acf55bfccafcc679dc3ceb86af3ee91be9c5c9a4d7 |
| LowlightISP | 318af6c3f139df58b5bdce776c79000f | 5513f89a3d29536ca95c3ea41d191c1cd90a6f5de4ebf061024154656bc8a7ff |

V2 근거는 각 `build/<ISP>/solution1/impl/ip/component.xml`이다. XSim은 제공된 Verilog를 사용했다. [파일 목록과 SHA-256](SHA256SUMS)은 두 ISP 디렉터리의 전체 파일을 포함한다.

```bash
cd ip_repo && sha256sum -c SHA256SUMS
```
