#include <stdio.h>
#include <stdlib.h>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

#define BIDIRECTIONAL 0 /* change to 1 if you're doing extra credit */
						/* and write a routine called B_output */

/* possible events: */
#define TIMER_INTERRUPT 0
#define FROM_LAYER5 1
#define FROM_LAYER3 2

#define OFF 0
#define ON 1
#define A 0
#define B 1

#define WINDOWSIZE 20
#define MSGSIZE 20
#define TIMEOUT 500
#define ACK "ACK"

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg
{
	char data[MSGSIZE];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt
{
	int seqnum;
	int acknum;
	int checksum;
	char payload[MSGSIZE];
};

// *******************************************************************************
// *******************************************************************************
// ************ Começo do código modificado
// *******************************************************************************
// *******************************************************************************

// Janela de envio, indicando o pacote a ser enviado e o próximo a ser enviado */
struct window
{
	struct pkt *packet;
	struct window *next;
};

// Auxiliares para controle de janela de A e B
struct window *A_baseWindow = NULL; // Base de envio de A
struct window *A_endWindow = NULL;	// Final de envio de A
struct window *B_baseWindow = NULL; // Base de envio de B
struct window *B_endWindow = NULL;	// Final de envio de B

// Último ACK recebido de A e B
struct pkt *A_last_ack = NULL;
struct pkt *B_last_ack = NULL;

// Auxiliar para contar o próximo seqnum esperado
int A_next_seqnum = 0;
int B_next_seqnum = 0;

// Retorna o próximo seqnum não usado
int get_next_seqnum(struct window *start)
{
	if (start == NULL)
		return 0;

	int seqnum = 0;

	while (start != NULL)
	{
		if (start->next == NULL)
		{
			seqnum = start->packet->seqnum + 1;
			break;
		}
		else
			start = start->next;
	}

	return seqnum;
}

// Calcula o checksum do pacote
int calc_checksum(struct pkt *tgt_pkt)
{
	int checksum = 0;
	checksum += tgt_pkt->seqnum;
	checksum += tgt_pkt->acknum;
	for (int i = 0; i < MSGSIZE; i++)
		checksum += tgt_pkt->payload[i];
	return checksum;
}

// Cria um novo pacote com base num seqnum e um payload
struct pkt *make_pkt(int seqnum, char data[])
{
	struct pkt *packet = (struct pkt *)malloc(sizeof(struct pkt));
	packet->seqnum = seqnum;
	packet->acknum = 0;

	// Copia o payload
	for (int i = 0; i < MSGSIZE; i++)
		packet->payload[i] = data[i];

	// calcula checksum
	packet->checksum = calc_checksum(packet);
	return packet;
}

// Envia um ACK de AorB do packet para o outro lado
void send_ack(int AorB, struct pkt *packet)
{
	int seqnum = packet->seqnum;
	char msg[MSGSIZE] = "ACK";

	// Build packet
	struct pkt *packet_ack = make_pkt(seqnum, msg);
	packet_ack->acknum = packet->seqnum;

	// Recalcula checksum com novos dados
	packet_ack->checksum = calc_checksum(packet_ack);

	// Envia
	tolayer3(AorB, *packet_ack);
}

// Envia um pacote de AorB para o outro lado, e cria um timeout
void send_pkt(int AorB, struct pkt *pkt_to_send)
{
	if (AorB == A)
		printf("[A] Pacote enviado.\n");
	else if (AorB == B)
		printf("[B] Pacote enviado.\n");
	tolayer3(AorB, *pkt_to_send);
	starttimer(AorB, TIMEOUT);
}

// Mensagem que veio de cima, envia para baixo...
// Recebe mensagem e envia um pacote para B
void A_output(struct msg message)
{
	printf("[A] Mensagem recebida.\n");
	int seqnum = get_next_seqnum(A_endWindow);

	struct pkt *packet = make_pkt(seqnum, message.data);
	struct window *newElement = (struct window *)malloc(sizeof(struct window));
	newElement->packet = packet;
	newElement->next = NULL;

	if (A_baseWindow == NULL) // Se for o primeiro pacote a ser enviado
	{
		A_baseWindow = newElement;
		A_endWindow = newElement;
		send_pkt(A, packet);
		return;
	}
	else // Se não, adiciona na fila
	{
		A_endWindow->next = newElement;
	}

	// Conta a quantidade de pacotes entre o começo e o final
	struct window *start = A_baseWindow,
				  *end = A_endWindow;
	int packets = 0;
	while (start != NULL && start->packet->seqnum <= end->packet->seqnum)
	{
		packets++;
		start = start->next;
	}

	// Verifica se é possível enviar mais pacotes
	while (packets <= WINDOWSIZE)
	{
		if (A_endWindow != NULL && A_endWindow->next != NULL)
		{
			// Envia pacote e define o final da janela como sendo o último enviado
			send_pkt(A, A_endWindow->next->packet);
			A_endWindow = A_endWindow->next;
		}
		else
			break;
	}
}

void B_output(struct msg message) /* need be completed only for extra credit */
{
	printf("[B] Mensagem recebida.\n");
}

// Pacote recebido da camada 3 para cima...
// Recebe um pacote e envia uma mensagem
void A_input(struct pkt packet)
{
	printf("[A] Pacote recebido. ");

	if (packet.seqnum != A_next_seqnum)
		return printf("(descartado)\n"); // Pacote é descartado, timeout de B irá disparar

	int local_checksum = calc_checksum(&packet);

	// Verifica o checksum
	if (local_checksum == packet.checksum) // Checksum válido
	{
		if (strncmp(packet.payload, ACK, strlen(ACK)) == 0) // Pacote é um ACK
		{
			printf("(ACK)\n");

			if (packet.acknum <= A_endWindow->packet->seqnum) // Verifica se o ACKNUM é válido
			{
				// Ajusta a base de envio da janela para o próximo pacote
				A_last_ack = &packet;
				A_baseWindow = A_baseWindow->next;
				stoptimer(A);
			}
			// Se o ACKNUM não for válido, é ignorado e o timeout vai disparar
		}
		else // Se não for um ACK
		{
			printf("(MSG)\n");

			// Envia mensagem para a camada de cima...
			stoptimer(A);
			tolayer5(A, packet.payload);
		}

		// Ajusta o próximo seqnum esperado
		A_next_seqnum = packet.seqnum + 1;
	}
	// Se o checksum for inválido, é ignorado e o timeout vai disparar
}

// Timeout de A
void A_timerinterrupt(void)
{
	printf("[A] Timeout. ");
	struct window *current_window;

	// Verifica se há pacotes que não receberam ACK
	if (A_last_ack == NULL || (A_last_ack->acknum <= A_endWindow->packet->seqnum))
	{
		printf("(Reenviando pacotes)\n");
		current_window = A_baseWindow;
		while (current_window != NULL)
		{
			send_pkt(A, current_window->packet);
			current_window = current_window->next;
		}
	}
	else
		printf("\n");
}

// Inicializa o A
void A_init(void)
{
	A_baseWindow = NULL;
	A_endWindow = NULL;
	A_last_ack = NULL;
	A_next_seqnum = 0;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

// Pacote recebido da camada 3 que vai para cima...
// Recebe um pacote e envia uma mensagem
void B_input(struct pkt packet)
{
	printf("[B] Pacote recebido. ");

	if (packet.seqnum != B_next_seqnum)
		return printf("(descartado)\n"); // Pacote é descartado, timeout de A irá disparar

	// Verifica checksum do pacote
	int local_checksum = calc_checksum(&packet);

	// Verifica o checksum
	if (local_checksum == packet.checksum) // Checksum válido
	{
		if (strncmp(packet.payload, ACK, strlen(ACK)) == 0) // Pacote é um ACK (não usado aqui)
		{
			printf("(ACK)\n");

			if (packet.acknum <= B_endWindow->packet->seqnum) // Verifica se o ACKNUM é válido
			{
				// Ajuda a base de envio da janela para o próximo pacote
				B_last_ack = &packet;
				B_baseWindow = B_baseWindow->next;
			}
			// Se o ACKNUM não for válido, é ignorado
		}
		else // Se não for um ACK
		{
			printf("(MSG)\n");

			// Envia mensagem para a camada de cima e envia um ACK para outro lado...
			send_ack(B, &packet);
			tolayer5(B, packet.payload);
		}

		// Ajusta o próximo seqnum esperado
		B_next_seqnum = packet.seqnum + 1;
	}
}

// Timeout de B
void B_timerinterrupt(void)
{
}

// Inicializa B
void B_init(void)
{
	B_baseWindow = NULL;
	B_endWindow = NULL;
	B_last_ack = NULL;
	B_next_seqnum = 0;
}

// *******************************************************************************
// *******************************************************************************
// ************ Final do código modificado
// *******************************************************************************
// *******************************************************************************

/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event
{
	float evtime;		/* event time */
	int evtype;			/* event type code */
	int eventity;		/* entity where event occurs */
	struct pkt *pktptr; /* ptr to packet (if any) assoc w/ this event */
	struct event *prev;
	struct event *next;
};
struct event *evlist = NULL; /* the event list */

// initialize globals
int TRACE = 1;	 /* for my debugging */
int nsim = 0;	 /* number of messages from 5 to 4 so far */
int nsimmax = 0; /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;	   /* probability that a packet is dropped  */
float corruptprob; /* probability that one bit is packet is flipped */
float lambda;	   /* arrival rate of messages from layer 5 */
int ntolayer3;	   /* number sent into layer 3 */
int nlost;		   /* number lost in media */
int ncorrupt;	   /* number corrupted by media*/

int main(int argc, char *argv[])
{
	struct event *eventptr;
	struct msg msg2give;
	struct pkt pkt2give;

	int i, j;
	//   char c;

	init();
	A_init();
	B_init();

	while (1)
	{
		eventptr = evlist; /* get next event to simulate */
		if (eventptr == NULL)
			goto terminate;
		evlist = evlist->next; /* remove this event from event list */
		if (evlist != NULL)
			evlist->prev = NULL;
		if (TRACE >= 2)
		{
			printf("\nEVENT time: %f,", eventptr->evtime);
			printf("  type: %d", eventptr->evtype);
			if (eventptr->evtype == 0)
				printf(", timerinterrupt  ");
			else if (eventptr->evtype == 1)
				printf(", fromlayer5 ");
			else
				printf(", fromlayer3 ");
			printf(" entity: %d\n", eventptr->eventity);
		}
		time = eventptr->evtime; /* update time to next event time */
		if (nsim == nsimmax)
			break; /* all done with simulation */
		if (eventptr->evtype == FROM_LAYER5)
		{
			generate_next_arrival(); /* set up future arrival */
			/* fill in msg to give with string of same letter */
			j = nsim % 26;
			for (i = 0; i < 20; i++)
				msg2give.data[i] = 97 + j;
			if (TRACE > 2)
			{
				printf("          MAINLOOP: data given to student: ");
				for (i = 0; i < 20; i++)
					printf("%c", msg2give.data[i]);
				printf("\n");
			}
			nsim++;
			if (eventptr->eventity == A)
				A_output(msg2give);
			else
				B_output(msg2give);
		}
		else if (eventptr->evtype == FROM_LAYER3)
		{
			pkt2give.seqnum = eventptr->pktptr->seqnum;
			pkt2give.acknum = eventptr->pktptr->acknum;
			pkt2give.checksum = eventptr->pktptr->checksum;
			for (i = 0; i < 20; i++)
				pkt2give.payload[i] = eventptr->pktptr->payload[i];
			if (eventptr->eventity == A) /* deliver packet by calling */
				A_input(pkt2give);		 /* appropriate entity */
			else
				B_input(pkt2give);
			free(eventptr->pktptr); /* free the memory for packet */
		}
		else if (eventptr->evtype == TIMER_INTERRUPT)
		{
			if (eventptr->eventity == A)
				A_timerinterrupt();
			else
				B_timerinterrupt();
		}
		else
		{
			printf("INTERNAL PANIC: unknown event type \n");
		}
		free(eventptr);
	}

terminate:
	printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n", time, nsim);
	return 0;
}

init() /* initialize the simulator */
{
	int i;
	float sum, avg;
	float jimsrand();

	printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
	printf("Enter the number of messages to simulate: ");
	scanf("%d", &nsimmax);
	printf("Enter  packet loss probability [enter 0.0 for no loss]:");
	scanf("%f", &lossprob);
	printf("Enter packet corruption probability [0.0 for no corruption]:");
	scanf("%f", &corruptprob);
	printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
	scanf("%f", &lambda);
	printf("Enter TRACE:");
	scanf("%d", &TRACE);

	srand(9999); /* init random number generator */
	sum = 0.0;	 /* test random number generator for students */
	for (i = 0; i < 1000; i++)
		sum = sum + jimsrand(); /* jimsrand() should be uniform in [0,1] */
	avg = sum / 1000.0;
	if (avg < 0.25 || avg > 0.75)
	{
		printf("It is likely that random number generation on your machine\n");
		printf("is different from what this emulator expects.  Please take\n");
		printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
		exit(1);
	}

	ntolayer3 = 0;
	nlost = 0;
	ncorrupt = 0;

	time = 0.0;				 /* initialize time to 0.0 */
	generate_next_arrival(); /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand(void)
{
	double mmm = 2147483647;   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
	float x;				   /* individual students may need to change mmm */
	x = (float)(rand() / mmm); /* x should be uniform in [0,1] */
	return (x);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

void generate_next_arrival(void)
{
	double x, log(), ceil();
	struct event *evptr;
	char *p = malloc(1);
	//   float ttime;
	//   int tempint;

	if (TRACE > 2)
		printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

	x = lambda * jimsrand() * 2; /* x is uniform on [0,2*lambda] */
								 /* having mean of lambda        */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtime = (float)(time + x);
	evptr->evtype = FROM_LAYER5;
	if (BIDIRECTIONAL && (jimsrand() > 0.5))
		evptr->eventity = B;
	else
		evptr->eventity = A;
	insertevent(evptr);
}

void insertevent(struct event *p)
{
	struct event *q, *qold;

	if (TRACE > 2)
	{
		printf("            INSERTEVENT: time is %lf\n", time);
		printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
	}
	q = evlist; /* q points to header of list in which p struct inserted */
	if (q == NULL)
	{ /* list is empty */
		evlist = p;
		p->next = NULL;
		p->prev = NULL;
	}
	else
	{
		for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next)
			qold = q;
		if (q == NULL)
		{ /* end of list */
			qold->next = p;
			p->prev = qold;
			p->next = NULL;
		}
		else if (q == evlist)
		{ /* front of list */
			p->next = evlist;
			p->prev = NULL;
			p->next->prev = p;
			evlist = p;
		}
		else
		{ /* middle of list */
			p->next = q;
			p->prev = q->prev;
			q->prev->next = p;
			q->prev = p;
		}
	}
}

void printevlist(void)
{
	struct event *q;
	//  int i;
	printf("--------------\nEvent List Follows:\n");
	for (q = evlist; q != NULL; q = q->next)
	{
		printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype, q->eventity);
	}
	printf("--------------\n");
}

/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB)
/* A or B is trying to stop timer */
{
	struct event *q; //,*qold;

	if (TRACE > 2)
		printf("          STOP TIMER: stopping timer at %f\n", time);
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
	for (q = evlist; q != NULL; q = q->next)
		if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
		{
			/* remove this event */
			if (q->next == NULL && q->prev == NULL)
				evlist = NULL;		  /* remove first and only event on list */
			else if (q->next == NULL) /* end of list - there is one in front */
				q->prev->next = NULL;
			else if (q == evlist)
			{ /* front of list - there must be event after */
				q->next->prev = NULL;
				evlist = q->next;
			}
			else
			{ /* middle of list */
				q->next->prev = q->prev;
				q->prev->next = q->next;
			}
			free(q);
			return;
		}
	printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

void starttimer(int AorB, float increment)
/* A or B is trying to stop timer */

{

	struct event *q;
	struct event *evptr;
	char *p = malloc(1);

	if (TRACE > 2)
		printf("          START TIMER: starting timer at %f\n", time);
	/* be nice: check to see if timer is already started, if so, then  warn */
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
	for (q = evlist; q != NULL; q = q->next)
		if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB))
		{
			printf("Warning: attempt to start a timer that is already started\n");
			return;
		}

	/* create future event for when timer goes off */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtime = time + increment;
	evptr->evtype = TIMER_INTERRUPT;
	evptr->eventity = AorB;
	insertevent(evptr);
}

