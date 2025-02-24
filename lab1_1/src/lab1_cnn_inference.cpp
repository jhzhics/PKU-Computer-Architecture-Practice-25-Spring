#include "stdlib.h"
#include "math.h"
#include "string.h"
#include "memory.h"
#include "time.h"
#include <iostream>
#include "sys/time.h"

#define BUF_SIZE 3300000	//Max_size 448*448*16=3211264
#define W_SIZE 15860000		//Max_size 15855536
#define B_SIZE 3200			//Max_size 3181
#define X_SIZE 3200			//Max_size 3181
#define Y_SIZE 3200			//Max_size 3181

using namespace std;

void zero_padding(float *In, int Ri, int Ci, int CHi, float *Out)
{
	memset(Out, 0, BUF_SIZE*sizeof(float));
	int Ro = Ri + 2;
	int Co = Ci + 2;

	for(int chi=0; chi<CHi; chi++)
	{
		for(int ri=0; ri<Ri; ri++)
		{
			for(int ci=0; ci<Ci; ci++)
			{
				Out[chi*Ro*Co + (ri+1)*Co + (ci+1)] = In[chi*Ri*Ci + ri*Ci + ci];
			}
		}
	}
}

void convolution(float *In, float *Out, float *W, int W_addr, float *B, int B_addr, int Ro, int Co, int CHi, int CHo, int K, int S)
{
	//Initialize to 0
	memset(Out, 0, BUF_SIZE*sizeof(float));

	int Ri = Ro+2;
	int Ci = Co+2;

	//Convolution
	for(int cho=0; cho<CHo; cho++)
	{
		for(int chi=0; chi<CHi; chi++)
		{
			for(int ro=0; ro<Ro; ro++)
			{
				for(int co=0; co<Co; co++)
				{
					for(int ki=0; ki<K; ki++)
					{
						for(int kj=0; kj<K; kj++)
						{
							int ri = ro*S + ki;
							int ci = co*S + kj;
							Out[cho*Ro*Co + ro*Co + co] += In[chi*Ri*Ci + ri*Ci + ci] * W[W_addr+cho*CHi*K*K + chi*K*K + ki*K + kj];
						}
					}
				}
			}
		}
	}

	//Bias_adding
	for(int cho=0; cho<CHo; cho++)
	{
		for(int ro=0; ro<Ro; ro++)
		{
			for(int co=0; co<Co; co++)
			{
				Out[cho*Ro*Co + ro*Co + co] += B[B_addr+cho];
			}
		}
	}

}

void activation(float *In, int CHi, int Ri, int Ci)
{
	//Leaky ReLU with factor = 0.1
	for(int chi=0; chi<CHi; chi++)
	{
		for(int ri=0; ri<Ri; ri++)
		{
			for(int ci=0; ci<Ci; ci++)
			{
				In[chi*Ri*Ci + ri*Ci + ci] = In[chi*Ri*Ci + ri*Ci + ci]<0 ? (0.1*In[chi*Ri*Ci + ri*Ci + ci]) : In[chi*Ri*Ci + ri*Ci + ci];
			}
		}
	}
}

void batch_norm(float *In, int CHi, int Ri, int Ci, float *X, int X_addr, float *Y, int Y_addr)
{
	for(int chi=0; chi<CHi; chi++)
	{
		for(int ri=0; ri<Ri; ri++)
		{
			for(int ci=0; ci<Ci; ci++)
			{
				In[chi*Ri*Ci + ri*Ci + ci] = In[chi*Ri*Ci + ri*Ci + ci]*X[X_addr+chi] + Y[Y_addr+chi];
			}
		}
	}
}

float my_max(float a, float b, float c, float d)
{
	float tmp0 = a>b ? a:b;
	float tmp1 = c>d ? c:d;
	float result = tmp0>tmp1 ? tmp0:tmp1;
	return result;
}

void max_pooling(float *In, float *Out, int CHi, int Ri, int Ci)
{
	//Max-pooling in a 2x2 window
	int Ro = Ri/2;
	int Co = Ci/2;
	memset(Out, 0, BUF_SIZE*sizeof(float));

	for(int chi=0; chi<CHi; chi++)
	{
		for(int ro=0; ro<Ro; ro++)
		{
			for(int co=0; co<Co; co++)
			{
				Out[chi*Ro*Co + ro*Ro + co] = my_max(In[chi*Ri*Ci + ro*2*Ri + co*2], In[chi*Ri*Ci + ro*2*Ri + co*2+1],
													 In[chi*Ri*Ci + (ro*2+1)*Ri + co*2], In[chi*Ri*Ci+ (ro*2+1)*Ri + co*2+1]);
			}
		}
	}

}

