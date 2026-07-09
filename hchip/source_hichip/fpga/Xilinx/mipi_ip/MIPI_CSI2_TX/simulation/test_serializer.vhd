
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
 
-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--USE ieee.numeric_std.ALL;
 
ENTITY test_serializer IS
END test_serializer;
 
ARCHITECTURE behavior OF test_serializer IS 
 
    -- Component Declaration for the Unit Under Test (UUT)
 
    COMPONENT simple_serializer
    PORT(
         clk : IN  std_logic;
         data_in : IN  std_logic_vector(7 downto 0);
         data_out : OUT  std_logic;
         gate : IN  std_logic
        );
    END COMPONENT;
    

   --Inputs
   signal clk : std_logic := '0';
   signal data_in : std_logic_vector(7 downto 0) := (others => '0');
   signal gate : std_logic := '0';

 	--Outputs
   signal data_out : std_logic;
   
   --helpers   

   -- Clock period definitions
   constant clk_period : time := 1 ns;
   constant gate_period : time := 8 ns;
 
BEGIN
 
	-- Instantiate the Unit Under Test (UUT)
   uut: simple_serializer PORT MAP (
          clk => clk,
          data_in => data_in,
          data_out => data_out,
          gate => gate
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
      gate <= '0';
      wait for clk_period*10;
      wait for clk_period/2; --important (sampling on rising edge)

      -- insert stimulus here 
      gate <= '1';
      data_in <= "10011011";
      wait for gate_period;
      data_in <= "01100100";
      wait for gate_period;
      data_in <= "10011011";
      wait for gate_period;
      data_in <= "01100100";

      wait;
   end process;

END;
