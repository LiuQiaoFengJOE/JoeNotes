----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 08/01/2017 11:24:50 PM
-- Design Name: 
-- Module Name: test_video_generator - Behavioral
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
--USE ieee.numeric_std.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity test_video_generator is 
    Generic (	
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
--  Port ( );
end test_video_generator;

architecture Behavioral of test_video_generator is

 -- Component Declaration for the Unit Under Test (UUT)
 
  COMPONENT  video_source is generic (	
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
           
  END COMPONENT;         

   --Inputs
   signal clk : std_logic := '0';
   signal rst : std_logic := '0';
 	--Outputs
   signal vsync :  STD_LOGIC  := '0';
   signal hsync :  STD_LOGIC := '0';
   signal data_valid :  STD_LOGIC  := '0';
   signal pixel_data :  STD_LOGIC_VECTOR (PIXEL_WIDTH - 1 downto 0) := (others => '0');
  
   constant clk_period : time := 10 ns;  
    
begin

	-- Instantiate the Unit Under Test (UUT)
	uut: video_source PORT MAP(
	        clk => clk,
            rst => rst,
            vsync =>  vsync,
            hsync => hsync, 
            data_valid =>   data_valid,
            pixel_data => pixel_data
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
              
               wait for clk_period*10;
                 
                 --reset
                 rst <= '1';
                 wait for clk_period*10;    
                 rst <= '0';
                 
                 wait for clk_period*10;
            end process;  

end Behavioral;
