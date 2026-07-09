
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

use work.Common.all;

entity test_send_video_line is
--  Port ( );
end test_send_video_line;

architecture Behavioral of test_send_video_line is

-- Component Declaration for the Unit Under Test (UUT)

COMPONENT send_video_line is
    Port (clk : in std_logic; --data in/out clock HS clock, ~100 MHZ
    rst : in  std_logic;
	 word_cound : in std_logic_vector(15 downto 0); --length of the valid payload, bytes
	 vc_num : in std_logic_vector(1 downto 0); --virtual channel number  
	 data_type : in packet_type_t; --data type - YUV,RGB,RAW etc
	 video_data_in : in std_logic_vector(7 downto 0); --one byte of video payload
	 start_transmission : in std_logic; --trigger to start transmission,one clock cycle enough- word_cound,vc_num,video_data_in should be valid.
	 csi_data_out : out  std_logic_vector(7 downto 0); --one byte of CSI stream that goes to serializer
	 transmission_finished : out std_logic; --rised once the header, payload and footer data is sent.
	 data_out_valid : out std_logic; --goes high when csi_data_out is valid	
	 ready_for_data_in_next_cycle : out std_logic --goest high one clock cycle before ready to get data 
	 );
END COMPONENT;


--send_video_line
--Inputs
signal clk : std_logic; --word clock in
signal rst : std_logic; --asynchronous active high reset
signal word_cound :  std_logic_vector(15 downto 0) := (others => '0'); --length of the valid payload, bytes
signal vc_num :  std_logic_vector(1 downto 0) := "00"; --virtual channel number  
signal data_type :  packet_type_t; --data type - YUV,RGB,RAW etc
signal video_data_in :  std_logic_vector(7 downto 0) := (others => '0'); --one byte of video payload
signal start_transmission :  std_logic := '0'; --trigger to start transmission,one clock cycle enough- word_cound,vc_num,video_data_in 
                                          --should be valid.
--Outputs
signal csi_data_out :   std_logic_vector(7 downto 0); --one byte of CSI stream that goes to serializer
signal transmission_finished :  std_logic; --rised once the header, payload and footer data is sent.
signal data_out_valid :  std_logic; --goes high when csi_data_out is valid	 
signal ready_for_data_in_next_cycle : std_logic; --goest high one clock cycle before ready to get data

constant clk_period : time := 10 ns; --LP = 100 Mhz
constant CRC0 : std_logic_vector(15 downto 0) := (others => '1'); --Control mode: Stop

signal counter : integer := 0;
type arr_of_crc is array (0 to 23) of std_logic_vector(0 to 7);
signal crc_arr1 : arr_of_crc := (x"FF",x"00",x"00",x"02",x"B9",x"DC",x"F3",x"72",
                                 x"BB",x"D4",x"B8",x"5A",x"C8",x"75",x"C2",x"7C",
                                 x"81",x"F8",x"05",x"DF",x"FF",x"00",x"00",x"01"); --Checksum LS byte and MS byte: F0 00

signal crc_arr2 : arr_of_crc := (x"FF",x"00",x"00",x"00",x"1E",x"F0",x"1E",x"C7",
                                 x"4F",x"82",x"78",x"C5",x"82",x"E0",x"8C",x"70",
								         x"D2",x"3C",x"78",x"E9",x"FF",x"00",x"00",x"01");--Checksum LS byte and MS byte: 69 E5

begin

-- Instantiate the Unit Under Test (UUT)
uut: send_video_line PORT MAP(clk => clk,
    rst => rst,
	 word_cound => word_cound,
	 vc_num => vc_num,
	 data_type => data_type,
	 video_data_in => video_data_in,
	 start_transmission => start_transmission,
	 csi_data_out => csi_data_out,
	 transmission_finished => transmission_finished,
	 data_out_valid => data_out_valid,
	 ready_for_data_in_next_cycle =>  ready_for_data_in_next_cycle );


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
   for I in 0 to 23 loop			 
		 video_data_in <= crc_arr2(I); --crc_arr1       
       counter <= counter + 1;
       wait for clk_period;
   end loop;
   video_data_in <= x"AB";
   
   wait for clk_period*20;
   

   
end process;


end Behavioral;
