compile:
	@echo "Compiling lc3.c to executable lc3-vm..."
	gcc -rdynamic -o lc3-vm lc3.c
	chmod a+x lc3-vm

assemble:
	@echo "Assembling LC3 rom ${rom}.asm..."
	lc3as $(rom).asm

run:
	make assemble
	@echo "Running rom ${rom}.asm..."
	./lc3-vm ${rom}.obj
