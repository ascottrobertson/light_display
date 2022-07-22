module crc7_padding(data_in, data_out);
  
  input [6:0] data_in;
  output [6:0] data_out;
  
  assign data_out[0] = data_in[0] ^ data_in[4];
  assign data_out[1] = data_in[1] ^ data_in[5];
  assign data_out[2] = data_in[2] ^ data_in[6];
  assign data_out[3] = data_in[3] ^ data_out[0];
  assign data_out[4] = data_in[4] ^ data_out[1];
  assign data_out[5] = data_in[5] ^ data_out[2];
  assign data_out[6] = data_in[6] ^ data_in[4];
  
endmodule