#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/lapic.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/sched.h>

void sched_halt(void);

// Choose a user environment to run and run it.
#ifdef ROUND_ROBIN
void
sched_yield(void)
{
	struct Env *idle;
	struct Env *found = NULL;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	idle = curenv ? curenv + 1 : envs;
	int env_index = ENVX(idle->env_id);

	for (int i = env_index; i < NENV + env_index; i++) {
		idle = &envs[i % NENV];
		if (idle->env_status == ENV_RUNNABLE) {
			found = idle;
			break;
		}
	}

	if (!found && curenv && curenv->env_status == ENV_RUNNING) {
		found = curenv;
	}

	if (found) {
		env_run(found);
	}

	// sched_halt never returns
	sched_halt();
}
#endif

#ifdef LOTTERY

int total_tickets = 0;
int scheds_counter = 0;
#define MAX_HISTORY_OF_EXECUTIONS 1000
envid_t history_of_executions[MAX_HISTORY_OF_EXECUTIONS];
int history_of_executions_counter = 0;

// Park-Miller psuedo-random number generator
// Source: https://www.cs.virginia.edu/~cr4bd/4414/S2019/lottery.html
// from Wikipedia’s page on Lehmer random number generators.
// Source: https://en.wikipedia.org/wiki/Lehmer_random_number_generator
static unsigned random_seed = 23;  // No me deja usar get_lapic_tcc() como seed
#define RANDOM_MAX ((1u << 31u) - 1u)

unsigned
lcg_parkmiller(unsigned *state)
{
	const unsigned N = 0x7fffffff;
	const unsigned G = 48271u;

	/*
	Indirectly compute state*G%N.

	Let:
	div = state/(N/G)
	rem = state%(N/G)

	Then:
	rem + div*(N/G) == state
	rem*G + div*(N/G)*G == state*G

	Now:
	div*(N/G)*G == div*(N - N%G) === -div*(N%G)  (mod N)

	Therefore:
	rem*G - div*(N%G) === state*G  (mod N)

	Add N if necessary so that the result is between 1 and N-1.
	*/

	unsigned div =
	        *state / (N / G); /* max : 2,147,483,646 / 44,488 = 48,271 */
	unsigned rem =
	        *state % (N / G); /* max : 2,147,483,646 % 44,488 = 44,487 */

	unsigned a = rem * G;       /* max : 44,487 * 48,271 = 2,147,431,977 */
	unsigned b = div * (N % G); /* max : 48,271 * 3,399 = 164,073,129 */

	return *state = (a > b) ? (a - b) : (a + (N - b));
}

unsigned
next_random()
{
	return lcg_parkmiller(&random_seed);
}

// Este es el boosting que pensamos inicialmente,
// en el que todos los envs reciben la maxima prioridad
// al aumentar su cantidad de tickets.
/*void
boosting()
{
        scheds_counter = 0;
        total_tickets = 0;
        for (int i = 0; i < NENV; i++) {
                envs[i].env_tickets = MAX_TICKETS;
                total_tickets += MAX_TICKETS;
        }
}*/

// Con este boosting deberia pasar la prueba de primes,
// aunque no es un boosting correcto.
void
boosting()
{
	scheds_counter = 0;
	total_tickets = 0;
	for (int i = 0; i < NENV; i++) {
		total_tickets += TICKET_BOOST;
		envs[i].env_tickets += TICKET_BOOST;
	}
}

struct Env *
find_lottery_winner()
{
	struct Env *idle;
	struct Env *found = NULL;

	int ticket_counter = 0;

	unsigned winning_ticket = next_random() % total_tickets + 1;

	idle = curenv ? curenv + 1 : envs;
	int env_index = ENVX(idle->env_id);

	for (int i = env_index; i < NENV + env_index; i++) {
		idle = &envs[i % NENV];
		ticket_counter += idle->env_tickets;
		if (ticket_counter > winning_ticket &&
		    idle->env_status == ENV_RUNNABLE) {
			found = idle;
			if (!found && !curenv) {
				continue;
			} else {
				break;
			}
		}
	}
	return found;
}

void
sched_yield(void)
{
	scheds_counter += 1;

	if (scheds_counter >= SCHEDS_TO_BOOST) {
		boosting();
	}

	struct Env *found = find_lottery_winner();

	if (!found && !curenv) {
		found = find_lottery_winner();
	}

	if (!found && curenv && curenv->env_status == ENV_RUNNING) {
		found = curenv;
	}

	if (found) {
		found->execution_count++;
		found->start_time = get_lapic_tcc();
		history_of_executions[history_of_executions_counter] =
		        found->env_id;
		history_of_executions_counter = history_of_executions_counter + 1;
		if (found->env_tickets > MIN_TICKETS) {
			found->env_tickets -= 1;
			total_tickets -= 1;
		}
		env_run(found);
	}

	// sched_halt never returns
	sched_halt();
}

#endif

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
#ifdef LOTTERY
		cprintf("Scheduler Statistics:\n");
		cprintf("Number of scheduler invocations: %d\n", scheds_counter);

		// Print execution statistics for each process
		for (int i = 0; i < NENV; i++) {
			if (envs[i].execution_count > 0) {
				cprintf("Process %d:\n", envs[i].env_id);
				cprintf("  Execution count: %d\n",
				        envs[i].execution_count);
				cprintf("  Start time: %u\n", envs[i].start_time);
				cprintf("  Tickets: %u\n", envs[i].env_tickets);
			}
		}

		cprintf("Executions history:");
		cprintf("[");
		for (int i = 0; i < history_of_executions_counter; i++) {
			cprintf("%d", history_of_executions[i]);
			if (i < history_of_executions_counter - 1) {
				cprintf(", ");
			}
		}
		cprintf("]\n");
#endif
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile("movl $0, %%ebp\n"
	             "movl %0, %%esp\n"
	             "pushl $0\n"
	             "pushl $0\n"
	             "sti\n"
	             "1:\n"
	             "hlt\n"
	             "jmp 1b\n"
	             :
	             : "a"(thiscpu->cpu_ts.ts_esp0));
}
