#include "queue.c"
#include <stdbool.h>

int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 40; // frequency of emergency
float p = 0.2;               // probability of a ground job (launch & assembly)
int n = -1;                   //the second when we start outputing waiting jobs

void* LandingJob(void *arg); 
void* LaunchJob(void *arg);
void* EmergencyJob(void *arg); 
void* AssemblyJob(void *arg); 
void* ControlTower(void *arg);
void* padAJob(void *arg);
void* padBJob(void *arg); 
void* printWaiting(void *arg);	//to print waiting jobs in every seconds after nth second

pthread_mutex_t mutexA;		//to block changes to queue padA at the same time
pthread_mutex_t mutexB;		//to block changes to queue padB at the same time
pthread_mutex_t ID_mutex;		//to block changes to the global variable myID
pthread_mutex_t qLand;			//to block changes to the land queue
pthread_mutex_t qLaunch;		//to block changes to the launch queue
pthread_mutex_t qAssemb;		//to block changes to the assembly queue
pthread_mutex_t qEmerg;		//to block changes to the emergency queue
pthread_mutex_t doneJ;			//to block changes to the doneJobs queue

Queue *launch;				//the queue that launching jobs were first placed when they were created
Queue *land;				//the queue that landing jobs were first placed when they were created
Queue *assemble;			//the queue that assembly jobs were first placed when they were created
Queue *emergency;			//the queue that emergency jobs were first placed when they were created
Queue *padA;				//the queue of pad A
Queue *padB;				//the queue of pad B
Queue *doneJobs;			//the queue that jobs were placed when they were finished
struct timeval begin, end, jStart, jEnd;
int myID = 1;				//to hold IDs of jobs
double currentTime;
bool exceed_max_wait = false;		//to avoid starvation for landing jobs

//for part2
#define MAX_WAIT_TIME 10		//max waiting time for landing job
#define COUNT_LIMIT 3			//to avoid starvation for assembly and launching jobs


// pthread sleeper function
int pthread_sleep (int seconds)
{
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    struct timespec timetoexpire;
    if(pthread_mutex_init(&mutex,NULL))
    {
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL))
    {
        return -1;
    }
    struct timeval tp;
    //When to expire is an absolute time, so get the current time and add it to our delay time
    gettimeofday(&tp, NULL);
    timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;
    
    pthread_mutex_lock (&mutex);
    int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock (&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);
    
    //Upon successful completion, a value of zero shall be returned
    return res;
}

