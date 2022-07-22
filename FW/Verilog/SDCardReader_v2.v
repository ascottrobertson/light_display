module SDCardReader_v2(writedata, write, address, readdata, read, clk, reset_n, mosi, miso, scl, ss_n);
  
  input [31:0] writedata;
  input write, address, read, clk, reset_n, miso;
  
  output [31:0] readdata;
  output mosi, scl, ss_n;
  
  // State Machine Inputs
  wire write_addr_p;
  wire crc_match;
  wire bit_zero, bit_zero_p;
  wire byte_zero, byte_zero_p;
  wire [7:0] rx_lb; // Low byte of RX register
  wire rx_fe; // rx_lb == 0xFE
  wire rx_err; // |(rx_lb & 0b01111100)
  wire [2:0] state, nextstate;
  
  // State Machine Outputs
  wire tx_load;
  wire tx_source;
  wire bit_load, byte_load;
  wire [1:0] bit_source;
  wire clr_err;
  wire reset_wdt;
  wire tx_shift_en;
  wire bit_cnt_en;
  wire set_tx_blank, clr_tx_blank;
  wire byte_source;
  wire clr_crc7, clr_crc16;
  wire byte_cnt_en;
  wire push_data, push_data_p;
  wire set_crc_err, set_cmd_err, set_timeout_err;
  wire clr_fifo;
  
  // Wires
  wire [31:0] rx_reg_out;
  wire [31:0] ram_out;
  wire [31:0] status;
  wire [7:0] inptr, outptr;
  wire tx_shift_out;
  wire [6:0] _crc7, crc7;
  wire [15:0] crc16;
  wire write_addr, write_ctrl;
  wire [31:0] block_addr;
  wire [39:0] tx_loadval;
  wire tx_blank;
  wire [5:0] bit_loadval;
  wire [5:0] bit_count;
  wire [6:0] byte_loadval;
  wire [6:0] byte_count;
  wire read_status, read_data, _read_data, read_data_p;
  wire _read1, _read2, read_p;
  wire timeout_err, cmd_err, crc_err, busy, data_valid;
  wire [15:0] wdt_count;
  wire in_eq_out, full, empty, last_write_read_n;
  wire ss_n, scl_en;
  wire [7:0] inptr_p;
  
  // Debug
  wire [6:0] state_flags;
  wire [7:0] r1;
  wire addr;
  
  lpm_ff #(.lpm_width(1)) addr_reg(.data(address), .clock(clk), .enable(read), .q(addr));
  
  // Per-State input selectors
  wire bit_byte_zero = bit_zero_p & byte_zero_p;
  wire rx_rcv = bit_zero_p & ~rx_lb[7];
  wire x0, x1, x2;
  // lpm_mux #(.lpm_width(1), .lpm_widths(3), .lpm_size(8)) x0_mux(.data({1'b0, ~bit_byte_zero, 1'b0, (bit_zero & rx_fe), 1'b0, bit_zero, bit_zero, write_address}), .sel(state), .result(x0));
  lpm_mux #(.lpm_width(1), .lpm_widths(3), .lpm_size(8)) x0_mux(.data({1'b0, (bit_zero_p & rx_fe), ~bit_byte_zero, 1'b0, bit_zero_p, 1'b0,               bit_zero_p, write_addr_p}), .sel(state), .result(x0));
  // lpm_mux #(.lpm_width(1), .lpm_widths(3), .lpm_size(8)) x1_mux(.data({2'b0, ~bit_byte_zero, 1'b0, ~(rx_rcv & rx_err), bit_zero, bit_zero, write_address}), .sel(state), .result(x1));
  lpm_mux #(.lpm_width(1), .lpm_widths(3), .lpm_size(8)) x1_mux(.data({~bit_byte_zero, 3'b0,                           bit_zero_p, ~(rx_rcv & rx_err), bit_zero_p, write_addr_p}), .sel(state), .result(x1));
  // lpm_mux #(.lpm_width(1), .lpm_widths(3), .lpm_size(8)) x2_mux(.data({1'b0, ~bit_byte_zero, 2'b0, (rx_rcv & ~rx_err), bit_zero, bit_zero, write_address}), .sel(state), .result(x2));
  lpm_mux #(.lpm_width(1), .lpm_widths(3), .lpm_size(8)) x2_mux(.data({2'b0,                     ~bit_byte_zero, 1'b0, bit_zero_p, (rx_rcv & ~rx_err), bit_zero_p, write_addr_p}), .sel(state), .result(x2));
  
  // State
  assign nextstate[0] = (x0 & state[2]) | (~x0 & ~state[2] & state[0]) | (&state) | (x0 & ~|state[2:1]);
  // assign nextstate[1] = (state[2] & ~state[0]) | (x1 & state[1]) | (x1 & ~state[2] & state[0]) | (~state[2] & &state[1:0]);
  assign nextstate[1] = (~state[2] & &state[1:0]) | (&state[2:1] & ~state[0]) | (x1 & ~state[2] & state[0]) | (x1 & state[1]);
  // assign nextstate[2] = (state[2] & ~state[0]) | (&state[2:1]) | (x2 & state[2]) | (x2 & state[1] & ~state[0]);
  assign nextstate[2] = (x2 & state[2]) | (&state[2:1]) | (x2 & state[1] & ~state[0]);
  lpm_ff #(.lpm_width(3)) state_reg(.data(nextstate), .clock(~clk), .aclr(~reset_n | timeout), .q(state));
  
  // Watchdog timer
  lpm_counter #(.lpm_width(16), .lpm_direction("DOWN")) wdt_counter(.data(16'b1111111111111111), .clock(clk), .sload(reset_wdt), .aload(~reset_n), .q(wdt_count));
  assign timeout = ~|wdt_count;
  
  // FIFO
  lpm_ram_dp #(.lpm_width(32), .lpm_widthad(8)) data_ram(.data(rx_reg_out), .wraddress(inptr_p), .wren(push_data_p), .wrclock(~clk), .rdaddress(outptr), .rdclock(clk), .q(ram_out));
  lpm_counter #(.lpm_width(8)) inptr_counter(.clock(~clk), .cnt_en(push_data_p), .aclr(clr_fifo | ~reset_n), .q(inptr));
  lpm_ff #(.lpm_width(8)) inptr_buffer(.data(inptr), .clock(clk), .q(inptr_p));
  lpm_counter #(.lpm_width(8)) outptr_counter(.clock(~clk), .cnt_en(read_data_p), .aclr(clr_fifo | ~reset_n), .q(outptr));
  lpm_ff #(.lpm_width(1)) last_write_read_reg(.data(push_data_p), .clock(~clk), .enable(read_data_p | push_data_p), .aclr(clr_fifo | ~reset_n), .q(last_write_read_n));
  lpm_compare #(.lpm_width(8)) ptr_compare(.dataa(inptr), .datab(outptr), .aeb(in_eq_out));
  assign full = in_eq_out & last_write_read_n;
  assign empty = in_eq_out & ~last_write_read_n;
  
  // CRC Logic
  crc7 crc7_gen(.data_in(tx_shift_out), .clk(clk), .reset_n(reset_n & ~clr_crc7), .data_out(_crc7));
  crc7_padding crc7_pad(.data_in(_crc7), .data_out(crc7));
  crc16 crc16_gen(.data_in(miso), .clk(clk), .reset_n(reset_n & ~clr_crc16), .data_out(crc16));
  assign crc_match = ~|crc16;
  
  // Block Address Logic
  lpm_ff #(.lpm_width(32)) block_addr_reg(.data(writedata), .clock(clk), .enable(write_addr), .aclr(~reset_n), .q(block_addr));
  lpm_ff #(.lpm_width(1)) write_addr_reg(.data(write_addr), .clock(clk), .aclr(~reset_n), .q(write_addr_p));
  
  // Write Logic
  lpm_decode #(.lpm_width(1), .lpm_decodes(2)) write_demux(.data(address), .enable(write), .eq({write_ctrl, write_addr}));
  
  // TX Logic
  lpm_mux #(.lpm_width(40), .lpm_widths(1), .lpm_size(2)) tx_load_mux(.data({crc7, 33'h100000000, 8'h51, block_addr}), .sel(tx_source), .result(tx_loadval));
  lpm_shiftreg #(.lpm_width(40)) tx_shiftreg(.data(tx_loadval), .clock(~clk), .enable(tx_shift_en | tx_load), .load(tx_load), .aclr(~reset_n), .shiftout(tx_shift_out));
  lpm_ff #(.lpm_width(1)) tx_blank_reg(.data(set_tx_blank), .clock(~clk), .enable(set_tx_blank | clr_tx_blank), .aset(~reset_n), .q(tx_blank));
  assign mosi = tx_shift_out | tx_blank;
  
  // RX Logic
  lpm_shiftreg #(.lpm_width(32)) rx_shiftreg(.shiftin(miso), .clock(clk), .aset(~reset_n), .q(rx_reg_out));
  assign rx_lb = rx_reg_out[7:0];
  lpm_compare #(.lpm_width(8)) rx_fe_compare(.dataa(rx_lb), .datab(8'hfe), .aeb(rx_fe));
  assign rx_err = |(rx_lb & 8'b01111100);
  
  // Bit Counter
  lpm_mux #(.lpm_width(6), .lpm_widths(2), .lpm_size(4)) bit_mux(.data({6'd0, 6'd31, 6'd39, 6'd7}), .sel(bit_source), .result(bit_loadval));
  lpm_counter #(.lpm_width(6), .lpm_direction("DOWN")) bit_counter(.data(bit_loadval), .clock(~clk), .sload(bit_load), .cnt_en(bit_cnt_en), .aclr(~reset_n), .q(bit_count));
  assign bit_zero = ~|bit_count;
  lpm_ff #(.lpm_width(1)) bit_zero_reg(.data(bit_zero), .clock(clk), .q(bit_zero_p));
  
  // Byte Counter
  lpm_mux #(.lpm_width(7), .lpm_widths(1), .lpm_size(2)) byte_mux(.data({7'd1, 7'd127}), .sel(byte_source), .result(byte_loadval));
  lpm_counter #(.lpm_width(7), .lpm_direction("DOWN")) byte_counter(.data(byte_loadval), .clock(~clk), .sload(byte_load), .cnt_en(byte_cnt_en), .aclr(~reset_n), .q(byte_count));
  assign byte_zero = ~|byte_count;
  lpm_ff #(.lpm_width(1)) byte_zero_reg(.data(byte_zero), .clock(clk), .q(byte_zero_p));
  
  // Read Logic
  lpm_decode #(.lpm_width(1), .lpm_decodes(2)) read_demux(.data(address), .enable(read), .eq({read_status, read_data}));
  lpm_mux #(.lpm_width(32), .lpm_widths(1), .lpm_size(2)) read_mux(.data({status, ram_out[7:0], ram_out[15:8], ram_out[23:16], ram_out[31:24]}), .sel(address), .result(readdata));
  lpm_ff #(.lpm_width(1)) read_data_reg(.data(read_data), .clock(clk), .aclr(~reset_n), .q(_read_data));
  lpm_ff #(.lpm_width(1)) _read1_reg(.data(read), .clock(clk), .aclr(~reset_n), .q(_read1));
  lpm_ff #(.lpm_width(1)) _read2_reg(.data(_read1), .clock(~clk), .aclr(~reset_n), .q(_read2));
  lpm_ff #(.lpm_width(1)) read_p_reg(.data(_read2), .clock(clk), .aclr(~reset_n), .q(read_p));
  assign read_data_p = read_p & _read_data;// & _read_data;
  
  // Status
  lpm_ff #(.lpm_width(1)) cmd_err_reg(.data(set_cmd_err), .clock(~clk), .enable(set_cmd_err | clr_err), .aclr(~reset_n), .q(cmd_err));
  lpm_ff #(.lpm_width(1)) crc_err_reg(.data(set_crc_err), .clock(~clk), .enable(set_crc_err | clr_err), .aclr(~reset_n), .q(crc_err));
  lpm_ff #(.lpm_width(1)) timeout_err_reg(.data(set_timeout_err), .clock(~clk), .enable(set_timeout_err | clr_err), .aclr(~reset_n), .q(timeout_err));
  assign busy = |state;
  assign data_valid = ~empty;
  assign status = {last_write_read_n, state_flags, inptr, outptr, 3'b0, timeout_err, crc_err, cmd_err, data_valid, busy};
  
  // State Machine Output Equations
  assign tx_load = x0;
  assign tx_source = state[0];
  assign bit_load = bit_zero_p | (~|state & write_addr_p);
  assign bit_source[0] = ~|state[1:0];
  assign bit_source[1] = &state[2:1] & ((~state[0] & bit_zero_p & rx_fe) | (state[0] & bit_zero_p & ~byte_zero_p));
  assign clr_err = ~|state & write_addr_p;
  assign reset_wdt = ~|state[1:0] | (state[0] & bit_zero_p) | (~state[2] & state[1] & ~state[0] & rx_rcv & ~rx_err) | (&state[2:1] & ~state[0] & bit_zero_p & rx_fe) | (&state);
  assign tx_shift_en = 1'b1;
  assign bit_cnt_en = 1'b1;
  assign set_tx_blank = state[1] & x0;
  assign clr_tx_blank = ~|state & write_addr_p;
  assign byte_load = &state[2:1] & ((~state[0] & bit_zero_p & rx_fe) | (state[0] & bit_byte_zero));
  assign byte_source = &state & bit_byte_zero;
  assign clr_crc7 = ~(~|state[2:1] & state[0]);
  assign clr_crc16 = ~(state[2] & state[0]);
  assign byte_cnt_en = bit_zero_p;
  assign push_data = &state & bit_zero;
  assign set_crc_err = state[2] & ~state[1] & state[0] & bit_byte_zero & ~crc_match;
  assign set_cmd_err = ~state[2] & state[1] & ~state[0] & rx_rcv & rx_err;
  assign set_timeout_err = ~|wdt_count;
  assign ss_n = ~busy;
  assign scl_en = busy;
  assign clr_fifo = ~|state & x0;
  assign scl = clk & scl_en;
  
  lpm_ff #(.lpm_width(1)) push_data_reg(.data(push_data), .clock(clk), .q(push_data_p));
  lpm_ff #(.lpm_width(1)) state_flag_0(.data(~|state[0]), .clock(clk), .enable(~|state[0]), .aclr(~reset_n), .q(state_flags[0]));
  lpm_ff #(.lpm_width(1)) state_flag_1(.data(~|state[2:1] & state[0]), .clock(clk), .enable(~|state[2:1] & state[0]), .aclr(~reset_n),  .q(state_flags[1]));
  lpm_ff #(.lpm_width(1)) state_flag_2(.data(~state[2] & &state[1:0]), .clock(clk), .enable(~state[2] & &state[1:0]), .aclr(~reset_n),  .q(state_flags[2]));
  lpm_ff #(.lpm_width(1)) state_flag_3(.data(~state[2] & state[1] & ~state[0]), .clock(clk), .enable(~state[2] & state[1] & ~state[0]), .aclr(~reset_n),  .q(state_flags[3]));
  lpm_ff #(.lpm_width(1)) state_flag_4(.data(&state[2:1] & ~state[0]), .clock(clk), .enable(&state[2:1] & ~state[0]), .aclr(~reset_n),  .q(state_flags[4]));
  lpm_ff #(.lpm_width(1)) state_flag_5(.data(&state), .clock(clk), .enable(&state), .aclr(~reset_n),  .q(state_flags[5]));
  lpm_ff #(.lpm_width(1)) state_flag_6(.data(state[2] & ~state[1] & state[0]), .clock(clk), .enable(state[2] & ~state[1] & state[0]), .aclr(~reset_n),  .q(state_flags[6]));
  
  lpm_ff #(.lpm_width(8)) r1_reg(.data(rx_lb), .clock(~clk), .enable(~state[2] & state[1] & ~state[0] & rx_rcv), .aclr(~reset_n), .q(r1));
    
endmodule
