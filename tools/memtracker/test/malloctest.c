#include<stdio.h>  
#include<malloc.h>

int main()  
{  
    int n = 10;
	stacktest();
	malloctest(n);	
    return 0;  
}

void stacktest() {
	int i = 0, j = 20;
	int ret = 0;
	for (; i < j; i++) {
		int n = i + j;
		ret += n;
	}
}

void malloctest(int n) {
	int i = 0;
	for (; i < n; i++) {
		char *p;
		p = (char *)malloc(20);
		if (p)
			printf("Memory Allocated at: %x\n",p);
		else 
			printf("Not Enough Memory!\n");
		free(p);
	}
}
