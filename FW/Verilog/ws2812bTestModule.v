module ws2812bTestModule(writedata, readdata, write, read, clk, reset_n, btrig, wtrig, data_out);
  
  input [31:0] writedata;
  output [31:0] readdata;
  
  input write, read, clk, btrig, wtrig, reset_n;
  
  output data_out;
  
  wire [23:0] _pdata;
  wire _sdata;
  
  wire _load, _valid, _tx_en;
  
  ws2812bBufferAvalonInterface avalon_interface(.writedata(writedata), .readdata(readdata), .write(write), .read(read), .clk(clk), .reset_n(reset_n), .pop(wtrig), .data_out(_pdata), .valid(_valid));
  
  ws2812bBuffer buffer(.data_in(_pdata), .scl(clk), .load(wtrig), .shift(btrig), .valid(_valid), .data_out(_sdata), .reset_n(reset_n), .tx_en(_tx_en));
  
  wire _btrig1, _btrig2;
  
  lpm_ff #(.lpm_width(1)) btrig_buffer_1(.data(btrig), .clock(clk), .q(_btrig1));
  lpm_ff #(.lpm_width(1)) btrig_buffer_2(.data(_btrig1), .clock(~clk), .q(_btrig2));
  
  ws2812bTransmitter transmitter(.data_in(_sdata), .scl(clk), .load(_btrig2), .reset_n(reset_n & _tx_en), .data_out(data_out));
  
endmodule