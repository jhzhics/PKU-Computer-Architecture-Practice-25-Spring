# Define the target 'all' which depends on 'generator'
all: generator c rust

# Rule for building 'generator'
generator: generator.cpp
	g++ generator.cpp -O2 -o $@

c: c.c
	gcc c.c -O2 -o $@

rust: rust_tester/src/main.rs
	cd rust_tester && cargo build --release
	cp rust_tester/target/release/rust_tester rust

# Mark 'clean' as a phony target
.PHONY: clean

# Rule for 'clean' to remove the compiled output
clean:
	rm -f generator
	rm -f c
	rm -f rust
