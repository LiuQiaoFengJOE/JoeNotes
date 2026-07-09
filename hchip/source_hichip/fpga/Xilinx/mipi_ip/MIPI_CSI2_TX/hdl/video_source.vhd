----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 08/01/2017 09:19:06 PM
-- Design Name: 
-- Module Name: video_source - Behavioral
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
--use IEEE.STD_LOGIC_ARITH.ALL;
--use IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity video_source is generic (	
	VIDEO_WIDTH : integer := 32;  -- actual width of the video QVGA = 320
	VIDEO_HEIGHT : integer := 16; -- actual height of the video QVGA = 240
	PIXEL_WIDTH : integer := 24;    -- bits per pixel
	LINE_PRE_HEADER  : integer := 10; --number of pixels before data starts
	LINE_POST_HEADER  :  integer := 14; --number of pixels after data stops
	FRAME_PRE_HEADER  : integer := 2; --number of lines before frame starts
    FRAME_POST_HEADER :  integer := 4; --number of pixels after data stops   
    MAX_PIXELS_PER_LINE_WIDTH : integer := 13;  -- 2**13 = 8192
    MAX_LINE_PER_FRAME_WIDTH : integer := 13  -- 2**13 = 8192
	);

      Port(clk : in STD_LOGIC;
           rst : in  STD_LOGIC;
           vsync : out STD_LOGIC;
           hsync : out STD_LOGIC;
           data_valid : out STD_LOGIC;
           pixel_data : out STD_LOGIC_VECTOR (PIXEL_WIDTH - 1 downto 0)        
           );

end video_source;

architecture Behavioral of video_source is

type state_type is (idle,framePreHeader,linePreHeader,dataGoesOut,
                    linePostHeader,framePostHeader);	
    signal state_reg, state_next : state_type := idle;
    signal data_out_reg,data_out_next : STD_LOGIC_VECTOR (PIXEL_WIDTH - 1 downto 0) := (others => '0');
    signal pixel_counter_reg,pixel_counter_next : STD_LOGIC_VECTOR (2**MAX_PIXELS_PER_LINE_WIDTH - 1 downto 0) := (others => '0');
    signal line_counter_reg,line_counter_next : STD_LOGIC_VECTOR (2**MAX_LINE_PER_FRAME_WIDTH - 1 downto 0) := (others => '0');
    
begin

pixel_data <= data_out_reg;
--data_valid <= data_valid_reg;

--FSMD state & data registers
FSMD_state : process(clk,rst)
begin
		if (rst = '1') then 
			state_reg <= idle;
			data_out_reg <= (others => '0');
			line_counter_reg <=  (others => '0');
			pixel_counter_reg <= (others => '0');
		elsif (clk'event and clk = '1') then 		
			state_reg <= state_next;
			data_out_reg <= data_out_next;
			line_counter_reg <=  line_counter_next;
			pixel_counter_reg <= pixel_counter_next;
		end if;
						
end process; --FSMD_state


--video output state machine
Video_Out_FSMD : process(state_reg,pixel_counter_reg,line_counter_reg)
begin

    state_next <= state_reg;
    line_counter_next <= line_counter_reg;
    pixel_counter_next <= pixel_counter_reg;
    data_out_next <= (others => '0');

    case state_reg is 
            when idle =>
                 state_next <= framePreHeader;
            when framePreHeader =>  
                 state_next <= linePreHeader;
            when linePreHeader =>
                 state_next <= dataGoesOut;            
            when dataGoesOut =>
                 data_out_next <= (others => '1');
                 if (to_integer(unsigned(pixel_counter_reg)) = VIDEO_WIDTH) then
                    state_next <= linePostHeader;
                    pixel_counter_next <= (others => '0');
                 else
                    pixel_counter_next <= std_logic_vector(unsigned(pixel_counter_reg) + 1);
                 end if;
                
            when linePostHeader =>
                 state_next <= framePostHeader;
            when framePostHeader =>
                 state_next <= framePreHeader;
            

    end case; --state_reg

end process; --Video_Out_FSMD


end Behavioral;
