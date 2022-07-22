module ws2812bBufferAvalonInterface(writedata, readdata, write, read, clk, reset_n, pop, data_out, valid);
  
  input [31:0] writedata;
  output [31:0] readdata;
  
  input write, read, clk, reset_n;
  
  input pop;
  output [23:0] data_out;
  output valid;
  
  wire [23:0] _data;
  wire _valid;
  
  lpm_ff #(.lpm_width(24)) data_reg(.data(writedata[23:0]), .clock(clk), .aclr(~reset_n), .enable(write), .q(_data));
  lpm_ff #(.lpm_width(24)) data_buffer(.data(_data), .clock(~clk), .aclr(~reset_n), .q(data_out));
  
  lpm_ff #(.lpm_width(1)) valid_reg(.data(write), .clock(clk), .aclr(~reset_n), .enable(write | pop), .q(_valid));
  lpm_ff #(.lpm_width(1)) valid_buffer(.data(_valid), .clock(~clk), .aclr(~reset_n), .q(valid));
  
  assign readdata = {23'b0, valid};
    
endmodule