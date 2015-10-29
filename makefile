CFLAGS=-Wall -Werror -m32 -std=c99 -nostartfiles -nostdlib -nodefaultlibs -static -Iinc
EXCLUDE=src/bootsect.s src/setup.s src/head.s
CSOURCE=$(filter-out $(EXCLUDE),$(wildcard src/*.c))
ASMSOURCE=$(filter-out $(EXCLUDE),$(wildcard src/*.s))


bin/c.img:bin/bootsect.sb bin/setup.sb bin/system.cb
	dd if=bin/bootsect.sb of=bin/c.img bs=512 count=1 conv=notrunc
	dd if=bin/setup.sb of=bin/c.img bs=512 count=4 conv=notrunc seek=1
	dd if=bin/system.cb of=bin/c.img bs=512 count=384 conv=notrunc seek=5

bin/system.cb:obj/head.so $(patsubst src/%.c,obj/%.o,$(CSOURCE)) $(patsubst src/%.s,obj/%.so,$(ASMSOURCE))
	$(LD) --oformat=binary -m elf_i386 -M -x -e startup_32 --section-start .text=0xc0100000 -o $@ $^ |\
	tee os.symbols.origin |\
	grep -E "^\s+0x[0-9a-f]+\s+[a-zA-Z_]\S+$$" | sed -r "s/^[\t ]+//" | sed -r "s/[\t ]+/ /" > os.symbols

#$(CC) $(CFLAGS) -S -o $@ $^

obj/%.so:src/%.s
	nasm -f elf $< -o $@

obj/%.o:src/%.c dep/%.d
	$(CC) $(CFLAGS) -c $< -o $@

dep/%.d:src/%.c
	@$(RM) $(DEPDIR)/$@;
	@$(CC) -MM $(CFLAGS) $< | sed 's/\($(patsubst %.c,%,$(notdir $<))\)\.o *: */obj\/\1.o dep\/\1.d:/' > $@

bin/%.sb:src/%.s
	nasm $< -o $@

ifneq ($(MAKECMDGOALS),clean)
sinclude $(patsubst src/%.c,dep/%.d,$(CSOURCE))
endif

.PHONY: clean run

clean:
	$(RM) obj/*
	$(RM) bin/*.sb
	$(RM) bin/*.cb
	$(RM) dep/*
	dd if=/dev/zero of=bin/c.img bs=512 count=2880 conv=notrunc

run:bin/c.img
	bochs -q -f .bochsrc
