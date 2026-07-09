----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 09/08/2017 09:57:14 AM
-- Design Name: 
-- Module Name: fmc_mipi_top - Behavioral
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
use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
library UNISIM;
use UNISIM.VComponents.all;

entity fmc_mipi_top is
   Port ( 
   sys_clk_p : in std_logic;  --AD12 SYSCLK_P
   sys_clk_n : in std_logic;  --AD11 SYSCLK_N
   rst : in std_logic; -- AB7 use CPU_RESET button
      
   FMC_Switch1_SEL1 : out std_logic := '0'; --H30 HPC_LA06_P --put LVDS to HS mode  ( 1 = LP mode, 0 = HS mode)
   FMC_Switch1_SEL2 : out std_logic := '0'; --G30 HPC_LA06_N --put LVDS to HS mode  ( 1 = LP mode, 0 = HS mode)
   FMC_Switch2_SEL1 : out std_logic := '1'; --F12 HPC_HA09_P --put CML to LP mode  ( 1 = LP mode, 0 = HS mode)
   FMC_Switch2_SEL2 : out std_logic := '1'; --E13 HPC_HA09_N --put CML to LP mode  ( 1 = LP mode, 0 = HS mode)
   
   FMC_I2C_CAM_CLK : inout std_logic; --F11 HPC_HA04_P
   FMC_I2C_CAM_DAT : inout std_logic; --C12 HPC_HA03_P
   
   FMC_I2C_GP0_CLK_1V8 : inout std_logic; --C11 HPC_HA02_N
   FMC_I2C_GP0_DAT_1V8 : inout std_logic; --B14 HPC_HA07_P
     
   FMC_TR_1DIR : out std_logic := '0'; --E11 HPC_HA04_N --I2C data transmission FROM Jetson to FPGA 
   
--BUG - no connection from FMC to FPGA in KC705;   FMC_CAM_VSYNC  : in std_logic;    -- HPC_HB16_P
   FMC_CAM_FLASH_EN  : in std_logic; --C22 HPC_LA30_N

--BUG - no connection from FMC to FPGA in KC705;   FMC_CAM0_MCLK : in std_logic; -- HPC_HB12_P
   FMC_CAM0_PWR  : in std_logic; --C16 HPC_LA28_N
   FMC_CAM0_RST  : in std_logic; --D22 HPC_LA30_P

--BUG - no connection from FMC to FPGA in KC705;   FMC_CAM2_MCLK  : in std_logic; -- HPC_HB16_N
--BUG - no connection from FMC to FPGA in KC705;   FMC_CAM2_PWRD  : in std_logic; -- HPC_HB20_P
--BUG - no connection from FMC to FPGA in KC705;   FMC_CAM2_RST  : in std_logic;  -- HPC_HB20_N
   
   --******MIPI CSI LVDS based implemntation*******
   --Low Power lines - LP
   LP_D3_P_lp  : out std_logic := '0'; --B19 HPC_LA27_N
   LP_D3_N_lp  : out std_logic := '0'; --C19 HPC_LA27_P 
--BUG - no connection from FMC to FPGA in KC705;   LP_D2_P_lp  : out std_logic := '0'; -- HPC_HB07_P FMC_LP_D2_P
--BUG - no connection from FMC to FPGA in KC705;   LP_D2_N_lp  : out std_logic := '0'; -- HPC_HB07_N HPC_FMC_LP_D2_N
   LP_D1_P_lp  : out std_logic := '0'; --B22 HPC_LA23_P
   LP_D1_N_lp  : out std_logic := '0'; --A22 HPC_LA23_N
   LP_D0_P_lp  : out std_logic := '0'; --B28 HPC_LA14_P
   LP_D0_N_lp  : out std_logic := '0'; --A28 HPC_LA14_N
   LP_CLK_P_lp : out std_logic := '0'; --J11 HPC_HA21_P FMC_LP_CLK_P may be need to change to input to enble HS free-run clock hack!!! PLL takes up to 1ms to lock
   LP_CLK_N_lp : out std_logic := '0'; --J12 HPC_HA21_N FMC_LP_CLK_N may be need to change to input to enble HS free-run clock hack!!! PLL takes up to 1ms to lock
   
   --High Speed lines - HS
    HS_CLK_P : out std_logic := '0'; --C25 HPC_LA00_CC_P
    HS_CLK_N : out std_logic := '0'; --B25 HPC_LA00_CC_N
    HS_D0_P  : out std_logic := '0'; --H24 HPC_LA02_P
    HS_D0_N  : out std_logic := '0'; --H25 HPC_LA02_N
    HS_D1_P  : out std_logic := '0'; --H26 HPC_LA03_P
    HS_D1_N  : out std_logic := '0'; --H27 HPC_LA03_N
    HS_D2_P  : out std_logic := '0'; --G28 HPC_LA04_P
    HS_D2_N  : out std_logic := '0'; --F28 HPC_LA04_N
    HS_D3_P  : out std_logic := '0'; --E29 HPC_LA08_P
    HS_D3_N  : out std_logic := '0'; --E30 HPC_LA08_N

   --******MIPI CSI MGT based implemntation*******   
    --Low Power lines - LP
   LP2_CLK_P_lp : out std_logic := '0'; --B13 HPC_HA11_P FMC_LP2_CLK_P
   LP2_CLK_N_lp : out std_logic := '0'; --A13 HPC_HA11_N FMC_LP2_CLK_N
   LP2_D0_P_lp  : out std_logic := '0'; --C29 HPC_LA12_P FMC_LP2_D0_P
   LP2_D0_N_lp  : out std_logic := '0'; --B29 HPC_LA12_N FMC_LP2_D0_N
   LP2_D1_P_lp  : out std_logic := '0'; --J16 HPC_HA14_P FMC_LP2_D1_P
   LP2_D1_N_lp  : out std_logic := '0'; --H16 HPC_HA14_N FMC_LP2_D1_N
   LP2_D2_P_lp  : out std_logic := '0'; --F13 HPC_HA17_N_CC FMC_LP2_D2_P  !!! -> N <-> P switched why!!! -> this is LP, single ended,doesn't matter too much, but the naming is bad
   LP2_D2_N_lp  : out std_logic := '0'; --G13 HPC_HA17_P_CC FMC_LP2_D2_N   !!! -> N <-> P switched why!!! -> this is LP, single ended,doesn't matter too much, but the naming is bad
   
   --debug IO              
   leds_debug : out std_logic_vector(7 downto 0) := (others => '1'); --debug
   GPIO_SW_N  : in std_logic; -- AA12   up button
   GPIO_SW_E  : in std_logic; -- AG5    right button 
   GPIO_SW_S  : in std_logic; -- AB12   down button
   GPIO_SW_W  : in std_logic; -- AC6    left button
   GPIO_SW_C  : in std_logic  -- G12    Center button
   
   );
