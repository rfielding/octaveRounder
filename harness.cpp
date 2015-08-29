#include <stdio.h>
#include <stdlib.h>
//Define a logging mechanism that is used both in the actual code, and in the test harness
#define logit(msg) printf(msg)

/**

  Mock the Arduino SDK

 */

typedef unsigned char byte;

#define true 1
#define false 0

const size_t SerialDeviceBufferSize = 4096;

//Some totally generic serial device
class SerialDevice
{
	const byte* buffer;
	size_t bufferCount;
	size_t index;
public:
	//Start with an empty buffer
	SerialDevice()
	{
		index = 0;
		buffer = NULL;
		bufferCount = 0;
	}

	void dataSet(const byte* data, size_t len)
	{
		printf("set data\n");
		buffer = data;
		bufferCount = len;
		index = 0;
	}

	void begin(int rate)
	{
		printf("rate=%d\n", rate);
	}

	void write(byte b)
	{
		if(b & 0x80)
		{
			printf("\n");
		}
		printf("!%02x ", b);
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
			printf("?%02x ", buffer[index-1]);
			return buffer[index-1];

		}
		logit("!!blocked on read!!\n");
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
    Include the hardware code
 */

#include "octaveRounder.ino"


const byte test1data[12] = {
  0x90, 0x61, 0x7f,
  0x90, 0x63, 0x7f,
  0x90, 0x61, 0x00,
  0x90, 0x63, 0x00
};

int runTest(const byte* data, size_t size, size_t iterations);

/**
    Some Unit tests
 */
int runTest(const byte* data, size_t size, size_t iterations)
{
	Serial.dataSet(data, size);
	setup();
	for(int i=0; i<iterations; i++)
	{
		loop();
	}
	return 0;
}


/**
    Simulate running
 */
int main(int argc, char** argv, char** envp)
{
	runTest( test1data, 12, 100);
	return 0;
}

