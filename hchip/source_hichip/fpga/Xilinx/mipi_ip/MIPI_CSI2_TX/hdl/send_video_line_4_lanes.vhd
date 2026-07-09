library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

use work.Common.all;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

-- NOTE: ASSUMES DATA LENGTH IS MULTIPLY OF 4 !!!
--Sends entire video line to the serializer
-- *Assigns entire virtual channel to entire line
-- *Generates packet header
---*generates ECC for packet footer
---buffers first 4 bytes of data
-- NOTE: ASSUMES DATA LENGTH IS MULTIPLY OF 4 !!!
entity send_video_line_4_lanes is
Port(clk : in std_logic; --data in/out clock HS clock, ~100 MHZ
     rst : in  std_logic;
	 word_cound : in std_logic_vector(15 downto 0); --length of the valid payload, bytes, --assumes data length is a multiply of 4
	 vc_num : in std_logic_vector(1 downto 0); --virtual channel number  
	 data_type : in packet_type_t; --data type - YUV,RGB,RAW etc
	 video_data_in : in std_logic_vector(31 downto 0); --4 bytes of video payload
	 start_transmission : in std_logic; --trigger to start transmission,one clock cycle enough- word_cound,vc_num,video_data_in should be valid.
	 csi_lane0 : out  std_logic_vector(7 downto 0); --one byte of CSI stream that goes to serializer
	 csi_lane1 : out  std_logic_vector(7 downto 0); --one byte of CSI stream that goes to serializer	 
	 csi_lane2 : out  std_logic_vector(7 downto 0); --one byte of CSI stream that goes to serializer
	 csi_lane3 : out  std_logic_vector(7 downto 0); --one byte of CSI stream that goes to serializer	 	 
	 transmission_finished : out std_logic; --rised once the header, payload and footer data is sent,data still valid
	 data_out_valid : out std_logic; --goes high when csi_data_out is valid	 	 
	 ready_for_data_in_next_cycle : out std_logic --goest high one clock cycle before ready to get data
	 );
end send_video_line_4_lanes;

architecture Behavioral of send_video_line_4_lanes is


type state_type is (idle,get_packet_header,transmission_loop,send_crc);
signal state_reg, state_next : state_type := idle;
signal data_out_valid_reg,data_out_valid_next : STD_LOGIC := '0';
signal is_initilized_reg,is_initilized_next  : STD_LOGIC := '0';
signal tr_finished_reg,tr_finished_next : STD_LOGIC := '0';
signal data_in_reg,data_in_next : std_logic_vector(31 downto 0) := (others  => '0'); --4 byte also on input
signal data_out_reg,data_out_next : std_logic_vector(31 downto 0) := (others  => '0');
signal long_packet_header : std_logic_vector(31 downto 0) := (others  => '0'); --long packet header
signal word_count_reg,word_count_next : std_logic_vector(13 downto 0) := (others  => '0'); --data counters (13 bit due to assumption that data is multiply of 4)
signal crc_reg,crc_next:  std_logic_vector(15 downto 0) := x"FFFF";
signal crc_byte0,crc_byte1,crc_byte2 :  std_logic_vector(15 downto 0);

begin

