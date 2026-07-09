--------------------------------------------------------------------------------
-- Company: 
-- Engineer:
--
-- Create Date:   19:18:33 10/24/2017
-- Design Name:   
-- Module Name:   /home/smishash/ProgSources/MIPI_CSI2_TX/simulation/test_full_RX.vhd
-- Project Name:  testCamera
-- Target Device:  
-- Tool versions:  
-- Description:   
-- 
-- VHDL Test Bench Created by ISE for module: ov13850_demo
-- 
-- Dependencies:
-- 
-- Revision:
-- Revision 0.01 - File Created
-- Additional Comments:
--
-- Notes: 
-- This testbench has been automatically generated using types std_logic and
-- std_logic_vector for the ports of the unit under test.  Xilinx recommends
-- that these types always be used for the top-level I/O of a design in order
-- to guarantee that the testbench will bind correctly to the post-implementation 
-- simulation model.
--------------------------------------------------------------------------------
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
 
-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--USE ieee.numeric_std.ALL;
 
ENTITY test_full_RX IS
END test_full_RX;
 
ARCHITECTURE behavior OF test_full_RX IS 
 
    -- Component Declaration for the Unit Under Test (UUT)
 
    COMPONENT ov13850_demo
    PORT(
         clock_p : IN  std_logic;
         clock_n : IN  std_logic;
         reset_n : IN  std_logic;
         hdmi_clk : OUT  std_logic_vector(1 downto 0);
         hdmi_d0 : OUT  std_logic_vector(1 downto 0);
         hdmi_d1 : OUT  std_logic_vector(1 downto 0);
         hdmi_d2 : OUT  std_logic_vector(1 downto 0);
         vga_hsync : OUT  std_logic;
         vga_vsync : OUT  std_logic;
         vga_r : OUT  std_logic_vector(4 downto 0);
         vga_g : OUT  std_logic_vector(5 downto 0);
         vga_b : OUT  std_logic_vector(4 downto 0);
         zoom_mode : IN  std_logic;
         freeze : IN  std_logic;
         csi0_clk : IN  std_logic_vector(1 downto 0);
         csi0_d0 : IN  std_logic_vector(1 downto 0);
         csi0_d1 : IN  std_logic_vector(1 downto 0);
         csi0_d2 : IN  std_logic_vector(1 downto 0);
         csi0_d3 : IN  std_logic_vector(1 downto 0);
         cam_mclk : OUT  std_logic;
         cam_rstn : OUT  std_logic;
         cam_i2c_sda : INOUT  std_logic;
         cam_i2c_sck : INOUT  std_logic;
         ddr3_addr : OUT  std_logic_vector(14 downto 0);
         ddr3_ba : OUT  std_logic_vector(2 downto 0);
         ddr3_cas_n : OUT  std_logic;
         ddr3_ck_n : OUT  std_logic_vector(0 downto 0);
         ddr3_ck_p : OUT  std_logic_vector(0 downto 0);
         ddr3_cke : OUT  std_logic_vector(0 downto 0);
         ddr3_ras_n : OUT  std_logic;
         ddr3_reset_n : OUT  std_logic;
         ddr3_we_n : OUT  std_logic;
         ddr3_dq : INOUT  std_logic_vector(31 downto 0);
         ddr3_dqs_n : INOUT  std_logic_vector(3 downto 0);
         ddr3_dqs_p : INOUT  std_logic_vector(3 downto 0);
         ddr3_cs_n : OUT  std_logic_vector(0 downto 0);
         ddr3_dm : OUT  std_logic_vector(3 downto 0);
         ddr3_odt : OUT  std_logic_vector(0 downto 0)
        );
    END COMPONENT;
    

   --Inputs
   signal clock_p : std_logic := '0';
   signal clock_n : std_logic := '0';
   signal reset_n : std_logic := '0';
   signal zoom_mode : std_logic := '0';
   signal freeze : std_logic := '0';
   signal csi0_clk : std_logic_vector(1 downto 0) := (others => '0');
   signal csi0_d0 : std_logic_vector(1 downto 0) := (others => '0');
   signal csi0_d1 : std_logic_vector(1 downto 0) := (others => '0');
   signal csi0_d2 : std_logic_vector(1 downto 0) := (others => '0');
   signal csi0_d3 : std_logic_vector(1 downto 0) := (others => '0');

	--BiDirs
   signal cam_i2c_sda : std_logic;
   signal cam_i2c_sck : std_logic;
   signal ddr3_dq : std_logic_vector(31 downto 0);
   signal ddr3_dqs_n : std_logic_vector(3 downto 0);
   signal ddr3_dqs_p : std_logic_vector(3 downto 0);

 	--Outputs
   signal hdmi_clk : std_logic_vector(1 downto 0);
   signal hdmi_d0 : std_logic_vector(1 downto 0);
   signal hdmi_d1 : std_logic_vector(1 downto 0);
   signal hdmi_d2 : std_logic_vector(1 downto 0);
   signal vga_hsync : std_logic;
   signal vga_vsync : std_logic;
   signal vga_r : std_logic_vector(4 downto 0);
   signal vga_g : std_logic_vector(5 downto 0);
   signal vga_b : std_logic_vector(4 downto 0);
   signal cam_mclk : std_logic;
   signal cam_rstn : std_logic;
   signal ddr3_addr : std_logic_vector(14 downto 0);
   signal ddr3_ba : std_logic_vector(2 downto 0);
   signal ddr3_cas_n : std_logic;
   signal ddr3_ck_n : std_logic_vector(0 downto 0);
   signal ddr3_ck_p : std_logic_vector(0 downto 0);
   signal ddr3_cke : std_logic_vector(0 downto 0);
   signal ddr3_ras_n : std_logic;
   signal ddr3_reset_n : std_logic;
   signal ddr3_we_n : std_logic;
   signal ddr3_cs_n : std_logic_vector(0 downto 0);
   signal ddr3_dm : std_logic_vector(3 downto 0);
   signal ddr3_odt : std_logic_vector(0 downto 0);

   -- Clock period definitions
   constant clock_p_period : time := 5 ns; --200 MHz
   constant clock_n_period : time := 5 ns; --200 MHz
   constant hdmi_clk_period : time := 6.75 ns; --148 Mhz = 6.75 ns
   constant csi0_clk_period : time := 1.35 ns; --740 Mhz = 1.35 ns
   constant cam_mclk_period : time := 41 ns; -- 24.399__MHz = 0.000 000 041 sec
 
