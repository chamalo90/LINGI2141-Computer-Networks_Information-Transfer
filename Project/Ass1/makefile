server:
	gcc server.c -g -o server -pthread
 
client:
	gcc client.c -g -o client -pthread

tclient:
	./client ::1 32000
tserver: 
	./server 32000

