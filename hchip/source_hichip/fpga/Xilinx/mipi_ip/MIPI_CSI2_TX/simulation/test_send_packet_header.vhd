--------------------------------------------------------------------------------
-- Company: 
-- Engineer:
--
-- Create Date:   10:53:25 10/15/2017
-- Design Name:   
-- Module Name:   /home/smishash/ProgSources/MIPI_CSI2_TX/simulation/test_send_packet_header.vhd
-- Project Name:  testISE
-- Target Device:  
-- Tool versions:  
-- Description:   
-- 
-- VHDL Test Bench Created by ISE for module: send_packet_header
-- 
-- Dependencies:
-- 
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
--
-- Notes: 
-- This testbench has been automatically generated using types std_logic and
-- std_logic_vector for the ports of the unit under test.  Xilinx recommends
-- that these types always be used for the top-level I/O of a design in order
-- to guarantee that the testbench will bind correctly to the post-implementation 
-- simulation model.
--------------------------------------------------------------------------------
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;

use work.Common.all;
 
-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--USE ieee.numeric_std.ALL;
 
ENTITY test_send_packet_header IS
END test_send_packet_header;
 
ARCHITECTURE behavior OF test_send_packet_header IS 
 
    -- Component Declaration for the Unit Under Test (UUT)
 
    COMPONENT send_packet_header
    PORT(
         clk : IN  std_logic;
         rst : IN  std_logic;
         packet_to_send : IN  std_logic_vector(31 downto 0);
         start_sending_packet : IN  std_logic;
         packet_done_next_cyle : OUT  std_logic;
         data_out : OUT  std_logic_vector(7 downto 0);
         data_valid : OUT  std_logic
        );
    END COMPONENT;
    

   --Inputs
   signal clk : std_logic := '0';
   signal rst : std_logic := '0';
   signal packet_to_send : std_logic_vector(31 downto 0) := (others => '0');
   signal start_sending_packet : std_logic := '0';

 	--Outputs
   signal packet_done_next_cyle : std_logic;
   signal data_out : std_logic_vector(7 downto 0);
   signal data_valid : std_logic;

   -- Clock period definitions
   constant clk_period : time := 10 ns;
 
BEGIN
 
	-- Instantiate the Unit Under Test (UUT)
   uut: send_packet_header PORT MAP (
          clk => clk,
          rst => rst,
          packet_to_send => packet_to_send,
          start_sending_packet => start_sending_packet,
          packet_done_next_cyle => packet_done_next_cyle,
          data_out => data_out,
          data_valid => data_valid
        );

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
  
   wait for clk_period*5;
     
   --reset
   rst <= '1';
   wait for clk_period*5;    
   rst <= '0';
   
   wait for clk_period*5;
   
   --assign data for test - frame_start
   packet_to_send <= get_short_packet("00",frame_start,x"FFFF");
   --togle start of transmission
   start_sending_packet <= '1';
   wait for clk_period;
   start_sending_packet <= '0';
   
   
   wait until packet_done_next_cyle = '1';
      
   wait for clk_period*20;
   
   --assign data for test - line_start
   packet_to_send <= get_short_packet("00",line_start,x"FFFF");
      --togle start of transmission
   start_sending_packet <= '1';
   wait for clk_period;
   start_sending_packet <= '0';
   
  
   wait until packet_done_next_cyle = '1';
      
   wait for clk_period*20;
   
   
end process;

END;
