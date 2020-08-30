librosec := librosec.a
mode	 := release

$(librosec):
	cargo build --target i686-cosec.json --$(mode)
	cp target/i686-cosec/$(mode)/librosec.a $@

clean:
	cargo clean
	rm $(librosec)
