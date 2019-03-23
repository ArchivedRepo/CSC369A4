all: test ext2_mkdir ext2_cp

test: path.c path.h ext2.h test.c
	gcc -g -o test test.c path.c

ext2_mkdir: path.c path.h ext2.h ext2_mkdir.c
	gcc -g -o ext2_mkdir path.c ext2_mkdir.c

ext2_cp: path.c path.h ext2.h ext2_cp.c
	gcc -g -o ext2_cp path.c ext2_cp.c

clean:
	rm -rf test ext2_mkdir ext2_cp *.dSYM