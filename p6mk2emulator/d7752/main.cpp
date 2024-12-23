#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <assert.h>
#include <math.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <utility>
#include <map>
#include <iomanip>

#include "d7752e.h"

using namespace std;

#if 0
void Test(istream* in, ostream* out)
{
	D7752* vg = D7752_Open();
	if (!vg)
		throw string("Can't make D7752 object");

	D7752_Start(vg, 0);

	while (!in->eof())
	{
		int i;
		unsigned char param[7];
		for (i=0; i<7; i++)
		{
			unsigned char c;
			char s[64];
			*in >> s;
			c = (unsigned char) strtoul(s, 0, 0);
//			cerr << hex << (int) c << " ";
			param[i] = c;
		}

		D7752_SAMPLE sample[240];
		D7752_Synth(vg, param, sample);

		int size = D7752_GetFrameSize(vg);
		for (i=0; i<size; i++)
		{
			*out << sample[i] << endl;
		}
	}
}
#else
void Test(istream* in, ostream* out)
{
	D7752e* vg = D7752e_Open();
	if (!vg)
		throw string("Can't make D7752e object");

    D7752e_SetMode(vg, 0);
    D7752e_SetRate(vg, 10000, 44100);
    D7752e_SetBufferLength(vg, 14);
    D7752e_SetCommand(vg, 0xfe);

    while ((D7752e_GetStatus(vg) & D7752E_BSY) && !in->eof())
    {
        // パラメータを受け付けられるようならパラメータを与えてやる
        while (D7752e_GetStatus(vg) & D7752E_REQ)
        {
			char s[64];
			*in >> s;
            int v = strtoul(s, 0, 0);
//            cerr << "[" << v << "]";
			D7752e_SetData(vg, (unsigned char) v);
        }

        // 適当にデータを受信
        const int unit = 10;
        D7752_SAMPLE sample[unit];
        D7752e_Synth(vg, sample, unit);
        for (int i=0; i<unit; i++)
        {
            *out << sample[i] << endl;
        }
    }
}
#endif

int main(int ac, char** av)
{
	try
	{
		istream* in = &cin;
		ostream* out = &cout;
		if (ac >= 2)
		{
			in = new ifstream(av[1]);
			if (ac >= 3)
				out = new ofstream(av[2]);
		}
		Test(in, out);
		if (in != &cin)
			delete in;
		if (out != &cout)
			delete out;
	}
	catch (string s)
	{
		cerr << s.c_str() << endl;
	}
	catch (...)
	{
		cerr << "unknown exception" << endl;
	}
	return 0;
}
