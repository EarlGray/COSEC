ifneq ($(LUA),)

LUA_VER  ?= 5.2.2
LUA_DIR  := lib/lua-$(LUA_VER)

# include/lua/lua.h: $(LUA_DIR)

$(CACHE_DIR)/lua-$(LUA_VER).tar.gz:
	mkdir -p $(CACHE_DIR) ; cd $(CACHE_DIR); \
    curl -LR -O http://www.lua.org/ftp/$(notdir $@)

$(LUA_DIR): $(CACHE_DIR)/lua-$(LUA_VER).tar.gz
	tar xf $< -C lib/
	cd include ; test -L lua || ln -s ../$(LUA_DIR)/src lua

$(liblua): $(LUA_DIR)
	make -C $(LUA_DIR)/src \
			CC=$(cc) RANLIB=$(ranlib) AR='$(ar) rcu --target elf32-i386' \
	        SYSCFLAGS="-m32 -nostdinc -fno-stack-protector -I $(STDINC_DIR)" liblua.a
	mv $(LUA_DIR)/src/liblua.a $(top_dir)/$@

endif

clean_lua:
	rm -rf include/lua lib/liblua.a $(LUA_DIR) || true

