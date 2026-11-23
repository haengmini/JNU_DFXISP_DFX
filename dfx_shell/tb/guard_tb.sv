`timescale 1ns/1ps
module guard_tb;
 reg clk=0; always #5 clk=~clk;
 reg rst_n=0; reg[7:0] ctrl=0; reg[1:0] candidate=0;
 reg rm_idle=0,drain_ctrl=0,drain_read=0,drain_write=0;
 reg shutdown_req=0,decouple=0,event_error=0,status_valid=1;
 reg[31:0] dfx_status=7;
 wire[1:0] hw_triggers; wire shutdown_ack; wire[31:0] guard_status;
 integer checks=0, edges=0;
 dfx_safety_guard #(.TIMEOUT_CYCLES(12)) dut(.*);
 always @(posedge clk) if(hw_triggers!=0) edges=edges+1;
 task step; begin @(posedge clk); #1; end endtask
 task check(input bit ok,input string msg); begin
  checks=checks+1; if(!ok) $fatal(1,"FAIL %0d %s",checks,msg);
  $display("PASS %0d %s",checks,msg);
 end endtask
 task reset; begin @(negedge clk); rst_n=0; ctrl=0; candidate=0; shutdown_req=0; decouple=0; event_error=0; dfx_status=7; step(); rst_n=1; step(); end endtask
 task arm; begin @(negedge clk); ctrl=1; step(); check(guard_status[2],"explicit arm"); end endtask
 initial begin
  step(); check(hw_triggers==0 && !shutdown_ack && !guard_status[2],"reset disabled"); rst_n=1; step();
  candidate=2; repeat(3) step(); check(hw_triggers==0,"stale request cannot arm"); candidate=0; arm();
  candidate=2; step(); check(guard_status[4] && hw_triggers==0,"request staged, no DFXC edge");
  ctrl=9; step(); check(!guard_status[4] && hw_triggers==0,"cancel before commit");
  ctrl=1; repeat(3) step(); check(!guard_status[4],"held request not replayed after cancel");
  candidate=0; step(); candidate=1; step(); ctrl=7; step(); check(hw_triggers==0,"false permit with real idle low cannot trigger");
  ctrl=3; step(); rm_idle=1; ctrl=7; step(); check(hw_triggers==1 && guard_status[5],"Normal single trigger commit");
  ctrl=3; step(); repeat(2) step(); check(hw_triggers==0 && edges==1,"exactly one edge despite held request");
  shutdown_req=1; dfx_status=1; step(); check(!shutdown_ack,"no drain no ack");
  drain_ctrl=1; drain_read=1; step(); check(!shutdown_ack,"write drain missing");
  drain_write=1; step(); check(shutdown_ack,"actual idle plus all drains plus permit ack");
  rm_idle=0; #1; check(!shutdown_ack,"real active RM kills ack even false SW permit"); rm_idle=1;
  event_error=1; #1; check(hw_triggers==0 && !shutdown_ack,"immediate error kills outputs"); step();
  event_error=0; shutdown_req=0; dfx_status=7; candidate=0; repeat(3) step(); check(guard_status[3] && !guard_status[2],"error remains latched after busy release");
  ctrl=96; step(); check(guard_status[3],"cannot recover ambiguous accepted DFXC trigger");
  reset(); arm(); candidate=2; step(); ctrl=3; step(); ctrl=7; step(); check(hw_triggers==2,"Lowlight commit");
  ctrl=3; candidate=0; shutdown_req=1; dfx_status=1; step(); decouple=1; dfx_status=4; step();
  shutdown_req=0; decouple=0; dfx_status=6; step(); check(guard_status[5],"reset state is not success");
  dfx_status=7; step(); check(guard_status[5],"wrong RM identity is not success");
  dfx_status=32'h107; step(); check(!guard_status[5] && !guard_status[3],"FULL correct RM completes");
  reset(); arm(); candidate=1; step(); ctrl=3; step(); ctrl=7; step(); ctrl=3; candidate=0;
  repeat(14) step(); check(guard_status[3] && hw_triggers==0,"acceptance timeout latches fault");
  reset(); arm(); candidate=2; step(); ctrl=16; step(); check(guard_status[3] && !guard_status[4],"SW fault cancels uncommitted request");
  candidate=0; ctrl=0; step(); ctrl=32; step(); check(guard_status[3],"unverified recovery rejected");
  ctrl=0; step(); ctrl=96; step(); check(!guard_status[3] && !guard_status[2],"verified precommit recovery stays disabled");
  arm(); dfx_status=32'h88; step(); check(guard_status[3] && !guard_status[2],"status error latch");
  reset(); arm(); candidate=2; step(); ctrl=7; step();
  ctrl=15; #1; check(hw_triggers==0,"cancel kills commit pulse before DFXC capture"); step();
  check(guard_status[3] && !guard_status[2],"pulse-phase cancel conservatively latches HOLD");
  reset(); arm(); candidate=2; step(); ctrl=7; step();
  ctrl=5; #1; check(hw_triggers==0,"permit withdrawal kills commit pulse before capture"); step();
  reset(); arm(); candidate=2; step(); ctrl=7; step();
  rm_idle=0; #1; check(hw_triggers==0,"real idle withdrawal kills commit pulse before capture"); step(); rm_idle=1;
  $display("GUARD_PASS checks=%0d",checks); $finish;
 end
 initial begin #10000; $fatal(1,"TB timeout"); end
endmodule
