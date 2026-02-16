#include "kernel.h"
#include "common.h"

extern char __bss[], __bss_end[], __stack_top[], __free_ram[], __free_ram_end[], __kernel_base[];

sbiret sbi_call(long arg0,long arg1,long arg2,long arg3,long arg4,long arg5, long fid, long eid);



// syscall handler
void handle_syscall(struct trap_frame *f);

// input to the debug console
long getchar(void);



// User mode
void user_entry(void);


// process

struct process procs[PROCS_MAX]; // All process control structures.

// prepara un nuovo processo per eseguire
struct process *create_process(const void *image, size_t image_size);


// scheduler

struct process *current_proc;	// processo che sta eseguendo
struct process *idle_proc;	// idle process

void yield(void);


// page table stuff
void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);




// page allocator
paddr_t alloc_pages(uint32_t n);


// implementazione del context switch per la gestione delle interruzioni

__attribute__((naked)) // per evitare che il compilatore modifichi le istruzioni per ottimizzarle
__attribute__((aligned(4))) // per assicurarsi che l'entry point della funzione sia un multiplo di 4
void kernel_entry(void);

// context switch

__attribute__((naked))
void switch_context(uint32_t *prev_sp, uint32_t *next_sp);

// legge il motivo per cui si è verificata l'eccezione e lo mostra a video
void handle_trap(struct trap_frame *f);

void putchar(char ch);


// void delay(void){
	// 	for (int i = 0; i < 30000000; i++){
		// 		__asm__ __volatile__("nop"); // non fare nulla
		// 	}
		// }
		
		// struct process *proc_a;
		// struct process *proc_b;
		
		
		// void proc_a_entry(void){
			// 	printf("starting process A\n");
			// 	while (1){
				// 		// printf("%x\n", SATP_SV32);
				// 		putchar('A');
				// 		yield();
				// 	}
				
				// }
				// void proc_b_entry(void){
					// 	printf("starting process B\n");
					// 	while (1){
						// 		putchar('B');
						// 		yield();
						// 	}
						
						// }
						
						// * main
						void kernel_main(void) {
							memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);
							memset(procs, 0, sizeof(procs)); // Explicitly clear procs to ensure clean state
							
							printf("\n\n");
							
							// lezione allocator
							// paddr_t paddr0 = alloc_pages(2);
							// paddr_t paddr1 = alloc_pages(1);
							
							// printf("alloc_pages test: paddr0=%x\n", paddr0);
							// printf("alloc_pages test: paddr1=%x\n", paddr1);
							
							
							WRITE_CSR(stvec, (uint32_t)kernel_entry);
							// __asm__ __volatile__ ("unimp"); // pseudo istruzione che attiva un'eccezione illegale
							// PANIC("booted!");
							// printf("unreachable here!\n");
							// idle loop
    // per evitare che il kernel vadi in una busy wait, eseguiamo la istruzione `wfi` (wait for interrupt)
	
	// proc_a = create_process((uint32_t) proc_a_entry);
	// proc_b = create_process((uint32_t) proc_b_entry);
	
	// proc_a_entry();
	
	
	// PANIC("unreachable here!");
	
	idle_proc = create_process(NULL, 0);
	idle_proc->pid = 0;
	current_proc = idle_proc;
	
	create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);
	
	yield();
	PANIC("switched to idle process");
	
	
	for (;;){
		__asm__ __volatile__("wfi");
	} 
}

__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void){
	__asm__ __volatile__(
		"mv sp, %[stack_top]\n" // set the stack pointer
		"j kernel_main\n" // jump to the kernel main function
		:
		: [stack_top] "r" (__stack_top) // passa l'indirizzo dello stack top come %[stack_top]
	);
	
	
}


sbiret sbi_call(long arg0,long arg1,long arg2,long arg3,long arg4,long arg5, long fid, long eid) {
	
	register long a0 __asm__("a0") = arg0;	
	register long a1 __asm__("a1") = arg1;
	register long a2 __asm__("a2") = arg2;
	register long a3 __asm__("a3") = arg3;
	register long a4 __asm__("a4") = arg4;
	register long a5 __asm__("a5") = arg5;
	register long a6 __asm__("a6") = fid;
	register long a7 __asm__("a7") = eid;
	
	__asm__ __volatile__("ecall"
		: "=r"(a0), "=r"(a1)
		: "r"(a0),"r"(a1),"r"(a2),"r"(a3),"r"(a4),"r"(a5), "r"(a6), "r"(a7)
		:"memory"
	);
	
	return (sbiret) { .error = a0, .value = a1};
}



void putchar(char ch){
	// il valore 1 in eid identifica il servizio di OpenSBI che vogliamo sia eseguito.
	// consiste nell'analogo dei system call number
	sbi_call(ch, 0,0,0,0,0,0,1);
}



long getchar(void){
	sbiret ret = sbi_call(0,0,0,0,0,0,0,2);
	return ret.error;
}


