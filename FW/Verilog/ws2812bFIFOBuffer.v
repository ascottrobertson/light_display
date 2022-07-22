module ws2812bFIFOBuffer(writedata, address, readdata, write, clk, reset_n, pop, dout, valid);
  
  input [31:0] writedata;
  input address, write, clk, reset_n, pop;
  
  output [31:0] readdata;
  output [23:0] dout;
  output valid;
  
  wire write_tx_en, write_data;
  wire _full, _empty, _valid;
  wire full, empty, in_eq_out;
  wire last_op;
  wire [9:0] _inptr, _outptr;
  wire [9:0] inptr, outptr;
  wire [31:0] _data_out;
  
  /* Write Data Logic */
  assign write_data = write & ~address;
  
  /* Write TX Enable Logic */
  assign write_tx_en = write & writedata[0] & address;
  
  /* Valid Status Logic */
  lpm_ff #(.lpm_width(1)) tx_en_ff(.data(write_tx_en & ~empty), .clock(clk), .enable((write_tx_en & ~empty) | empty), .aclr(~reset_n), .q(_valid));
  lpm_ff #(.lpm_width(1)) valid_buffer(.data(_valid), .clock(~clk), .aclr(~reset_n), .q(valid));
  
  /* Capacity Status Logic */
  lpm_compare #(.lpm_width(10)) ptr_compare(.dataa(_inptr), .datab(_outptr), .aeb(in_eq_out));
  lpm_ff #(.lpm_width(1)) last_op_ff(.data(write_data), .clock(clk), .enable(write_data | pop), .aclr(~reset_n), .q(last_op));
  assign _full = in_eq_out & last_op;
  assign _empty = in_eq_out & ~last_op;
  lpm_ff #(.lpm_width(1)) full_buffer(.data(_full), .clock(~clk), .aclr(~reset_n), .q(full));
  lpm_ff #(.lpm_width(1)) empty_buffer(.data(_empty), .clock(~clk), .aclr(~reset_n), .q(empty));
  
  /* Input Pointer */
  lpm_counter #(.lpm_width(10)) inptr_counter(.cnt_en(write_data & ~full), .clock(clk), .aclr(~reset_n), .q(_inptr));
  lpm_ff #(.lpm_width(10)) inptr_buffer(.data(_inptr), .clock(~clk), .aclr(~reset_n), .q(inptr));
  
  /* Output Pointer */
  lpm_counter #(.lpm_width(10)) outptr_counter(.cnt_en(pop & valid), .clock(clk), .aclr(~reset_n), .q(_outptr));
  lpm_ff #(.lpm_width(10)) outptr_buffer(.data(_outptr), .clock(~clk), .aclr(~reset_n), .q(outptr));
  
  /* RAM */
  lpm_ram_dp #(.lpm_width(32), .lpm_widthad(10)) color_data_ram(.data(writedata), .wren(write_data), .wraddress(inptr), .wrclock(clk), .rdaddress(outptr), .rdclock(clk), .q(_data_out));
  lpm_ff #(.lpm_width(24)) color_data_buffer(.data(_data_out[23:0]), .clock(~clk), .aclr(~reset_n), .q(dout));
  
  /* Readdata */
  assign readdata = {22'b0, empty, full, 8'b0};
  
endmodule
