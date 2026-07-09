----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    14:17:41 10/18/2017 
-- Design Name: 
-- Module Name:    demult_4lanes - Behavioral 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------

--TODO: BAD,BAD,BAD !!! doen't take into account SoT signal that need to be boradcasted to all 4 lanes!!!
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity demult_4lanes is
	Port(clk             : in  STD_LOGIC; --clock of hs_data_in
	     hs_data_in      : in  std_logic_vector(7 downto 0); --one byte of CSI stream that comes from frame generator
	     hs_data_valid   : in  std_logic; --1 when hs_data_out is valid
	     hs_4lanes_clock : out std_logic; -- clock for demuxed lanes , 1/4 of clk in;
	     hs_demuxed_valid :  out std_logic; -- 1 when demuxed data is valid
	     hs_data_lane0   : out std_logic_vector(7 downto 0);
	     hs_data_lane1   : out std_logic_vector(7 downto 0);
	     hs_data_lane2   : out std_logic_vector(7 downto 0);
	     hs_data_lane3   : out std_logic_vector(7 downto 0)
	    );
end demult_4lanes;

architecture Behavioral of demult_4lanes is

	type state_type is (idle, byte1, byte2, byte3);
	signal state_reg, state_next : state_type := idle;

	signal all_lanes_reg,all_lanes_next : std_logic_vector(31 downto 0) := (others => '0');
	signal all_lanes_old_reg,all_lanes_old_next : std_logic_vector(31 downto 0) := (others => '0');
	signal mux_done_reg,mux_done_next : std_logic := '0';
	
    signal counter_reg,counter_next : unsigned (1 downto 0) := "00";

begin

	hs_data_lane0 <= all_lanes_old_reg(31 downto 24) when (mux_done_reg = '1') else (others => '0');
	hs_data_lane1 <= all_lanes_old_reg(23 downto 16) when (mux_done_reg = '1') else (others => '0');
	hs_data_lane2 <= all_lanes_old_reg(15 downto 8)  when (mux_done_reg = '1') else (others => '0');
	hs_data_lane3 <= all_lanes_old_reg(7 downto 0)    when (mux_done_reg = '1') else (others => '0');
	hs_demuxed_valid <= '1' when (mux_done_reg = '1')  else '0';

--remove "not" to shift the phase of the clock 180 deg., to be rising edge synchronous with data change		
    hs_4lanes_clock <= not counter_reg(1); 

	--FSMD state & data registers
	FSMD_state : process(clk)
	begin
        if (clk'event and clk = '1') then
			state_reg <= state_next;
			all_lanes_old_reg <= all_lanes_old_next;
			mux_done_reg <= mux_done_next;
			all_lanes_reg <= all_lanes_next; 
			counter_reg <= counter_next;			
		end if;
	end process;

	HEADER_FSMD : process(state_reg, hs_data_valid, hs_data_in,all_lanes_old_reg,all_lanes_reg,mux_done_reg,counter_reg)
	begin
		state_next         <= state_reg;
		all_lanes_old_next <= all_lanes_old_reg;
		mux_done_next      <= mux_done_reg;
		all_lanes_next     <= all_lanes_reg;
		counter_next       <= counter_reg;
		

		--clock out related'
		if (counter_reg = 3) then
			counter_next <= "00";
		else
			counter_next <= counter_reg + 1;
		end if;
		
				
		case state_reg is

		when idle =>
						
			all_lanes_next <= (others => '0');

				if (hs_data_valid = '1') then
					all_lanes_next(31 downto 24)  <= hs_data_in;
					state_next <= byte1;
				else
					if (counter_reg = "01") then --change "01" according to "not" in line 65 (hs_4lanes_clock <= not counter_reg(1);) 
					all_lanes_old_next <=  (others => '0');
					all_lanes_next  <=  (others => '0');
					mux_done_next <= '0';
--					counter_next <= "00";
					end if;
				end if;

			when byte1 =>
				all_lanes_next(23 downto 16)  <= hs_data_in;
				state_next <= byte2;
			when byte2 =>
				all_lanes_next(15 downto 8)  <= hs_data_in;
				state_next <= byte3;
			when byte3 =>
				--all_lanes_next(7 downto 0)  <= hs_data_in;
				all_lanes_old_next <= all_lanes_reg;
				all_lanes_old_next(7 downto 0)  <= hs_data_in;
				mux_done_next <= '1';
				state_next <= idle;
		end case;                       --state_reg

	end process;                        --HEADER_FSMD  

end Behavioral;

