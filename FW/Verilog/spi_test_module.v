module spi_test_module(readdata, read, clk, reset_n, address, scl, miso, mosi, ss_n);
  
  output [31:0] readdata;
  input read, clk, reset_n, address;
  
  input scl, mosi, ss_n;
  output miso;
  
  wire [7:0] shiftreg_out;
  wire [7:0] ram_out;
  
  wire push_byte;
  wire empty;
  
  wire [3:0] count;
  
  wire [9:0] _inptr, inptr, _outptr, outptr;
  
  wire clear;
  assign clear = ~reset_n | ss_n;
  
  // shiftreg
  lpm_shiftreg #(.lpm_width(8), .lpm_direction("LEFT")) shiftreg(.shiftin(mosi), .enable(~ss_n), .clock(scl), .aclr(clear), .q(shiftreg_out));
  
  // bit_counter
  lpm_counter #(.lpm_width(4)) bit_counter(.cnt_en(~ss_n), .clock(scl), .aclr(clear), .sclr(push_byte), .q(count));
  assign push_byte = count[3];
  
  // ram
  lpm_ram_dp #(.lpm_width(8), .lpm_widthad(10)) ram(.data(shiftreg_out), .wren(push_byte), .wraddress(inptr), .wrclock(~scl), .rdaddress(outptr), .rdclock(clk), .q(ram_out));
  
  // inptr
  lpm_counter #(.lpm_width(10)) inptr_counter(.cnt_en(push_byte), .clock(~scl), .aclr(clear), .q(_inptr));
  lpm_ff #(.lpm_width(10)) inptr_buffer(.data(_inptr), .clock(scl), .aclr(clear), .q(inptr));
  
  // outptr
  wire _read_a, _read_b;
  lpm_ff #(.lpm_width(1)) read_buffer_a(.data(read), .clock(clk), .aclr(clear), .q(_read_a));
  lpm_ff #(.lpm_width(1)) read_buffer_b(.data(_read_a), .clock(~clk), .aclr(clear), .q(_read_b));
  lpm_counter #(.lpm_width(10)) outptr_counter(.cnt_en(read & _read_b & ~address), .clock(clk), .aclr(clear), .q(_outptr));
  lpm_ff #(.lpm_width(10)) outptr_buffer(.data(_outptr), .clock(~clk), .aclr(clear), .q(outptr));
  
  // Empty
  lpm_compare #(.lpm_width(10)) empty_comp(.dataa(inptr), .datab(outptr), .aeb(empty));
  
  // MM Register MUX
  lpm_mux #(.lpm_width(32), .lpm_widths(1), .lpm_size(2)) reg_mux(.data({{31'b0, empty}, {24'b0, ram_out}}), .sel(address), .result(readdata));
  
endmodule