// page table

void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags){
	if (!is_aligned(vaddr, PAGE_SIZE)){
		PANIC("unaligned vaddr %x", vaddr);
	}
	
	if (!is_aligned(paddr, PAGE_SIZE)){
		PANIC("unaligned vaddr %x", paddr);
	}
	
	uint32_t vpn1  = (vaddr >> 22) & 0x3ff;
	
	if ((table1[vpn1] & PAGE_V) == 0){
		// crea la entry della prima page table se questa non esiste.
		uint32_t pt_paddr = alloc_pages(1);
		table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
	}
	
	// crea la page table di secondo entry per mappare la pagina fisica
	uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
	uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
	table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}


// User mode
__attribute__((naked)) // IMPORTANT
void user_entry(void){
	__asm__ __volatile__(
		"csrw sepc, %[sepc]\n"
		"csrw sstatus, %[sstatus]\n"
		"sret \n"
		:
		: [sepc] "r" (USER_BASE),
			[sstatus] "r" (SSTATUS_SPIE)
	);
	
}


// prepara un nuovo processo per eseguire
struct process *create_process(const void *image, size_t image_size){
	// find an unused process control structure
	
	struct process *proc = NULL;
	int i = 0;
	for (i = 0; i < PROCS_MAX; i++){
		if  (procs[i].state == PROC_UNUSED) {
			proc = &procs[i];
			break;
		}
	}
	
	if (!proc){
		PANIC("no free process slots");
	}
	
	// creiamo un contesto fittizio
	uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
	*--sp = 0; // s11
	*--sp = 0; // s10
	*--sp = 0; // s9
	*--sp = 0; // s8
	*--sp = 0; // s7
	*--sp = 0; // s6
	*--sp = 0; // s5
	*--sp = 0; // s4
	*--sp = 0; // s3
	*--sp = 0; // s2
	*--sp = 0; // s1
	*--sp = 0; // s0
	*--sp = (uint32_t) user_entry;


	// allochiamo le pagine del kernel

	uint32_t * page_table = (uint32_t *) alloc_pages(1);
	for (paddr_t paddr = (paddr_t) __kernel_base;
		 paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE)
		map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
	 
	// Map delle pagine user

