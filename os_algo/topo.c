#include <stdio.h>
#include <stdlib.h>

int start[10000],end[10000];
int stack[10000];
int topo[10000];

int main()
{
	int v,e;
	scanf("%d %d",&v,&e);
	
	int i;
	for(i=0;i<e;i++){
		scanf("%d %d",&start[i],&end[i]);
		stack[end[i]]++;
	}
	
	int t=0;
	for(i=0;i<v;i++){
		if(!stack[i]){
			topo[t] = i;
			t++;
		}
	}
	
	int head,j;
	for(i=0;i<v;i++){
		head = topo[i];
		printf("topo %d \n",topo[i]);
		for(j=0;j<e;j++){
			if(start[j]==head && stack[end[j]] != 0){
				stack[end[j]]--;
				if(stack[end[j]] == 0){
					topo[t] = end[j];
					printf("topo %d  value:%d \n",t,end[j]);
					t++;
				}
			}
		}
	}

	for(i=0;i<v;i++){
		printf("junban:%d  value:%d \n",i,topo[i]);
	}
	return 0;
}
