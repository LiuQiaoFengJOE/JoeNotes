----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 08/21/2017 09:41:30 PM
-- Design Name: 
-- Module Name: test_crc_checksum - Behavioral
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

entity test_crc_checksum is
--  Port ( );
end test_crc_checksum;

architecture Behavioral of test_crc_checksum is
  
constant CRC0 : std_logic_vector(15 downto 0) := (others => '1'); --Control mode: Stop

signal clk : std_logic;
signal rst : std_logic;
signal data_in : std_logic_vector(7 downto 0);
signal crc_in  : std_logic_vector(15 downto 0) := CRC0;
signal crc_out : std_logic_vector(15 downto 0);
signal counter : integer := 0;

constant clk_period : time := 10 ns; --LP = 100 Mhz

type arr_of_crc is array (0 to 23) of std_logic_vector(7 downto 0);
signal crc_arr1 : arr_of_crc := (x"FF",x"00",x"00",x"02",x"B9",x"DC",x"F3",x"72",
                                 x"BB",x"D4",x"B8",x"5A",x"C8",x"75",x"C2",x"7C",
                                 x"81",x"F8",x"05",x"DF",x"FF",x"00",x"00",x"01");
								 
								 
signal crc_arr2 : arr_of_crc := (x"FF",x"00",x"00",x"00",x"1E",x"F0",x"1E",x"C7",
                                 x"4F",x"82",x"78",x"C5",x"82",x"E0",x"8C",x"70",
								         x"D2",x"3C",x"78",x"E9",x"FF",x"00",x"00",x"01");

begin

-- LP Clock process definitions
clk_process :process
begin
     clk <= '1';
     wait for clk_period/2;
     clk <= '0';
     wait for clk_period/2;
end process;   
        
        
-- Stimulus process
stim_proc: process
begin        
  
   wait for clk_period*2;
   
   --crc_in <=  nextCRC16_D8(data_in,CRC0);
   --crc_out <= crc_in;
     data_in <= crc_arr2(counter); --crc_arr1
     crc_in <=  nextCRC16_D8(crc_arr2(counter),crc_in);    
     counter <= counter +1;
 
end process;

end Behavioral;
