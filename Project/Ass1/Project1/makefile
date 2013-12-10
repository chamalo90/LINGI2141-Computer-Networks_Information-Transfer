server:
	gcc server.c -g -o server -pthread
 
client:
	gcc client.c -g -o client -pthread

tclient:
	./client ::1 32000  ./network.jpg
tserver:  
	./server 32000 ./network2.jpg
clean:
	rm -f client
	rm -f server
	rm -f network2.jpg

