/* a fifo queue that never fills.
 * licensed under GNU Lesser Public License version 2.1 or later.
 * Copyright (C) 2005 Elwood C. Downey ecdowney@clearskyinstitute.com
 * includes standalone commandline test program, see below.
 */

#include <stdlib.h>
#include <string.h>

#include "fq.h"

/* generic fifo queue.
 * an FQ is a FIFO list of pointers to void, each called an "element".
 * elements are added at q[head]. there are (nq) elements in the list. the
 * element to be removed next is q[head-nq]. there are (head-nq) empty slots
 * at the front of the q array. there are (nmem-head) elements available at
 * the end. if the head reaches the end, existing enties are slid to the front
 * of the array and total memory is adjusted up or down as required.
 *
 * example:
 *   
 *   <-------------------- nmem = 17 --------------------------------->
 *   <-- head - nq = 6 --->   <-- nq = 4 -->  <---- nmem - head = 7 -->
 *  ---------------------------------------------------------------------
 *  |   |   |   |   |   |   | x | x | x | x |   |   |   |   |   |   |   |
 *  ---------------------------------------------------------------------
 *    0   1   2   3   4   5   6   7   8   9   ^ 
 *                                           head = 10
 *
 */
struct _FQ {
    void **q;				/* malloced array of (void *) */
    int nq;				/* number of elements on queue */
    int head;				/* index into q[] of next empty spot */
    int nmem;				/* number of total slots in q[] */
    int grow;				/* n elements to grow when out of room*/
};

/* default memory managers, override with setMemFuncsFQ() */
static void *(*mymalloc)(size_t size) = malloc;
static void *(*myrealloc)(void *ptr, size_t size) = realloc;
static void (*myfree)(void *ptr) = free;

static void chkFQ (FQ *q);

/* return pointer to a new FQ, or NULL if no more memory.
 * grow is an efficiency hint of the number of elements to grow when out of
 *   room, nothing terrible happens if it is wrong.
 */
FQ *
newFQ (int grow)
{
	FQ *q = (FQ *)(*mymalloc)(sizeof(FQ));
	memset (q, 0, sizeof(FQ));
	q->q = (*mymalloc) (1);		/* seed for realloc */
	q->grow = grow > 0 ? grow : 1;
	return (q);
}

/* delete a FQ no longer needed */
void
delFQ (FQ *q)
{
	(*myfree) (q->q);		/* guaranteed set in newFQ() */
	(*myfree) ((void *)q);
}

/* push an element onto the given FQ */
void
pushFQ (FQ *q, void *e)
{
	chkFQ (q);
	q->q[q->head++] = e;
	q->nq++;
}

/* pop and return the next element in the given FQ, or NULL if empty */
void *
popFQ (FQ *q)
{
	return (q->nq > 0 ? q->q[q->head - q->nq--] : NULL);
}

/* return next element in the given FQ leaving it on the q, or NULL if empty */
void *
peekFQ (FQ *q)
{
	return (q->nq > 0 ? q->q[q->head - q->nq] : NULL);
}

/* return the number of elements in the given FQ */
int
nFQ (FQ *q)
{
	return (q->nq);
}

/* install new version of malloc/realloc/free.
 * N.B. don't call after first use of any other FQ function
 */
void
setMemFuncsFQ (void *(*newmalloc)(size_t size),
           void *(*newrealloc)(void *ptr, size_t size),
	   void (*newfree)(void *ptr))
{
	mymalloc = newmalloc;
	myrealloc = newrealloc;
	myfree = newfree;
}

/* insure q can hold one more element */
static void
chkFQ (FQ *q)
{
	int infront;

	/* done if still room at end */
	if (q->nmem > q->head)
	    return;

	/* move list to front */
	infront = q->head - q->nq;
	memmove (q->q, &q->q[infront], q->nq * sizeof(void*));
	q->head -= infront;

	/* realloc to minimum number of grow-sized chunks required */
	q->nmem = q->grow*(q->head/q->grow+1);
	q->q = (*myrealloc) (q->q, q->nmem * sizeof(void*));
}

#if defined(TEST_FQ)

/* to build a stand-alone commandline test program:
 *   cc -DTEST_FQ -o fq fq.c
 * run ./fq to test push/pop/peek and watch the queue after each operation.
 * the queue test elements are char, please excuse the ugly casts.
 */

#include <stdio.h>

/* draw a simple graphical representation of the given FQ */
static void
prFQ (FQ *q)
{
	int i;

	/* print the q, empty slots print as '.' */
	for (i = 0; i < q->nmem; i++) {
	    if (i >= q->head - q->nq && i < q->head)
		printf ("%c", (char)(int)q->q[i]);
	    else
		printf (".");
	}

	/* add right-justified stats */
	printf ("%*s nmem = %2d head = %2d nq = %2d\n", 50-i, "", q->nmem,
							    q->head, q->nq);
}

int
main (int ac, char *av[])
{
	FQ *q = newFQ(8);
	int c, e = -1;
	void *p;

	printf ("Commands:\n");
	printf (" P  = push a letter a-z\n");
	printf (" p  = pop a letter\n");
	printf (" k  = peek into queue\n");

	while ((c = fgetc(stdin)) != EOF) {
	    switch (c) {
	    case 'P':
		pushFQ (q, (void*)('a'+(e=(e+1)%26)));
		prFQ(q);
		break;
	    case 'p':
		p = popFQ (q);
		if (p)
		    printf ("popped %c\n", (char)(int)p);
		else
		    printf ("popped empty q\n");
		prFQ(q);
		break;
	    case 'k':
		p = peekFQ (q);
		if (p)
		    printf ("peeked %c\n", (char)(int)p);
		else
		    printf ("peeked empty q\n");
		prFQ(q);
		break;
	    default:
		break;
	    }
	}

	return (0);
}
#endif /* TEST_FQ */

