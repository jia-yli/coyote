# # pblk_hbm0
# create_pblock pblk_hbm0
# resize_pblock pblk_hbm0 -add SLICE_X0Y0:SLICE_X28Y59
# add_cells_to_pblock pblk_hbm0 [get_cells [list inst_int_hbm/path_0]]
# add_cells_to_pblock pblk_hbm0 [get_cells [list inst_int_hbm/path_1]]
# add_cells_to_pblock pblk_hbm0 [get_cells [list inst_int_hbm/path_2]]
# add_cells_to_pblock pblk_hbm0 [get_cells [list inst_int_hbm/path_3]]

# # pblk_hbm1
# create_pblock pblk_hbm1
# resize_pblock pblk_hbm1 -add SLICE_X29Y0:SLICE_X57Y59
# add_cells_to_pblock pblk_hbm1 [get_cells [list inst_int_hbm/path_4]]
# add_cells_to_pblock pblk_hbm1 [get_cells [list inst_int_hbm/path_5]]
# add_cells_to_pblock pblk_hbm1 [get_cells [list inst_int_hbm/path_6]]
# add_cells_to_pblock pblk_hbm1 [get_cells [list inst_int_hbm/path_7]]

# # pblk_hbm2
# create_pblock pblk_hbm2
# resize_pblock pblk_hbm2 -add SLICE_X58Y0:SLICE_X86Y59
# add_cells_to_pblock pblk_hbm2 [get_cells [list inst_int_hbm/path_8]]
# add_cells_to_pblock pblk_hbm2 [get_cells [list inst_int_hbm/path_9]]
# add_cells_to_pblock pblk_hbm2 [get_cells [list inst_int_hbm/path_10]]
# add_cells_to_pblock pblk_hbm2 [get_cells [list inst_int_hbm/path_11]]

# # pblk_hbm3
# create_pblock pblk_hbm3
# resize_pblock pblk_hbm3 -add SLICE_X87Y0:SLICE_X115Y59
# add_cells_to_pblock pblk_hbm3 [get_cells [list inst_int_hbm/path_12]]
# add_cells_to_pblock pblk_hbm3 [get_cells [list inst_int_hbm/path_13]]
# add_cells_to_pblock pblk_hbm3 [get_cells [list inst_int_hbm/path_14]]
# add_cells_to_pblock pblk_hbm3 [get_cells [list inst_int_hbm/path_15]]

# # pblk_net_module_0
# create_pblock pblk_net_module_0
# resize_pblock pblk_net_module_0 -add CLOCKREGION_X0Y6:CLOCKREGION_X0Y7
# add_cells_to_pblock pblk_net_module_0 [get_cells [list inst_network_top_0/inst_network_module]]

# # pblk_net_early_ccross_0
# create_pblock pblk_net_early_ccross_0
# resize_pblock pblk_net_early_ccross_0 -add CLOCKREGION_X0Y4:CLOCKREGION_X0Y5
# add_cells_to_pblock pblk_net_early_ccross_0 [get_cells [list inst_network_top_0/inst_early_ccross]]

# # pblk_net_stack_0
# create_pblock pblk_net_stack_0
# resize_pblock pblk_net_stack_0 -add CLOCKREGION_X1Y4:CLOCKREGION_X3Y7
# add_cells_to_pblock pblk_net_stack_0 [get_cells [list inst_network_top_0/inst_network_stack]]

# app: dedup
create_pblock pblock_dedupCore
resize_pblock pblock_dedupCore -add CLOCKREGION_X0Y0:CLOCKREGION_X7Y7
add_cells_to_pblock pblock_dedupCore [get_cells [list inst_dynamic/inst_dedup/dedupCore]]

create_pblock pblock_sha3Grp
resize_pblock pblock_sha3Grp -add CLOCKREGION_X0Y2:CLOCKREGION_X7Y7
add_cells_to_pblock pblock_sha3Grp [get_cells [list inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp]]

create_pblock pblock_sha3Grp1
resize_pblock pblock_sha3Grp1 -add CLOCKREGION_X0Y6:CLOCKREGION_X5Y7
add_cells_to_pblock pblock_2 [get_cells [list inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_0 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_1 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_2 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_3 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_4 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_5 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_6 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_7 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_8 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_9 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_10 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_11 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_12 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_13 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_14 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_15]]

create_pblock pblock_sha3Grp2
resize_pblock pblock_sha3Grp2 -add CLOCKREGION_X0Y2:CLOCKREGION_X3Y4
resize_pblock pblock_sha3Grp2 -add CLOCKREGION_X4Y2:CLOCKREGION_X7Y3
add_cells_to_pblock pblock_3 [get_cells [list inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_16 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_17 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_18 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_19 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_20 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_21 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_22 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_23 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_24 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_25 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_26 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_27 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_28 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_29 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_30 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_31]]

create_pblock pblock_sha3Grp3
resize_pblock pblock_sha3Grp3 -add CLOCKREGION_X0Y4:CLOCKREGION_X5Y5
add_cells_to_pblock pblock_5 [get_cells [list inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_32 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_33 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_34 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_35 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_36 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_37 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_38 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_39 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_40 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_41 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_42 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_43 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_44 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_45 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_46 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_47]]

create_pblock pblock_sha3Grp4
resize_pblock pblock_sha3Grp4 -add CLOCKREGION_X4Y4:CLOCKREGION_X7Y7
add_cells_to_pblock pblock_6 [get_cells [list inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_48 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_49 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_50 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_51 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_52 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_53 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_54 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_55 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_56 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_57 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_58 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_59 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_60 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_61 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_62 inst_dynamic/inst_dedup/dedupCore/hashTableSS/sha3Grp/sha3CoreGrp_63]]

create_pblock pblock_1
resize_pblock pblock_1 -add CLOCKREGION_X1Y1:CLOCKREGION_X3Y2
add_cells_to_pblock pblock_1 [get_cells [list inst_dynamic/inst_dedup/dedupCore/hashTableSS/decodedReadyInstrQueue inst_dynamic/inst_dedup/dedupCore/hashTableSS/decodedWaitingInstrQueue inst_dynamic/inst_dedup/dedupCore/hashTableSS/instrDecoder inst_dynamic/inst_dedup/dedupCore/hashTableSS/instrIssuer]]

create_pblock pblock_SHA3ResQueue
resize_pblock pblock_SHA3ResQueue -add CLOCKREGION_X2Y1:CLOCKREGION_X3Y5
add_cells_to_pblock pblock_SHA3ResQueue [get_cells [list inst_dynamic/inst_dedup/dedupCore/hashTableSS/SHA3ResQueue]]

create_pblock pblock_lookupEngine
resize_pblock pblock_lookupEngine -add CLOCKREGION_X0Y0:CLOCKREGION_X3Y1
add_cells_to_pblock pblock_lookupEngine [get_cells [list inst_dynamic/inst_dedup/dedupCore/hashTableSS/lookupEngine]]

create_pblock pblock_pgWriterSS
resize_pblock pblock_pgWriterSS -add CLOCKREGION_X1Y1:CLOCKREGION_X3Y1
add_cells_to_pblock pblock_pgWriterSS [get_cells [list inst_dynamic/inst_dedup/dedupCore/pgWriterSS]] 
