`timescale 1ns/1ps
module pattern_detector (
    input      clk,
    input      nrst,
    input      serial_in,
    output reg [3:0] parallel_out,
    output reg       match
);

    // Target pattern to detect
    localparam [3:0] TARGET_PATTERN = 4'b1010;
    
    always @(posedge clk or negedge nrst) begin
        if (!nrst) begin
            parallel_out <= 4'b0000;
            match <= 1'b0;
        end else begin
            // Always shift in new bit from serial_in at LSB
            parallel_out <= {parallel_out[2:0], serial_in};
            
            // Check if the new parallel_out value (after shifting) matches target
            // We need to check the value that parallel_out will become after the shift
            if ({parallel_out[2:0], serial_in} == TARGET_PATTERN) begin
                match <= 1'b1;
            end else begin
                match <= 1'b0;
            end
        end
    end

endmodule