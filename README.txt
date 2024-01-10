COMP 304 - Project 2
Meryem Karakaş    69074
Mehmet Çuhadar    68835

compile with: 
	gcc -o p2 project_2.c -lpthread
	
to run the program with desired argumens, you can run with:
	./p2 -n 30 -p 0.2 -t 120 -s 10
	
	
All parts of our code is working.
For all of the parts, we commented on our code when it's required.
We used counter for launching and assembly jobs to prevent starvation. The priority is given to the landing jobs until size of the waiting landing and lunching jobs reach 3. Also, to prevent starvation for landing jobs, we assigned max_wait_time for landing jobs, we assigned it to 10. Whenever control tower puts launching or assembly jobs to the pad A or pad B queue, we increased waitTime for the landing jobs in land queue. Therefore, when waitTime of the job in land queue exceeds max_wait_time, it is given priority to be put on pad A or pad B queue. We added average of the launching and landing execution times ((12+4)/2) to waitTime of jobs because we do not know which pad will be chosen to give the next job, so if the landing job will be put the pad A, previous job is launching, the landing job will wait extra 4 seconds. If the landing job will be put the pad B, previous job is assembly, the landing job will wait extra 12 seconds. We took the average to increase waitTime.
