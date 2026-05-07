#include "syscall.h"
#include "defs.h"
#include "loader.h"
#include "syscall_ids.h"
#include "timer.h"
#include "trap.h"
#include "proc.h"

uint64 sys_write(int fd, uint64 va, uint len)
{
	debugf("sys_write fd = %d va = %x, len = %d", fd, va, len);
	if (fd != STDOUT)
		return -1;
	struct proc *p = curr_proc();
	char str[MAX_STR_LEN];
	int size = copyinstr(p->pagetable, str, va, MIN(len, MAX_STR_LEN));
	debugf("size = %d", size);
	for (int i = 0; i < size; ++i) {
		console_putchar(str[i]);
	}
	return size;
}

__attribute__((noreturn)) void sys_exit(int code)
{
	exit(code);
	__builtin_unreachable();
}

uint64 sys_sched_yield()
{
	yield();
	return 0;
}

// 虚存版本：需要用 copyout 把数据写到用户空间
uint64 sys_gettimeofday(TimeVal *val, int _tz)
{
	struct proc *p = curr_proc();
	uint64 cycle = get_cycle();
	TimeVal tv;
	tv.sec = cycle / CPU_FREQ;
	tv.usec = (cycle % CPU_FREQ) * 1000000 / CPU_FREQ;
	if (copyout(p->pagetable, (uint64)val, (char *)&tv, sizeof(tv)) < 0)
		return -1;
	return 0;
}

uint64 sys_sbrk(int n)
{
	uint64 addr;
	struct proc *p = curr_proc();
	addr = p->program_brk;
	if (growproc(n) < 0)
		return -1;
	return addr;
}

// 虚存版本的 sys_trace
uint64 sys_trace(int trace_request, unsigned long id, uint8 data)
{
	struct proc *p = curr_proc();
	pte_t *pte;
	uint64 pa;

	switch (trace_request) {
	case 0: // 读取一个字节
		// 检查地址是否用户可读
		pte = walk(p->pagetable, (uint64)id, 0);
		if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 || (*pte & PTE_R) == 0)
			return -1;
		pa = PTE2PA(*pte);
		return (uint64)(*(uint8 *)(pa + ((uint64)id & 0xFFF)));
	case 1: // 写入一个字节
		pte = walk(p->pagetable, (uint64)id, 0);
		if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 || (*pte & PTE_W) == 0)
			return -1;
		pa = PTE2PA(*pte);
		*(uint8 *)(pa + ((uint64)id & 0xFFF)) = data;
		return 0;
	case 2: // 查询系统调用次数
		if (id >= 512)
			return 0;
		return p->syscall_counts[id];
	default:
		return -1;
	}
}

// mmap 系统调用
int sys_mmap(void *start, unsigned long long len, int prot, int flags)
{
	struct proc *p = curr_proc();
	uint64 addr = (uint64)start;
	uint64 npages;

	// 参数检查
	if ((addr % PGSIZE) != 0)
		return -1;
	if ((prot & ~0x7) != 0)
		return -1;
	if ((prot & 0x7) == 0)
		return -1;
	if (len == 0)
		return 0;

	npages = PGROUNDUP(len) / PGSIZE;
	if (addr + npages * PGSIZE < addr)
		return -1; // 溢出

	// 检查是否有重叠映射
	for (uint64 a = addr; a < addr + npages * PGSIZE; a += PGSIZE) {
		pte_t *pte = walk(p->pagetable, a, 0);
		if (pte && (*pte & PTE_V))
			return -1;
	}

	// 逐页映射
	int perm = PTE_U;
	if (prot & 0x1) perm |= PTE_R;
	if (prot & 0x2) perm |= PTE_W;
	if (prot & 0x4) perm |= PTE_X;

	for (uint64 a = addr; a < addr + npages * PGSIZE; a += PGSIZE) {
		char *mem = kalloc();
		if (mem == 0)
			return -1;
		memset(mem, 0, PGSIZE);
		if (mappages(p->pagetable, a, PGSIZE, (uint64)mem, perm) < 0) {
			kfree(mem);
			return -1;
		}
	}

	// 更新 max_page
	uint64 end = addr + npages * PGSIZE;
	if (PGROUNDUP(end) > p->max_page)
		p->max_page = PGROUNDUP(end);

	return 0;
}

// munmap 系统调用
int sys_munmap(void *start, unsigned long long len)
{
	struct proc *p = curr_proc();
	uint64 addr = (uint64)start;
	uint64 npages;

	if ((addr % PGSIZE) != 0)
		return -1;
	if (len == 0)
		return 0;

	npages = PGROUNDUP(len) / PGSIZE;

	// 检查是否全部存在映射
	for (uint64 a = addr; a < addr + npages * PGSIZE; a += PGSIZE) {
		pte_t *pte = walk(p->pagetable, a, 0);
		if (pte == 0 || (*pte & PTE_V) == 0)
			return -1;
	}

	uvmunmap(p->pagetable, addr, npages, 1);
	return 0;
}

extern char trap_page[];

void syscall()
{
	struct trapframe *trapframe = curr_proc()->trapframe;
	int id = trapframe->a7, ret;
	uint64 args[6] = { trapframe->a0, trapframe->a1, trapframe->a2,
			   trapframe->a3, trapframe->a4, trapframe->a5 };
	tracef("syscall %d args = [%x, %x, %x, %x, %x, %x]", id, args[0],
	       args[1], args[2], args[3], args[4], args[5]);

	// 系统调用计数
	struct proc *p = curr_proc();
	if (id >= 0 && id < 512) {
		p->syscall_counts[id]++;
	}

	switch (id) {
	case SYS_write:
		ret = sys_write(args[0], args[1], args[2]);
		break;
	case SYS_exit:
		sys_exit(args[0]);
	case SYS_sched_yield:
		ret = sys_sched_yield();
		break;
	case SYS_gettimeofday:
		ret = sys_gettimeofday((TimeVal *)args[0], args[1]);
		break;
	case SYS_sbrk:
		ret = sys_sbrk(args[0]);
		break;
	case SYS_trace:
		ret = sys_trace((int)args[0], args[1], (uint8)args[2]);
		break;
	case SYS_mmap:
		ret = sys_mmap((void *)args[0], args[1], (int)args[2], (int)args[3]);
		break;
	case SYS_munmap:
		ret = sys_munmap((void *)args[0], args[1]);
		break;
	default:
		ret = -1;
		errorf("unknown syscall %d", id);
	}
	trapframe->a0 = ret;
	tracef("syscall ret %d", ret);
}