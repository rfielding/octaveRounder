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
		printf("\n");
		buffer = data;
		bufferCount = len;
		index = 0;
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
		//logit("!!blocked on read!! %d\n", index);
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


const byte test1data[] = {
  0x90, 0x31, 0x7f,
  0xE0, 0x05, 0x40,
  0x90, 0x63, 0x7f,
  0x90, 0x63, 0x00,
  0x90, 0x63, 0x7f,
  0x90, 0x63, 0x00,
  0x80, 0x31, 0x64,
};

const byte test2data[] = {
  0x90, 0x40, 0x7F,
  0x90, 0x40, 0x00,
  0x90, 0x43, 0x7F,
  0x90, 0x43, 0x00,
  0x90, 0x47, 0x7F,
  0x90, 0x47, 0x00,
  0x90, 0x40, 0x7F,
  0x90, 0x40, 0x00,
};

const byte test3data[] = {
  0x90, 0x40, 0x7F,
  0x90, 0x40, 0x00,
  0x90, 0x47, 0x7F,
  0x90, 0x47, 0x00,
  0x90, 0x43, 0x7F,
  0x90, 0x43, 0x00,
  0x90, 0x40, 0x7F,
  0x90, 0x40, 0x00,
};

const byte test4data[] = {
  0x90, 0x3c, 0x7f, 
  0x90, 0x32, 0x7f,
  0x90, 0x3f, 0x7f,
  0x90, 0x3f, 0x00,
  0x90, 0x32, 0x00,
  0x90, 0x3c, 0x00, 
};

const byte test5data[] = {
  0x90, 0x3c, 0x6f,
  0x90, 0x48, 0x60,
  0x90, 0x48, 0x00,
  0x90, 0x48, 0x7f,
  0x90, 0x48, 0x00,
  0x90, 0x48, 0x73,
  0x90, 0x48, 0x00,
  0x90, 0x3c, 0x00,
};

const byte test6data[] = {
  0x90, 0x3c, 0x66,
  0xF8, 0xF8, 0xF8, 0xF8,
  0x90, 0x3c, 0x00,
};

int runTest(const byte* data, size_t size, size_t iterations);

/**
    Some Unit tests
 */
int runTest(const byte* data, size_t size, size_t iterations) {
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
		printf("\nfinalVolume:%d\n", totalVolume);
	}
	return 0;
}


/**
    Simulate running
 */
int main(int argc, char** argv, char** envp)
{
	runTest( test3data, sizeof(test3data), 100);
	runTest( test2data, sizeof(test2data), 100);
	runTest( test1data, sizeof(test1data), 100);
	runTest( test4data, sizeof(test4data), 100);
	runTest( test5data, sizeof(test5data), 100);
	runTest( test6data, sizeof(test6data), 100);
	return 0;
}