--FSMD state & data registers
FSMD_state : process(clk,rst)
begin
		if (rst = '1') then 
		    state_reg <= idle;
		    data_out_valid_reg <= '0';
		    data_in_reg <= (others => '0');
		    data_out_reg <= (others => '0');
		    word_count_reg <= (others => '0');
		    crc_reg  <= x"FFFF";
		    tr_finished_reg <= '0';
		    is_initilized_reg <= '0';
		    
		elsif (clk'event and clk = '1') then 		
		    data_out_valid_reg <= data_out_valid_next;
		    data_in_reg <= data_in_next;
		    state_reg <= state_next;  		
		    data_out_reg <= data_out_next;
		    word_count_reg <= word_count_next;
		    crc_reg <= crc_next;
		    tr_finished_reg <= tr_finished_next;
		    is_initilized_reg <= is_initilized_next;
			
		end if;
						
end process; --FSMD_state


data_out_valid <= data_out_valid_reg;
csi_lane0 <= data_out_reg(31 downto 24);
csi_lane1 <= data_out_reg(23 downto 16);	 
csi_lane2 <= data_out_reg(15 downto 8);
csi_lane3 <= data_out_reg(7 downto 0);	

long_packet_header <= get_short_packet(vc_num,data_type,word_cound);
transmission_finished <= tr_finished_reg;

--line output state machine
LINE_OUT_FSMD : process(state_reg,data_in_reg,data_out_valid_reg,start_transmission,video_data_in,
								data_out_reg,long_packet_header,word_count_reg,crc_reg,word_cound,
								tr_finished_reg,is_initilized_reg,crc_byte0,crc_byte1,crc_byte2)
begin

	state_next   <= state_reg;
	data_in_next <= data_in_reg;
	data_out_valid_next <= data_out_valid_reg;
	data_out_next <= 	data_out_reg;
	word_count_next <=  word_count_reg;
    tr_finished_next <= 	tr_finished_reg;
	crc_next <= crc_reg;
	ready_for_data_in_next_cycle <= '0'; --default
    is_initilized_next <= is_initilized_reg;
    
    crc_byte0 <= (others => '0');
	crc_byte1 <= (others => '0');
    crc_byte2 <= (others => '0');
			    
     --idle,get_first_byte,get_second_byte,get_third_byte,get_forth_byte,transmission_loop,first_byte_of_crc,second_byte_of_crc
    case state_reg is 
	
		when idle =>
		   data_out_valid_next <= '0'; --no valid by default
		   tr_finished_next <= '0';
		   word_count_next <= (others => '0');
		   is_initilized_next <= '0';
		   crc_next  <= x"FFFF";
		   		   
			if (start_transmission = '1') then
			
				data_out_valid_next <= '1';
				data_out_next(31 downto 24) <= Sync_Sequence; --Syncronization byte of packet header,  start of transmission
                data_out_next(23 downto 16) <= Sync_Sequence; --Syncronization byte of packet header,  start of transmission				
                data_out_next(15 downto 8) <= Sync_Sequence; --Syncronization byte of packet header,  start of transmission
                data_out_next(7  downto 0) <= Sync_Sequence; --Syncronization byte of packet header,  start of transmission
				
				state_next <= get_packet_header;
			end if;
			
		when get_packet_header =>
			data_out_next <= long_packet_header; --packet header
            ready_for_data_in_next_cycle  <= '1';	
			state_next <= transmission_loop;
				              		
		when transmission_loop =>
							                     
			data_out_next <= video_data_in;

			
			--assumes data length is a multiply of 4
			crc_byte0 <= nextCRC16_D8(video_data_in(31 downto 24),crc_reg);
			crc_byte1 <= nextCRC16_D8(video_data_in(23 downto 16),crc_byte0);
			crc_byte2 <= nextCRC16_D8(video_data_in(15 downto 8),crc_byte1);
			crc_next <=  nextCRC16_D8(video_data_in(7  downto 0),crc_byte2);
			
			word_count_next <= std_logic_vector( unsigned(word_count_reg) + 1 ); --reduces 100 MHz speed on Artix 7

			
			if (word_count_reg = std_logic_vector( unsigned(word_cound(15 downto 2)) -1 )) then --finish of transmission
			   --crc_next <= crc_reg; --no more CRC calc. needed
				state_next <= send_crc;   
			end if;					
		
		when	send_crc =>

			   data_out_next(31 downto 24) <= crc_reg(7 downto 0);  --send out first byte of CRC (LSB)			
			   data_out_next(23 downto 16) <= crc_reg(15  downto 8);  --send out second byte of CRC (MSB)
			   data_out_next(15 downto 0)  <= (others => '0');
			   
			   data_out_valid_next <= '1'; 
			   tr_finished_next <= '1';
			   crc_next  <= x"FFFF";
				state_next <= idle;   
		
               
    end case; --state_reg

end process; --LINE_OUT_FSMD



end Behavioral;
