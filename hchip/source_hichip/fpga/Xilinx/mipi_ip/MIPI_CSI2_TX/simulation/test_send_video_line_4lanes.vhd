
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
 
-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--USE ieee.numeric_std.ALL;

use work.Common.all;
 
ENTITY test_send_video_line_4lanes IS
END test_send_video_line_4lanes;
 
ARCHITECTURE Behavioral OF test_send_video_line_4lanes IS 
 
    -- Component Declaration for the Unit Under Test (UUT)
 
    COMPONENT send_video_line_4_lanes
    PORT(
         clk : IN  std_logic;
         rst : IN  std_logic;
         word_cound : IN  std_logic_vector(15 downto 0);
         vc_num : IN  std_logic_vector(1 downto 0);
         data_type : IN  packet_type_t; --data type - YUV,RGB,RAW etc;
         video_data_in : IN  std_logic_vector(31 downto 0);
         start_transmission : IN  std_logic;
         csi_lane0 : OUT  std_logic_vector(7 downto 0);
         csi_lane1 : OUT  std_logic_vector(7 downto 0);
         csi_lane2 : OUT  std_logic_vector(7 downto 0);
         csi_lane3 : OUT  std_logic_vector(7 downto 0);
         transmission_finished : OUT  std_logic;
         data_out_valid : OUT  std_logic;
         ready_for_data_in_next_cycle : OUT  std_logic
        );
    END COMPONENT;
    

   --Inputs
   signal clk : std_logic := '0';
   signal rst : std_logic := '0';
   signal word_cound : std_logic_vector(15 downto 0) := (others => '0');
   signal vc_num : std_logic_vector(1 downto 0) := (others => '0');
   signal data_type : packet_type_t; --data type - YUV,RGB,RAW etc
   signal video_data_in : std_logic_vector(31 downto 0) := (others => '0');
   signal start_transmission : std_logic := '0';

 	--Outputs
   signal csi_lane0 : std_logic_vector(7 downto 0);
   signal csi_lane1 : std_logic_vector(7 downto 0);
   signal csi_lane2 : std_logic_vector(7 downto 0);
   signal csi_lane3 : std_logic_vector(7 downto 0);
   signal transmission_finished : std_logic;
   signal data_out_valid : std_logic;
   signal ready_for_data_in_next_cycle : std_logic;

   -- Clock period definitions
   constant clk_period : time := 10 ns;
   
   
constant CRC0 : std_logic_vector(15 downto 0) := (others => '1'); --Control mode: Stop

signal counter : integer := 0;
type arr_of_crc is array (0 to 23) of std_logic_vector(0 to 7);
signal crc_arr1 : arr_of_crc := (x"FF",x"00",x"00",x"02",x"B9",x"DC",x"F3",x"72",
                                 x"BB",x"D4",x"B8",x"5A",x"C8",x"75",x"C2",x"7C",
                                 x"81",x"F8",x"05",x"DF",x"FF",x"00",x"00",x"01"); --Checksum LS byte and MS byte: F0 00

signal crc_arr2 : arr_of_crc := (x"FF",x"00",x"00",x"00",x"1E",x"F0",x"1E",x"C7",
                                 x"4F",x"82",x"78",x"C5",x"82",x"E0",x"8C",x"70",
								         x"D2",x"3C",x"78",x"E9",x"FF",x"00",x"00",x"01");--Checksum LS byte and MS byte: 69 E5
signal crc_test : arr_of_crc := crc_arr2;
 
BEGIN
 
	-- Instantiate the Unit Under Test (UUT)
   uut: send_video_line_4_lanes PORT MAP (
          clk => clk,
          rst => rst,
          word_cound => word_cound,
          vc_num => vc_num,
          data_type => data_type,
          video_data_in => video_data_in,
          start_transmission => start_transmission,
          csi_lane0 => csi_lane0,
          csi_lane1 => csi_lane1,
          csi_lane2 => csi_lane2,
          csi_lane3 => csi_lane3,
          transmission_finished => transmission_finished,
          data_out_valid => data_out_valid,
          ready_for_data_in_next_cycle => ready_for_data_in_next_cycle
        );

 
-- Clock process definitions
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
  
   wait for clk_period*5;
     
   --reset
   rst <= '1';
   wait for clk_period*5;    
   rst <= '0';
   
   wait for clk_period*20;
   
   
   --****START LINE GENERATION*****
    word_cound <= x"0018"; --24 dec = length of crc_arr1 and crc_arr2
	 vc_num <= "00"; --using VC 0;
	 data_type <= RGB888;--RGB888	
	 
	 --toggle transmission
	 start_transmission <= '1';
	 wait for clk_period;   
	 start_transmission <= '0';  
	 
	 --wait until ready
	 wait until ready_for_data_in_next_cycle = '1';
	 
   --wait one clock cycle
	 wait for clk_period;  
	 
   --start to otput the data array
   for I in 0 to 5 loop			 
		 --video_data_in(31 downto 24) <= crc_arr2(I); --crc_arr1  --
         video_data_in(31 downto 24) <= crc_test(counter);
		 video_data_in(23 downto 16) <= crc_test(counter + 1);
		 video_data_in(15 downto 8)  <= crc_test(counter + 2);
		 video_data_in(7  downto 0)  <= crc_test(counter + 3);     
       counter <= counter + 4;
       wait for clk_period;
   end loop;
   video_data_in <= x"ABCDEFAA";
   
   wait for clk_period*20;
   

   
end process;


end Behavioral;