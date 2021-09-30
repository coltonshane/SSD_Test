# Constraints for SSD Test w/ TE0803 SoM, using GTH Quad 224

# Quad 224 GT Clock Input from clock generator (100MHz).
set_property PACKAGE_PIN V6 [get_ports gt_clk_in_clk_p]
set_property PACKAGE_PIN V5 [get_ports gt_clk_in_clk_n]
create_clock -name gt_clk_in -period 10.000 [get_ports gt_clk_in_clk_p]

# SSD PCI Express reset B25_L4_N (perst)
set_property PACKAGE_PIN H12 [get_ports pcie_perstn]
set_property IOSTANDARD LVCMOS33 [get_ports pcie_perstn]

# UART0 to terminal (3.3V).
set_property PACKAGE_PIN J10 [get_ports emio_uart0_txd]
set_property IOSTANDARD LVCMOS33 [get_ports emio_uart0_txd]
set_property PACKAGE_PIN F10 [get_ports emio_uart0_rxd]
set_property IOSTANDARD LVCMOS33 [get_ports emio_uart0_rxd]
