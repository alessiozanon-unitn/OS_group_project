build: obj/restaurant.o obj/cook.o obj/customer.o obj/waiter.o obj/menu.o obj/kitchen.o obj/splitmix64.o obj/xoshiro256plusplus.o obj/utils.o
	gcc -o restaurant obj/restaurant.o obj/cook.o obj/customer.o obj/waiter.o obj/menu.o obj/kitchen.o obj/splitmix64.o obj/xoshiro256plusplus.o obj/utils.o -lm

clean:
	rm -f ./obj/*.o #Remove object files
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
obj/menu.o: src/menu.c
	gcc -c src/menu.c -o obj/menu.o
obj/kitchen.o: src/kitchen.c
	gcc -c src/kitchen.c -o obj/kitchen.o
obj/utils.o: src/utils.c
	gcc -c src/utils.c -o obj/utils.o


obj/splitmix64.o: src/splitmix64.c
	gcc -c src/splitmix64.c -o obj/splitmix64.o
obj/xoshiro256plusplus.o: src/xoshiro256plusplus.c
	gcc -c src/xoshiro256plusplus.c -o obj/xoshiro256plusplus.o
