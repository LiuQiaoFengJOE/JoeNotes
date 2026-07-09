
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
 
-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--USE ieee.numeric_std.ALL;
 
ENTITY test_demult_4lanes IS
END test_demult_4lanes;
 
ARCHITECTURE behavior OF test_demult_4lanes IS 
 
    -- Component Declaration for the Unit Under Test (UUT)
 
    COMPONENT demult_4lanes
    PORT(
         clk : IN  std_logic;       
         hs_data_in : IN  std_logic_vector(7 downto 0);
         hs_data_valid : IN  std_logic;
         hs_4lanes_clock : OUT  std_logic;
         hs_demuxed_valid :  out std_logic; -- 1 when demuxed data is valid
         hs_data_lane0 : OUT  std_logic_vector(7 downto 0);
         hs_data_lane1 : OUT  std_logic_vector(7 downto 0);
         hs_data_lane2 : OUT  std_logic_vector(7 downto 0);
         hs_data_lane3 : OUT  std_logic_vector(7 downto 0)
        );
    END COMPONENT;
    

   --Inputs
   signal clk : std_logic := '0';
   signal hs_data_in : std_logic_vector(7 downto 0) := (others => '0');
   signal hs_data_valid : std_logic := '0';

 	--Outputs
   signal hs_4lanes_clock : std_logic;
   signal hs_demuxed_valid :  std_logic; -- 1 when demuxed data is valid
   signal hs_data_lane0 : std_logic_vector(7 downto 0);
   signal hs_data_lane1 : std_logic_vector(7 downto 0);
   signal hs_data_lane2 : std_logic_vector(7 downto 0);
   signal hs_data_lane3 : std_logic_vector(7 downto 0);

   -- Clock period definitions
   constant clk_period : time := 10 ns;
 
BEGIN
 
	-- Instantiate the Unit Under Test (UUT)
   uut: demult_4lanes PORT MAP (
          clk => clk,
          hs_data_in => hs_data_in,
          hs_data_valid => hs_data_valid,
          hs_4lanes_clock => hs_4lanes_clock,
          hs_demuxed_valid => hs_demuxed_valid,
          hs_data_lane0 => hs_data_lane0,
          hs_data_lane1 => hs_data_lane1,
          hs_data_lane2 => hs_data_lane2,
          hs_data_lane3 => hs_data_lane3
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

   	wait for clk_period*10;
   	
   	hs_data_valid <= '1';
   	
   	hs_data_in <= x"FF";   --1   	
   	wait for clk_period;   
   	
   	hs_data_in <= x"AB";
   	wait for clk_period; ---2
   	
   	hs_data_in <= x"98";
   	wait for clk_period; --3
   	
   	hs_data_in <= x"AA";
   	wait for clk_period; --4
   	
   	
   	
   	hs_data_in <= x"23";
   	wait for clk_period; --5
   	
   	hs_data_in <= x"00";
   	wait for clk_period; --6
   	
   	hs_data_in <= x"FA";
   	wait for clk_period; --7
   	
   	
    hs_data_valid <= '0'; --reset
   	hs_data_in <= x"00";

      -- insert stimulus here 

      wait;
   end process;

END;
