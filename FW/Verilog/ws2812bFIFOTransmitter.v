module ws2812bFIFOTransmitter(writedata, address, readdata, write, read, clk, reset_n, btrig, wtrig, data_out);
  
  input [31:0] writedata;
  input address, write, read, clk, reset_n, btrig, wtrig;
  
  output [31:0] readdata;
  output data_out;
  
  wire [23:0] _pdata;
  wire _sdata, _sdata1, _sdata2;
  
  wire _load, _valid, _tx_en;
  
  ws2812bFIFOBuffer avalon_fifo_interface(.writedata(writedata), .address(address), .readdata(readdata), .write(write), .clk(clk), .reset_n(reset_n), .pop(wtrig), .dout(_pdata), .valid(_valid));
  
  ws2812bBuffer buffer(.data_in(_pdata), .scl(clk), .load(wtrig), .shift(btrig), .valid(_valid), .data_out(_sdata), .reset_n(reset_n), .tx_en(_tx_en));
  
  wire _btrig1, _btrig2;
  
  lpm_ff #(.lpm_width(1)) btrig_buffer_1(.data(btrig), .clock(clk), .q(_btrig1));
  lpm_ff #(.lpm_width(1)) btrig_buffer_2(.data(_btrig1), .clock(~clk), .q(_btrig2));
  
  lpm_ff #(.lpm_width(1)) sdata_buffer_1(.data(_sdata), .clock(clk), .q(_sdata1));
  lpm_ff #(.lpm_width(1)) sdata_buffer_2(.data(_sdata1), .clock(~clk), .q(_sdata2));
  
  ws2812bTransmitter transmitter(.data_in(_sdata2), .scl(clk), .load(_btrig2), .reset_n(reset_n & _tx_en), .data_out(data_out));
  
endmodule