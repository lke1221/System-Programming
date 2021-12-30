#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/uaccess.h>

#define PROC_DIRNAME "myproc"
#define PROC_FILENAME "myproc"

#define Q_SIZE 1024 
char proc_buffer[Q_SIZE][33]; //순환큐에서 정보를 읽어와 문자열 형태로 담아 둘 buffer (정채운, 11/2)

static struct proc_dir_entry *proc_dir; 
static struct proc_dir_entry *proc_file;

struct elem {
	unsigned long long block_n;
    long time; 
	char* fs_name;
};
extern struct elem buffer[Q_SIZE]; //blk-core.c에서 선언한 순환큐 (정채운, 11/2)
extern int q_index; //순환큐의 현재 인덱스 (정채운, 11/2)

static int my_open(struct inode *inode, struct file *file) {
	printk(KERN_INFO "Module Open\n"); //파일 오픈 시 간단한 메세지 출력 (정채운, 11/2)
	return 0;
}

static ssize_t my_write(struct file *file, const char __user * user_buffer, size_t count, loff_t *ppos) {
	printk(KERN_INFO "Module Write\n");
	
	int p_index = q_index; //커널에서 다음에 입력할 인덱스의 값을 받아옴 = 현재 큐에 들어있는 값들 중 가장 오래된 정보를 가리킴 (정채운, 11/2)
	int i;
	for (i = 0; i < Q_SIZE; i++) { //p_index에서 시작해서 Q_SIZE만큼 반복(한 바퀴 순회)하면서 가장 오래된 값부터 문자열로 저장 (정채운, 11/2)
		sprintf(proc_buffer[i],	"(%10lld, %10ld, %6s)\n", buffer[p_index].block_n, buffer[p_index].time, buffer[p_index].fs_name);
		p_index = (p_index + 1) % Q_SIZE; 
	}
	return count;
}


static ssize_t my_read(struct file *file, char __user * user_buffer, size_t count, loff_t *ppos) {
	printk(KERN_INFO "Module Read\n");
	
	unsigned long proc_count = sizeof(proc_buffer); 
	copy_to_user(user_buffer, proc_buffer, proc_count); //proc_buffer에 저장된 값을 user_buffer에 넘겨 줌 (정채운, 11/2)
	
	*ppos += proc_count; //읽은 바이트수만큼 ppos 이동 (정채운, 11/2)
	if (*ppos > proc_count){
		return 0; //더이상 읽을 것이 없으면 0 리턴 (정채운, 11/2)
	} else {
		return proc_count; //읽은 바이트수 리턴 (정채운, 11/2)
	}
}


static const struct file_operations myproc_fops = { //file_operations 선언 (정채운, 11/2)
	.owner = THIS_MODULE,
	.open = my_open,
	.write = my_write,
	.read = my_read,
};

static int __init simple_init(void) {
	printk(KERN_INFO "Module Init\n");

	proc_dir = proc_mkdir(PROC_DIRNAME, NULL); //proc directory 생성 (정채운, 11/2)
	proc_file = proc_create(PROC_FILENAME, 0600, proc_dir, &myproc_fops); //proc file 생성 (정채운, 11/2)
	return 0;
}

static void __exit simple_exit(void) {
	printk(KERN_INFO "Module Exit\n");

	proc_remove(proc_file); //proc file 제거 (정채운, 11/2)
	proc_remove(proc_dir); //proc directory 제거 (정채운, 11/2)
	return;
}

module_init(simple_init);
module_exit(simple_exit);

MODULE_DESCRIPTION("Proc Write & Read Module");
MODULE_LICENSE("GPL");
MODULE_VERSION("NEW");

