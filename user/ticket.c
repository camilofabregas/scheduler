#include <inc/lib.h>

#define NUM_ITERATIONS 10

void
tickets_process(void)
{
	int i, j;
	envid_t env_id = sys_getenvid();

	// Print the environment ID
	cprintf("Tickets Process (ID: %08x) starting...\n", env_id);

	// Perform a series of computations
	for (i = 0; i < NUM_ITERATIONS; i++) {
		for (j = 0; j < 10000000; j++) {
			// Perform some dummy computations
			int temp = (j + 1) * (i + 1);
			temp = temp / (j + 1);
		}
	}

	// Print a message to indicate the end of the process
	cprintf("Tickets Process (ID: %08x) completed!\n", env_id);
}


void
umain(int argc, char **argv)
{
	envid_t ticket_process_1, ticket_process_2;

	// Fork the high tickets process
	ticket_process_1 = fork();
	if (ticket_process_1 < 0)
		panic("umain: fork for first tickets process failed");

	if (ticket_process_1 == 0) {
		tickets_process();
		return;
	}

	// Fork the low tickets process
	ticket_process_2 = fork();
	if (ticket_process_2 < 0)
		panic("umain: fork for second tickets process failed");

	if (ticket_process_2 == 0) {
		tickets_process();
		return;
	}

	// Wait for both processes to complete
	// Wait for the first to finish forking
	while (envs[ENVX(ticket_process_1)].env_status != ENV_FREE)
		asm volatile("pause");
	// Wait for the second to finish forking
	while (envs[ENVX(ticket_process_2)].env_status != ENV_FREE)
		asm volatile("pause");

	// Print a message to indicate the end of the program
	cprintf("Process completed!\n");
}