end fmc_mipi_top;

architecture Behavioral of fmc_mipi_top is

signal clk_200Mhz,clk_10MHz,locked : std_logic;
signal hs_clk,hs_d0,hs_d1,hs_d2,hs_d3 : std_logic;

--test LVDS levels related
signal  counter : integer; --5 clock cycles of 100 Mhz = 50 ns
--end of test LVDS levels related
begin

--hs_clk <= '1';
hs_d0 <= '1';
hs_d1 <= '0';
--hs_d2 <= '1';
hs_d3 <= '0';

--test LVDS levels

hs_clk <= '1' when counter > 200 else '0';
--hs_clk <= clk_200Mhz; --push 200 Mhz
hs_d2 <= '1' when counter > 2 else '0'; --with replaced resistors - 500 and 33 Ohm
--end of test LVDS levels

leds_debug(0) <= '1';
leds_debug(1) <= '1';
leds_debug(2) <= '0';
leds_debug(3) <= '1';
leds_debug(4) <= '0';
leds_debug(5) <= FMC_CAM_FLASH_EN;
leds_debug(6) <= FMC_CAM0_RST;
leds_debug(7) <= FMC_CAM0_PWR;
       
FMC_I2C_CAM_CLK <= 'Z';
FMC_I2C_CAM_DAT <= 'Z';
   
FMC_I2C_GP0_CLK_1V8 <= 'Z';
FMC_I2C_GP0_DAT_1V8 <= 'Z';


--FSMD state & data registers
FSMD_state : process(clk_200Mhz,rst)
begin
        
     if (rst = '1') then 
			
		elsif (clk_200Mhz'event and clk_200Mhz = '1') then 	
		   counter <= counter +1;			
		   if (counter = 400) then
		   counter <= 0;
		   end if; 
		end if;
						
end process; --FSMD_state
--end of test LVDS levels

--instantinate differential pairs
clck_in_IBUFDS: unisim.vcomponents.IBUFDS
port map (
  I  => sys_clk_p,
  IB => sys_clk_n,
  O  => clk_200Mhz
);

hs_clk_OBUFDS : unisim.vcomponents.OBUFDS
  port map (
    I => hs_clk,
    O => HS_CLK_P,
    OB => HS_CLK_N
  ); 

hs_d0_OBUFDS : unisim.vcomponents.OBUFDS
  port map (
    I => hs_d0,
    O => HS_D0_P,
    OB => HS_D0_N
  );   

hs_d1_OBUFDS : unisim.vcomponents.OBUFDS
  port map (
    I => hs_d1,
    O => HS_D1_P,
    OB => HS_D1_N
  ); 
    
hs_d2_OBUFDS : unisim.vcomponents.OBUFDS
  port map (
    I => hs_d2,
    O => HS_D2_P,
    OB => HS_D2_N
  ); 
    
hs_d3_OBUFDS : unisim.vcomponents.OBUFDS
  port map (
    I => hs_d3,
    O => HS_D3_P,
    OB => HS_D3_N
  );     
  
end Behavioral;
