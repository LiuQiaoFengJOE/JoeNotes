library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
--use IEEE.STD_LOGIC_ARITH.ALL;
--use IEEE.STD_LOGIC_UNSIGNED.ALL;
use IEEE.NUMERIC_STD.ALL;

use work.Common.all;

--transmits frame in 4 lane
entity transmit_frame_4lanes is
	Port(clk                          : in  std_logic; --data in/out clock HS clock, ~100 MHZ
	     clk_lp                       : in  std_logic; --LP clock, ~100 MHZ. not necessarily the same as clk HS
	     rst                          : in  std_logic;
	     bytes_per_line               : in  std_logic_vector(15 downto 0); --bytes per video line (take into account virtual channels data reduction)
	     lines_per_frame              : in  std_logic_vector(15 downto 0); --lines per video frame (take into account virtual channels data reduction? should I?)
	     vc_num                       : in  std_logic_vector(1 downto 0); --virtual channel number  
	     data_type                    : in  packet_type_t; --data type - YUV,RGB,RAW etc    
	     frame_data_in                : in  std_logic_vector(31 downto 0); --4 byte bytes of video payload   
	     frame_number                 : in  std_logic_vector(15 downto 0);
	     start_frame_transmission     : in  std_logic; --triggers LP dance
	     hs_data_lane0                : out std_logic_vector(7 downto 0); --lane0 one byte of CSI stream that goes to serializer
	     hs_data_lane1                : out std_logic_vector(7 downto 0); --lane1 one byte of CSI stream that goes to serializer
	     hs_data_lane2                : out std_logic_vector(7 downto 0); --lane2 one byte of CSI stream that goes to serializer
	     hs_data_lane3                : out std_logic_vector(7 downto 0); --lane3 one byte of CSI stream that goes to serializer
	     lp_data_out                  : out std_logic_vector(1 downto 0); --bit 1 = Dp line, bit 0 = Dn line
	     hs_data_valid                : out std_logic; --1 when hs_data_out is valid
	     is_hs_mode                   : out std_logic; --0 when  in LP mode, 1 when in HS mode. --goes high once ready to accept the HS data
	     --(goes 1, N clock cicles before the actual
	     --reading, to give a time for producer to prepare)
	     ready_for_data_in_next_cycle : out std_logic; --goest high one clock cycle before ready 
	     --to get data                                                                        
	     line_sending_finished        : out std_logic; --goes high when finished one line transmission, 
	     -- hs_data_out is still valid (last byte)                                                 
	     frame_sending_finished       : out std_logic --goes high once frame sending is complete                                          
	    );
end transmit_frame_4lanes;

