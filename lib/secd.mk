ifneq ($(SECD),)
SECD_VER 	:= 0.1.2
SECD_TARGZ	:= $(CACHE_DIR)/secd-$(SECD_VER).tar.gz

cc_includes += -I $(top_dir)/lib/secd/include

$(libsecd): $(secdsrc)
	$(cc) -c $< -o $@ $(cc_includes) $(cc_flags)

$(secdsrc): $(build) lib/secd/libsecd.c include/secd
	cp lib/secd/libsecd.c $@

include/secd: lib/secd lib/secd/repl.secd
	mkdir -p $@
	cp -r lib/secd/include/secd/* $@/
	xxd -i lib/secd/repl.secd > $@/repl.inc

lib/secd/libsecd.c: lib/secd
	make -C $^ libsecd.c

lib/secd/repl.secd:
	make -C lib/secd repl.secd

lib/secd: $(SECD_TARGZ)
	tar -C lib/ -xf $(SECD_TARGZ)
	mv lib/SECD-$(SECD_VER) lib/secd

$(SECD_TARGZ):
	mkdir -p $(CACHE_DIR)
	curl -R -L -o $@ https://github.com/EarlGray/SECD/archive/$(SECD_VER).tar.gz

endif
clean_secd:
	rm -r $(secdsrc) include/secd || true
	make -C lib/secd clean || true