int main(int argc,char **argv){
    // -p (float) => sets p
    // -t (int) => simulation time in seconds
    // -s (int) => change the random seed
    for(int i=1; i<argc; i++){
        if(!strcmp(argv[i], "-p")) {p = atof(argv[++i]);}
        else if(!strcmp(argv[i], "-t")) {simulationTime = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-s"))  {seed = atoi(argv[++i]);}
        else if(!strcmp(argv[i], "-n"))  {n = atoi(argv[++i]);}
    }
    
    srand(seed); // feed the seed
    
    /* Queue usage example
        Queue *myQ = ConstructQueue(1000);
        Job j;
        j.ID = myID;
        j.type = 2;
        Enqueue(myQ, j);
        Job ret = Dequeue(myQ);
        DestructQueue(myQ);
    */

    // your code goes here
    launch = ConstructQueue(1000);
    land = ConstructQueue(1000);
    assemble = ConstructQueue(1000);
    emergency = ConstructQueue(1000);
    padA = ConstructQueue(1000);
    padB = ConstructQueue(1000);
    doneJobs = ConstructQueue(1000);
    
    pthread_mutex_init(&mutexA, NULL); 
    pthread_mutex_init(&mutexB, NULL); 
    pthread_mutex_init(&ID_mutex, NULL); 
    pthread_mutex_init(&qLand, NULL);
    pthread_mutex_init(&qLaunch, NULL);
    pthread_mutex_init(&qAssemb, NULL);
    pthread_mutex_init(&qEmerg, NULL);
    pthread_mutex_init(&doneJ, NULL);
    
    

    pthread_t landing;
    pthread_t launching;
    pthread_t assembly;
    pthread_t emrg;
    pthread_t printW;
    pthread_t control;
    pthread_t pA;
    pthread_t pB;
	
    //the first launching job that is created at time 0
    gettimeofday(&begin, NULL);
    Job j;
    j.ID = myID;
    j.type = 1;
    j.requestTime = begin.tv_sec;
    Enqueue(launch, j);
    myID++;
    
    
    pthread_create(&landing, NULL,&LandingJob , NULL);
    pthread_create(&launching, NULL,&LaunchJob , NULL);
    pthread_create(&assembly, NULL,&AssemblyJob , NULL);
    pthread_create(&emrg, NULL,&EmergencyJob , NULL);
    pthread_create(&printW, NULL,&printWaiting , NULL);
    pthread_create(&control, NULL,&ControlTower , NULL);
    pthread_create(&pA, NULL,&padAJob , NULL);
    pthread_create(&pB, NULL,&padBJob , NULL);
    
    
    pthread_join(landing, NULL);
    pthread_join(launching, NULL);
    pthread_join(assembly, NULL);
    pthread_join(emrg, NULL);
    pthread_join(control, NULL);
    pthread_join(pA, NULL);
    pthread_join(pB, NULL);

    //to output logs at the end of the program
    printf(" EventID       Status       Request Time       End Time       Turnaround Time       Pad\n");
    printf("----------------------------------------------------------------------------------------\n");
    while(!isEmpty(doneJobs)){
    	Job done = Dequeue(doneJobs);
    	printf("%5d%13c%17d%17d%18d%16c\n", done.ID, done.status, done.req, done.end, done.tA, done.pad);
    }
    DestructQueue(doneJobs);	

    return 0;
}

void* printWaiting(void *arg){
	while(currentTime<simulationTime){
		
		Queue *waitingLand;
		Queue *waitingLaunch;
		Queue *waitingAssembly;
		
		waitingLand = ConstructQueue(1000);
		waitingLaunch = ConstructQueue(1000);
		waitingAssembly = ConstructQueue(1000);
    	
    		if(currentTime >= n && n != -1){
    		
    			//get waiting jobs from padA queue and put them corresponding waiting job queues
    			pthread_mutex_lock(&mutexA);
    			if((padA->head) != NULL){	    			
	    			NODE *qp1 = padA->head;
	    			while(qp1->prev != NULL){
	    				if((qp1->data).type == 1){
	    					Job j = qp1->data;
	    					Enqueue(waitingLaunch, j);
	    					
	    				}else if((qp1->data).type == 2){
	    					Job j = qp1->data;
	    					Enqueue(waitingLand, j);
	    					
	    				}else if((qp1->data).type == 3){
	    					Job j = qp1->data;
	    					Enqueue(waitingAssembly, j);
	    					
	    				}
	    				qp1 = qp1->prev;
	    			}
	    			if((qp1->data).type == 1){
	    				Job j = qp1->data;
	    				Enqueue(waitingLaunch, j);
	    					
	    			}else if((qp1->data).type == 2){
	    				Job j = qp1->data;
	    				Enqueue(waitingLand, j);
	    					
	    			}else if((qp1->data).type == 3){
	    				Job j = qp1->data;
	    				Enqueue(waitingAssembly, j);		
	    			}
	    		
	    		}
	    		pthread_mutex_unlock(&mutexA);
	    		
	    		//get waiting jobs from padB queue and put them corresponding waiting job queues
	    		pthread_mutex_lock(&mutexB);
	    		if((padB->head) != NULL){	    			
	    			NODE *qp1 = padB->head;
	    			while(qp1->prev != NULL){
	    				if((qp1->data).type == 1){
	    					Job j = qp1->data;
	    					Enqueue(waitingLaunch, j);
	    					
	    				}else if((qp1->data).type == 2){
	    					Job j = qp1->data;
	    					Enqueue(waitingLand, j);
	    					
	    				}else if((qp1->data).type == 3){
	    					Job j = qp1->data;
	    					Enqueue(waitingAssembly, j);
	    					
	    				}
	    				qp1 = qp1->prev;
	    			}
	    			if((qp1->data).type == 1){
	    				Job j = qp1->data;
	    				Enqueue(waitingLaunch, j);
	    					
	    			}else if((qp1->data).type == 2){
	    				Job j = qp1->data;
	    				Enqueue(waitingLand, j);
	    					
	    			}else if((qp1->data).type == 3){
	    				Job j = qp1->data;
	    				Enqueue(waitingAssembly, j);		
	    			}
	
	    		}
	    		pthread_mutex_unlock(&mutexB);
	    		
	    		printf("At %d sec landing : ",(int)currentTime);
	    		if((waitingLand->head) != NULL){
	    			while((waitingLand->head)->prev != NULL){
	    				printf("%d, ",((waitingLand->head)->data).ID);
	    				waitingLand->head = (waitingLand->head)->prev;
	    			}
	    			printf("%d\n",((waitingLand->head)->data).ID);
	    		}else{
	    			printf("\n");
	    		}	
	    		
	    		printf("At %d sec launch : ",(int)currentTime);
	    		if((waitingLaunch->head) != NULL){
	    			while((waitingLaunch->head)->prev != NULL){
	    				printf("%d, ",((waitingLaunch->head)->data).ID);
	    				waitingLaunch->head = (waitingLaunch->head)->prev;
	    			}
	    			printf("%d\n",((waitingLaunch->head)->data).ID);
	    		}else{
	    			printf("\n");
	    		}
	    		
	    		printf("At %d sec assembly : ",(int)currentTime);
	    		if((waitingAssembly->head) != NULL){
	    			while((waitingAssembly->head)->prev != NULL){
	    				printf("%d, ",((waitingAssembly->head)->data).ID);
	    				waitingAssembly->head = (waitingAssembly->head)->prev;
	    			}
	    			printf("%d\n\n",((waitingAssembly->head)->data).ID);
	    		}else{
	    			printf("\n\n");
	    		}
    		
    		}
    		pthread_sleep(1);	//to output waiting jobs in every seconds
    		gettimeofday(&end, NULL);
	    	currentTime = (end.tv_sec - begin.tv_sec);
	    	
	    	
    
    	}
    	pthread_exit(0);
}

// the function that creates plane threads for landing
void* LandingJob(void *arg){

	while(currentTime<simulationTime){
    	
    		pthread_sleep(2);
    		pthread_mutex_lock(&ID_mutex);
    		pthread_mutex_lock(&qLand);
	   	int random = rand() % 10;
	    	if(random < (1-p) *10) {
	    		Job j;
        		j.ID = myID;
        		j.type = 2;
        		j.waitTime = 0;
        		gettimeofday(&jStart, NULL);
        		j.requestTime = jStart.tv_sec;
        		Enqueue(land, j);
        		myID++;
	    	}
	    	
	    	pthread_mutex_unlock(&ID_mutex);
	    	pthread_mutex_unlock(&qLand);
    		
    		gettimeofday(&end, NULL);
	    	currentTime = (end.tv_sec - begin.tv_sec);
	    	
	    	
    
    	}
    	
    	DestructQueue(land);	
    	pthread_exit(0);
}

// the function that creates plane threads for departure
void* LaunchJob(void *arg){
        
	while(currentTime<simulationTime){
    	
    		pthread_sleep(2);
    		pthread_mutex_lock(&ID_mutex);
    		pthread_mutex_lock(&qLaunch);
    		int random = rand() % 10;
	    	if(random < p *10) {
	    		Job j;
        		j.ID = myID;
        		j.type = 1;
        		j.waitTime = 0;
        		gettimeofday(&jStart, NULL);
        		j.requestTime = jStart.tv_sec;
        		Enqueue(launch, j);
        		myID++;
	    	}
	    	
	    	pthread_mutex_unlock(&ID_mutex);
	    	pthread_mutex_unlock(&qLaunch);
    		
    		gettimeofday(&end, NULL);
	    	currentTime = (end.tv_sec - begin.tv_sec);
    
    	}	
    	DestructQueue(launch);	
    	pthread_exit(0);
}

// the function that creates plane threads for emergency landing
void* EmergencyJob(void *arg){

	while(currentTime<simulationTime){
    		//every 80 seconds create 2 emergency jobs and put them into the emergency queue
    		if((((int)currentTime%80) == 0) && (currentTime != 0)){ 
	    		pthread_mutex_lock(&ID_mutex);
	    		pthread_mutex_lock(&qEmerg);
	    		
		    	
		    	Job j;
			j.ID = myID;
			j.type = 4;
			gettimeofday(&jStart, NULL);
			j.requestTime = jStart.tv_sec;
			Enqueue(emergency, j);
			myID++;
			
			Job j2;
			j2.ID = myID;
			j2.type = 4;
			gettimeofday(&jStart, NULL);
			j2.requestTime = jStart.tv_sec;
			Enqueue(emergency, j2);
			myID++;
			
			pthread_mutex_unlock(&ID_mutex);
			pthread_mutex_unlock(&qEmerg);
		    	
		    	
    		}
    		
    		pthread_sleep(1);
    		gettimeofday(&end, NULL);
	    	currentTime = (end.tv_sec - begin.tv_sec);
    
    	}
    	DestructQueue(emergency);	
    	pthread_exit(0);
}

// the function that creates plane threads for emergency landing
void* AssemblyJob(void *arg){

	while(currentTime<simulationTime){
    	
    		pthread_sleep(2);
    		pthread_mutex_lock(&ID_mutex);
    		pthread_mutex_lock(&qAssemb);
    		int random = rand() % 10;
	    	if(random < p *10) {
	    		Job j;
        		j.ID = myID;
        		j.type = 3;
        		gettimeofday(&jStart, NULL);
        		j.requestTime = jStart.tv_sec;
        		Enqueue(assemble, j);
        		myID++;
	    	}
	    	
	    	pthread_mutex_unlock(&ID_mutex);
	    	pthread_mutex_unlock(&qAssemb);
    		
    		gettimeofday(&end, NULL);
	    	currentTime = (end.tv_sec - begin.tv_sec);
    
    	}
    	DestructQueue(assemble);	
    	pthread_exit(0);
}

// the function that controls the air traffic
void* ControlTower(void *arg){

	while(currentTime<simulationTime){
	
		// to avoid starvation for landing jobs in part2
		pthread_mutex_lock(&qLand);
		if(!isEmpty(land)){
			int waitT = ((land->head)->data).waitTime;
			if(waitT >= MAX_WAIT_TIME){
				exceed_max_wait = true;
			}else{
				exceed_max_wait = false;
			}
		}
		pthread_mutex_unlock(&qLand);
		
		//give priority to emergency jobs
	   	if(!isEmpty(emergency)){
	   		pthread_mutex_lock(&qEmerg);
	   		Job job = Dequeue(emergency);
	   		Job job2 = Dequeue(emergency);
	   		pthread_mutex_unlock(&qEmerg);
	   		//add job to pad A or pad B queue
	   		if(isEmpty(padA)){
	   			pthread_mutex_lock(&mutexA);
	   			Enqueue(padA,job);
	   			pthread_mutex_unlock(&mutexA);
	   		}else if(isEmpty(padB)){
	   			pthread_mutex_lock(&mutexB);
	   			Enqueue(padB,job);
	   			pthread_mutex_unlock(&mutexB);
	   		}else{
				pthread_mutex_lock(&mutexA);
	   			NODE *ptr = (NODE*) malloc(sizeof (NODE));
	   			ptr->data = job;
	   			ptr->prev = padA->head;
	   			padA->head = ptr;
	   			padA->size++;
	   			pthread_mutex_unlock(&mutexA);
	   		}
	   		//add job2 to pad A or pad B queue
	   		if(isEmpty(padA)){
	   			pthread_mutex_lock(&mutexA);
	   			Enqueue(padA,job2);
	   			pthread_mutex_unlock(&mutexA);
	   		}else if(isEmpty(padB)){
	   			pthread_mutex_lock(&mutexB);
	   			Enqueue(padB,job2);
	   			pthread_mutex_unlock(&mutexB);
	   		}else{
	   			pthread_mutex_lock(&mutexA);
	   			NODE *ptr = (NODE*) malloc(sizeof (NODE));
	   			ptr->data = job2;
	   			ptr->prev = padA->head;
	   			padA->head = ptr;
	   			padA->size++;
	   			pthread_mutex_unlock(&mutexA);
	   		}
	   	//if max waiting time has past for landing job, give it a priority
	   	}else if(exceed_max_wait){
	   		pthread_mutex_lock(&qLand);
	   		Job job = Dequeue(land);
	   		pthread_mutex_unlock(&qLand);
	   		if((padA->size) < (padB->size)) {
	   			pthread_mutex_lock(&mutexA);
	   			Enqueue(padA,job);
	   			pthread_mutex_unlock(&mutexA);
	   		}else{
	   			pthread_mutex_lock(&mutexB);
	   			Enqueue(padB,job);
	   			pthread_mutex_unlock(&mutexB);
	   		}
	   		exceed_max_wait = false;
	   	//give priority to landing jobs until the land queue is empty or
	   	//one of the launch or assemble queues size exceeds 3.
	   	}else if(!isEmpty(land) && (launch->size < COUNT_LIMIT) && (assemble->size < COUNT_LIMIT)){
	   		pthread_mutex_lock(&qLand);
	   		Job job = Dequeue(land);
	   		pthread_mutex_unlock(&qLand);
	   		if((padA->size) < (padB->size)) {
	   			pthread_mutex_lock(&mutexA);
	   			Enqueue(padA,job);
	   			pthread_mutex_unlock(&mutexA);
	   		}else{
	   			pthread_mutex_lock(&mutexB);
	   			Enqueue(padB,job);
	   			pthread_mutex_unlock(&mutexB);
	   		}
	   		
	   	}else{
	   		// to increase waiting times of landing jobs if they are not being dequeued
	   		//we used average waiting time because we do not know to which pad each jobs will be assigned
	   		if(!isEmpty(land)){
				pthread_mutex_lock(&qLand);
				NODE *ptr2 = (NODE*) malloc(sizeof (NODE));
				ptr2 = land->head;
				while(ptr2->prev != NULL){
					(ptr2->data).waitTime += 8; // (12+4)/2 which is average waiting time
					ptr2 = ptr2->prev;
				}
				(ptr2->data).waitTime += 8; // for the last one
				pthread_mutex_unlock(&qLand);
			}
	   		
			//put one launching job to padA
	   		if(!isEmpty(launch)){
	   			pthread_mutex_lock(&qLaunch);
	   			Job job = Dequeue(launch);
	   			pthread_mutex_unlock(&qLaunch);
	   			pthread_mutex_lock(&mutexA);
	   			Enqueue(padA,job);
	   			pthread_mutex_unlock(&mutexA);
	   		}
	   		//put one assembly job to padB
	   		if(!isEmpty(assemble)){
	   			pthread_mutex_lock(&qAssemb);
	   			Job job = Dequeue(assemble);
	   			pthread_mutex_unlock(&qAssemb);
	   			pthread_mutex_lock(&mutexB);
	   			Enqueue(padB,job);
	   			pthread_mutex_unlock(&mutexB);
	   		}
	   	}

	    	gettimeofday(&end, NULL);
	    	currentTime = (end.tv_sec - begin.tv_sec);
	    	
    
    	}
    	pthread_exit(0);

}

void* padAJob(void *arg){

	while(currentTime<simulationTime){
    		
    		if(!isEmpty(emergency)){
    			//do nothing, wait for emergency job to be assigned to this pad
    		}else if(!isEmpty(padA)){
	    		pthread_mutex_lock(&mutexA);
	    		Job jA = Dequeue(padA);	    		
        		int sleep_time = 0;
        		if(jA.type==1){
        			sleep_time = 4;
        		}else{
        			sleep_time = 2;
        		}
        		pthread_mutex_unlock(&mutexA);
        		pthread_sleep(sleep_time);
        		gettimeofday(&jEnd, NULL);
        		jA.endTime = jEnd.tv_sec;
	    		jA.req = jA.requestTime - begin.tv_sec;
	    		jA.end = jA.endTime - begin.tv_sec;
	    		jA.tA = jA.end - jA.req;
	    		jA.pad = 'A';
	    		if(jA.type == 1){
	    			jA.status = 'D';
	    		}else if(jA.type == 4){
	    			jA.status = 'E';
	    		}else{
	    			jA.status = 'L';
	    		}
	    		pthread_mutex_lock(&doneJ);
	    		Enqueue(doneJobs, jA);
	    		pthread_mutex_unlock(&doneJ);

	    	}
    		
    		gettimeofday(&end, NULL);
	    	currentTime = (end.tv_sec - begin.tv_sec);
    
    	}
    	
    	DestructQueue(padA);
    	pthread_exit(0);
}

void* padBJob(void *arg){

	while(currentTime<simulationTime){
    		
    		if(!isEmpty(emergency)){
    			//do nothing, wait for emergency job to be assigned to this pad
    		}else if(!isEmpty(padB)){
	    		pthread_mutex_lock(&mutexB);
	    		Job jB = Dequeue(padB);    		
        		int sleep_time = 0;
        		if(jB.type==3){
        			sleep_time = 12;
        		}else{
        			sleep_time = 2;
        		}
        		pthread_mutex_unlock(&mutexB);
        		pthread_sleep(sleep_time);
        		gettimeofday(&jEnd, NULL);
        		gettimeofday(&jEnd, NULL);
        		jB.endTime = jEnd.tv_sec;
	    		jB.req = jB.requestTime - begin.tv_sec;
	    		jB.end = jB.endTime - begin.tv_sec;
	    		jB.tA = jB.end - jB.req;
	    		jB.pad = 'B';
	    		if(jB.type == 2){
	    			jB.status = 'L';
	    		}else if(jB.type == 4){
	    			jB.status = 'E';
	    		}else{
	    			jB.status = 'A';
	    		}
	    		pthread_mutex_lock(&doneJ);
	    		Enqueue(doneJobs, jB);
	    		pthread_mutex_unlock(&doneJ);
	    		
	    	}
    		
    		gettimeofday(&end, NULL);
	    	currentTime = (end.tv_sec - begin.tv_sec);
    
    	}
    	
    	DestructQueue(padB);
    	pthread_exit(0);
}