void Layer_process(float *In, float *Out, float *W, int W_addr, float *B, int B_addr, int Ri, int Ci, int CHi, int CHo,
				   int K, int S, float *X, int X_addr, float *Y, int Y_addr, bool act, bool bn, bool pool)
{
	zero_padding(In, Ri, Ci, CHi, Out);

	convolution(Out, In, W, W_addr, B, B_addr, Ri, Ci, CHi, CHo, K, S);
	W_addr += CHi*CHo*Ri*Ci; B_addr += CHo;

	if(act)
		activation(In, CHi, Ri, Ci);

	if(bn)
	{
		batch_norm(In, CHi, Ri, Ci, X, X_addr, Y, Y_addr);
		 X_addr += CHo; Y_addr += CHo;
	}

	if(pool)
		max_pooling(In, Out, CHi, Ri, Ci);
}

int main() {

	//Allocate mem
	float *Buf0, *Buf1;
	Buf0 = (float *)malloc(BUF_SIZE*sizeof(float));
	Buf1 = (float *)malloc(BUF_SIZE*sizeof(float));
	memset(Buf0, 0, BUF_SIZE*sizeof(float));
	memset(Buf1, 0, BUF_SIZE*sizeof(float));

	float *W, *B, *X, *Y;
	W = (float *)malloc(W_SIZE*sizeof(float));
	B = (float *)malloc(B_SIZE*sizeof(float));
	X = (float *)malloc(X_SIZE*sizeof(float));
	Y = (float *)malloc(Y_SIZE*sizeof(float));

	//Init omitted

	//---CNN Inference Test---
	int w_addr = 0; int b_addr = 0;
	int x_addr=0; int y_addr = 0;

	struct timeval start, end;
	double timeused;
	
	//---Start Benchmark Timing Here---
	gettimeofday(&start, NULL);
	
	//Layer 1
	Layer_process(Buf0, Buf1, W, w_addr, B, b_addr, 448, 448, 3, 16, 3, 1, X, x_addr, Y, y_addr, true, true, true);
	//Layer 2
	Layer_process(Buf1, Buf0, W, w_addr, B, b_addr, 224, 224, 16, 32, 3, 1, X, x_addr, Y, y_addr, true, true, true);
	//Layer 3
	Layer_process(Buf0, Buf1, W, w_addr, B, b_addr, 112, 112, 32, 64, 3, 1, X, x_addr, Y, y_addr, true, true, true);
	//Layer 4
	Layer_process(Buf1, Buf0, W, w_addr, B, b_addr, 56, 56, 64, 128, 3, 1, X, x_addr, Y, y_addr, true, true, true);
	//Layer 5
	Layer_process(Buf0, Buf1, W, w_addr, B, b_addr, 28, 28, 128, 256, 3, 1, X, x_addr, Y, y_addr, true, true, true);
	//Layer 6
	Layer_process(Buf1, Buf0, W, w_addr, B, b_addr, 14, 14, 256, 512, 3, 1, X, x_addr, Y, y_addr, true, true, true);
	//Layer 7
	Layer_process(Buf0, Buf1, W, w_addr, B, b_addr, 7, 7, 512, 1024, 3, 1, X, x_addr, Y, y_addr, true, true, true);
	//Layer 8
	Layer_process(Buf1, Buf0, W, w_addr, B, b_addr, 7, 7, 1024, 1024, 3, 1, X, x_addr, Y, y_addr, true, true, false);
	//Layer 9
	Layer_process(Buf0, Buf1, W, w_addr, B, b_addr, 448, 448, 3, 16, 3, 1, X, x_addr, Y, y_addr, false, false, false);

	//---End Benchmark Timing Here---
	gettimeofday(&end, NULL);

	//Free mem
	free(Buf0); free(Buf1);
	free(W); free(B); free(X); free(Y);

	//Report time used for CNN inference
	timeused = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
	timeused /= 1000000;
	cout<<"Timeused for CNN inference is "<<timeused<<"s."<<endl;
	
	return 0;
}

