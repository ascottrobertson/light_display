// Module: 		ws2812bBuffer
// Author: 		Andrew Robertson
// Date: 		14 Mar 2020

module ws2812bBuffer(data_in, scl, load, shift, valid, data_out, reset_n, tx_en);
  
  input [23:0] data_in;
  input scl, load, shift, valid, reset_n;
  
  output data_out, tx_en;
  
  wire shift_out, _tx_en;
  
  lpm_shiftreg #(.lpm_width(24), .lpm_direction("LEFT")) data_shiftreg(.data(data_in), .clock(scl), .aclr(~reset_n), .load(load), .enable(shift | load), .shiftout(shift_out));
  lpm_ff #(.lpm_width(1)) data_out_buffer(.data(shift_out), .clock(~scl), .aclr(~reset_n), .q(data_out));
  
  lpm_ff #(.lpm_width(1)) tx_en_reg(.data(valid), .enable(load), .clock(scl), .aclr(~reset_n), .q(_tx_en));
  lpm_ff #(.lpm_width(1)) tx_en_buffer(.data(_tx_en), .clock(~scl), .aclr(~reset_n), .q(tx_en));
	
endmodule