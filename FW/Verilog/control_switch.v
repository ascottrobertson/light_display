module control_switch(writedata, write, clk, reset_n, in_a, in_b, out);
  
  input [31:0] writedata;
  input write, clk, reset_n;
  input [2:0] in_a, in_b;
  
  output [2:0] out;
  
  wire sel;
  
  lpm_ff #(.lpm_width(3)) switch_reg(.data(|writedata), .clock(clk), .enable(write), .aclr(~reset_n), .q(sel));
  lpm_mux #(.lpm_width(3), .lpm_widths(1), .lpm_size(2)) switch_mux(.data({in_b, in_a}), .sel(sel), .result(out));
  
endmodule