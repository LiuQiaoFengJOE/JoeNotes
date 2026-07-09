library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
--use IEEE.STD_LOGIC_ARITH.ALL;
--use IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.NUMERIC_STD.ALL;

--serializer for D_PHY protocol
entity one_lane_D_PHY is generic (	
	DATA_WIDTH_IN : integer := 8;
	DATA_WIDTH_OUT : integer := 8
	);
     Port(clk : in STD_LOGIC; --data in/out clock LP clock, ~100 MHZ
     rst : in  STD_LOGIC;
     start_transmission : in STD_LOGIC; --start of transmit trigger - performs the required LP dance
     stop_transmission  : in STD_LOGIC; --end of transmit trigger, enters into LP CTRL_Stop mode
     ready_to_transmit : out STD_LOGIC; --goes high once ready for transmission  of HS data
     hs_mode_flag : out STD_LOGIC; --signaling to enter/exit the HS mode. 1- enter, 0- exit. Good for flag of turning on the clock or
                                   -- as a trigger for muxer/switcher. This one goes high configurable number of clock cycles before
                                   -- ready_to_transmit goes high.
     lp_out : out STD_LOGIC_VECTOR(1 downto 0) --bit 1 = Dp line, bit 0 = Dn line
     --err_occured : out STD_LOGIC  --active highl 0 = no error, 1 - error acured
     );
end one_lane_D_PHY;

architecture Behavioral of one_lane_D_PHY is

constant LP_00 : std_logic_vector(1 downto 0) := "00"; --Control mode: Bridge
constant LP_01 : std_logic_vector(1 downto 0) := "01"; --Control mode: HS-Rqst
constant LP_10 : std_logic_vector(1 downto 0) := "10"; --Control mode: LP-Rqst
constant LP_11 : std_logic_vector(1 downto 0) := "11"; --Control mode: Stop
constant COUNTER_WIDTH : integer := 8; --max 256*10ns(100Mhz clock) = 2560ns delay
constant tLPX : integer := 5; --5 clock cycles of 100 Mhz = 50 ns
constant tHSprepare : integer := 6; --6 clock cycles of 100 Mhz = 60 ns
constant tHSzero : integer := 8; --8 clock cycles of 100 Mhz = 80 ns




type state_type is (CTRL_Stop,CTRL_Bridge,CTRL_HS_Rqst,CTRL_LP_Rqst,HS_Go,HS_Burst);
signal state_reg, state_next : state_type := CTRL_Stop;
signal lp_reg,lp_next :  STD_LOGIC_VECTOR(1 downto 0) := LP_11;
signal hs_mode_flag_reg,hs_mode_flag_next : STD_LOGIC := '0'; --default LS mode
signal ready_to_transmit_reg,ready_to_transmit_next : STD_LOGIC := '0'; 
signal reset_conter_reg,reset_conter_next : STD_LOGIC := '0';
signal counter_value :  STD_LOGIC_VECTOR (COUNTER_WIDTH - 1 downto 0) := (others => '0');
--components
component counter generic(n: natural :=COUNTER_WIDTH);
port(clk :	in std_logic;
	rst:	in std_logic;
	counter_out :	out std_logic_vector(COUNTER_WIDTH-1 downto 0)
);
end component;

begin

--instantinte components
delay_counter: counter 
    generic map(n => COUNTER_WIDTH)
    port map(clk => clk,
             rst => reset_conter_reg,
             counter_out => counter_value);           

--end of components instantinations

lp_out <= lp_reg;
hs_mode_flag <= hs_mode_flag_reg; -- to serializer
ready_to_transmit <= ready_to_transmit_reg;
--FSMD state & data registers
FSMD_state : process(clk,rst)
begin
		if (rst = '1') then 
			state_reg <= CTRL_Stop;
			lp_reg <= LP_11;
			hs_mode_flag_reg <= '0';
			ready_to_transmit_reg <= '0';
			reset_conter_reg <= '1';
		elsif (clk'event and clk = '1') then 		
			state_reg <= state_next;
			lp_reg <= lp_next;
			hs_mode_flag_reg <= hs_mode_flag_next;
			ready_to_transmit_reg <= ready_to_transmit_next;
			reset_conter_reg <= reset_conter_next;
		end if;
						
end process; --FSMD_state


--video output state machine, section 5.4, page 35
D_PHY_FSMD : process(state_reg,lp_reg,hs_mode_flag_reg,
                     start_transmission,stop_transmission,ready_to_transmit_reg,
                     counter_value)
begin

    state_next <= state_reg;
    lp_next <= lp_reg ;
    hs_mode_flag_next <= hs_mode_flag_reg;
    ready_to_transmit_next <= ready_to_transmit_reg;
    reset_conter_next <= '0'; --no counter reset by default
    
    case state_reg is 
            when CTRL_Stop =>
                 ready_to_transmit_next <= '0';
                 hs_mode_flag_next <= '0';                 
                 if (start_transmission = '1') then
                 state_next <= CTRL_HS_Rqst;
                 lp_next <= LP_01;                    
                 reset_conter_next  <= '1';      
                 end if;
            when CTRL_HS_Rqst =>
                 --wait tLPX = minimum 50 ns.
                 if (to_integer(unsigned(counter_value)) = tLPX) then
                    state_next <= CTRL_Bridge;   
                    lp_next <= LP_00;              
                    hs_mode_flag_next <= '0';
                    reset_conter_next  <= '1';
                 end if;
            when CTRL_Bridge =>  
                  --wait tHSprepare = min 40 ns + 4*UI,max 85 ns + 6*UI, UI (1 HS bit time) < 10 ns
                 if (to_integer(unsigned(counter_value)) = tHSprepare) then
                    hs_mode_flag_next <= '1';                   
                    state_next <= HS_Go;
                    reset_conter_next  <= '1';
                 end if;
            when HS_Go =>     
                  --wait tHSzero ~80 ns => (THS-PREPARE + THS-ZERO = min 145 ns + 10*UI)
                  if (to_integer(unsigned(counter_value)) = tHSzero) then
                     state_next <= HS_Burst;
                     reset_conter_next  <= '1';
                     ready_to_transmit_next <= '1'; --let to sender some time to prepare (2 clock cycles)
                  end if;
            when HS_Burst =>   
                 
                 --while here, high speed transmission occurs
                 
                 if ( stop_transmission = '1') then
                    state_next <= CTRL_Stop;
                    ready_to_transmit_next <= '0';
                    hs_mode_flag_next <= '0';
                    lp_next <= LP_11;  
                  end if;
            when CTRL_LP_Rqst =>                 
                 state_next <= CTRL_Stop;       
                 lp_next <= LP_11;             
                 hs_mode_flag_next <= '0';
    end case; --state_reg

end process; --D_PHY_FSMD


end Behavioral;