BEGIN
 
	-- Instantiate the Unit Under Test (UUT)
   uut: ov13850_demo PORT MAP (
          clock_p => clock_p,
          clock_n => clock_n,
          reset_n => reset_n,
          hdmi_clk => hdmi_clk,
          hdmi_d0 => hdmi_d0,
          hdmi_d1 => hdmi_d1,
          hdmi_d2 => hdmi_d2,
          vga_hsync => vga_hsync,
          vga_vsync => vga_vsync,
          vga_r => vga_r,
          vga_g => vga_g,
          vga_b => vga_b,
          zoom_mode => zoom_mode,
          freeze => freeze,
          csi0_clk => csi0_clk,
          csi0_d0 => csi0_d0,
          csi0_d1 => csi0_d1,
          csi0_d2 => csi0_d2,
          csi0_d3 => csi0_d3,
          cam_mclk => cam_mclk,
          cam_rstn => cam_rstn,
          cam_i2c_sda => cam_i2c_sda,
          cam_i2c_sck => cam_i2c_sck,
          ddr3_addr => ddr3_addr,
          ddr3_ba => ddr3_ba,
          ddr3_cas_n => ddr3_cas_n,
          ddr3_ck_n => ddr3_ck_n,
          ddr3_ck_p => ddr3_ck_p,
          ddr3_cke => ddr3_cke,
          ddr3_ras_n => ddr3_ras_n,
          ddr3_reset_n => ddr3_reset_n,
          ddr3_we_n => ddr3_we_n,
          ddr3_dq => ddr3_dq,
          ddr3_dqs_n => ddr3_dqs_n,
          ddr3_dqs_p => ddr3_dqs_p,
          ddr3_cs_n => ddr3_cs_n,
          ddr3_dm => ddr3_dm,
          ddr3_odt => ddr3_odt
        );

   -- Clock process definitions
   clock_p_process :process
   begin
		clock_p <= '0';
		wait for clock_p_period/2;
		clock_p <= '1';
		wait for clock_p_period/2;
   end process;
 
 clock_n <= not clock_p;
 
--   hdmi_clk_process :process
--   begin
--		hdmi_clk <= '0';
--		wait for hdmi_clk_period/2;
--		hdmi_clk <= '1';
--		wait for hdmi_clk_period/2;
--   end process;
 
   csi0_clk_process :process
   begin
		csi0_clk(0) <= '0';
		wait for csi0_clk_period/2;
		csi0_clk(0) <= '1';
		wait for csi0_clk_period/2;
   end process;
	
csi0_clk(1) <= not csi0_clk(0);
 
   cam_mclk_process :process
   begin
		--cam_mclk <= '0';
		wait for cam_mclk_period/2;
		--cam_mclk <= '1';
		wait for cam_mclk_period/2;
   end process;
 

   -- Stimulus process
   stim_proc: process
   begin		
      -- hold reset state for 100 ns.
		reset_n <= '0';
      wait for 100 ns;	
		reset_n <= '1';

      wait for clock_p_period*10;
		
		--enable <= '1';

      -- insert stimulus here 

      wait;
   end process;

END;
