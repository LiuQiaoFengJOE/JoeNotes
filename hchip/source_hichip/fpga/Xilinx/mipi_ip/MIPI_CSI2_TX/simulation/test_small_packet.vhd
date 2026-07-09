----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 09/18/2017 10:43:49 PM
-- Design Name: 
-- Module Name: test_small_packet - Behavioral
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

use work.Common.all;

entity test_small_packet is
--  Port ( );
end test_small_packet;

architecture Behavioral of test_small_packet is

-- Component Declaration for the Unit Under Test (UUT)

COMPONENT csi_rx_packet_handler is
    Port ( clock : in STD_LOGIC; --word clock in
           reset : in STD_LOGIC; --asynchronous active high reset
           enable : in STD_LOGIC; --active high enable
           data : in STD_LOGIC_VECTOR (31 downto 0); --data in from word aligner
           data_valid : in STD_LOGIC; --data valid in from word aligner
           sync_wait : out STD_LOGIC; --drives byte and word aligner wait_for_sync
           packet_done : out STD_LOGIC; --drives word aligner packet_done
           payload_out : out STD_LOGIC_VECTOR(31 downto 0); --payload out from long video packets
           payload_valid : out STD_LOGIC; --whether or not payload output is valid (i.e. currently receiving a long packet)
           vsync_out : out STD_LOGIC; --vsync output to timing controller
           in_frame : out STD_LOGIC; --whether or not currently in video frame (i.e. got FS but not FE)
		   in_line : out STD_LOGIC); --whether or not receiving video line
END COMPONENT;

--Inputs
signal clk : std_logic; --word clock in
signal rst : std_logic; --asynchronous active high reset
signal enable :  std_logic := '0';  --active high enable
signal data :  std_logic_vector (31 downto 0); --data in from word aligner
signal data_valid : std_logic := '0'; --data valid in from word aligner
--Outputs
signal sync_wait :  std_logic; --drives byte and word aligner wait_for_sync
signal packet_done :  std_logic; --drives word aligner packet_done
signal payload_out :  std_logic_vector(31 downto 0); --payload out from long video packets
signal payload_valid :  std_logic; --whether or not payload output is valid (i.e. currently receiving a long packet)
signal vsync_out :  std_logic; --vsync output to timing controller
signal in_frame :  std_logic; --whether or not currently in video frame (i.e. got FS but not FE)
signal in_line :  std_logic; --whether or not receiving video line

constant clk_period : time := 10 ns; --LP = 100 Mhz

begin

-- Instantiate the Unit Under Test (UUT)
uut: csi_rx_packet_handler PORT MAP(
           clock => clk,
           reset => rst,
           enable =>  enable,
           data => data,
           data_valid => data_valid,
           sync_wait => sync_wait,
           packet_done => packet_done,
           payload_out => payload_out,
           payload_valid => payload_valid,
           vsync_out => vsync_out,
           in_frame => in_frame,
		   in_line =>  in_line);


-- Clock process definitions
clk_process :process
begin
     clk <= '0';
     wait for clk_period/2;
     clk <= '1';
     wait for clk_period/2;
end process;   
        
        
-- Stimulus process
stim_proc: process
begin        
	
	data <= (others => '0');
   data_valid <= '0';
  
   wait for clk_period*5;
   wait for clk_period/2;
     
   --reset
   rst <= '1';
   wait for clk_period*5;    
   rst <= '0';
   
   wait for clk_period*5;
   
   --enable 
   enable <= '1';
   wait for clk_period*20;

   
   wait for clk_period*20;
   
   
   --assign data for test - frame_start
   data_valid <= '1';
   --wait for clk_period;
   data <= x"35ABCD00";--get_short_packet("00",frame_start,x"ABCD");
   wait for clk_period;
   data <= (others => '0');
   data_valid <= '0';
   
   
   wait for clk_period*20;
   
   --assign data for test - line_start
   data_valid <= '1';
   --wait for clk_period;
   data <= x"3EABCD02"; --get_short_packet("00",line_start,x"ABCD");  
   wait for clk_period;
   data <= (others => '0');
   data_valid <= '0';
   
   wait for clk_period*20;
   
   
end process;


end Behavioral;
