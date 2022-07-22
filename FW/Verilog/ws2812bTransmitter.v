// Module: 		ws2812bTransmitter
// Author: 		Andrew Robertson
// Date: 		14 Mar 2020
// Description: Generates signals to control WS2812b LED strips
// Inputs:		data_in: Serial Data Input - Bitstream representing the data bits to be transmitted
// 				scl: System Clock - Reference clock for the counter
//				load: Load Serial Data In - Trigger which latches the Serial Data Input
//				reset_n: Reset - Active low reset.  While asserted, data_out remains low
// Outputs:		data_out: Serial Data Output - Output signal to drive the data pin of the WS2812b LEDs

module ws2812bTransmitter(data_in, scl, load, reset_n, data_out);
	
	parameter TH1 = 6'd40;
	parameter TH0 = 6'd20;
	
	input data_in, scl, load, reset_n;
	output data_out;
	
	wire [5:0] counter_data;
	wire [5:0] count;
	wire count_zero_n;
	wire _load;
	
	lpm_mux #(.lpm_width(6), .lpm_size(2), .lpm_widths(1)) count_mux(.data({TH1, TH0}), .sel(data_in), .result(counter_data));
	
	lpm_counter #(.lpm_width(6), .lpm_direction("DOWN")) counter(.data(counter_data), .cnt_en(count_zero_n), .sload(load), .clock(scl), .aclr(~reset_n), .q(count));
	
	assign count_zero_n = |count;
	assign data_out = count_zero_n & reset_n;
	
endmodule