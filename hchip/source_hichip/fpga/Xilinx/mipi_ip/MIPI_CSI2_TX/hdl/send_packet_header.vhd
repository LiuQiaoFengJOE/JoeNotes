--sends MIPI CSI packet header including the sync. sequence. (can be used also to sent entire short packets)

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
--use IEEE.STD_LOGIC_ARITH.ALL;
--use IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.NUMERIC_STD.ALL;

use work.Common.all;

entity send_packet_header is
Port(clk : in std_logic; --data in/out clock HS clock, ~100 MHZ     
     rst : in  std_logic;
     packet_to_send : in std_logic_vector(31 downto 0); --packet to send
     start_sending_packet : in std_logic; --
     packet_done_next_cyle :  out std_logic; --indicates that packet was sent successully (one clock cycle before)
     data_out : out  std_logic_vector(7 downto 0); --one byte of CSI stream that goes to serializer
     data_valid : out std_logic --1 when hs_data_out is valid
	 );
end send_packet_header;

architecture Behavioral of send_packet_header is

type state_type is (idle,first_byte,second_byte,third_byte,forth_byte);
signal state_reg, state_next : state_type := idle;

signal packet_to_send_reg,packet_to_send_next : std_logic_vector(31 downto 0) := (others => '0'); --packet to send

begin

--FSMD state & data registers
FSMD_state : process(clk,rst)
begin
		if (rst = '1') then 
			state_reg <= idle;			
			packet_to_send_reg <= (others => '0');
		elsif (clk'event and clk = '1') then 		
			state_reg <= state_next;  					
			packet_to_send_reg <= packet_to_send_next;
		end if;
						
end process; --FSMD_state

      

HEADER_FSMD : process(state_reg,start_sending_packet,packet_to_send,packet_to_send_reg)
begin

	state_next <= state_reg;
	data_out <= (others => '0');
	data_valid <= '0';
   packet_done_next_cyle <= '0';
   packet_to_send_next <= packet_to_send_reg;
   
    case state_reg is 
            when idle =>                 

               if (start_sending_packet = '1') then
               
                 	data_out <= Sync_Sequence;
						data_valid <= '1';                  
            	   state_next <= first_byte;
            	   packet_to_send_next <= packet_to_send;
          	   end if;
                             
            when first_byte =>
            
               data_out <=  packet_to_send_reg(7 downto 0);
               data_valid <= '1';  
               state_next <=  second_byte;          
            
            when second_byte =>
               
               data_out <=  packet_to_send_reg(15 downto 8);
               data_valid <= '1';  
               state_next <= third_byte;            
            
            when third_byte =>
            
               data_out <=  packet_to_send_reg(23 downto 16);
               data_valid <= '1';  
               state_next <= forth_byte;            
            
            when forth_byte =>
               
               data_out <=  packet_to_send_reg(31 downto 24);
               data_valid <= '1';
               packet_done_next_cyle <= '1';  
               state_next <=  idle;
                          	      
                 
            
    end case; --state_reg

end process; --HEADER_FSMD

end Behavioral;


