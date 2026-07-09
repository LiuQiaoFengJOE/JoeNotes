----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 09/02/2017 09:57:43 PM
-- Design Name: 
-- Module Name: short_packet - Behavioral
-- Project Name: 
-- Target Devices: 
-- Tool Versions: 
-- Description: 
-- 
-- Dependencies: 
-- 
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
-- 
----------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

use work.Common.all;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity short_packet is
   Port ( clk : in STD_LOGIC;
          rst : in  STD_LOGIC;
          vc_num : in std_logic_vector(0 to 1); --virtual channel number          
          packet_type : in packet_type_t;          --short packet type
          packet_data : in std_logic_vector(0 to 15); --packet data: frame number for frame packet, line number for line packet
          short_packet_out : out std_logic_vector(0 to 31) --prepared short packet out
    );
end short_packet;


architecture Behavioral of short_packet is

constant Frame_Start_Code : std_logic_vector(0 to 7) := x"00";
constant Frame_End_Code : std_logic_vector(0 to 7)   := x"01";
constant Line_Start_Code : std_logic_vector(0 to 7)  := x"02";
constant Line_End_Code : std_logic_vector(0 to 7)    := x"03";

signal byte_1_out : std_logic_vector(0 to 7);  -- virtual channel number + short packet code
signal byte_2a3_out : std_logic_vector(0 to 15); --payload
signal byte_4_out : std_logic_vector(0 to 7); --ECC
signal byte_123_tmp : std_logic_vector(0 to 23); 

begin

byte_1_out(0 to 1) <= vc_num;
byte_1_out(2 to 7) <= Frame_Start_Code(2 to 7) when packet_type = Frame_Start else
                      Frame_End_Code(2 to 7) when packet_type = Frame_End else
                      Line_Start_Code(2 to 7) when packet_type = Line_Start else
                      Line_End_Code(2 to 7) when packet_type = Line_End else
                      (others => '0') ;
byte_2a3_out(0 to 15) <= packet_data;		

byte_123_tmp(0 to 7)   <= byte_1_out;
byte_123_tmp(8 to 23)  <= byte_2a3_out;
byte_4_out             <= get_ecc(byte_123_tmp(0 to 23));

short_packet_out(0 to 23) <= byte_123_tmp;
short_packet_out(24 to 31)<= byte_4_out;

					  

end Behavioral;
