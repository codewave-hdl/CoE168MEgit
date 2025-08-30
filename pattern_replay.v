`timescale 1ns/1ps
module pattern_replay(
    input      clk,
    input      nrst,
    input      serial_in,
    output reg [3:0] parallel_out,
    output reg [3:0] replay
);

    // Target pattern to detect
    localparam [3:0] TARGET_PATTERN = 4'b1010;
    
    reg replay_active;
    reg [1:0] replay_counter; //2 bits 4 cycles
    reg [3:0] memory[0:3]; //memory[0] to memory[3], 4 bits like parallel_out
    reg serial_in_prev;
    reg [1:0] counter; //2 bits 4 cycles
    reg capturing;
    reg replay_done;
    wire pos_edge_detected;
    
    assign pos_edge_detected = serial_in & ~serial_in_prev;
    
    always @(posedge clk or negedge nrst) begin
        if (!nrst) begin
            parallel_out <= 4'b0000;
            replay <= 4'b0000;
            counter <= 2'd0;
            serial_in_prev <= 1'b0; //initialize register used for previous serial_in
            memory[0] <= 4'b0000;//initialize memory
            memory[1] <= 4'b0000;//initialize memory
            memory[2] <= 4'b0000;//initialize memory
            memory[3] <= 4'b0000;//initialize memory
            replay_counter <= 2'd0;
            replay_active <= 1'b0;
            capturing <= 1'b0;
            replay_done <= 1'b0;
        end else begin
            serial_in_prev <= serial_in;
            if (!capturing && !replay_active && !replay_done) begin //normal operation
                parallel_out <= {parallel_out[2:0], serial_in};
            end
            if (pos_edge_detected && !capturing && !replay_active && !replay_done) begin
                capturing <= 1'b1;
                counter <= 2'd0;
            end else if (capturing) begin
                memory[counter] <= parallel_out;//recording for replay
                
                
                if (counter == 2'd3) begin
                    counter <= 2'd0;
                    capturing <= 1'b0;
                    replay_active <= 1'b1;
                    replay_counter <= 2'd0;
                end else begin
                    counter <= counter +1'b1;
                end
                
                parallel_out <= {parallel_out[2:0], serial_in}; //continue shifting during capture
            end
            
            //replay parallel_out from 4 cycles
            if (replay_active) begin
                replay <= memory[replay_counter];
                if (replay_counter == 2'd3) begin
                    replay_counter <= 2'd0;
                    replay_active <= 1'b0;
                    replay_done <= 1'b1;
                end else begin
                    replay_counter <= replay_counter + 1'b1;
                end
            end
            
            //after replay
            if (replay_done) begin
                 parallel_out <= 4'b0000;
                 replay <= 4'b0000;        
                 replay_done <= 1'b0;
            end                                          
        end
    end
endmodule