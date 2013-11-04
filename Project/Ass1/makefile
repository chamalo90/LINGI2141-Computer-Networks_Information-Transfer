server:
	gcc server.c -g -o server -pthread
 
client:
	gcc client.c -g -o client -pthread

tclient:
	./client ::1 32000  ./melissa.jpg
tserver:  
	./server 32000 ./melissa2.jpg
clean:
	rm -f client
	rm -f server
	rm -f melissa2.jpg

