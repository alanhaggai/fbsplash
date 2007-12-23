SUBDIRS = libs src docs misc scripts

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA  = libfbsplash.pc libfbsplashrender.pc
EXTRA_DIST      = libfbsplash.pc.in libfbsplashrender.pc.in

MOSTLYCLEANFILES = libfbsplash.pc libfbsplashrender.pc

%.pc: %.pc.in
	@$(call infmsg,CREATE,$@)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)rm -f $@
	$(Q)cat $< | $(SED) \
		-e 's|@prefix[@]|$(prefix)|g' \
		-e 's|@exec_prefix[@]|$(exec_prefix)|g' \
		-e 's|@libdir[@]|$(libdir)|g' \
		-e 's|@includedir[@]|$(includedir)|g' \
		-e 's|@libs_private[@]|$(strip $($(basename $(@F))_la_LIBADD))|g' \
		> $@




