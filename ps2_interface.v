`timescale 1ns / 1ps

module ps2_interface(
    input  wire        clk,        // 100 MHz system clock
    input  wire        nrst,       // Active-low reset
    input  wire        ps2_clk,    // PS/2 clock
    input  wire        ps2_data,   // PS/2 data
    output reg  [7:0]  keystroke,  // ASCII output
    output reg         done        // 1-cycle valid pulse
);

    // -------------------------------------------------------------------------
    // 1. Synchronizers
    // -------------------------------------------------------------------------
    reg [1:0] ps2_clk_sync;
    reg [1:0] ps2_data_sync;

    always @(posedge clk or negedge nrst) begin
        if (!nrst) begin
            ps2_clk_sync  <= 2'b11;
            ps2_data_sync <= 2'b11;
        end else begin
            ps2_clk_sync  <= {ps2_clk_sync[0], ps2_clk};
            ps2_data_sync <= {ps2_data_sync[0], ps2_data};
        end
    end

    wire ps2_clk_negedge = ps2_clk_sync[1] & ~ps2_clk_sync[0];
    wire data_in         = ps2_data_sync[1];

    // -------------------------------------------------------------------------
    // 2. PS/2 Serial Receiver (11-bit frame)
    // -------------------------------------------------------------------------
    reg [10:0] shift_reg;
    reg [3:0]  bit_count;
    reg        frame_received;

    always @(posedge clk or negedge nrst) begin
        if (!nrst) begin
            shift_reg      <= 11'd0;
            bit_count      <= 4'd0;
            frame_received <= 1'b0;
        end else begin
            frame_received <= 1'b0;
            if (ps2_clk_negedge) begin
                shift_reg <= {data_in, shift_reg[10:1]}; // LSB first
                if (bit_count == 4'd10) begin
                    bit_count      <= 4'd0;
                    frame_received <= 1'b1;
                end else begin
                    bit_count <= bit_count + 1'b1;
                end
            end
        end
    end

    wire valid_frame = (shift_reg[0]  == 1'b0) &&   // start bit
                       (shift_reg[10] == 1'b1);    // stop bit

    wire [7:0] scan_code = shift_reg[8:1];

    // -------------------------------------------------------------------------
    // 3. Key State & ASCII Mapping
    // -------------------------------------------------------------------------
    reg key_break;
    reg extended_key;
    reg shift_l, shift_r;
    reg caps_lock;

    always @(posedge clk or negedge nrst) begin
        if (!nrst) begin
            keystroke    <= 8'd0;
            done         <= 1'b0;
            key_break    <= 1'b0;
            extended_key <= 1'b0;
            shift_l      <= 1'b0;
            shift_r      <= 1'b0;
            caps_lock    <= 1'b0;
        end else begin
            done <= 1'b0;

            if (frame_received && valid_frame) begin

                // Prefix bytes
                if (scan_code == 8'hF0)
                    key_break <= 1'b1;
                else if (scan_code == 8'hE0)
                    extended_key <= 1'b1;
                else begin

                    // -------------------------
                    // Key release
                    // -------------------------
                    if (key_break) begin
                        if (scan_code == 8'h12) shift_l <= 1'b0;
                        if (scan_code == 8'h59) shift_r <= 1'b0;
                        key_break    <= 1'b0;
                        extended_key <= 1'b0;
                    end
                    // -------------------------
                    // Key press
                    // -------------------------
                    else begin
                        // Modifier keys
                        if (scan_code == 8'h12) shift_l <= 1'b1;
                        else if (scan_code == 8'h59) shift_r <= 1'b1;
                        else if (scan_code == 8'h58) caps_lock <= ~caps_lock;
                        else begin
                            done <= 1'b1;

                            // Extended keys (arrows)
                            if (extended_key) begin
                                case (scan_code)
                                    8'h75: keystroke <= 8'hE0; // Up
                                    8'h72: keystroke <= 8'hE1; // Down
                                    8'h6B: keystroke <= 8'hE2; // Left
                                    8'h74: keystroke <= 8'hE3; // Right
                                    default: done <= 1'b0;
                                endcase
                                extended_key <= 1'b0;
                            end
                            // Normal keys
                            else begin
                                case (scan_code)

                                    // Control
                                    8'h29: keystroke <= 8'h20; // Space
                                    8'h66: keystroke <= 8'h08; // Backspace

                                    // Number row
                                    8'h16: keystroke <= (shift_l|shift_r) ? "!" : "1";
                                    8'h1E: keystroke <= (shift_l|shift_r) ? "@" : "2";
                                    8'h26: keystroke <= (shift_l|shift_r) ? "#" : "3";
                                    8'h25: keystroke <= (shift_l|shift_r) ? "$" : "4";
                                    8'h2E: keystroke <= (shift_l|shift_r) ? "%" : "5";
                                    8'h36: keystroke <= (shift_l|shift_r) ? "^" : "6";
                                    8'h3D: keystroke <= (shift_l|shift_r) ? "&" : "7";
                                    8'h3E: keystroke <= (shift_l|shift_r) ? "*" : "8";
                                    8'h46: keystroke <= (shift_l|shift_r) ? "(" : "9";
                                    8'h45: keystroke <= (shift_l|shift_r) ? ")" : "0";
                                    8'h4E: keystroke <= (shift_l|shift_r) ? "_" : "-";
                                    8'h55: keystroke <= (shift_l|shift_r) ? "+" : "=";

                                    // Q-P
                                    8'h15: keystroke <= ((shift_l|shift_r)^caps_lock) ? "Q":"q";
                                    8'h1D: keystroke <= ((shift_l|shift_r)^caps_lock) ? "W":"w";
                                    8'h24: keystroke <= ((shift_l|shift_r)^caps_lock) ? "E":"e";
                                    8'h2D: keystroke <= ((shift_l|shift_r)^caps_lock) ? "R":"r";
                                    8'h2C: keystroke <= ((shift_l|shift_r)^caps_lock) ? "T":"t";
                                    8'h35: keystroke <= ((shift_l|shift_r)^caps_lock) ? "Y":"y";
                                    8'h3C: keystroke <= ((shift_l|shift_r)^caps_lock) ? "U":"u";
                                    8'h43: keystroke <= ((shift_l|shift_r)^caps_lock) ? "I":"i";
                                    8'h44: keystroke <= ((shift_l|shift_r)^caps_lock) ? "O":"o";
                                    8'h4D: keystroke <= ((shift_l|shift_r)^caps_lock) ? "P":"p";
                                    8'h54: keystroke <= (shift_l|shift_r) ? "{" : "[";
                                    8'h5B: keystroke <= (shift_l|shift_r) ? "}" : "]";
                                    8'h5D: keystroke <= (shift_l|shift_r) ? "|" : "\\";

                                    // A-L
                                    8'h1C: keystroke <= ((shift_l|shift_r)^caps_lock) ? "A":"a";
                                    8'h1B: keystroke <= ((shift_l|shift_r)^caps_lock) ? "S":"s";
                                    8'h23: keystroke <= ((shift_l|shift_r)^caps_lock) ? "D":"d";
                                    8'h2B: keystroke <= ((shift_l|shift_r)^caps_lock) ? "F":"f";
                                    8'h34: keystroke <= ((shift_l|shift_r)^caps_lock) ? "G":"g";
                                    8'h33: keystroke <= ((shift_l|shift_r)^caps_lock) ? "H":"h";
                                    8'h3B: keystroke <= ((shift_l|shift_r)^caps_lock) ? "J":"j";
                                    8'h42: keystroke <= ((shift_l|shift_r)^caps_lock) ? "K":"k";
                                    8'h4B: keystroke <= ((shift_l|shift_r)^caps_lock) ? "L":"l";
                                    8'h4C: keystroke <= (shift_l|shift_r) ? ":" : ";";
                                    8'h52: keystroke <= (shift_l|shift_r) ? "\"" : "'";

                                    // Z-M
                                    8'h1A: keystroke <= ((shift_l|shift_r)^caps_lock) ? "Z":"z";
                                    8'h22: keystroke <= ((shift_l|shift_r)^caps_lock) ? "X":"x";
                                    8'h21: keystroke <= ((shift_l|shift_r)^caps_lock) ? "C":"c";
                                    8'h2A: keystroke <= ((shift_l|shift_r)^caps_lock) ? "V":"v";
                                    8'h32: keystroke <= ((shift_l|shift_r)^caps_lock) ? "B":"b";
                                    8'h31: keystroke <= ((shift_l|shift_r)^caps_lock) ? "N":"n";
                                    8'h3A: keystroke <= ((shift_l|shift_r)^caps_lock) ? "M":"m";
                                    8'h41: keystroke <= (shift_l|shift_r) ? "<" : ",";
                                    8'h49: keystroke <= (shift_l|shift_r) ? ">" : ".";
                                    8'h4A: keystroke <= (shift_l|shift_r) ? "?" : "/";

                                    default: done <= 1'b0;
                                endcase
                            end
                        end
                    end
                end
            end
        end
    end

endmodule
