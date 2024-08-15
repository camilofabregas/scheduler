/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_SCHED_H
#define JOS_KERN_SCHED_H
#ifndef JOS_KERNEL
#error "This is a JOS kernel header; user programs should not #include it"
#endif

#ifdef LOTTERY
#define SCHEDS_TO_BOOST 100
#define MAX_TICKETS 25
#define MIN_TICKETS 1
#define TICKET_BOOST 5
extern int total_tickets;
#endif

// This function does not return.
// void sched_yield(void) __attribute__((noreturn));
void sched_yield(void);

#endif  // !JOS_KERN_SCHED_H
