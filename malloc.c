// a simple malloc program

#include<stdio.h>
#include<stdlib.h>

void main()
{
	//creating a integer pointer 'a', getting memory using malloc
	//will assign value to it and print it. 
	
	int *a;
	a = (int *)malloc(sizeof(int));
	*a = 2;
	printf("%d", *a);
	
}
