module crc16(data_in, clk, reset_n, data_out);
  
  input data_in, clk, reset_n;
  
  output [15:0] data_out;
  wire [15:0] data_out_2;
  
  wire s1_in, s2_in, s3_in;
  wire [4:0] s1_out;
  wire [6:0] s2_out;
  wire [3:0] s3_out;
  
  assign s1_in = data_in ^ s3_out[0];
  assign s2_in = s1_out[0] ^ s3_out[0];
  assign s3_in = s2_out[0] ^ s3_out[0];
  
  lpm_shiftreg #(.lpm_width(5), .lpm_direction("RIGHT")) shift_1(.shiftin(s1_in), .clock(clk), .aclr(~reset_n), .q(s1_out));
  lpm_shiftreg #(.lpm_width(7), .lpm_direction("RIGHT")) shift_2(.shiftin(s2_in), .clock(clk), .aclr(~reset_n), .q(s2_out));
  lpm_shiftreg #(.lpm_width(4), .lpm_direction("RIGHT")) shift_3(.shiftin(s3_in), .clock(clk), .aclr(~reset_n), .q(s3_out));
  
  assign data_out = {s3_out[0], s3_out[1], s3_out[2], s3_out[3], s2_out[0], s2_out[1], s2_out[2], s2_out[3], s2_out[4], s2_out[5], s2_out[6], s1_out[0], s1_out[1], s1_out[2], s1_out[3], s1_out[4]};
  
endmodule