all: test ext2_mkdir ext2_cp ext2_ln ext2_rm ext2_restore

test: path.c path.h ext2.h test.c
	gcc -Wall -g -o test test.c path.c

ext2_mkdir: path.c path.h ext2.h ext2_mkdir.c
	gcc -Wall -g -o ext2_mkdir path.c ext2_mkdir.c

ext2_cp: path.c path.h ext2.h ext2_cp.c
	gcc -Wall -g -o ext2_cp path.c ext2_cp.c

ext2_ln: path.c path.h ext2.h ext2_ln.c
	gcc -Wall -g -o ext2_ln path.c ext2_ln.c

ext2_rm: path.c path.h ext2.h ext2_rm.c
	gcc -Wall -g -o ext2_rm path.c ext2_rm.c

ext2_restore: path.c path.h ext2.h ext2_restore.c
	gcc -Wall -g -o ext2_restore path.c ext2_restore.c

clean:
	rm -rf test ext2_mkdir ext2_cp ext2_ln ext2_rm ext2_restore *.dSYM