FLAGS = -pthread -lm -lc
COMMON_OBJ = obj/cook.o obj/customer.o obj/waiter.o obj/menu.o obj/kitchen.o obj/splitmix64.o obj/xoshiro256plusplus.o obj/utils.o

.PHONY: test

build: $(COMMON_OBJ) obj/restaurant.o
	gcc -o restaurant obj/restaurant.o $(COMMON_OBJ) $(FLAGS)

clean:
	rm -f ./obj/*.o #Remove object files
	rm -f ./restaurant #Remove executable
	rm -f ./test #Remove test executable
	rm -f /tmp/restaurant.pid #remove possible leftover pid file
run: build
	./script/bootstrap.sh $(ARGS)

test: $(COMMON_OBJ) obj/unity.o obj/test.o
	gcc -o test obj/test.o $(COMMON_OBJ) obj/unity.o $(FLAGS)
	./test

gdb: clean src/restaurant.c src/cook.c src/customer.c src/waiter.c src/menu.c src/kitchen.c src/utils.c src/splitmix64.c src/xoshiro256plusplus.c
	gcc -c $(FLAGS) -g src/restaurant.c -o obj/restaurant.o
	gcc -c $(FLAGS) -g src/cook.c -o obj/cook.o
	gcc -c $(FLAGS) -g src/customer.c -o obj/customer.o
	gcc -c $(FLAGS) -g src/waiter.c -o obj/waiter.o
	gcc -c $(FLAGS) -g src/menu.c -o obj/menu.o
	gcc -c $(FLAGS) -g src/kitchen.c -o obj/kitchen.o
	gcc -c $(FLAGS) -g src/utils.c -o obj/utils.o
	gcc -c $(FLAGS) -g src/splitmix64.c -o obj/splitmix64.o
	gcc -c $(FLAGS) -g src/xoshiro256plusplus.c -o obj/xoshiro256plusplus.o
	gcc -o $(FLAGS) -g obj/restaurant.o $(COMMON_OBJ) -o restaurant
	gdb restaurant

obj/restaurant.o: src/restaurant.c
	gcc -c -pthread src/restaurant.c -o obj/restaurant.o
obj/cook.o: src/cook.c
	gcc -c -pthread src/cook.c -o obj/cook.o
obj/customer.o: src/customer.c
	gcc -c -pthread src/customer.c -o obj/customer.o
obj/waiter.o: src/waiter.c
	gcc -c -pthread src/waiter.c -o obj/waiter.o
obj/menu.o: src/menu.c
	gcc -c -pthread src/menu.c -o obj/menu.o
obj/kitchen.o: src/kitchen.c
	gcc -c -pthread src/kitchen.c -o obj/kitchen.o
obj/utils.o: src/utils.c
	gcc -c -pthread src/utils.c -o obj/utils.o

# Testing
obj/unity.o: tests/unity/unity.c
	gcc -c -pthread tests/unity/unity.c -o obj/unity.o
obj/test.o: tests/test.c
	gcc -c -pthread -I tests/unity -I src tests/test.c -o obj/test.o

# Random number generators
obj/splitmix64.o: src/splitmix64.c
	gcc -c -pthread src/splitmix64.c -o obj/splitmix64.o
obj/xoshiro256plusplus.o: src/xoshiro256plusplus.c
	gcc -c -pthread src/xoshiro256plusplus.c -o obj/xoshiro256plusplus.o
