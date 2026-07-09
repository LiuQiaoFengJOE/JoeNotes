----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 09/02/2017 10:16:15 PM
-- Design Name: 
-- Module Name: common - Behavioral
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
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

package Common is   

   type my_enum_type is (r1, r2, r3);
   type shortp_type is (frame_start,frame_end,line_start,line_end); --short packet type

     -- polynomial: x^16 + x^12 + x^5 + 1
 -- data width: 8
 -- convention: the first serial bit is D[7]
 function nextCRC16_D8
   (Data: std_logic_vector(0 to 7);
    crc:  std_logic_vector(0 to 15))
   return std_logic_vector;
  
end Common;

package body Common is
   
   
  function nextCRC16_D8
  (Data: std_logic_vector(0 to 7);
   crc:  std_logic_vector(0 to 15))
  return std_logic_vector is

  variable d:      std_logic_vector(0 to 7);
  variable c:      std_logic_vector(0 to 15);
  variable newcrc: std_logic_vector(0 to 15);

begin
  d := Data;
  c := crc;

  newcrc(0) := d(4) xor d(0) xor c(8) xor c(12);
  newcrc(1) := d(5) xor d(1) xor c(9) xor c(13);
  newcrc(2) := d(6) xor d(2) xor c(10) xor c(14);
  newcrc(3) := d(7) xor d(3) xor c(11) xor c(15);
  newcrc(4) := d(4) xor c(12);
  newcrc(5) := d(5) xor d(4) xor d(0) xor c(8) xor c(12) xor c(13);
  newcrc(6) := d(6) xor d(5) xor d(1) xor c(9) xor c(13) xor c(14);
  newcrc(7) := d(7) xor d(6) xor d(2) xor c(10) xor c(14) xor c(15);
  newcrc(8) := d(7) xor d(3) xor c(0) xor c(11) xor c(15);
  newcrc(9) := d(4) xor c(1) xor c(12);
  newcrc(10) := d(5) xor c(2) xor c(13);
  newcrc(11) := d(6) xor c(3) xor c(14);
  newcrc(12) := d(7) xor d(4) xor d(0) xor c(4) xor c(8) xor c(12) xor c(15);
  newcrc(13) := d(5) xor d(1) xor c(5) xor c(9) xor c(13);
  newcrc(14) := d(6) xor d(2) xor c(6) xor c(10) xor c(14);
  newcrc(15) := d(7) xor d(3) xor c(7) xor c(11) xor c(15);
  return newcrc;
end nextCRC16_D8;
   
end Common;