	for (uint32_t off = 0; off < image_size; off += PAGE_SIZE){
		paddr_t page = alloc_pages(1);

		// gestione del caso in cui i dati da copiare siano una quantità minore di una pagina

		size_t remaining = image_size - off;
		size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

		// riempiamo la pagina appena allocata con i dati

		memcpy((void *)page, image + off, copy_size);

		// mappiamo la pagina appena realizzata
		map_page(page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
	}



	 // inizializziamo i campi del nuovo processo
	proc->pid = i + 1;
	proc->state = PROC_RUNNABLE;
	proc->sp = (uint32_t) sp;
	proc->page_table = page_table;
	printf("processo creato\n");
	 
	return proc;
}

// scheduler
void yield(void){
	// Ricerca di un processo RUNNABLE
	
	struct process *next = idle_proc;
	
	for (int i = 0; i < PROCS_MAX; i++){
		struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
		if (proc->state == PROC_RUNNABLE && proc->pid > 0){
			next = proc;
			break;
		}
	}
	
	// se non ci sono processi RUNNABLE oltre il corrente, ritorna ad eseguire tale processo
	if (next == current_proc)
	return;
	

	__asm__ __volatile__(
		"sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"

		"csrw sscratch, %[sscratch]\n"
		:
		:[satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)), [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
	);

	// context switch
	struct process *prev = current_proc;
	current_proc = next;
	


	switch_context(&prev->sp, &next->sp);
}


// page allocator
paddr_t alloc_pages(uint32_t n){
	static paddr_t next_paddr = (paddr_t)__free_ram;
	paddr_t paddr = next_paddr;
	next_paddr += n * PAGE_SIZE;
	
	if (next_paddr > (paddr_t) __free_ram_end)
	PANIC("out of memory");
	
	memset((void*)paddr, 0, n * PAGE_SIZE);

	return paddr;
}


// implementazione del context switch per la gestione delle interruzioni

__attribute__((naked)) // per evitare che il compilatore modifichi le istruzioni per ottimizzarle
__attribute__((aligned(4))) // per assicurarsi che l'entry point della funzione sia un multiplo di 4
void kernel_entry(void){
	__asm__ __volatile__(
		"csrrw sp, sscratch, sp\n"


		"addi sp, sp, -4 * 31\n"
		
		"sw ra,  4 * 0(sp)\n"
		"sw gp,  4 * 1(sp)\n"
		"sw tp,  4 * 2(sp)\n"
		"sw t0,  4 * 3(sp)\n"
		"sw t1,  4 * 4(sp)\n"
		"sw t2,  4 * 5(sp)\n"
		"sw t3,  4 * 6(sp)\n"
		"sw t4,  4 * 7(sp)\n"
		"sw t5,  4 * 8(sp)\n"
		"sw t6,  4 * 9(sp)\n"
		"sw a0,  4 * 10(sp)\n"
		"sw a1,  4 * 11(sp)\n"
		"sw a2,  4 * 12(sp)\n"
		"sw a3,  4 * 13(sp)\n"
		"sw a4,  4 * 14(sp)\n"
		"sw a5,  4 * 15(sp)\n"
		"sw a6,  4 * 16(sp)\n"
		"sw a7,  4 * 17(sp)\n"
		"sw s0,  4 * 18(sp)\n"
		"sw s1,  4 * 19(sp)\n"
		"sw s2,  4 * 20(sp)\n"
		"sw s3,  4 * 21(sp)\n"
		"sw s4,  4 * 22(sp)\n"
		"sw s5,  4 * 23(sp)\n"
		"sw s6,  4 * 24(sp)\n"
		"sw s7,  4 * 25(sp)\n"
		"sw s8,  4 * 26(sp)\n"
		"sw s9,  4 * 27(sp)\n"
		"sw s10, 4 * 28(sp)\n"
		"sw s11, 4 * 29(sp)\n"
		
		"csrr a0, sscratch\n"
		"sw a0, 4 * 30(sp)\n"

		"addi a0, sp, 4 * 31\n"
		"csrw sscratch, a0\n"
		
		
		"mv a0, sp\n"
		"call handle_trap\n"
		
		"lw ra,  4 * 0(sp)\n"
		"lw gp,  4 * 1(sp)\n"
		"lw tp,  4 * 2(sp)\n"
		"lw t0,  4 * 3(sp)\n"
		"lw t1,  4 * 4(sp)\n"
		"lw t2,  4 * 5(sp)\n"
		"lw t3,  4 * 6(sp)\n"
		"lw t4,  4 * 7(sp)\n"
		"lw t5,  4 * 8(sp)\n"
		"lw t6,  4 * 9(sp)\n"
		"lw a0,  4 * 10(sp)\n"
		"lw a1,  4 * 11(sp)\n"
		"lw a2,  4 * 12(sp)\n"
		"lw a3,  4 * 13(sp)\n"
		"lw a4,  4 * 14(sp)\n"
		"lw a5,  4 * 15(sp)\n"
		"lw a6,  4 * 16(sp)\n"
		"lw a7,  4 * 17(sp)\n"
		"lw s0,  4 * 18(sp)\n"
		"lw s1,  4 * 19(sp)\n"
		"lw s2,  4 * 20(sp)\n"
		"lw s3,  4 * 21(sp)\n"
		"lw s4,  4 * 22(sp)\n"
		"lw s5,  4 * 23(sp)\n"
		"lw s6,  4 * 24(sp)\n"
		"lw s7,  4 * 25(sp)\n"
		"lw s8,  4 * 26(sp)\n"
		"lw s9,  4 * 27(sp)\n"
		"lw s10, 4 * 28(sp)\n"
		"lw s11, 4 * 29(sp)\n"
		"lw sp,  4 * 30(sp)\n"
		"sret\n"
	);
}



// context switch

__attribute__((naked))
void switch_context(uint32_t *prev_sp, uint32_t *next_sp){
	__asm__ __volatile__(
		// salvataggio del contesto
		// riserviamo lo spazio per 13 registri nello stack 
        "addi sp, sp, -13 * 4\n" // Allocate stack space for 13 4-byte registers
        "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers only
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"
		
		// cambiamo lo stack pointer
        "sw sp, (a0)\n"         // *prev_sp = sp;
        "lw sp, (a1)\n"         // Switch stack pointer (sp) here
        
		
		// carica del contesto
        "lw ra,  0  * 4(sp)\n"  // Restore callee-saved registers only
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"	// eliminiamo lo spazio che abbimo creato prima
		
        "ret\n"
		
	);
}


// legge il motivo per cui si è verificata l'eccezione e lo mostra a video
void handle_trap(struct trap_frame *f) {
	uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

	if (scause == SCAUSE_ECALL){
		handle_syscall(f);
		user_pc += 4;
	}else{
		PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
	}

	WRITE_CSR(sepc, user_pc);	
}

// syscall handler
void handle_syscall(struct trap_frame *f){
	switch (f->a3){
		case SYS_PUTCHAR:
			putchar(f->a0);
			break;
		case SYS_GETCHAR:
			while (1) {
				// polling
				long ch = getchar();
				if (ch >= 0){
					f->a0 = ch;
					break;
				}
				yield();
			}
			break;
		case SYS_EXIT:
			printf("process %d exited\n", current_proc->pid);
			current_proc->state = PROC_EXITED;
			yield();
			PANIC("unreachable");
		default:
			PANIC("unexpected syscall a3=%x\n", f->a3);
	}
}