architecture Behavioral of transmit_frame_4lanes is

	-- components

	COMPONENT send_video_line_4_lanes
		PORT(
			clk                          : IN  std_logic;
			rst                          : IN  std_logic;
			word_cound                   : IN  std_logic_vector(15 downto 0);
			vc_num                       : IN  std_logic_vector(1 downto 0);
			data_type                    : IN  packet_type_t; --data type - YUV,RGB,RAW etc;
			video_data_in                : IN  std_logic_vector(31 downto 0);
			start_transmission           : IN  std_logic;
			csi_lane0                    : OUT std_logic_vector(7 downto 0);
			csi_lane1                    : OUT std_logic_vector(7 downto 0);
			csi_lane2                    : OUT std_logic_vector(7 downto 0);
			csi_lane3                    : OUT std_logic_vector(7 downto 0);
			transmission_finished        : OUT std_logic;
			data_out_valid               : OUT std_logic;
			ready_for_data_in_next_cycle : OUT std_logic
		);
	END COMPONENT;

	COMPONENT one_lane_D_PHY is
		generic(
			DATA_WIDTH_IN  : integer := 8;
			DATA_WIDTH_OUT : integer := 8
		);
		Port(clk                : in  STD_LOGIC; --LP data in/out clock     
		     rst                : in  STD_LOGIC;
		     start_transmission : in  STD_LOGIC; --start of transmit trigger - performs the required LP dance
		     stop_transmission  : in  STD_LOGIC; --end of transmit trigger, enters into LP CTRL_Stop mode
		     ready_to_transmit  : out STD_LOGIC; --goes high once ready for transmission of HS data
		     hs_mode_flag       : out STD_LOGIC; --signaling to enter/exit the HS mode. 1- enter, 0- exit. Good for flag of turning on the clock or
		     -- as a trigger for muxer/switcher. This one goes high configurable number of clock cycles before
		     -- ready_to_transmit goes high.
		     lp_out             : out STD_LOGIC_VECTOR(1 downto 0) --bit 1 = Dp line, bit 0 = Dn line
		     --err_occured : out STD_LOGIC  --active highl 0 = no error, 1 - error acured
		    );
	END COMPONENT;

	constant COUNTER_WIDTH : integer := 8; --max 256*10ns(100Mhz clock) = 2560ns delay

	component counter
		generic(n : natural := COUNTER_WIDTH);
		port(clk         : in  std_logic;
		     rst         : in  std_logic;
		     counter_out : out std_logic_vector(COUNTER_WIDTH - 1 downto 0)
		    );
	end component;

	--end of components

	constant tLowPower             : integer                       := 20; --20 clock cycles of 100 Mhz = 200 ns
	constant tClockStabilize       : integer                       := 24; --8 clock cycles of 100 Mhz = 240 ns
	constant tEndOfHS_TX           : integer                       := 20; --8 clock cycles of 100 Mhz = 240 ns
	constant EnableLineCountingOut : std_logic                     := '1'; --use zero=0 to disable line counting
	constant DefaultLineNumber     : std_logic_vector(15 downto 0) := x"0001";

	type state_type is (idle, lp_mode_frame_start, hs_mode_frame_start_short_packet, hs_mode_frame_start_short_packet_end,
	                    delay_between_FS_and_LS, lp_mode_line_start, hs_mode_line_start_short_packet,
	                    hs_mode_line_start_short_packet_end, delay_between_LS_and_line_payload,
	                    lp_mode_transmit_line, hs_mode_transmit_line,
	                    lp_mode_line_stop, hs_mode_line_stop_short_packet, hs_mode_line_stop_short_packet_end,
	                    delay_between_LineStop_and_LineStart_loopback,
	                    lp_mode_frame_end, hs_mode_frame_end_short_packet,
	                    hs_mode_frame_end_short_packet_end, delay_between_frame_end_and_idle
	                   );

	signal state_reg, state_next                              : state_type                                   := idle;
	signal reset_conter_reg, reset_conter_next                : STD_LOGIC                                    := '0';
	signal counter_value                                      : STD_LOGIC_VECTOR(COUNTER_WIDTH - 1 downto 0) := (others => '0');
	signal line_number_reg, line_number_next, line_number_out : std_logic_vector(15 downto 0)                := DefaultLineNumber;

	--related to inst_one_lane_D_PHY
	signal start_dphy_transmission_reg, start_dphy_transmission_next : std_logic                    := '0';
	signal stop_dphy_transmission_reg, stop_dphy_transmission_next   : std_logic                    := '0';
	signal dphy_ready_to_transmit                                    : std_logic                    := '0';
	signal dphy_hs_mode_flag                                         : std_logic                    := '0';
	signal dphy_lp_out                                               : std_logic_vector(1 downto 0) := "11"; -- =LP_11

	--related to send_video_line
	signal vline_start_transmission                                           : std_logic                    := '0';
	signal vline_csi_lane0, vline_csi_lane1, vline_csi_lane2, vline_csi_lane3 : std_logic_vector(7 downto 0) := (others => '0'); --one byte of CSI stream that goes to serializer
	signal vline_transmission_finished                                        : std_logic                    := '0';
	signal vline_data_out_valid                                               : std_logic                    := '0';

	--helpers
	signal data_to_send : std_logic_vector(31 downto 0) := (others => '0');

