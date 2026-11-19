# icap_pr_latency_tb.v — 측정 방법 노트

`icap_pr_latency_tb.v` 소스 주석에서 분리한 문서이다.
(출처: haengmini/dfxisp 커밋 `ea6c2de`.)

## 측정 대상

PR(부분 재구성) 지연 테스트벤치: 실제 `rm_lowlight_partial.bit` 페이로드를 Xilinx
ICAPE3 UNISIM 동작 모델에 정격 최대 클럭(100 MHz, AMD UG570)으로 흘려 넣고,
"DFX 트리거" 펄스(첫 번째 유효 ICAP 쓰기)부터 스트리밍된 비트스트림 내용에 **실제
DESYNC 명령**(Type1 write CMD=0x0000000D, opcode 0x30008001)이 나타나는 시점까지의
시뮬레이션 시간을 측정한다.

## PRDONE 대신 DESYNC 검출을 쓰는 이유

ICAPE3 자체의 PRDONE 출력(내장 SIM_CONFIGE3 패킷 파서 경유)을 먼저 시도했으나
ICAPE3 단독 테스트벤치에서는 한 번도 assert되지 않았다. PRDONE은 eos_startup을
필요로 하는데, 이는 이 테스트벤치가 제공하지 않는 완전한 디바이스
STARTUPE3/컨피규레이션 시퀀스 시뮬레이션 컨텍스트에 의존하기 때문이다. 이는
ICAP 단독 시뮬레이션에서의 UNISIM 모델 자체의 실제 한계이지 테스트벤치 버그가
아니다.

신뢰할 수 있는 대안은 스트리밍되는 내용에서 실제 DESYNC 명령을 직접 검출하는
것이다. DESYNC는 실제 비트스트림에서 컨피규레이션 시퀀스를 끝내는 실제 명령이며,
후처리로 독립적으로 위치를 확인한 결과 **두 부분 비트스트림 모두 워드 인덱스
171,615(전체 171,633 워드 중)**에 있다. 위치가 동일하다는 것은 두 RM이 동일한
프레임 주소 명령 구조를 공유하고 프레임 데이터 내용만 다르다는 것을 확인해 준다.
