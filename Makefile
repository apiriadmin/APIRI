#Get the current directory where this Makeile resides
TOP := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# library and executable projects
all_projects := \
tod/src \
fio/src/fioapi \
fio/src/fiotest \
fpu/FrontPanelSystem/libfpui \
fpu/FrontPanelSystem/AuxSampleApplication \
fpu/FrontPanelSystem/fpuiexec \
fpu/FrontPanelSystem/FrontPanelManager \
fpu/FrontPanelSystem/MasterSelection \
fpu/FrontPanelSystem/SampleApplication \
fpu/FrontPanelSystem/SystemConfiguration \
fpu/FrontPanelSystem/SystemUtilities \
apps \
utils/timesrc

# kernel modules:
# must define LINUX_DIR specifying linux build dir,
# add toolchain bin to PATH and specify ARCH and CROSS_COMPILE,
# e.g. ARCH=powerpc CROSS_COMPILE=powerpc-unknown-linux-uclibc-
all_kmods := \
fio/src/fiodriver \
fpu/FrontPanelSystem/FrontPanelDriver


.PHONY: $(all_projects)
all: $(all_projects)
$(all_projects):
	$(MAKE) --directory=$@ all

.PHONY: $(all_kmods)
$(all_kmods):
	$(MAKE) -C $(LINUX_DIR) M=$(TOP)$@ modules

#Need different target names for "all" vs "clean".  So the clean_projects 
#name has an trailing slash at the end.
clean_projects := $(addsuffix /, $(all_projects))
.PHONY: $(clean_projects)
$(clean_projects):
	$(MAKE) --directory=$@ clean

clean_kmods := $(addsuffix /, $(all_kmods))
.PHONY: $(clean_kmods)
$(clean_kmods):
	find $(TOP)$@ -regex ".*\.\(o\|ko\)" -type f -delete

.PHONY: api
api: $(all_projects)

.PHONY: kmods
kmods: $(all_kmods)

all: $(all_projects) $(all_kmods)
clean: $(clean_projects) $(clean_kmods)

