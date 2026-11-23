`timescale 1ns/1ps
module response_tb;
 reg clk=0; always #5 clk=~clk;
 reg rst_n=0,rv=0,rr=0,bv=0,br=0;
 reg [1:0] rp=0,bp=0;
 reg reset_req=0,confirm=0,quiet=0;
 wire rf,wf,hold_fault,now_fault,ge,sa,ra;
 reg [7:0] ctrl=0; reg [1:0] candidate=0;
 reg shutdown=0,decouple=0,idle=1,drain=1;
 reg [31:0] status=7;
 wire [1:0] triggers; wire ack; wire [31:0] gs;
 integer checks=0;
 axi_response_error_observer obs(clk,rst_n,rv,rr,rp,bv,br,bp,reset_req,confirm,quiet,rf,wf,now_fault,hold_fault);
 response_authorization_gate auth(now_fault,1'b0,1'b1,1'b1,confirm,ge,sa,ra);
 dfx_safety_guard #(.TIMEOUT_CYCLES(1000)) guard(
 .clk(clk),.rst_n(rst_n),.ctrl(ctrl),.candidate(candidate),.rm_idle(idle),
 .drain_ctrl(drain),.drain_read(drain),.drain_write(drain),
 .shutdown_req(shutdown),.decouple(decouple),.event_error(ge),
 .status_valid(1'b1),.dfx_status(status),.hw_triggers(triggers),.shutdown_ack(ack),.guard_status(gs));
 task ck(input bit test,input string label);
 begin checks++; if (!test) $fatal(1,"FAIL %s checks=%0d gs=%h",label,checks,gs); end endtask
 task step; begin @(posedge clk); #1; end endtask
 task off; begin @(negedge clk); rv=0;rr=0;bv=0;br=0;reset_req=0;confirm=0;quiet=0; end endtask
 task reboot; begin off; rst_n=0;ctrl=0;candidate=0;shutdown=0;decouple=0; step; @(negedge clk);rst_n=1;step; end endtask
 task arm; begin @(negedge clk);ctrl=3;step; ck(gs[2],"guard enabled"); @(negedge clk);candidate=1;step;ck(gs[4],"pending");end endtask
 initial begin
 reboot;
 // Legal OKAY/EXOKAY, multi-beat RAW bursts incl first/middle/last.
 for(integer resp=0;resp<2;resp++) begin
  for(integer beat=0;beat<16;beat++) begin
   off;rv=1;rr=(beat%3!=0);rp=resp;bv=1;br=(beat%4!=0);bp=resp;
   step;ck(!now_fault&&!hold_fault,"success/stall no fault");
  end
 end
 // Both response codes, both channels, all VALID/READY combinations.
 for(integer channel=0;channel<2;channel++)
 for(integer resp=2;resp<4;resp++)
 for(integer valid=0;valid<2;valid++)
 for(integer ready=0;ready<2;ready++) begin
  reboot;off;rp=resp;bp=resp;
  if(channel==0) begin rv=valid;rr=ready;end else begin bv=valid;br=ready;end
  #1;ck(now_fault==(valid&&ready),"handshake immediate");step;
  ck(hold_fault==(valid&&ready),"handshake sticky");
  if(valid&&ready) begin
   ck(!sa&&!ra,"start recovery blocked");off;step;ck(hold_fault,"sticky survives traffic disappearing");
   reset_req=1;quiet=1;step;ck(hold_fault,"unconfirmed reset rejected");
   confirm=1;quiet=0;step;ck(hold_fault,"nonquiescent reset rejected");
   quiet=1;rv=1;rr=0;step;ck(hold_fault,"pending stalled response prevents clear");
   rv=0;step;ck(!hold_fault,"explicit safe reset clears");
  end
 end
 // Error position in read burst: never depend on RLAST (not an observer input).
 for(integer position=0;position<4;position++) begin
  reboot;
  for(integer beat=0;beat<4;beat++) begin
   off;rv=1;rr=1;rp=(position==beat)?2:0;step;
   ck(hold_fault==(beat>=position),"burst any beat");
  end
 end
 reboot;off;rv=1;rr=1;rp=2;bv=1;br=1;bp=3;reset_req=1;confirm=1;quiet=1;step;
 ck(rf&&wf,"simultaneous errors dominate reset");
 // Race 1: error coincides with commit edge.
 reboot;arm;off;ctrl=7;rv=1;rr=1;rp=2;#1;ck(now_fault,"commit lookahead");step;
 ck(triggers==0&&gs[3],"commit edge error kills trigger");
 // Race 2: error arrives during registered trigger pulse before DFXC capture.
 reboot;arm;off;ctrl=7;step;ck(triggers==1,"clean trigger pulse");
 @(negedge clk);bv=1;br=1;bp=3;#1;
 ck(triggers==0&&!sa&&!ra,"pulse combinational kill before capture");step;
 ck(gs[3],"guard latches pulse-cycle error");
 off;ctrl=8'h60;candidate=0;step;ck(gs[3],"recovery cannot bypass observer HOLD");
 // ACK immediate kill, with all shutdown drains true.
 reboot;@(negedge clk);ctrl=3;step;shutdown=1;#1;ck(ack,"clean ACK");
 rv=1;rr=1;rp=3;#1;ck(!ack,"error same cycle ACK kill");step;ck(!ack&&gs[3],"ACK stays blocked");
 // Safe observer clear does not silently recover the guard.
 reboot;off;rv=1;rr=1;rp=2;step;ck(gs[3],"guard fault established");
 off;reset_req=1;confirm=1;quiet=1;step;ck(!hold_fault&&gs[3],"observer clear leaves guard fault");
 off;ctrl=8'h20;step;ck(gs[3],"guard recovery lacks verification");
 @(negedge clk);ctrl=0;step;@(negedge clk);ctrl=8'h60;step;
 ck(!gs[3]&&!gs[2],"confirmed guard recovery disabled until rearm");
 $display("PASS response observer + actual dfx_safety_guard checks=%0d",checks);$finish;
 end
 initial begin #100000;$fatal(1,"timeout");end
endmodule
