//Copyright 1986-2021 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2021.1 (win64) Build 3247384 Thu Jun 10 19:36:33 MDT 2021
//Date        : Wed Sep 29 20:26:44 2021
//Host        : DESKTOP-CFPRRMI running 64-bit major release  (build 9200)
//Command     : generate_target SSD_Test_wrapper.bd
//Design      : SSD_Test_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module SSD_Test_wrapper
   (emio_uart0_rxd,
    emio_uart0_txd,
    gt_clk_in_clk_n,
    gt_clk_in_clk_p,
    pcie_mgt_rxn,
    pcie_mgt_rxp,
    pcie_mgt_txn,
    pcie_mgt_txp,
    pcie_perstn);
  input emio_uart0_rxd;
  output emio_uart0_txd;
  input [0:0]gt_clk_in_clk_n;
  input [0:0]gt_clk_in_clk_p;
  input [3:0]pcie_mgt_rxn;
  input [3:0]pcie_mgt_rxp;
  output [3:0]pcie_mgt_txn;
  output [3:0]pcie_mgt_txp;
  output pcie_perstn;

  wire emio_uart0_rxd;
  wire emio_uart0_txd;
  wire [0:0]gt_clk_in_clk_n;
  wire [0:0]gt_clk_in_clk_p;
  wire [3:0]pcie_mgt_rxn;
  wire [3:0]pcie_mgt_rxp;
  wire [3:0]pcie_mgt_txn;
  wire [3:0]pcie_mgt_txp;
  wire pcie_perstn;

  SSD_Test SSD_Test_i
       (.emio_uart0_rxd(emio_uart0_rxd),
        .emio_uart0_txd(emio_uart0_txd),
        .gt_clk_in_clk_n(gt_clk_in_clk_n),
        .gt_clk_in_clk_p(gt_clk_in_clk_p),
        .pcie_mgt_rxn(pcie_mgt_rxn),
        .pcie_mgt_rxp(pcie_mgt_rxp),
        .pcie_mgt_txn(pcie_mgt_txn),
        .pcie_mgt_txp(pcie_mgt_txp),
        .pcie_perstn(pcie_perstn));
endmodule
