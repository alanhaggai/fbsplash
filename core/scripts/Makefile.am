bin_SCRIPTS   = bootsplash2fbsplash splash_manager splash_resize splashy2fbsplash.py
eexecsbin_SCRIPTS = splash-functions.sh
if CONFIG_HELPER
sbin_SCRIPTS   = splash_geninitramfs
endif

EXTRA_DIST  = bootsplash2fbsplash.in splash-functions.sh.in splash_geninitramfs.in splash_manager.in splash_resize.in splashy2fbsplash.py.in
MOSTLYCLEANFILES = bootsplash2fbsplash splash-functions.sh splash_geninitramfs splash_manager splash_resize splashy2fbsplash.py

%: %.in
	@$(call infmsg,CREATE,$@)
	$(Q)$(MKDIR_P) $(@D)
	$(Q)rm -f $@
	$(Q)cat $< | $(SED) \
		-e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
		-e 's|@bindir[@]|$(eexecbindir)|g' \
		-e 's|@sbindir[@]|$(eexecsbindir)|g' \
		-e 's|@sysconfdir[@]|$(sysconfdir)|g' \
		-e 's|@themedir[@]|$(themedir)|g' \
		-e 's|@libdir[@]|$(eexeclibdir)|g' \
		> $@

