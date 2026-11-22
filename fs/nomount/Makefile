# Makefile for NoMount

NOMOUNT_COMMITS := 788
ccflags-y += -DNOMOUNT_COMMIT_COUNT=\"$(NOMOUNT_COMMITS)\"

obj-$(CONFIG_NOMOUNT) += nomount.o
ccflags-y += -std=gnu11 -Wno-declaration-after-statement

ifeq ($(shell grep -qE '\(\*[[:space:]]*__get\)' $(srctree)/include/linux/xattr.h 2>/dev/null && echo 1 || echo 0),1)
ccflags-y += -DNM_HAS_XATTR__GET
endif
