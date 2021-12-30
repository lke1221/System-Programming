#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/netfilter.h>
#include <asm/uaccess.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/netfilter_ipv4.h> //priority

#define PROC_DIRNAME "myproc"
#define PROC_FILENAME "myproc"
#define PROC_MAX 1024
#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]

static struct proc_dir_entry *proc_dir; 
static struct proc_dir_entry *proc_file;

char proc_buffer[PROC_MAX];
//forward/drop할 포트번호 초기값 설정, proc write를 통해 사용자에게 입력받아 변경할 수 있음
unsigned short forward_port = 1111;
unsigned short drop_port = 2222;

/*
각 후킹 포인트마다 하나의 함수 등록...
*/

/*page 10: 넷필터의 후킹 포인트 중 한 곳(PRE_ROUTING)에 패킷 forward/drop 함수를 등록*/
// Forwarding (Sport == forward_port)
// 라우팅 테이블에 add명령어로 정보 추가 필요 (route add -net {해당되는 IP 주소} netmask 255.255.255.0 dev {Iface값}(ex- enp0s8 : s8로 끝나는 인터페이스로 보내라))
// 들어온 패킷 정보 출력 - forward:PRE_ROUTING packet (Protocol;Sport;Dport;SIP;DIP)
// Sport, Dport 7777로 변경 (struct tcphdr *tcp = tcp_hdr(skb); tcp->source=...; tcp->dest=...;)
// 포워딩할수있게 IP주소 변경 (struct iphdr *nh = ip_hdr(skb); nh->saddr=...; nh->daddr=...;)
// NF_ACCEPT 리턴

// Drop (Sport == drop_port)
// 들어온 패킷 정보 출력 - drop:PRE_ROUTING packet (Protocol;Sport;Dport;SIP;DIP)
// Sport, Dport 3333으로 변경 (struct tcphdr *tcp = tcp_hdr(skb); tcp->source=...; tcp->dest=...;)
// 드랍하기 전 패킷 정보 출력 - drop:PRE_ROUTING packet (Protocol;Sport;Dport;SIP;DIP)
// 단! 정보 출력 시 hard coding이 아닌 구조체로부터 정보 얻어와야 함
// NF_DROP 리턴

unsigned int inet_addr(char *str)
{
	int a, b, c, d;
    char arr[4];
    sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d);
    arr[0] = a; arr[1] = b; arr[2] = c; arr[3] = d;
    return *(unsigned int *)arr;
    
}

