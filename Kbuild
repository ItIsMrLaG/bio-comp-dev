include $(PWD)/flags.mk

bio_comp_dev-y := bcomp_module.o bcomp.o

bio_comp_dev-y += compression_profiles/lz4_comp.o 
bio_comp_dev-y += compression_profiles/empty_comp.o
bio_comp_dev-y += compression_profiles/comp_common.o 

bio_comp_dev-y += map_profiles/liniar_map.o
bio_comp_dev-y += map_profiles/map_common.o
bio_comp_dev-y += map_profiles/cell_manager.o

bio_comp_dev-y += utils/settings.o utils/stats.o

obj-m := bio_comp_dev.o