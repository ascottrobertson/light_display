module crc7(data_in, clk, reset_n, data_out);
  
  input data_in, clk, reset_n;
  output [6:0] data_out;
  
  wire [2:0] s1_out;
  wire [3:0] s2_out;
  
  wire s1_in, s2_in;
  
  assign s1_in = data_in ^ s2_out[0];
  assign s2_in = s1_out[0] ^ s2_out[0];
  
  lpm_shiftreg #(.lpm_width(3), .lpm_direction("RIGHT")) shift_1(.shiftin(s1_in), .clock(clk), .aclr(~reset_n), .q(s1_out));
  lpm_shiftreg #(.lpm_width(4), .lpm_direction("RIGHT")) shift_2(.shiftin(s2_in), .clock(clk), .aclr(~reset_n), .q(s2_out));
  
  assign data_out = {s2_out[0], s2_out[1], s2_out[2], s2_out[3], s1_out[0], s1_out[1], s1_out[2]};
  
endmodule