-------------------------------------------------------------------------------
-- Copyright (c) 2013 Xilinx, Inc.
-- All Rights Reserved
-------------------------------------------------------------------------------
--   ____  ____
--  /   /\/   /
-- /___/  \  /    Vendor     : Xilinx
-- \   \   \/     Version    : 14.4
--  \   \         Application: XILINX CORE Generator
--  /   /         Filename   : ila_icon.vhd
-- /___/   /\     Timestamp  : Mon Jul 08 15:03:47 CEST 2013
-- \   \  /  \
--  \___\/\___\
--
-- Design Name: VHDL Synthesis Wrapper
-------------------------------------------------------------------------------
-- This wrapper is used to integrate with Project Navigator and PlanAhead

LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
ENTITY ila_icon IS
  port (
    CONTROL0: inout std_logic_vector(35 downto 0));
END ila_icon;

ARCHITECTURE ila_icon_a OF ila_icon IS
BEGIN

END ila_icon_a;
