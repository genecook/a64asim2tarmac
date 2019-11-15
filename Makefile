A64SIM = ${HOME}/Desktop/windvane

LDFLAGS := ${A64SIM}/lib/libsimX.a

CFLAGS := -I . -I${A64SIM}/include -fPIC -std=c++11 -Wall -Werror -Wno-unknown-pragmas

# tarmac writer plugin:

libtarmacplugin.so: tarmac_plugin.h tarmac_plugin.C
	g++ $(CFLAGS) -shared -o $@ $^ ${LDFLAGS}

# -L ${A64SIM}/boot_code/boot2el0.obj   -- boot code for the example test
# -n 1000                               -- simulate 1k instructions max
# -D                                    -- stream disassembly to stdout
# -F                                    -- generate random instructions as required
# -U ./libtarmacplugin.so               -- path to plugin to load
# -O 'sample_test.tarmac'               -- specify plugin option. in this case tarmac file name

test:
	${A64SIM}/bin/a64sim -L ${A64SIM}/boot_code/boot2el0.obj -n 1000 -D -F -U ./libtarmacplugin.so -O 'sample_test.tarmac' 1>tlog

clean:
	rm -f libtarmacplugin.so sample_test.tarmac sample_test.top sample_test.0 tlog
