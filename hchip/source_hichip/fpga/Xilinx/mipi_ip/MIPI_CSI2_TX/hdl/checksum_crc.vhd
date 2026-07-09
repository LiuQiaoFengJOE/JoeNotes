----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 08/20/2017 08:52:26 PM
-- Design Name: 
-- Module Name: checksum_crc - Behavioral
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

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity checksum_crc is
   Port (data_in : in std_logic_vector(7 downto 0);
     crc_in  : in  std_logic_vector(15 downto 0);
     crc_out : out  std_logic_vector(15 downto 0)
   );
end checksum_crc;

architecture Behavioral of checksum_crc is

signal d : std_logic_vector(7 downto 0);
signal c : std_logic_vector(15 downto 0);

begin

d <= data_in;
c <= crc_in;

crc_out(0) <= d(4) xor d(0) xor c(8) xor c(12);
crc_out(1) <= d(5) xor d(1) xor c(9) xor c(13);
crc_out(2) <= d(6) xor d(2) xor c(10) xor c(14);
crc_out(3) <= d(7) xor d(3) xor c(11) xor c(15);
crc_out(4) <= d(4) xor c(12);
crc_out(5) <= d(5) xor d(4) xor d(0) xor c(8) xor c(12) xor c(13);
crc_out(6) <= d(6) xor d(5) xor d(1) xor c(9) xor c(13) xor c(14);
crc_out(7) <= d(7) xor d(6) xor d(2) xor c(10) xor c(14) xor c(15);
crc_out(8) <= d(7) xor d(3) xor c(0) xor c(11) xor c(15);
crc_out(9) <= d(4) xor c(1) xor c(12);
crc_out(10) <= d(5) xor c(2) xor c(13);
crc_out(11) <= d(6) xor c(3) xor c(14);
crc_out(12) <= d(7) xor d(4) xor d(0) xor c(4) xor c(8) xor c(12) xor c(15);
crc_out(13) <= d(5) xor d(1) xor c(5) xor c(9) xor c(13);
crc_out(14) <= d(6) xor d(2) xor c(6) xor c(10) xor c(14);
crc_out(15) <= d(7) xor d(3) xor c(7) xor c(11) xor c(15);


end Behavioral;
