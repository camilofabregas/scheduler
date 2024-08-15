/*
 * Your code here for context_switch
 *
 * It should restore the register values in the Trapframe with the 'iret' instruction.
 * This exits the kernel and starts executing some environment's code.
 *
 * This function does not return.
 */

.globl context_switch;
context_switch:
	movl 4(%esp), %esp /* Move stack pointer into TrapFrame */
	
	popal /* Pop general purpose registers */
	pop %es
	pop	%ds
	
	add	$4, %esp /* Move stack pointer to ignore trapno */
	add	$4, %esp /* Move stack pointer to ignore err */
	
	iret /* Interrupt return, modifies eip, cs, esp */

spin:
	jmp spin
