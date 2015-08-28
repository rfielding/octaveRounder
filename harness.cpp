/**

  Mock the Arduino SDK

 */

typedef unsigned char byte;

#define true 1
#define false 0

//Some totally generic serial device
class SerialDevice
{
public:
	void begin(int rate)
	{
	}

	void write(byte b)
	{
	}

	byte read()
	{
		return 0;
	}

	bool available()
	{
		return true;
	}
} Serial;

#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0

void pinMode(int num, bool val)
{
}

void digitalWrite(int num, bool val)
{
}

bool digitalRead(int num)
{
	return false;
}

#include "octaveRounder.ino"

int main(int argc, char** argv, char** envp)
{
	int iterations = 1000;
	setup();
	for(int i=0; i<iterations; i++)
	{
		loop();
	}
	return 0;
}
