#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

//Define a logging mechanism that is used both in the actual code, and in the test harness
#define logit(msg,...) printf(msg,__VA_ARGS__)

/**

  Mock the Arduino SDK

 */

typedef unsigned char byte;

#define true 1
#define false 0

const size_t SerialDeviceBufferSize = 4096;
const size_t SerialDeviceResultSize = 4096;

//Some totally generic serial device
class SerialDevice
{
public:
	const byte* buffer;
	size_t bufferCount;
	size_t index;
	byte result[SerialDeviceResultSize];
	size_t result_index;
	//Start with an empty buffer
	SerialDevice()
	{
		index = 0;
		buffer = NULL;
		bufferCount = 0;
		result_index = 0;
		for(int i=0; i<SerialDeviceResultSize; i++)
		{
			result[i] = 0;
		}
	}

	void dataSet(const byte* data, size_t len)
	{
		printf("\n");
		buffer = data;
		bufferCount = len;
		index = 0;
		result_index = 0;
	}

	void begin(int rate)
	{
		//printf("rate=%d\n", rate);
	}

	void write(byte b)
	{
		if(b & 0x80)
		{
			printf("\n");
		}
		printf("\x1b[31m!%02x \x1b[0m", b);
		assert(result_index < SerialDeviceResultSize);
		result[result_index] = b;
		result_index++;
	}

	byte read()
	{
		if( available() )
		{
			index++;
			if(buffer[index-1] & 0x80)
			{
				printf("\n");
			}
			printf("\x1b[32m?%02x \x1b[0m", buffer[index-1]);
			return buffer[index-1];

		}
		assert("reading while blocked" && 0);
		return 0;
	}

	bool available()
	{
		return ( index < bufferCount );
	}
} Serial;

#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0

void pinMode(int num, bool val)
{
	if(val == INPUT)
	{
		printf("%d<- ", num);
	}
	else
	{
		printf("%d-> ", num);
	}
}

void digitalWrite(int num, bool val)
{
		printf("p%d=%d ", num, val);
}

bool digitalRead(int num)
{
	bool ret = false;
	printf("%d==p%d ", ret, num);
	return ret;
}

/**
    Include the hardware code.
    This odd directory structing is designed to hide this suite from the Arduino toolkit, which will try to use the
    cpp file.
 */

#include "../octaveRounder.ino"

#define RUNTEST(nm) runTest( nm##in, sizeof(nm##in), nm##out, sizeof(nm##out), 100) 
#include "data/test1.h"
#include "data/test2.h"
#include "data/test3.h"
#include "data/test4.h"
#include "data/test5.h"
#include "data/test6.h"

/**
    Some Unit tests
 */
int runTest(const byte* data, size_t size, const byte* result, size_t result_size, size_t iterations) {
	Serial.dataSet(data, size);
	setup();
	for(int i=0; i<iterations; i++) {
		loop();
	}
	int totalVolume = 0;
	for(int i=0; i<midi_note_count; i++) {
		totalVolume += notes[i].sent_vol;
	}
	if(totalVolume != 0) {
		printf("stuck note!\n");
		exit(-1);
	}
	for(int i=0; i<result_size-1; i++) {
		if(result[i] != Serial.result[i]) {
			printf("byte %d is wrong!\n", i);
			exit(-1);
		}
	}
	//TODO: check against expected result - mock needs to track data result
	return 0;
}


/**
    Simulate running
 */
int main(int argc, char** argv, char** envp)
{
	RUNTEST(test1);
	RUNTEST(test2);
	RUNTEST(test3);
	RUNTEST(test4);
	RUNTEST(test5);
	RUNTEST(test6);
	return 0;
}

