UNITY ?= ../../Unity

INCLUDES += -I$(UNITY)/src -I..

CFLAGS += -g -Wall $(INCLUDES)

UNITY_OBJ = $(UNITY)/src/unity.o

TESTS =

all: test

test: $(TESTS)

%_run: %_runner
	./$^

%_runner.c: %.c
	ruby $(UNITY)/auto/generate_test_runner.rb $^ $@

clean:
	rm -f *.o 
	rm -f *_runner *_runner.c

vpath %.c ..

.PHONY: clean test
