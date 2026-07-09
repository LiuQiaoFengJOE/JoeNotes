--------------------------------------------------------------------------------
-- Company: 
-- Engineer:
--
-- Create Date:   10:27:32 09/30/2017
-- Design Name:   
-- Module Name:   /home/smishash/ProgSources/MIPI_CSI2_TX/simulation/test_transmit_frame.vhd
-- Project Name:  testISE
-- Target Device:  
-- Tool versions:  
-- Description:   
-- 
-- VHDL Test Bench Created by ISE for module: transmit_frame
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
 
-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--USE ieee.numeric_std.ALL;

use work.Common.all;
 
ENTITY test_transmit_frame IS
END test_transmit_frame;
 
ARCHITECTURE behavior OF test_transmit_frame IS 
 
    -- Component Declaration for the Unit Under Test (UUT)
 
    COMPONENT transmit_frame
    PORT(
         clk : IN  std_logic;
         clk_lp : IN  std_logic;
         rst : IN  std_logic;
         bytes_per_line : IN  std_logic_vector(15 downto 0);
         lines_per_frame : IN  std_logic_vector(15 downto 0);
         vc_num : IN  std_logic_vector(1 downto 0);
         data_type : IN   packet_type_t; --data type - YUV,RGB,RAW etc    
         frame_data_in : IN  std_logic_vector(7 downto 0);
      	frame_number : in std_logic_vector(15 downto 0);
         start_frame_transmission : IN  std_logic;
         hs_data_out : OUT  std_logic_vector(7 downto 0);
         lp_data_out : OUT  std_logic_vector(1 downto 0);
         hs_data_valid : OUT  std_logic;
         is_hs_mode : OUT  std_logic;
         ready_for_data_in_next_cycle : out std_logic; --goest high one clock cycle before ready 
                                                       --to get data
         line_sending_finished : out std_logic; --goes high when finished one line transmission, 
                                               -- hs_data_out is still valid (last byte)                                                   
         frame_sending_finished : out std_logic --goes high once frame sending is complete             
        );
    END COMPONENT;
    

   --Inputs
   signal clk : std_logic := '0';
   signal clk_lp : std_logic := '0';
   signal rst : std_logic := '0';
   signal bytes_per_line : std_logic_vector(15 downto 0) := x"0018"; --24 dec = length of crc_arr1 and crc_arr2 --:= (others => '0');
   signal lines_per_frame : std_logic_vector(15 downto 0) := x"0003"; --(others => '1');
   signal vc_num : std_logic_vector(1 downto 0) := (others => '0');
   signal data_type :  packet_type_t := RGB888; --data type - YUV,RGB,RAW etc    
   signal frame_data_in : std_logic_vector(7 downto 0) := (others => '0');
   signal frame_number : std_logic_vector(15 downto 0) := (others => '1');
   signal start_frame_transmission : std_logic := '0';

 	--Outputs
   signal hs_data_out : std_logic_vector(7 downto 0);
   signal lp_data_out : std_logic_vector(1 downto 0);
   signal hs_data_valid : std_logic;
   signal is_hs_mode : std_logic;
   signal ready_for_data_in_next_cycle : std_logic;
   signal line_sending_finished : std_logic;
   signal frame_sending_finished : std_logic;
   
   -- Clock period definitions
   constant clk_period : time := 10 ns;
   constant clk_lp_period : time := 10 ns;
 
BEGIN
 
	-- Instantiate the Unit Under Test (UUT)
   uut: transmit_frame PORT MAP (
          clk => clk,
          clk_lp => clk_lp,
          rst => rst,
          bytes_per_line => bytes_per_line,
          lines_per_frame => lines_per_frame,
          vc_num => vc_num,
          data_type => data_type,
          frame_data_in => frame_data_in,
          frame_number => frame_number,
          start_frame_transmission => start_frame_transmission,
          hs_data_out => hs_data_out,
          lp_data_out => lp_data_out,
          hs_data_valid => hs_data_valid,
          is_hs_mode => is_hs_mode,
          ready_for_data_in_next_cycle => ready_for_data_in_next_cycle,
          line_sending_finished => line_sending_finished,
          frame_sending_finished => frame_sending_finished
        );

   -- Clock process definitions
   clk_process :process
   begin
		clk <= '0';
		wait for clk_period/2;
		clk <= '1';
		wait for clk_period/2;
   end process;
 
   clk_lp_process :process
   begin
		clk_lp <= '0';
		wait for clk_lp_period/2;
		clk_lp <= '1';
		wait for clk_lp_period/2;
   end process;
 

   -- Stimulus process
   stim_proc: process
   begin		
		wait for clk_period*5;
     
		--reset
	   rst <= '1';
   	wait for clk_period*5;    
   	rst <= '0';
   
   	wait for clk_period*20;

 		--trigger frame transmission 
      start_frame_transmission <= '1';
      wait for clk_period;
      start_frame_transmission <= '0';

      wait;
   end process;

END;