static unsigned int foward_or_drop_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {
	/* 후킹함수 작성 */
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = tcp_hdr(skb);

	unsigned char protocol = iph->protocol;
	unsigned short sport = ntohs(tcph->source);
	unsigned short dport = ntohs(tcph->dest);

    if (sport == forward_port) {
		printk(KERN_INFO "forward:PRE_ROUTING packet(%u; %u; %u; %d.%d.%d.%d; %d.%d.%d.%d)\n", protocol, sport, dport, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
        tcph->source = htons(7777);
        tcph->dest = htons(7777);
		iph->daddr = inet_addr("100.1.1.0"); 
		//page 11: NF_INET_PRE_ROUTING, NF_INET_FORWARD, NF_INET_POST_ROUTING 에서 hooking 되었는지 확인한다.
		return NF_ACCEPT;
	}
	else if (sport == drop_port) {
		printk(KERN_INFO "drop:PRE_ROUTING packet(%u; %u; %u; %d.%d.%d.%d; %d.%d.%d.%d)\n", protocol, sport, dport, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
        tcph->source = htons(3333);
        tcph->dest = htons(3333);
		//page 10: 넷필터의 후킹 포인트 중 '패킷 drop 함수를 등록한 후킹 포인트'에 drop한 패킷을 확인하는 함수를 등록
        //page 12: NF_INET_PRE_ROUTING, NF_INET_LOCAL_IN 에서hooking 되었는지 확인한다.
		printk(KERN_INFO "drop:PRE_ROUTING packet(%u; %u; %u; %d.%d.%d.%d; %d.%d.%d.%d)\n", protocol, ntohs(tcph->source), ntohs(tcph->dest), NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
		return NF_DROP;
	}
	else {
		return NF_ACCEPT;
	} 
}

static struct nf_hook_ops forward_or_drop = {
	.hook = foward_or_drop_hook, //등록하려는 함수
	.pf = PF_INET, //네트워크 family
	.hooknum = NF_INET_PRE_ROUTING, //후킹 포인트
	.priority = NF_IP_PRI_FIRST, //우선순위
};


/*page 12: NF_INET_LOCAL_IN 에서 port가 3333인 패킷이 있는지 확인하는 코드가 있어야 한다.*/
// LOCAL_IN:
// -Drop
// port가 3333인 패킷이 있는지 확인
// 3333이면 printk 출력하고 NF_DROP, 아니면 NF_ACCEPT 리턴
static unsigned int check_drop_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {
	/* 후킹함수 작성 */
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = tcp_hdr(skb);

	unsigned char protocol = iph->protocol;
	unsigned short sport = ntohs(tcph->source);
	unsigned short dport = ntohs(tcph->dest);

	if(sport==3333){
		printk(KERN_INFO "drop:LOCAL_IN packet(%u; %u; %u; %d.%d.%d.%d; %d.%d.%d.%d)\n", protocol, sport, dport, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
		return NF_DROP;
	}

	return NF_ACCEPT;
}

static struct nf_hook_ops check_packet_drop = {
	.hook = check_drop_hook, //등록하려는 함수
	.pf = PF_INET, //네트워크 family
	.hooknum = NF_INET_LOCAL_IN, //후킹 포인트
	.priority = NF_IP_PRI_FIRST, //우선순위
};


/*page 10: 넷필터의 후킹 포인트 중 '다른 두 곳'에 forward한 패킷을 확인하는 함수를 등록
  page 11: NF_INET_FORWARD,NF_INET_POST_ROUTING 에서 hooking 되었는지 확인한다*/ 
// 패킷 정보 출력 - forward:FORWARD packet (Protocol;Sport;Dport;SIP;DIP)
// NF_ACCEPT 리턴
static unsigned int check_forward_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state) {
	/* 후킹함수 작성 */
	struct iphdr *iph = ip_hdr(skb);
	struct tcphdr *tcph = tcp_hdr(skb);

	unsigned char protocol = iph->protocol;
	unsigned short sport = ntohs(tcph->source);
	unsigned short dport = ntohs(tcph->dest);
	
	if(sport==7777) {
		if(state->hook == NF_INET_FORWARD)
			printk(KERN_INFO "forward:FORWARD packet(%u; %u; %u; %d.%d.%d.%d; %d.%d.%d.%d)\n", protocol, sport, dport, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
		else
			printk(KERN_INFO "forward:POST_ROUTING packet(%u; %u; %u; %d.%d.%d.%d; %d.%d.%d.%d)\n", protocol, sport, dport, NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));
	}
	return NF_ACCEPT;
}

//후킹포인트 : FORWARD
static struct nf_hook_ops check_packet_forward_f = {
	.hook = check_forward_hook, //등록하려는 함수
	.pf = PF_INET, //네트워크 family
	.hooknum = NF_INET_FORWARD, //후킹 포인트
	.priority = NF_IP_PRI_FIRST, //우선순위
};
//후킹포인트: POST_ROUTING:
static struct nf_hook_ops check_packet_forward_p = {
	.hook = check_forward_hook, //등록하려는 함수
	.pf = PF_INET, //네트워크 family
	.hooknum = NF_INET_POST_ROUTING, //후킹 포인트
	.priority = NF_IP_PRI_FIRST, //우선순위
};


//forward/drop할 포트번호 전달할 때 proc 사용
static ssize_t my_write(struct file *file, const char __user * user_buffer, size_t count, loff_t *ppos) {
	//user_buffer에 있는 값으로 전역변수(포워딩/드랍할 포트번호) 설정 예) sudo echo 1111 2222 > myproc
	size_t size = count;
	//proc_buffer 크기만큼만 입력받음
	if (size > PROC_MAX) {
		size = PROC_MAX;
	}
	//user_buffer에 있는 값 size만큼 proc_buffer에 복사
	if (copy_from_user(proc_buffer, user_buffer, size)) {
		return -EFAULT;
	}
	//forward_port, drop_port 값 저장
	sscanf(proc_buffer, "%hu%hu", &forward_port, &drop_port);
	//저장한 값 출력
	printk(KERN_INFO "forward_port:%u, drop_port:%u\n", forward_port, drop_port);
	
	return size;
}

static const struct file_operations myproc_fops = { 
	.owner = THIS_MODULE,
	.write = my_write,
};

static int __init simple_init(void) {
	printk(KERN_INFO "module init");
	proc_dir = proc_mkdir(PROC_DIRNAME, NULL);
	proc_file = proc_create(PROC_FILENAME, 0600, proc_dir, &myproc_fops);
	
	nf_register_hook(&forward_or_drop);
	nf_register_hook(&check_packet_drop);
	nf_register_hook(&check_packet_forward_f);
	nf_register_hook(&check_packet_forward_p);
	return 0;
}

static void __exit simple_exit(void) {
	printk(KERN_INFO "module exit");
	proc_remove(proc_file);
	proc_remove(proc_dir);

	nf_unregister_hook(&forward_or_drop);
	nf_unregister_hook(&check_packet_drop);
	nf_unregister_hook(&check_packet_forward_f);
	nf_unregister_hook(&check_packet_forward_p);
}

module_init(simple_init);
module_exit(simple_exit);

MODULE_DESCRIPTION("Netfilter firewall module");
MODULE_LICENSE("GPL");
MODULE_VERSION("NEW");