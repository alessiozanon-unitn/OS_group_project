build: obj/restaurant.o obj/cook.o obj/customer.o obj/waiter.o
	gcc -o restaurant obj/restaurant.o obj/cook.o obj/customer.o obj/waiter.o

clean:
	rm -f ./src/*.o #Remove object files
	rm -f ./restaurant #Remove executable

run: build
	./script/bootstrap.sh $(ARGS)

obj/restaurant.o: src/restaurant.c
	gcc -c src/restaurant.c -o obj/restaurant.o
obj/cook.o: src/cook.c
	gcc -c src/cook.c -o obj/cook.o
obj/customer.o: src/customer.c
	gcc -c src/customer.c -o obj/customer.o
obj/waiter.o: src/waiter.c
	gcc -c src/waiter.c -o obj/waiter.o