/************************** TOLAYER3 ***************/
tolayer3(AorB, packet) int AorB; /* A or B is trying to stop timer */
struct pkt packet;
{
	struct pkt *mypktptr;
	struct event *evptr, *q;
	//  char *malloc();
	float lastime, x, jimsrand();
	int i;

	ntolayer3++;

	/* simulate losses: */
	if (jimsrand() < lossprob)
	{
		nlost++;
		if (TRACE > 0)
			printf("          TOLAYER3: packet being lost\n");
		return;
	}

	/* make a copy of the packet student just gave me since he/she may decide */
	/* to do something with the packet after we return back to him/her */
	mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
	mypktptr->seqnum = packet.seqnum;
	mypktptr->acknum = packet.acknum;
	mypktptr->checksum = packet.checksum;
	for (i = 0; i < 20; i++)
		mypktptr->payload[i] = packet.payload[i];
	if (TRACE > 2)
	{
		printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
			   mypktptr->acknum, mypktptr->checksum);
		for (i = 0; i < 20; i++)
			printf("%c", mypktptr->payload[i]);
		printf("\n");
	}

	/* create future event for arrival of packet at the other side */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtype = FROM_LAYER3;	  /* packet will pop out from layer3 */
	evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
	evptr->pktptr = mypktptr;		  /* save ptr to my copy of packet */
									  /* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
	lastime = time;
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
	for (q = evlist; q != NULL; q = q->next)
		if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity))
			lastime = q->evtime;
	evptr->evtime = lastime + 1 + 9 * jimsrand();

	/* simulate corruption: */
	if (jimsrand() < corruptprob)
	{
		ncorrupt++;
		if ((x = jimsrand()) < .75)
			mypktptr->payload[0] = 'Z'; /* corrupt payload */
		else if (x < .875)
			mypktptr->seqnum = 999999;
		else
			mypktptr->acknum = 999999;
		if (TRACE > 0)
			printf("          TOLAYER3: packet being corrupted\n");
	}

	if (TRACE > 2)
		printf("          TOLAYER3: scheduling arrival on other side\n");
	insertevent(evptr);
}

tolayer5(AorB, datasent) int AorB;
char datasent[20];
{
	int i;
	if (TRACE > 2)
	{
		printf("          TOLAYER5: data received: ");
		for (i = 0; i < 20; i++)
			printf("%c", datasent[i]);
		printf("\n");
	}
}