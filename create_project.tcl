ren SSD_Test.srcs temp.srcs
create_project -part xczu4ev-sfvc784-1-e SSD_Test
ren temp.srcs SSD_Test.srcs
set_property BOARD_PART trenz.biz:te0803_4ev_1e:part0:1.0 [current_project]
add_files -fileset sources_1 SSD_Test.srcs/sources_1/bd/SSD_Test/SSD_Test.bd
add_files -fileset constrs_1 SSD_Test.srcs/constrs_1/SSD_Test_TE0803.xdc
make_wrapper -top -import [get_files SSD_Test.srcs/sources_1/bd/SSD_Test/SSD_Test.bd]