begin
	--instantinate components
	inst_one_lane_D_PHY : one_lane_D_PHY
		PORT MAP(
			clk                => clk_lp,
			rst                => rst,
			start_transmission => start_dphy_transmission_reg,
			stop_transmission  => stop_dphy_transmission_reg,
			ready_to_transmit  => dphy_ready_to_transmit,
			hs_mode_flag       => dphy_hs_mode_flag,
			lp_out             => dphy_lp_out
		);

	inst_send_video_4lanes : send_video_line_4_lanes
		PORT MAP(
			clk                          => clk,
			rst                          => rst,
			word_cound                   => bytes_per_line,
			vc_num                       => vc_num,
			data_type                    => data_type,
			video_data_in                => frame_data_in,
			start_transmission           => vline_start_transmission,
			csi_lane0                    => vline_csi_lane0,
			csi_lane1                    => vline_csi_lane1,
			csi_lane2                    => vline_csi_lane2,
			csi_lane3                    => vline_csi_lane3,
			transmission_finished        => vline_transmission_finished,
			data_out_valid               => vline_data_out_valid,
			ready_for_data_in_next_cycle => ready_for_data_in_next_cycle
		);

	inst_counter : counter
		generic map(n => COUNTER_WIDTH)
		port map(clk         => clk,
		         rst         => reset_conter_reg,
		         counter_out => counter_value);

	--FSMD state & data registers
	FSMD_state : process(clk, rst)
	begin
		if (rst = '1') then
			state_reg                   <= idle;
			start_dphy_transmission_reg <= '0';
			stop_dphy_transmission_reg  <= '0';
			reset_conter_reg            <= '1';
			line_number_reg             <= DefaultLineNumber;
		elsif (clk'event and clk = '1') then
			state_reg                   <= state_next;
			start_dphy_transmission_reg <= start_dphy_transmission_next;
			stop_dphy_transmission_reg  <= stop_dphy_transmission_next;
			reset_conter_reg            <= reset_conter_next;
			line_number_reg             <= line_number_next;
		end if;

	end process;                        --FSMD_state

	lp_data_out           <= dphy_lp_out;
	is_hs_mode            <= dphy_hs_mode_flag;
	line_sending_finished <= vline_transmission_finished;
	line_number_out       <= line_number_reg when (EnableLineCountingOut = '1') else (others => '0');

	FRAME_FSMD : process(state_reg, start_frame_transmission, dphy_ready_to_transmit, dphy_hs_mode_flag,
		data_to_send, vc_num, frame_number, counter_value, lines_per_frame, vline_data_out_valid, 
		vline_csi_lane0, vline_csi_lane1, vline_csi_lane2, vline_csi_lane3, vline_transmission_finished, 
		line_number_reg, line_number_out
	)
	begin
		state_next                   <= state_reg;
		line_number_next             <= line_number_reg;
		start_dphy_transmission_next <= '0';
		stop_dphy_transmission_next  <= '0';
		hs_data_valid                <= '0';

		data_to_send <= (others => '0');

		--hs_data_out <= (others => '0');
		hs_data_lane0 <= (others => '0');
		hs_data_lane1 <= (others => '0');
		hs_data_lane2 <= (others => '0');
		hs_data_lane3 <= (others => '0');

		reset_conter_next        <= '0'; --no counter reset by default
		vline_start_transmission <= '0';
		frame_sending_finished   <= '0';

		case state_reg is
			when idle =>
				stop_dphy_transmission_next <= '0';

				if (start_frame_transmission = '1') then
					start_dphy_transmission_next <= '1';
					state_next                   <= lp_mode_frame_start;
				end if;

			when lp_mode_frame_start =>

				if (dphy_hs_mode_flag = '1') then
					hs_data_valid <= '0';
					state_next    <= hs_mode_frame_start_short_packet;
				end if;

			when hs_mode_frame_start_short_packet => --frame start short packet starts

				if (dphy_ready_to_transmit = '1') then --HS transmission starts;
					hs_data_valid <= '1';

					hs_data_lane0 <= Sync_Sequence;
					hs_data_lane1 <= Sync_Sequence;
					hs_data_lane2 <= Sync_Sequence;
					hs_data_lane3 <= Sync_Sequence;

					state_next <= hs_mode_frame_start_short_packet_end;
				end if;

			when hs_mode_frame_start_short_packet_end => --frame start short packet ends            	

				hs_data_valid <= '1';

				data_to_send  <= get_short_packet(vc_num, Frame_Start, frame_number);
				hs_data_lane0 <= data_to_send(31 downto 24);
				hs_data_lane1 <= data_to_send(23 downto 16);
				hs_data_lane2 <= data_to_send(15 downto 8);
				hs_data_lane3 <= data_to_send(7 downto 0);

				state_next                  <= delay_between_FS_and_LS; --delay between frame start and line start short packets
				stop_dphy_transmission_next <= '1';
				reset_conter_next           <= '1';

			when delay_between_FS_and_LS => --delay between frame start and line start short packets

				if (to_integer(unsigned(counter_value)) = tLowPower) then
					start_dphy_transmission_next <= '1';
					state_next                   <= lp_mode_line_start;
				end if;

			when lp_mode_line_start =>

				if (dphy_hs_mode_flag = '1') then
					hs_data_valid <= '0';
					state_next    <= hs_mode_line_start_short_packet;
				end if;

			when hs_mode_line_start_short_packet =>
				if (dphy_ready_to_transmit = '1') then --HS transmission starts;
					hs_data_valid <= '1';

					hs_data_lane0 <= Sync_Sequence;
					hs_data_lane1 <= Sync_Sequence;
					hs_data_lane2 <= Sync_Sequence;
					hs_data_lane3 <= Sync_Sequence;

					state_next <= hs_mode_line_start_short_packet_end;
				end if;

			when hs_mode_line_start_short_packet_end =>

				hs_data_valid <= '1';

				data_to_send  <= get_short_packet(vc_num, Line_Start, line_number_out); --should be actual line number or Zero = 0
				hs_data_lane0 <= data_to_send(31 downto 24);
				hs_data_lane1 <= data_to_send(23 downto 16);
				hs_data_lane2 <= data_to_send(15 downto 8);
				hs_data_lane3 <= data_to_send(7 downto 0);

				state_next                  <= delay_between_LS_and_line_payload; --delay_between_LS_and_line_payload
				stop_dphy_transmission_next <= '1';
				reset_conter_next           <= '1';

			when delay_between_LS_and_line_payload =>
				if (to_integer(unsigned(counter_value)) = tLowPower) then
					start_dphy_transmission_next <= '1';
					state_next                   <= lp_mode_transmit_line;
					reset_conter_next            <= '1';
				end if;

			when lp_mode_transmit_line =>
				if (dphy_hs_mode_flag = '1') then
					hs_data_valid <= '0';
					--delay to allow for clock to stabilize
					if (to_integer(unsigned(counter_value)) = tClockStabilize) then
						state_next               <= hs_mode_transmit_line;
						vline_start_transmission <= '1';
					end if;
				end if;

			when hs_mode_transmit_line =>
				hs_data_valid <= vline_data_out_valid;

				hs_data_lane0 <= vline_csi_lane0;
				hs_data_lane1 <= vline_csi_lane1;
				hs_data_lane2 <= vline_csi_lane2;
				hs_data_lane3 <= vline_csi_lane3;

				if (vline_transmission_finished = '1') then
					reset_conter_next           <= '1';
					stop_dphy_transmission_next <= '1';
				end if;

				--switching to LP for end of line short packet, inserting some delay
				if ( (to_integer(unsigned(counter_value)) = tEndOfHS_TX) and (vline_data_out_valid = '0') ) then
					start_dphy_transmission_next <= '1';
					state_next                   <= lp_mode_line_stop;
				end if;

			when lp_mode_line_stop =>

				if (dphy_hs_mode_flag = '1') then
					hs_data_valid <= '0';
					state_next    <= hs_mode_line_stop_short_packet;
				end if;

			when hs_mode_line_stop_short_packet =>

				if (dphy_ready_to_transmit = '1') then --HS transmission starts;

					hs_data_valid <= '1';

					hs_data_lane0 <= Sync_Sequence;
					hs_data_lane1 <= Sync_Sequence;
					hs_data_lane2 <= Sync_Sequence;
					hs_data_lane3 <= Sync_Sequence;

					state_next <= hs_mode_line_stop_short_packet_end;
				end if;

			when hs_mode_line_stop_short_packet_end =>
				hs_data_valid <= '1';

				data_to_send  <= get_short_packet(vc_num, Line_End, line_number_out); --should be actual line number or Zero = 0
				hs_data_lane0 <= data_to_send(31 downto 24);
				hs_data_lane1 <= data_to_send(23 downto 16);
				hs_data_lane2 <= data_to_send(15 downto 8);
				hs_data_lane3 <= data_to_send(7 downto 0);

				state_next                  <= delay_between_LineStop_and_LineStart_loopback; --delay_between_LS_and_line_payload
				stop_dphy_transmission_next <= '1';
				reset_conter_next           <= '1';

			when delay_between_LineStop_and_LineStart_loopback =>
				if (to_integer(unsigned(counter_value)) = tLowPower) then
					start_dphy_transmission_next <= '1';
					reset_conter_next            <= '1';

					if (line_number_reg = lines_per_frame) then --we finished the frame, just send Frame End short packet
						line_number_next <= DefaultLineNumber;
						state_next       <= lp_mode_frame_end;
					else
						state_next       <= lp_mode_line_start; --loop back if currLineNumber < totalLines
						line_number_next <= std_logic_vector(unsigned(line_number_reg) + 1);
					end if;
				end if;

			--end of frame
			when lp_mode_frame_end =>

				if (dphy_hs_mode_flag = '1') then
					hs_data_valid <= '0';
					state_next    <= hs_mode_frame_end_short_packet;
				end if;

			when hs_mode_frame_end_short_packet =>
				if (dphy_ready_to_transmit = '1') then --HS transmission starts;	
					hs_data_valid <= '1';

					hs_data_lane0 <= Sync_Sequence;
					hs_data_lane1 <= Sync_Sequence;
					hs_data_lane2 <= Sync_Sequence;
					hs_data_lane3 <= Sync_Sequence;

					state_next <= hs_mode_frame_end_short_packet_end;
				end if;

			when hs_mode_frame_end_short_packet_end =>

				hs_data_valid <= '1';

				data_to_send  <= get_short_packet(vc_num, Frame_End, frame_number);
				hs_data_lane0 <= data_to_send(31 downto 24);
				hs_data_lane1 <= data_to_send(23 downto 16);
				hs_data_lane2 <= data_to_send(15 downto 8);
				hs_data_lane3 <= data_to_send(7 downto 0);

				state_next                  <= delay_between_frame_end_and_idle; --delay_between_LS_and_line_payload
				stop_dphy_transmission_next <= '1';
				reset_conter_next           <= '1';

			when delay_between_frame_end_and_idle =>
				if (to_integer(unsigned(counter_value)) = tLowPower) then
					--start_dphy_transmission_next <= '1';
					state_next             <= idle;
					reset_conter_next      <= '1';
					frame_sending_finished <= '1';
				end if;

		end case;                       --state_reg

	end process;                        --FRAME_FSMD

end Behavioral;
