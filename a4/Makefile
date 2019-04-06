all: ext2_mkdir ext2_cp ext2_ln ext2_rm ext2_restore ext2_checker

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

ext2_checker: path.c path.h ext2.h ext2_checker.c
	gcc -Wall -g -o ext2_checker path.c ext2_checker.c

clean:
	rm -rf ext2_mkdir ext2_cp ext2_ln ext2_rm ext2_restore ext2_checker *.dSYM