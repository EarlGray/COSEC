ifneq ($(RUST),)

cargo_mode	 := release

$(librs):
	cd lib/rosec && cargo build --target i686-cosec.json --$(cargo_mode)
	cp lib/rosec/target/i686-cosec/$(cargo_mode)/librosec.a $@

endif

clean_rust:
	-cd lib/rosec && which cargo && cargo clean
	-test $(librs) && rm $(librs)
