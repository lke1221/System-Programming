blk-core.c
: /usr/src/linux-4.4/block에 위치한 파일입니다. 기존의 소스코드에 순환큐를 정의하고, block number, time, file system name로 이루어진 구조체를 큐에 담는 과정을 추가하였습니다.

segbuf.c
: /usr/src/linux-4.4/fs/nilfs2에 위치한 파일입니다. nilfs2의 superblock을 참조하도록 기존의 소스코드를 수정하였습니다.

lkm.c
: /home/[사용자명]/module에 위치한 LKM 소스코드입니다. proc_buffer에 있는 값을 user_buffer로 넘겨주는 역할을 합니다. (copy_to_user)

Makefile
: /home/[사용자명]/module에 위치한 파일입니다. lkm.c 소스 코드로부터 lkm.ko모듈을 만들어줍니다.